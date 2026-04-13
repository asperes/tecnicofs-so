-- 002_indexes.sql
-- Practical indexes for the RAG tables.
--
-- Run after 001_init.sql:
--   psql -h <DB_HOST> -U <DB_USER> -d <DB_NAME> -f 002_indexes.sql

-- ── rag_documents indexes ─────────────────────────────────────────────────

-- Fast lookup by GCS URI
CREATE INDEX IF NOT EXISTS idx_rag_documents_uri
    ON rag_documents (uri);

-- Filter documents by inferred type (cv, proposta, contrato, …)
CREATE INDEX IF NOT EXISTS idx_rag_documents_doc_type
    ON rag_documents (doc_type);

-- Filter documents by detected language
CREATE INDEX IF NOT EXISTS idx_rag_documents_language
    ON rag_documents (language);

-- JSONB GIN index — supports containment (@>) and key-existence (?) queries
-- on extra_metadata, e.g.: WHERE extra_metadata @> '{"author": "Alice"}'
CREATE INDEX IF NOT EXISTS idx_rag_documents_extra_metadata_gin
    ON rag_documents USING GIN (extra_metadata);

-- ── rag_chunks indexes ────────────────────────────────────────────────────

-- Fast lookup of all chunks belonging to a document
CREATE INDEX IF NOT EXISTS idx_rag_chunks_doc_id
    ON rag_chunks (doc_id);

-- Ordered retrieval of chunks within a document
CREATE INDEX IF NOT EXISTS idx_rag_chunks_doc_id_chunk_index
    ON rag_chunks (doc_id, chunk_index);

-- Filter chunks by section heading
CREATE INDEX IF NOT EXISTS idx_rag_chunks_section_heading
    ON rag_chunks (section_heading)
    WHERE section_heading IS NOT NULL;

-- ── Vector (ANN) index ────────────────────────────────────────────────────
-- IVFFlat index for approximate nearest-neighbour search.
-- Requires at least ~1 000 rows before it becomes faster than a seq scan.
-- Tune lists = sqrt(row_count). Set ivfflat.probes at query time for recall/speed trade-off.
--
-- Uncomment after you have loaded a representative dataset:
--
-- CREATE INDEX IF NOT EXISTS idx_rag_chunks_embedding_ivfflat
--     ON rag_chunks USING ivfflat (embedding vector_cosine_ops)
--     WITH (lists = 100);
--
-- For smaller datasets (< 100k rows) an HNSW index is often better:
--
-- CREATE INDEX IF NOT EXISTS idx_rag_chunks_embedding_hnsw
--     ON rag_chunks USING hnsw (embedding vector_cosine_ops)
--     WITH (m = 16, ef_construction = 64);
