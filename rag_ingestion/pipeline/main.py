"""Apache Beam pipeline — GCS → parse → chunk → embed → CloudSQL/pgvector.

Run locally (DirectRunner):
    python -m pipeline.main

Run on Dataflow:
    See infra/dataflow_job.sh
"""

from __future__ import annotations

import argparse
import logging
import os

import apache_beam as beam
from apache_beam.options.pipeline_options import PipelineOptions, SetupOptions

from pipeline.chunking import chunk_document
from pipeline.config import PipelineConfig, get_config
from pipeline.embeddings import generate_embeddings
from pipeline.gcs_io import list_gcs_objects, read_gcs_document
from pipeline.metadata import detect_language, infer_doc_type
from pipeline.models import ParsedDocument, RawDocument
from pipeline.parsers import parse_document
from pipeline.sinks import upsert_chunks, upsert_document

logger = logging.getLogger(__name__)


# ── DoFn definitions ──────────────────────────────────────────────────────────

class ReadFromGCSFn(beam.DoFn):
    """Emit one RawDocument per file under the configured GCS prefix.

    Note: This DoFn is seeded by a single-element PCollection and lists GCS
    objects serially. This is intentional for a starter template and works well
    for moderate-sized prefixes (hundreds of files). For very large prefixes,
    consider splitting the listing into parallel sub-prefixes or using a
    dedicated Beam source with splittable DoFn.
    """

    def __init__(self, gcs_prefix: str):
        self._prefix = gcs_prefix

    def process(self, _ignored):
        for blob in list_gcs_objects(self._prefix):
            try:
                yield read_gcs_document(blob)
            except Exception as exc:
                logger.error("Failed to read %s: %s", blob.name, exc)


class ParseDocumentFn(beam.DoFn):
    """Convert a RawDocument into a ParsedDocument."""

    def process(self, raw: RawDocument):
        try:
            text = parse_document(raw.mime_type, raw.content)
            doc_type = infer_doc_type(text, raw.filename)
            language = detect_language(text)
            parsed = ParsedDocument(
                uri=raw.uri,
                filename=raw.filename,
                mime_type=raw.mime_type,
                doc_type=doc_type,
                language=language,
                text=text,
                char_count=len(text),
            )
            yield parsed
        except Exception as exc:
            logger.error("Parse error for %s: %s", raw.uri, exc)


class ChunkDocumentFn(beam.DoFn):
    """Emit (ParsedDocument, List[Chunk]) for each parsed document."""

    def __init__(self, max_tokens: int, overlap_tokens: int):
        self._max_tokens = max_tokens
        self._overlap_tokens = overlap_tokens

    def process(self, doc: ParsedDocument):
        try:
            chunks = chunk_document(
                doc_id=doc.doc_id,
                text=doc.text,
                max_tokens=self._max_tokens,
                overlap_tokens=self._overlap_tokens,
            )
            if chunks:
                yield (doc, chunks)
            else:
                logger.warning("No chunks produced for %s", doc.uri)
        except Exception as exc:
            logger.error("Chunking error for %s: %s", doc.uri, exc)


class EmbedAndSinkFn(beam.DoFn):
    """Generate embeddings for all chunks and persist doc + chunks to the DB."""

    def __init__(self, config: PipelineConfig):
        self._config = config

    def process(self, element):
        doc, chunks = element
        try:
            # Generate embeddings
            texts = [c.text for c in chunks]
            vectors = generate_embeddings(
                texts=texts,
                model_name=self._config.embedding_model,
                project=self._config.project,
                region=self._config.region,
                batch_size=self._config.embedding_batch_size,
            )
            for chunk, vector in zip(chunks, vectors):
                chunk.embedding = vector

            # Persist
            upsert_document(doc, self._config.db_url)
            upsert_chunks(chunks, self._config.db_url)

            logger.info(
                "Ingested %s → %d chunks (doc_type=%s, lang=%s)",
                doc.filename,
                len(chunks),
                doc.doc_type,
                doc.language,
            )
        except Exception as exc:
            logger.error("Embed/sink error for %s: %s", doc.uri, exc)


# ── Pipeline builder ──────────────────────────────────────────────────────────

def build_pipeline(pipeline, cfg: PipelineConfig):
    """Attach all transforms to *pipeline* and return the final PCollection."""
    return (
        pipeline
        | "Seed" >> beam.Create([None])
        | "ReadGCS" >> beam.ParDo(ReadFromGCSFn(cfg.gcs_input_prefix))
        | "ParseDocs" >> beam.ParDo(ParseDocumentFn())
        | "ChunkDocs" >> beam.ParDo(ChunkDocumentFn(cfg.chunk_max_tokens, cfg.chunk_overlap_tokens))
        | "EmbedAndSink" >> beam.ParDo(EmbedAndSinkFn(cfg))
    )


# ── Entry point ───────────────────────────────────────────────────────────────

def run(argv=None):
    parser = argparse.ArgumentParser(description="RAG ingestion pipeline")
    _, beam_args = parser.parse_known_args(argv)

    cfg = get_config()

    options = PipelineOptions(beam_args)
    options.view_as(SetupOptions).save_main_session = True

    with beam.Pipeline(options=options) as p:
        build_pipeline(p, cfg)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    run()
