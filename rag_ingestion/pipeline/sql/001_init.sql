-- 001_init.sql
-- Initialise the pgvector extension and create the core RAG tables.
--
-- Run once against your CloudSQL PostgreSQL instance:
--   psql -h <DB_HOST> -U <DB_USER> -d <DB_NAME> -f 001_init.sql
--
-- NOTE: Adjust the vector dimension (768) if you change the embedding model.
--   text-embedding-005  → 768
--   textembedding-gecko → 768
--   text-embedding-004  → 768 (or 256 / 512 if using reduced output)
-- After changing the dimension, DROP and re-CREATE the rag_chunks table.

-- ── Extension ─────────────────────────────────────────────────────────────

CREATE EXTENSION IF NOT EXISTS vector;

-- ── Documents table ────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS rag_documents (
    doc_id            TEXT        PRIMARY KEY,
    uri               TEXT        NOT NULL,           -- gs:// URI
    filename          TEXT        NOT NULL,
    mime_type         TEXT        NOT NULL,
    doc_type          TEXT        NOT NULL DEFAULT 'unknown',
                                                      -- cv | proposta | contrato | relatorio | unknown
    language          TEXT        NOT NULL DEFAULT 'unknown',
    char_count        INTEGER     NOT NULL DEFAULT 0,
    ingest_timestamp  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    extra_metadata    JSONB       NOT NULL DEFAULT '{}'
);

-- ── Chunks table ───────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS rag_chunks (
    chunk_id          TEXT        PRIMARY KEY,
    doc_id            TEXT        NOT NULL REFERENCES rag_documents(doc_id) ON DELETE CASCADE,
    chunk_index       INTEGER     NOT NULL,
    section_heading   TEXT,                           -- NULL when no heading detected
    chunk_text        TEXT        NOT NULL,
    token_count       INTEGER     NOT NULL DEFAULT 0,
    -- Change 768 → your model's output dimension if needed (see NOTE above).
    embedding         vector(768)
);
