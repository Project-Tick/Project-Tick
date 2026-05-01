FROM debian:stable@sha256:e969bc4a6af81e66bf2ee7e79cecb24885929111d5eddb97dd80bed3b90a5e93 AS builder
COPY --from=ghcr.io/astral-sh/uv:latest@sha256:3b7b60a81d3c57ef471703e5c83fd4aaa33abcd403596fb22ab07db85ae91347 /uv /uvx /bin/
COPY pyproject.toml uv.lock /
ENV UV_PYTHON_INSTALL_DIR=/python
RUN uv python install 3.13
RUN uv venv && uv sync

FROM debian:stable-slim@sha256:8f0c555de6a2f9c2bda1b170b67479d11f7f5e3b66bb4a7a1d8843361c9dd3ff
ENV PATH="/.venv/bin:$PATH" \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PORT=8000

RUN apt-get update \
    && apt-get install -y --no-install-recommends git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /.venv /.venv
COPY --from=builder /python /python
COPY . /app
WORKDIR /app

EXPOSE 8000
ENTRYPOINT ["/app/docker-entrypoint.sh"]
