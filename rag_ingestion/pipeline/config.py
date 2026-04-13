"""Pipeline configuration — loaded from environment variables or .env file."""

import os
from dataclasses import dataclass, field
from dotenv import load_dotenv

load_dotenv()  # loads .env / infra/env.example values if present


@dataclass
class PipelineConfig:
    # ── GCS ────────────────────────────────────────────────────────────────
    gcs_input_prefix: str = field(
        default_factory=lambda: os.environ["GCS_INPUT_PREFIX"]
    )  # e.g. gs://my-bucket/docs/

    # ── GCP Project / Region ───────────────────────────────────────────────
    project: str = field(default_factory=lambda: os.environ["GCP_PROJECT"])
    region: str = field(default_factory=lambda: os.getenv("GCP_REGION", "europe-west1"))

    # ── Vertex AI Embeddings ───────────────────────────────────────────────
    embedding_model: str = field(
        default_factory=lambda: os.getenv("EMBEDDING_MODEL", "text-embedding-005")
    )
    # text-embedding-005 outputs 768-dimensional vectors.
    # If you switch models, update EMBEDDING_DIM and re-run 001_init.sql.
    embedding_dim: int = field(
        default_factory=lambda: int(os.getenv("EMBEDDING_DIM", "768"))
    )
    embedding_batch_size: int = field(
        default_factory=lambda: int(os.getenv("EMBEDDING_BATCH_SIZE", "5"))
    )

    # ── Chunking ───────────────────────────────────────────────────────────
    chunk_max_tokens: int = field(
        default_factory=lambda: int(os.getenv("CHUNK_MAX_TOKENS", "512"))
    )
    chunk_overlap_tokens: int = field(
        default_factory=lambda: int(os.getenv("CHUNK_OVERLAP_TOKENS", "64"))
    )

    # ── CloudSQL / pgvector ────────────────────────────────────────────────
    db_host: str = field(default_factory=lambda: os.environ["DB_HOST"])
    db_port: int = field(default_factory=lambda: int(os.getenv("DB_PORT", "5432")))
    db_name: str = field(default_factory=lambda: os.environ["DB_NAME"])
    db_user: str = field(default_factory=lambda: os.environ["DB_USER"])
    db_password: str = field(default_factory=lambda: os.environ["DB_PASSWORD"])

    @property
    def db_url(self) -> str:
        return (
            f"postgresql+psycopg2://{self.db_user}:{self.db_password}"
            f"@{self.db_host}:{self.db_port}/{self.db_name}"
        )


def get_config() -> PipelineConfig:
    """Return a fully-populated PipelineConfig from the current environment."""
    return PipelineConfig()
