"""Domain models shared across pipeline stages."""

from __future__ import annotations

import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import List, Optional


@dataclass
class RawDocument:
    """A document as read directly from GCS (binary content + metadata)."""

    uri: str                        # gs:// URI
    filename: str
    mime_type: str
    content: bytes                  # raw bytes


@dataclass
class ParsedDocument:
    """Plain-text representation of a document together with its metadata."""

    doc_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    uri: str = ""
    filename: str = ""
    mime_type: str = ""
    doc_type: str = "unknown"       # cv | proposta | contrato | relatorio | unknown
    language: str = "unknown"
    text: str = ""
    char_count: int = 0
    ingest_timestamp: str = field(
        default_factory=lambda: datetime.now(timezone.utc).isoformat()
    )


@dataclass
class Chunk:
    """A text chunk derived from a ParsedDocument."""

    chunk_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    doc_id: str = ""
    chunk_index: int = 0
    section_heading: Optional[str] = None
    text: str = ""
    token_count: int = 0
    embedding: Optional[List[float]] = None
