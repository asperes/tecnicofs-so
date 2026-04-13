# RAG Ingestion — Dataflow + Cloud Storage + CloudSQL pgvector

Pipeline de ingestão de documentos para RAG (Retrieval-Augmented Generation) no Google Cloud Platform, construído com **Apache Beam / Dataflow**, **Cloud Storage** e **CloudSQL PostgreSQL + pgvector**.

---

## Índice

1. [Pré-requisitos e APIs](#pré-requisitos-e-apis)
2. [Estrutura do projecto](#estrutura-do-projecto)
3. [Configuração do Cloud Storage](#configuração-do-cloud-storage)
4. [Configuração do CloudSQL + pgvector](#configuração-do-cloudsql--pgvector)
5. [Configuração local do ambiente](#configuração-local-do-ambiente)
6. [Execução local (DirectRunner)](#execução-local-directrunner)
7. [Deploy e execução no Dataflow](#deploy-e-execução-no-dataflow)
8. [Modelo de metadados](#modelo-de-metadados)
9. [Exemplo de query vetorial](#exemplo-de-query-vetorial)
10. [Melhorias recomendadas](#melhorias-recomendadas)

---

## Pré-requisitos e APIs

### Software local

| Ferramenta | Versão mínima |
|---|---|
| Python | 3.10+ |
| gcloud CLI | última versão |
| psql | 14+ |

### APIs GCP a activar

```bash
gcloud services enable \
    dataflow.googleapis.com \
    storage.googleapis.com \
    sqladmin.googleapis.com \
    aiplatform.googleapis.com \
    secretmanager.googleapis.com \
    --project=SEU_PROJECTO
```

### Autenticação

```bash
gcloud auth application-default login
gcloud config set project SEU_PROJECTO
```

---

## Estrutura do projecto

```
rag_ingestion/
├── README.md                   # Este ficheiro
├── requirements.txt            # Dependências Python
├── infra/
│   ├── env.example             # Variáveis de ambiente (copiar para .env)
│   └── dataflow_job.sh         # Script de submit para Dataflow
└── pipeline/
    ├── __init__.py
    ├── main.py                 # Entry point do pipeline Beam
    ├── config.py               # Configuração via env vars
    ├── models.py               # Modelos de dados (dataclasses)
    ├── parsers.py              # Parsers de documentos (PDF, DOCX, TXT, HTML, JSON)
    ├── metadata.py             # Inferência de tipo de doc, detecção de língua, headings
    ├── chunking.py             # Chunking inteligente com overlap
    ├── embeddings.py           # Geração de embeddings via Vertex AI
    ├── gcs_io.py               # Leitura de documentos do GCS
    ├── sinks.py                # Escrita para CloudSQL / pgvector
    └── sql/
        ├── 001_init.sql        # Criação da extensão e tabelas
        └── 002_indexes.sql     # Índices (JSONB GIN, lookup, ANN vector)
```

---

## Configuração do Cloud Storage

```bash
# Criar bucket para os documentos
gsutil mb -p SEU_PROJECTO -l europe-west1 gs://meu-bucket-docs/

# Criar pastas de staging/temp para o Dataflow
gsutil mb -p SEU_PROJECTO -l europe-west1 gs://meu-bucket-dataflow/

# Carregar documentos de teste
gsutil -m cp /caminho/local/docs/*.pdf gs://meu-bucket-docs/docs/
```

---

## Configuração do CloudSQL + pgvector

### 1. Criar instância CloudSQL PostgreSQL 15

```bash
gcloud sql instances create rag-db \
    --database-version=POSTGRES_15 \
    --tier=db-custom-2-7680 \
    --region=europe-west1 \
    --storage-size=20GB \
    --storage-auto-increase \
    --no-assign-ip \
    --project=SEU_PROJECTO
```

### 2. Criar base de dados e utilizador

```bash
gcloud sql databases create ragdb --instance=rag-db
gcloud sql users create raguser --instance=rag-db --password=SENHA_SEGURA
```

### 3. Ligar via Cloud SQL Auth Proxy

```bash
# Instalar o Auth Proxy (se não estiver instalado)
curl -o cloud-sql-proxy https://storage.googleapis.com/cloud-sql-connectors/cloud-sql-proxy/v2.11.0/cloud-sql-proxy.linux.amd64
chmod +x cloud-sql-proxy

# Iniciar proxy em background
./cloud-sql-proxy SEU_PROJECTO:europe-west1:rag-db --port 5432 &
```

### 4. Aplicar migrações SQL

```bash
psql -h 127.0.0.1 -U raguser -d ragdb -f pipeline/sql/001_init.sql
psql -h 127.0.0.1 -U raguser -d ragdb -f pipeline/sql/002_indexes.sql
```

> **Nota sobre dimensão do vector:** O modelo `text-embedding-005` gera vectores de **768 dimensões**.
> Se mudar o modelo, actualize `EMBEDDING_DIM` no `.env` e recrie a coluna `embedding` em `rag_chunks`
> (`vector(NOVA_DIMENSAO)`). Consulte a documentação de cada modelo para confirmar a dimensão.

---

## Configuração local do ambiente

```bash
# A partir de rag_ingestion/
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Copiar e editar variáveis de ambiente
cp infra/env.example .env
# Editar .env com os seus valores reais
```

Variáveis obrigatórias no `.env`:

| Variável | Descrição |
|---|---|
| `GCP_PROJECT` | ID do projecto GCP |
| `GCP_REGION` | Região (ex: `europe-west1`) |
| `GCS_INPUT_PREFIX` | Prefixo GCS dos documentos (ex: `gs://bucket/docs/`) |
| `DATAFLOW_STAGING_BUCKET` | GCS para staging do Dataflow |
| `DATAFLOW_TEMP_LOCATION` | GCS para ficheiros temporários |
| `DB_HOST` | Host da BD (127.0.0.1 com Auth Proxy) |
| `DB_NAME` | Nome da base de dados |
| `DB_USER` | Utilizador PostgreSQL |
| `DB_PASSWORD` | Password PostgreSQL |

---

## Execução local (DirectRunner)

```bash
cd rag_ingestion
source .venv/bin/activate

# Carregar variáveis de ambiente
export $(grep -v '^#' .env | xargs)

# Executar pipeline localmente
python -m pipeline.main --runner=DirectRunner
```

O pipeline irá:
1. Listar todos os ficheiros sob `GCS_INPUT_PREFIX`
2. Fazer download e parsing de cada documento
3. Inferir o tipo de documento e língua
4. Dividir em chunks inteligentes com overlap
5. Gerar embeddings via Vertex AI
6. Persistir documentos e chunks no CloudSQL

---

## Deploy e execução no Dataflow

### Criar setup.py (necessário para Dataflow empacotar o módulo)

Crie um ficheiro `rag_ingestion/setup.py`:

```python
from setuptools import setup, find_packages

setup(
    name="rag_ingestion",
    version="1.0.0",
    packages=find_packages(),
    install_requires=open("requirements.txt").read().splitlines(),
)
```

### Submeter o job

```bash
cd rag_ingestion
chmod +x infra/dataflow_job.sh
./infra/dataflow_job.sh
```

O script lê automaticamente o ficheiro `.env` se existir.

### Monitorizar

```
https://console.cloud.google.com/dataflow/jobs?project=SEU_PROJECTO
```

Variáveis opcionais para ajustar o job:

| Variável | Default | Descrição |
|---|---|---|
| `JOB_NAME` | `rag-ingestion-<timestamp>` | Nome do job Dataflow |
| `MACHINE_TYPE` | `n1-standard-4` | Tipo de máquina dos workers |
| `NUM_WORKERS` | `2` | Workers iniciais |
| `MAX_WORKERS` | `10` | Workers máximos (autoscaling) |
| `DISK_SIZE_GB` | `50` | Disco por worker |

---

## Modelo de metadados

### Tabela `rag_documents`

| Coluna | Tipo | Descrição |
|---|---|---|
| `doc_id` | TEXT (PK) | UUID gerado automaticamente |
| `uri` | TEXT | URI GCS do ficheiro original |
| `filename` | TEXT | Nome do ficheiro |
| `mime_type` | TEXT | Tipo MIME detectado |
| `doc_type` | TEXT | Tipo inferido: `cv`, `proposta`, `contrato`, `relatorio`, `unknown` |
| `language` | TEXT | Código ISO 639-1 da língua (ex: `pt`, `en`) |
| `char_count` | INTEGER | Número de caracteres do texto extraído |
| `ingest_timestamp` | TIMESTAMPTZ | Momento da ingestão (UTC) |
| `extra_metadata` | JSONB | Metadados adicionais extensíveis |

### Tabela `rag_chunks`

| Coluna | Tipo | Descrição |
|---|---|---|
| `chunk_id` | TEXT (PK) | UUID gerado automaticamente |
| `doc_id` | TEXT (FK) | Referência ao documento pai |
| `chunk_index` | INTEGER | Posição ordinal do chunk no documento |
| `section_heading` | TEXT | Título da secção (se detectado), ou NULL |
| `chunk_text` | TEXT | Texto do chunk |
| `token_count` | INTEGER | Número de tokens (cl100k_base) |
| `embedding` | vector(768) | Vector de embedding (dimensão configurável) |

### Tipos de documento inferidos

O pipeline analisa os primeiros 4 000 caracteres do texto e o nome do ficheiro para detectar palavras-chave:

| Tipo | Exemplos de keywords detectadas |
|---|---|
| `cv` | curriculum, vitae, experiência profissional, skills, habilitações |
| `proposta` | proposta, orçamento, scope of work, deliverables |
| `contrato` | contrato, agreement, cláusula, rescisão, partes contratantes |
| `relatorio` | relatório, report, sumário executivo, conclusões, findings |
| `unknown` | Nenhuma keyword suficiente encontrada |

---

## Exemplo de query vetorial

```sql
-- Configurar o número de probes (trade-off recall/velocidade)
SET ivfflat.probes = 10;

-- Encontrar os 5 chunks mais semelhantes a um vector de query
-- Substitua '[0.1, 0.2, ...]' pelo vector gerado a partir da pergunta do utilizador.
SELECT
    c.chunk_id,
    c.chunk_text,
    c.section_heading,
    c.token_count,
    d.filename,
    d.doc_type,
    d.language,
    1 - (c.embedding <=> '[0.1, 0.2, 0.3]'::vector) AS cosine_similarity
FROM rag_chunks c
JOIN rag_documents d ON d.doc_id = c.doc_id
ORDER BY c.embedding <=> '[0.1, 0.2, 0.3]'::vector
LIMIT 5;

-- Filtrar por tipo de documento
SELECT
    c.chunk_text,
    d.filename,
    1 - (c.embedding <=> '[0.1, 0.2, 0.3]'::vector) AS similarity
FROM rag_chunks c
JOIN rag_documents d ON d.doc_id = c.doc_id
WHERE d.doc_type = 'cv'
ORDER BY c.embedding <=> '[0.1, 0.2, 0.3]'::vector
LIMIT 10;
```

Para gerar o vector da pergunta em Python:

```python
from pipeline.embeddings import generate_embeddings
from pipeline.config import get_config

cfg = get_config()
query = "Quais são as competências do candidato em Python?"
vectors = generate_embeddings(
    texts=[query],
    model_name=cfg.embedding_model,
    project=cfg.project,
    region=cfg.region,
)
query_vector = vectors[0]
print(query_vector[:5], "...")  # primeiros 5 valores
```

---

## Melhorias recomendadas

1. **Índice ANN**: Descomente o índice IVFFlat ou HNSW em `002_indexes.sql` após carregar dados suficientes (≥ 1 000 chunks) para acelerar as queries de similaridade.

2. **OCR para imagens**: Integrar o Cloud Vision API para extrair texto de PDFs com imagens digitalizadas.

3. **Desduplicação**: Calcular hash SHA-256 do conteúdo no GCS para evitar re-ingerir documentos já processados.

4. **Notificações GCS**: Usar Pub/Sub + Cloud Functions para trigger automático de ingestão quando novos ficheiros são carregados.

5. **Re-ranking**: Adicionar uma camada de re-ranking (ex: Vertex AI Reranker) após a pesquisa vetorial para melhorar a relevância.

6. **Metadados ricos**: Popular `extra_metadata` com dados estruturados extraídos dos documentos (ex: nome do candidato num CV, valor de um contrato).

7. **Monitorização**: Configurar alertas no Cloud Monitoring para erros de pipeline e latência de embedding.

8. **Segurança**: Mover `DB_PASSWORD` e outras credenciais para o **Secret Manager** e ler no pipeline via `google-cloud-secret-manager`.
