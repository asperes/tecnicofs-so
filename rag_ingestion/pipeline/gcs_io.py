"""Read documents from a GCS prefix and yield RawDocument objects."""

from __future__ import annotations

import logging
import os
from typing import Iterator

from google.cloud import storage

from pipeline.models import RawDocument
from pipeline.parsers import detect_mime_type

logger = logging.getLogger(__name__)

# Supported file extensions
SUPPORTED_EXTENSIONS = {".pdf", ".docx", ".txt", ".md", ".html", ".htm", ".json"}


def list_gcs_objects(gcs_prefix: str) -> Iterator[storage.Blob]:
    """Yield GCS blobs under the given gs:// prefix."""
    if not gcs_prefix.startswith("gs://"):
        raise ValueError(f"gcs_prefix must start with gs://, got: {gcs_prefix!r}")

    # Parse bucket and object prefix from the URI
    without_scheme = gcs_prefix[len("gs://"):]
    parts = without_scheme.split("/", 1)
    bucket_name = parts[0]
    prefix = parts[1] if len(parts) > 1 else ""

    client = storage.Client()
    bucket = client.bucket(bucket_name)
    for blob in bucket.list_blobs(prefix=prefix):
        _, ext = os.path.splitext(blob.name.lower())
        if ext in SUPPORTED_EXTENSIONS:
            yield blob


def read_gcs_document(blob: storage.Blob) -> RawDocument:
    """Download a GCS blob and wrap it in a RawDocument."""
    filename = os.path.basename(blob.name)
    content = blob.download_as_bytes()
    mime_type = detect_mime_type(
        filename=filename,
        hint=blob.content_type,
    )
    return RawDocument(
        uri=f"gs://{blob.bucket.name}/{blob.name}",
        filename=filename,
        mime_type=mime_type,
        content=content,
    )
