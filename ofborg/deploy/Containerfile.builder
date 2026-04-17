# Tickborg isolated build environment
# Build: podman build -t localhost/tickborg-builder:latest -f Containerfile.builder
FROM registry.access.redhat.com/ubi9/ubi:latest

# Build essentials
RUN dnf install -y \
    gcc gcc-c++ make cmake meson ninja-build \
    autoconf automake libtool pkg-config \
    ncurses-devel openssl-devel zlib-devel \
    git diffutils findutils patch \
    java-17-openjdk-devel \
    python3 \
    && dnf clean all

# Rust toolchain (for cargo builds)
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --profile minimal
ENV PATH="/root/.cargo/bin:${PATH}"

# Create non-root build user
RUN useradd -m -s /bin/bash builder
USER builder
WORKDIR /build

# Default entrypoint
ENTRYPOINT ["/bin/sh", "-c"]
