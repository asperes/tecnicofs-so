"""Metadata extraction: document type inference, language detection."""

from __future__ import annotations

import logging
import re
from typing import Optional

logger = logging.getLogger(__name__)

# ── Document type inference ───────────────────────────────────────────────────

# Keywords (case-insensitive) that suggest a particular document type.
# The first type whose keywords appear most frequently wins.
_DOC_TYPE_KEYWORDS: dict[str, list[str]] = {
    "cv": [
        "curriculum", "vitae", "cv", "resumo profissional", "experiência profissional",
        "habilitações", "competências", "skills", "education", "work experience",
        "professional experience",
    ],
    "proposta": [
        "proposta", "proposal", "orçamento", "budget", "solução proposta",
        "proposta técnica", "escopo", "scope of work", "deliverables",
    ],
    "contrato": [
        "contrato", "contract", "acordo", "agreement", "cláusula", "clause",
        "partes contratantes", "contraente", "rescisão", "termination",
    ],
    "relatorio": [
        "relatório", "report", "sumário executivo", "executive summary",
        "conclusões", "conclusions", "análise", "analysis", "resultados",
        "findings",
    ],
}


def infer_doc_type(text: str, filename: str = "") -> str:
    """Return a document type label based on keyword frequency."""
    text_lower = (filename + " " + text[:4000]).lower()
    scores: dict[str, int] = {}
    for doc_type, keywords in _DOC_TYPE_KEYWORDS.items():
        score = sum(text_lower.count(kw) for kw in keywords)
        if score > 0:
            scores[doc_type] = score
    if not scores:
        return "unknown"
    return max(scores, key=lambda k: scores[k])


# ── Language detection ────────────────────────────────────────────────────────

def detect_language(text: str) -> str:
    """Return ISO 639-1 language code, e.g. 'pt', 'en'.  Falls back to 'unknown'."""
    if not text or len(text.strip()) < 20:
        return "unknown"
    try:
        from langdetect import detect, LangDetectException
        return detect(text[:500])
    except Exception:
        return "unknown"


# ── Heading detection (used by chunking) ─────────────────────────────────────

# Markdown headings: # Title, ## Section, ### Subsection …
_RE_MD_HEADING = re.compile(r"^(#{1,6})\s+(.+)$")

# Numbered headings: "1.", "1.1", "2.3.1" etc.
_RE_NUMBERED_HEADING = re.compile(r"^\s*(\d+(?:\.\d+)*\.?)\s+([A-ZÁÉÍÓÚÀÂÃÊÔÜÇ][^\n]{3,80})$")

# UPPERCASE TITLE LINES (≥3 words, all caps, not too long)
_RE_UPPER_HEADING = re.compile(r"^[A-ZÁÉÍÓÚÀÂÃÊÔÜÇ\s]{3,80}$")


def is_heading(line: str) -> Optional[str]:
    """Return the heading text if the line looks like a section heading, else None."""
    stripped = line.strip()
    if not stripped:
        return None

    m = _RE_MD_HEADING.match(stripped)
    if m:
        return m.group(2).strip()

    m = _RE_NUMBERED_HEADING.match(stripped)
    if m:
        return stripped

    words = stripped.split()
    if (
        len(words) >= 2
        and _RE_UPPER_HEADING.match(stripped)
        and stripped == stripped.upper()
        and len(stripped) <= 80
    ):
        return stripped

    return None
