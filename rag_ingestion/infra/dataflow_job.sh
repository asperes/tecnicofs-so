#!/usr/bin/env bash
# dataflow_job.sh — Submit the RAG ingestion pipeline to Google Cloud Dataflow.
#
# Usage:
#   1. Copy infra/env.example → .env and fill in your values, OR
#      export the required variables manually.
#   2. chmod +x infra/dataflow_job.sh
#   3. ./infra/dataflow_job.sh
#
# Prerequisites:
#   - gcloud authenticated: gcloud auth application-default login
#   - Python virtual-env with requirements.txt installed
#   - Dataflow API enabled: gcloud services enable dataflow.googleapis.com

set -euo pipefail

# ── Load .env if present ──────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/../.env"
if [[ -f "${ENV_FILE}" ]]; then
    # shellcheck source=/dev/null
    set -a
    source "${ENV_FILE}"
    set +a
fi

# ── Validate required variables ───────────────────────────────────────────────
: "${GCP_PROJECT:?GCP_PROJECT is required}"
: "${GCP_REGION:?GCP_REGION is required}"
: "${GCS_INPUT_PREFIX:?GCS_INPUT_PREFIX is required}"
: "${DATAFLOW_STAGING_BUCKET:?DATAFLOW_STAGING_BUCKET is required}"
: "${DATAFLOW_TEMP_LOCATION:?DATAFLOW_TEMP_LOCATION is required}"
: "${DB_HOST:?DB_HOST is required}"
: "${DB_NAME:?DB_NAME is required}"
: "${DB_USER:?DB_USER is required}"
: "${DB_PASSWORD:?DB_PASSWORD is required}"

# ── Optional overrides ────────────────────────────────────────────────────────
JOB_NAME="${JOB_NAME:-rag-ingestion-$(date +%Y%m%d-%H%M%S)}"
MACHINE_TYPE="${MACHINE_TYPE:-n1-standard-4}"
MAX_WORKERS="${MAX_WORKERS:-10}"
NUM_WORKERS="${NUM_WORKERS:-2}"
DISK_SIZE_GB="${DISK_SIZE_GB:-50}"

# ── Move to the pipeline root ─────────────────────────────────────────────────
cd "${SCRIPT_DIR}/.."

echo "==> Submitting Dataflow job: ${JOB_NAME}"
echo "    Project : ${GCP_PROJECT}"
echo "    Region  : ${GCP_REGION}"
echo "    Input   : ${GCS_INPUT_PREFIX}"

python -m pipeline.main \
    --runner=DataflowRunner \
    --project="${GCP_PROJECT}" \
    --region="${GCP_REGION}" \
    --job_name="${JOB_NAME}" \
    --staging_location="${DATAFLOW_STAGING_BUCKET}" \
    --temp_location="${DATAFLOW_TEMP_LOCATION}" \
    --worker_machine_type="${MACHINE_TYPE}" \
    --num_workers="${NUM_WORKERS}" \
    --max_num_workers="${MAX_WORKERS}" \
    --disk_size_gb="${DISK_SIZE_GB}" \
    --experiments=use_runner_v2 \
    --setup_file=./setup.py \
    --save_main_session

echo "==> Job submitted. Monitor at:"
echo "    https://console.cloud.google.com/dataflow/jobs?project=${GCP_PROJECT}"
