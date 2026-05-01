# ── Stage 1: Build ─────────────────────────────────────
FROM rust:1.95-bookworm@sha256:adab7941580c74513aa3347f2d2a1f975498280743d29ec62978ba12e3540d3a AS builder

RUN apt-get update && apt-get install -y \
    cmake pkg-config libssl-dev git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY Cargo.toml Cargo.lock ./
COPY tickborg/ tickborg/
COPY tickborg-simple-build/ tickborg-simple-build/

RUN cargo build --release --workspace

# ── Stage 2: Runtime ──────────────────────────────────
FROM debian:bookworm-slim@sha256:f9c6a2fd2ddbc23e336b6257a5245e31f996953ef06cd13a59fa0a1df2d5c252

RUN apt-get update && apt-get install -y \
    ca-certificates git openssh-client \
    cmake make meson ninja-build autoconf automake libtool \
    gcc g++ pkg-config \
    default-jdk-headless \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -r -m -d /var/lib/tickborg -s /bin/bash tickborg

COPY --from=builder /build/target/release/builder /usr/local/bin/
COPY --from=builder /build/target/release/evaluation-filter /usr/local/bin/
COPY --from=builder /build/target/release/github-comment-filter /usr/local/bin/
COPY --from=builder /build/target/release/github-comment-poster /usr/local/bin/
COPY --from=builder /build/target/release/github-webhook-receiver /usr/local/bin/
COPY --from=builder /build/target/release/log-message-collector /usr/local/bin/
COPY --from=builder /build/target/release/logapi /usr/local/bin/
COPY --from=builder /build/target/release/mass-rebuilder /usr/local/bin/
COPY --from=builder /build/target/release/stats /usr/local/bin/
COPY --from=builder /build/target/release/tickborg-simple-build /usr/local/bin/

USER tickborg
WORKDIR /var/lib/tickborg

ENV RUST_LOG=tickborg=info
ENV RUST_BACKTRACE=1
