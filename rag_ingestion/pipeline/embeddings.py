"""Generate text embeddings via Vertex AI.

Uses the Vertex AI text-embedding API (model: text-embedding-005 by default).
Batches requests to stay within API limits.

Dimension note
--------------
text-embedding-005 → 768 dimensions.
If you change EMBEDDING_MODEL, also update EMBEDDING_DIM in your .env and
re-run 001_init.sql so the vector column uses the correct size.
"""

from __future__ import annotations

import logging
import time
from typing import List

from tenacity import retry, stop_after_attempt, wait_exponential

logger = logging.getLogger(__name__)


@retry(
    stop=stop_after_attempt(5),
    wait=wait_exponential(multiplier=1, min=2, max=30),
    reraise=True,
)
def _embed_batch(
    texts: List[str],
    model_name: str,
    project: str,
    location: str,
) -> List[List[float]]:
    """Call the Vertex AI Embeddings API for a batch of texts."""
    from vertexai.language_models import TextEmbeddingModel
    import vertexai

    vertexai.init(project=project, location=location)
    model = TextEmbeddingModel.from_pretrained(model_name)
    embeddings = model.get_embeddings(texts)
    return [e.values for e in embeddings]


def generate_embeddings(
    texts: List[str],
    model_name: str = "text-embedding-005",
    project: str = "",
    region: str = "europe-west1",
    batch_size: int = 5,
) -> List[List[float]]:
    """
    Generate embeddings for a list of texts, processing in batches.

    Args:
        texts:      List of text strings to embed.
        model_name: Vertex AI model identifier.
        project:    GCP project ID.
        region:     GCP region for Vertex AI endpoint.
        batch_size: Number of texts per API call (keep ≤ 5 to stay within limits).

    Returns:
        List of float vectors, one per input text.
    """
    all_vectors: List[List[float]] = []
    for i in range(0, len(texts), batch_size):
        batch = texts[i : i + batch_size]
        logger.debug("Embedding batch %d-%d (size=%d)", i, i + len(batch), len(batch))
        vectors = _embed_batch(batch, model_name, project, region)
        all_vectors.extend(vectors)
        # Brief pause to avoid hitting quota limits
        if i + batch_size < len(texts):
            time.sleep(0.2)
    return all_vectors
