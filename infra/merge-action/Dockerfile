FROM python:3.12 AS builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libcairo2-dev libgirepository1.0-dev

RUN curl -LsSf https://astral.sh/uv/0.7.5/install.sh | UV_INSTALL_DIR=/usr/bin sh

COPY . .

RUN uv export --no-emit-workspace --no-dev --no-annotate --no-header \
    --no-hashes --output-file /requirements.txt

RUN python -m venv /venv && \
    /venv/bin/python -m pip install -r /requirements.txt

FROM python:3.12-slim
ENV PATH="/venv/bin:$PATH"

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        git gh jq libcairo2 libgirepository-1.0-1 gir1.2-json-1.0 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /venv /venv
COPY . /app
WORKDIR /app

CMD ["/app/merge.py"]
