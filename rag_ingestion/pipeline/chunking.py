"""Intelligent text chunking.

Strategy (in order of preference):
1. Split text into sections by heading detection.
2. Within each section, split by paragraph boundaries.
3. If a paragraph is too large, split at sentence boundaries.
4. Apply token-aware overlap between consecutive chunks.
"""

from __future__ import annotations

import re
import logging
from typing import List, Optional, Tuple

from pipeline.metadata import is_heading
from pipeline.models import Chunk

logger = logging.getLogger(__name__)

# Sentence boundary pattern (supports English and Portuguese abbreviations).
_RE_SENTENCE_SPLIT = re.compile(r"(?<=[.!?])\s+(?=[A-ZÁÉÍÓÚÀÂÃÊÔÜÇ\"\'\(])")


def _count_tokens(text: str) -> int:
    """Approximate token count using tiktoken (cl100k_base encoder)."""
    try:
        import tiktoken
        enc = tiktoken.get_encoding("cl100k_base")
        return len(enc.encode(text))
    except Exception:
        # Fallback: rough approximation (1 token ≈ 4 chars)
        return max(1, len(text) // 4)


def _split_into_sentences(text: str) -> List[str]:
    return [s.strip() for s in _RE_SENTENCE_SPLIT.split(text) if s.strip()]


def _merge_sentences_into_chunks(
    sentences: List[str],
    max_tokens: int,
    overlap_tokens: int,
) -> List[str]:
    """Pack sentences greedily into chunks respecting max_tokens."""
    chunks: List[str] = []
    current_sentences: List[str] = []
    current_tokens = 0

    for sentence in sentences:
        s_tokens = _count_tokens(sentence)
        if current_tokens + s_tokens > max_tokens and current_sentences:
            chunks.append(" ".join(current_sentences))
            # Carry-over: keep tail sentences whose total ≤ overlap_tokens
            overlap_sentences: List[str] = []
            tail_tokens = 0
            for prev in reversed(current_sentences):
                pt = _count_tokens(prev)
                if tail_tokens + pt > overlap_tokens:
                    break
                overlap_sentences.insert(0, prev)
                tail_tokens += pt
            current_sentences = overlap_sentences
            current_tokens = tail_tokens
        current_sentences.append(sentence)
        current_tokens += s_tokens

    if current_sentences:
        chunks.append(" ".join(current_sentences))
    return chunks


def _chunk_paragraph(
    paragraph: str,
    max_tokens: int,
    overlap_tokens: int,
) -> List[str]:
    """Return one or more chunks for a single paragraph."""
    if _count_tokens(paragraph) <= max_tokens:
        return [paragraph]
    sentences = _split_into_sentences(paragraph)
    if not sentences:
        sentences = [paragraph]
    return _merge_sentences_into_chunks(sentences, max_tokens, overlap_tokens)


def _split_by_headings(text: str) -> List[Tuple[Optional[str], str]]:
    """Return list of (heading | None, body_text) sections."""
    lines = text.splitlines()
    sections: List[Tuple[Optional[str], str]] = []
    current_heading: Optional[str] = None
    current_lines: List[str] = []

    for line in lines:
        heading = is_heading(line)
        if heading:
            if current_lines:
                body = "\n".join(current_lines).strip()
                if body:
                    sections.append((current_heading, body))
            current_heading = heading
            current_lines = []
        else:
            current_lines.append(line)

    if current_lines:
        body = "\n".join(current_lines).strip()
        if body:
            sections.append((current_heading, body))

    # If no headings were found, treat the whole text as one section.
    if not sections:
        sections = [(None, text.strip())]

    return sections


def chunk_document(
    doc_id: str,
    text: str,
    max_tokens: int = 512,
    overlap_tokens: int = 64,
) -> List[Chunk]:
    """
    Split *text* into overlapping token-bounded chunks.

    Returns a list of :class:`Chunk` objects with section_heading and chunk_index
    populated; embeddings are not filled here.
    """
    if not text or not text.strip():
        return []

    sections = _split_by_headings(text)
    chunks: List[Chunk] = []
    chunk_index = 0

    for heading, body in sections:
        # Split body into paragraphs (blank-line separated).
        paragraphs = [p.strip() for p in re.split(r"\n{2,}", body) if p.strip()]
        if not paragraphs:
            paragraphs = [body]

        # Buffer paragraphs into token-aware groups before chunking.
        current_para_group: List[str] = []
        current_group_tokens = 0

        for para in paragraphs:
            para_tokens = _count_tokens(para)
            if current_group_tokens + para_tokens > max_tokens and current_para_group:
                group_text = "\n\n".join(current_para_group)
                for chunk_text in _chunk_paragraph(group_text, max_tokens, overlap_tokens):
                    if not chunk_text.strip():
                        continue
                    chunk = Chunk(
                        doc_id=doc_id,
                        chunk_index=chunk_index,
                        section_heading=heading,
                        text=chunk_text.strip(),
                        token_count=_count_tokens(chunk_text),
                    )
                    chunks.append(chunk)
                    chunk_index += 1
                # Overlap: keep last paragraph if it fits within overlap budget
                if _count_tokens(current_para_group[-1]) <= overlap_tokens:
                    current_para_group = [current_para_group[-1]]
                    current_group_tokens = _count_tokens(current_para_group[-1])
                else:
                    current_para_group = []
                    current_group_tokens = 0
            current_para_group.append(para)
            current_group_tokens += para_tokens

        if current_para_group:
            group_text = "\n\n".join(current_para_group)
            for chunk_text in _chunk_paragraph(group_text, max_tokens, overlap_tokens):
                if not chunk_text.strip():
                    continue
                chunk = Chunk(
                    doc_id=doc_id,
                    chunk_index=chunk_index,
                    section_heading=heading,
                    text=chunk_text.strip(),
                    token_count=_count_tokens(chunk_text),
                )
                chunks.append(chunk)
                chunk_index += 1

    return chunks
