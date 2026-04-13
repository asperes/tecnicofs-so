"""Write parsed documents and chunks (with embeddings) to CloudSQL / pgvector.

Tables used:
  - rag_documents  (see sql/001_init.sql)
  - rag_chunks     (see sql/001_init.sql)
"""

from __future__ import annotations

import json
import logging
from typing import List, Optional

from sqlalchemy import create_engine, text
from sqlalchemy.engine import Engine
from tenacity import retry, stop_after_attempt, wait_exponential

from pipeline.models import Chunk, ParsedDocument

logger = logging.getLogger(__name__)


def _get_engine(db_url: str) -> Engine:
    return create_engine(db_url, pool_pre_ping=True, pool_size=4, max_overflow=2)


@retry(stop=stop_after_attempt(5), wait=wait_exponential(multiplier=1, min=2, max=30))
def upsert_document(doc: ParsedDocument, db_url: str) -> None:
    """Insert or update a document record in rag_documents."""
    engine = _get_engine(db_url)
    sql = text(
        """
        INSERT INTO rag_documents
            (doc_id, uri, filename, mime_type, doc_type, language,
             char_count, ingest_timestamp, extra_metadata)
        VALUES
            (:doc_id, :uri, :filename, :mime_type, :doc_type, :language,
             :char_count, :ingest_timestamp, :extra_metadata::jsonb)
        ON CONFLICT (doc_id) DO UPDATE SET
            uri               = EXCLUDED.uri,
            filename          = EXCLUDED.filename,
            mime_type         = EXCLUDED.mime_type,
            doc_type          = EXCLUDED.doc_type,
            language          = EXCLUDED.language,
            char_count        = EXCLUDED.char_count,
            ingest_timestamp  = EXCLUDED.ingest_timestamp,
            extra_metadata    = EXCLUDED.extra_metadata
        """
    )
    with engine.begin() as conn:
        conn.execute(
            sql,
            {
                "doc_id": doc.doc_id,
                "uri": doc.uri,
                "filename": doc.filename,
                "mime_type": doc.mime_type,
                "doc_type": doc.doc_type,
                "language": doc.language,
                "char_count": doc.char_count,
                "ingest_timestamp": doc.ingest_timestamp,
                "extra_metadata": json.dumps({}),
            },
        )
    engine.dispose()


@retry(stop=stop_after_attempt(5), wait=wait_exponential(multiplier=1, min=2, max=30))
def upsert_chunks(chunks: List[Chunk], db_url: str) -> None:
    """Insert or update chunk records (with vectors) in rag_chunks."""
    if not chunks:
        return
    engine = _get_engine(db_url)
    sql = text(
        """
        INSERT INTO rag_chunks
            (chunk_id, doc_id, chunk_index, section_heading,
             chunk_text, token_count, embedding)
        VALUES
            (:chunk_id, :doc_id, :chunk_index, :section_heading,
             :chunk_text, :token_count, :embedding)
        ON CONFLICT (chunk_id) DO UPDATE SET
            doc_id          = EXCLUDED.doc_id,
            chunk_index     = EXCLUDED.chunk_index,
            section_heading = EXCLUDED.section_heading,
            chunk_text      = EXCLUDED.chunk_text,
            token_count     = EXCLUDED.token_count,
            embedding       = EXCLUDED.embedding
        """
    )
    rows = [
        {
            "chunk_id": chunk.chunk_id,
            "doc_id": chunk.doc_id,
            "chunk_index": chunk.chunk_index,
            "section_heading": chunk.section_heading,
            "chunk_text": chunk.text,
            "token_count": chunk.token_count,
            # pgvector expects a Python list; SQLAlchemy passes it as an array literal.
            "embedding": chunk.embedding,
        }
        for chunk in chunks
    ]
    with engine.begin() as conn:
        conn.executemany(sql, rows)
    engine.dispose()
