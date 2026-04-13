"""Document parsers for PDF, DOCX, TXT/MD, HTML, and JSON."""

from __future__ import annotations

import io
import json
import logging
from typing import Optional

logger = logging.getLogger(__name__)

# ── PDF ───────────────────────────────────────────────────────────────────────

def parse_pdf(content: bytes) -> str:
    """Extract text from a PDF file (via pypdf)."""
    try:
        from pypdf import PdfReader
        reader = PdfReader(io.BytesIO(content))
        pages = []
        for page in reader.pages:
            text = page.extract_text() or ""
            pages.append(text.strip())
        return "\n\n".join(p for p in pages if p)
    except Exception as exc:
        logger.warning("PDF parse error: %s", exc)
        return ""


# ── DOCX ──────────────────────────────────────────────────────────────────────

def parse_docx(content: bytes) -> str:
    """Extract text from a DOCX file (via python-docx)."""
    try:
        import docx
        doc = docx.Document(io.BytesIO(content))
        paragraphs = [p.text.strip() for p in doc.paragraphs if p.text.strip()]
        return "\n\n".join(paragraphs)
    except Exception as exc:
        logger.warning("DOCX parse error: %s", exc)
        return ""


# ── TXT / Markdown ────────────────────────────────────────────────────────────

def parse_text(content: bytes) -> str:
    """Decode plain text or Markdown content."""
    for encoding in ("utf-8", "latin-1", "cp1252"):
        try:
            return content.decode(encoding)
        except (UnicodeDecodeError, ValueError):
            continue
    return content.decode("utf-8", errors="replace")


# ── HTML ──────────────────────────────────────────────────────────────────────

def parse_html(content: bytes) -> str:
    """Extract visible text from HTML (via BeautifulSoup)."""
    try:
        from bs4 import BeautifulSoup
        soup = BeautifulSoup(content, "lxml")
        # Remove script/style elements
        for tag in soup(["script", "style", "head", "meta", "noscript"]):
            tag.decompose()
        text = soup.get_text(separator="\n")
        # Collapse excessive blank lines
        lines = [line.strip() for line in text.splitlines()]
        cleaned = "\n".join(lines)
        import re
        cleaned = re.sub(r"\n{3,}", "\n\n", cleaned)
        return cleaned.strip()
    except Exception as exc:
        logger.warning("HTML parse error: %s", exc)
        return ""


# ── JSON ──────────────────────────────────────────────────────────────────────

def parse_json(content: bytes) -> str:
    """Convert a JSON document to a readable text representation."""
    try:
        data = json.loads(content.decode("utf-8", errors="replace"))
        return _json_to_text(data)
    except Exception as exc:
        logger.warning("JSON parse error: %s", exc)
        return ""


def _json_to_text(obj, depth: int = 0) -> str:
    """Recursively flatten JSON into key: value lines."""
    indent = "  " * depth
    if isinstance(obj, dict):
        lines = []
        for k, v in obj.items():
            if isinstance(v, (dict, list)):
                lines.append(f"{indent}{k}:")
                lines.append(_json_to_text(v, depth + 1))
            else:
                lines.append(f"{indent}{k}: {v}")
        return "\n".join(lines)
    elif isinstance(obj, list):
        parts = [_json_to_text(item, depth) for item in obj]
        return "\n".join(parts)
    else:
        return f"{indent}{obj}"


# ── Dispatcher ────────────────────────────────────────────────────────────────

MIME_PARSERS = {
    "application/pdf": parse_pdf,
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document": parse_docx,
    "text/plain": parse_text,
    "text/markdown": parse_text,
    "text/html": parse_html,
    "application/json": parse_json,
}

EXTENSION_TO_MIME: dict[str, str] = {
    ".pdf": "application/pdf",
    ".docx": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    ".txt": "text/plain",
    ".md": "text/markdown",
    ".html": "text/html",
    ".htm": "text/html",
    ".json": "application/json",
}


def detect_mime_type(filename: str, hint: Optional[str] = None) -> str:
    """Return the MIME type for a file, using the extension as fallback."""
    import os
    if hint and hint in MIME_PARSERS:
        return hint
    ext = os.path.splitext(filename.lower())[1]
    return EXTENSION_TO_MIME.get(ext, "text/plain")


def parse_document(mime_type: str, content: bytes) -> str:
    """Dispatch to the appropriate parser based on MIME type."""
    parser = MIME_PARSERS.get(mime_type, parse_text)
    return parser(content)
