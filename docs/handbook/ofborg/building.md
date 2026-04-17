# Tickborg — Building

## Prerequisites

| Prerequisite | Minimum Version | Notes |
|-------------|-----------------|-------|
| Rust | Edition 2024 | `rustup default stable` |
| Cargo | Latest stable | Comes with Rust |
| Git | 2.x | For repository cloning |
| pkg-config | Any | Native dependency resolution |
| CMake | 3.x | If building CMake-based sub-projects |
| OpenSSL / rustls | — | TLS for AMQP + GitHub API |

---

## Quick Build

```bash
cd ofborg
cargo build --workspace
```

This compiles both workspace members:
- `tickborg` (main crate — library + 11 binaries)
- `tickborg-simple-build` (simplified build tool)

### Release Build

```bash
cargo build --workspace --release
```

The release profile includes debug symbols (`debug = true` in workspace
`Cargo.toml`) so that backtraces are readable in production.

### Build Individual Binaries

```bash
# Build only the webhook receiver
cargo build -p tickborg --bin github-webhook-receiver

# Build only the builder
cargo build -p tickborg --bin builder

# Build only the mass rebuilder
cargo build -p tickborg --bin mass-rebuilder
```

### List All Binary Targets

```bash
cargo build -p tickborg --bins 2>&1 | head -20
# Or:
ls tickborg/src/bin/
```

Available binaries:

| Binary | Source File |
|--------|-----------|
| `build-faker` | `src/bin/build-faker.rs` |
| `builder` | `src/bin/builder.rs` |
| `evaluation-filter` | `src/bin/evaluation-filter.rs` |
| `github-comment-filter` | `src/bin/github-comment-filter.rs` |
| `github-comment-poster` | `src/bin/github-comment-poster.rs` |
| `github-webhook-receiver` | `src/bin/github-webhook-receiver.rs` |
| `log-message-collector` | `src/bin/log-message-collector.rs` |
| `logapi` | `src/bin/logapi.rs` |
| `mass-rebuilder` | `src/bin/mass-rebuilder.rs` |
| `push-filter` | `src/bin/push-filter.rs` |
| `stats` | `src/bin/stats.rs` |

---

## Cargo.toml — Dependencies Deep Dive

### `tickborg/Cargo.toml`

```toml
[package]
name = "tickborg"
version = "0.1.0"
authors = ["Project Tick"]
build = "build.rs"
edition = "2024"
description = "Distributed CI bot for Project Tick monorepo"
license = "MIT"
```

### Core Dependencies

#### Async Runtime & Networking

```toml
tokio = { version = "1", features = ["rt-multi-thread", "net", "macros", "sync"] }
tokio-stream = "0.1"
futures = "0.3.31"
futures-util = "0.3.31"
async-trait = "0.1.89"
```

- **tokio**: The async runtime. `rt-multi-thread` enables the work-stealing
  scheduler. `net` provides TCP listeners. `macros` enables `#[tokio::main]`.
  `sync` provides `RwLock`, `Mutex`, etc.
- **tokio-stream**: `StreamExt` for consuming lapin message streams.
- **futures / futures-util**: `join_all`, `TryFutureExt`, and stream utilities.
- **async-trait**: Enables `async fn` in trait definitions (used by
  `SimpleNotifyWorker` and `NotificationReceiver`).

#### AMQP Client

```toml
lapin = "4.3.0"
```

- **lapin**: Pure-Rust AMQP 0-9-1 client. Provides `Connection`, `Channel`,
  `Consumer`, publish/consume/ack/nack operations. Built on tokio.

#### HTTP Server

```toml
hyper = { version = "1.0", features = ["full", "server", "http1"] }
hyper-util = { version = "0.1", features = ["server", "tokio", "http1"] }
http = "1"
http-body-util = "0.1"
```

- **hyper**: The webhook receiver and logapi/stats HTTP servers use hyper 1.0
  directly (no framework). `http1` feature is sufficient — no HTTP/2 needed.
- **hyper-util**: `TokioIo` adapter and server utilities.
- **http**: Standard HTTP types (`StatusCode`, `Method`, `Request`, `Response`).
- **http-body-util**: `Full<Bytes>` response body, `BodyExt` for collecting
  incoming bodies.

#### GitHub API

```toml
hubcaps = { git = "https://github.com/ofborg/hubcaps.git", rev = "0d7466e..." }
```

- **hubcaps**: GitHub REST API client. The custom fork adds
  `Conclusion::Skipped` for check runs. Provides:
  - `Github` client
  - `Credentials` (Client OAuth, JWT, InstallationToken)
  - `JWTCredentials`, `InstallationTokenGenerator`
  - Repository, Pull Request, Issue, Statuses, Check Runs APIs

#### Serialization

```toml
serde = { version = "1.0.217", features = ["derive"] }
serde_json = "1.0.135"
```

All message types, configuration, and GitHub event payloads use serde for
JSON serialization/deserialization.

#### Cryptography

```toml
hmac = "0.13.0"
sha2 = "0.11.0"
hex = "0.4.3"
md5 = "0.8.0"
```

- **hmac + sha2**: HMAC-SHA256 for GitHub webhook signature verification.
- **hex**: Hex encoding/decoding for signature comparison.
- **md5**: Hashing repository names for cache directory names (not security-critical).

#### TLS

```toml
rustls-pki-types = "1.14"
```

- Reading PEM-encoded private keys for GitHub App JWT authentication.

#### Parsing

```toml
nom = "8"
regex = "1.11.1"
brace-expand = "0.1.0"
```

- **nom**: Parser combinator library for the `@tickbot` comment command parser.
- **regex**: Pattern matching for PR title label extraction and commit scope
  parsing.
- **brace-expand**: Shell-style brace expansion (e.g., `{meshmc,mnv}`).

#### Logging

```toml
tracing = "0.1.41"
tracing-subscriber = { version = "0.3.19", features = ["json", "env-filter"] }
```

- **tracing**: Structured logging with spans and events.
- **tracing-subscriber**: `EnvFilter` for `RUST_LOG`-based filtering, JSON
  formatter for production logging.

#### Concurrency

```toml
parking_lot = "0.12.4"
fs2 = "0.4.3"
```

- **parking_lot**: Fast `Mutex` and `RwLock` (used for the snippet log in
  `BuildWorker` and the `DummyNotificationReceiver` in tests).
- **fs2**: File-based exclusive locking (`flock`) for git operations.

#### Utilities

```toml
chrono = { version = "0.4.38", default-features = false, features = ["clock", "std"] }
either = "1.13.0"
lru-cache = "0.1.2"
mime = "0.3"
tempfile = "3.15.0"
uuid = { version = "1.12", features = ["v4"] }
```

- **chrono**: Timestamps for check run `started_at` / `completed_at`.
- **lru-cache**: LRU eviction for open log file handles in the log collector.
- **tempfile**: Temporary files for build output capture.
- **uuid**: v4 UUIDs for `attempt_id` and `request_id`.

---

## Build Script (`build.rs`)

The crate has a build script at `tickborg/build.rs` that generates event
definitions at compile time:

```rust
// tickborg/src/stats.rs
include!(concat!(env!("OUT_DIR"), "/events.rs"));
```

The build script generates a `events.rs` file into `OUT_DIR` containing the
`Event` enum and related metric functions used by the stats system.

---

## Running Tests

```bash
# Run all tests
cargo test --workspace

# Run tests for tickborg only
cargo test -p tickborg

# Run a specific test
cargo test -p tickborg -- evaluationfilter::tests::changed_base

# Run tests with output
cargo test -p tickborg -- --nocapture

# Run tests with logging
RUST_LOG=tickborg=debug cargo test -p tickborg -- --nocapture
```

### Test Data

Test fixtures are located in:

```
tickborg/test-srcs/events/     — GitHub webhook JSON payloads
tickborg/test-scratch/         — Scratch test data
tickborg/test-nix/             — Legacy Nix test data
```

Tests load fixtures at compile time:

```rust
let data = include_str!("../../test-srcs/events/pr-changed-base.json");
let job: PullRequestEvent = serde_json::from_str(data).expect("...");
```

---

## Linting

```bash
# Check formatting
cargo fmt --check

# Run clippy
cargo clippy --workspace

# Both (as defined in the dev shell)
cargo fmt && cargo clippy
```

The dev shell sets `RUSTFLAGS = "-D warnings"` so that all warnings are treated
as errors in CI.

Known clippy allowances in the codebase:

```rust
#![allow(clippy::redundant_closure)]     // lib.rs — readability preference
#[allow(clippy::cognitive_complexity)]   // githubcommentfilter — complex match
#[allow(clippy::too_many_arguments)]     // OneEval::new
#[allow(clippy::upper_case_acronyms)]    // Subset::Project
#[allow(clippy::vec_init_then_push)]     // githubcommentposter — readability
```

---

## Nix-Based Build

### Dev Shell

```bash
nix develop ./ofborg
```

This provides:

```nix
nativeBuildInputs = with pkgs; [
  bash rustc cargo clippy rustfmt pkg-config git cmake
];

RUSTFLAGS = "-D warnings";
RUST_BACKTRACE = "1";
RUST_LOG = "tickborg=debug";
```

The dev shell also defines a `checkPhase` function:

```bash
checkPhase() (
    cd ofborg
    set -x
    cargo fmt
    git diff --exit-code
    cargo clippy
    cargo build && cargo test
)
```

### Nix Package

```bash
nix build ./ofborg#tickborg
```

The flake defines a `rustPlatform.buildRustPackage` derivation:

```nix
pkg = pkgs.rustPlatform.buildRustPackage {
    name = "tickborg";
    src = pkgs.nix-gitignore.gitignoreSource [ ] ./.;
    nativeBuildInputs = with pkgs; [ pkg-config pkgs.rustPackages.clippy ];
    preBuild = ''cargo clippy'';
    doCheck = false;
    cargoLock = {
        lockFile = ./Cargo.lock;
        outputHashes = {
            "hubcaps-0.6.2" = "sha256-Vl4wQIKQVRxkpQxL8fL9rndAN3TKLV4OjgnZOpT6HRo=";
        };
    };
};
```

The `outputHashes` entry pins the git-sourced `hubcaps` dependency for
reproducible builds.

---

## Docker Build

```bash
cd ofborg
docker build -t tickborg .
```

The `Dockerfile` performs a multi-stage build:

1. **Builder stage**: Compiles all binaries in release mode.
2. **Runtime stage**: Copies only the compiled binaries and necessary runtime
   dependencies.

For the full stack:

```bash
docker compose build
docker compose up -d
```

See [deployment.md](deployment.md) for production Docker usage.

---

## Dependency Management

### Updating Dependencies

```bash
cargo update               # Update all deps within semver ranges
cargo update -p lapin       # Update a specific dependency
```

### The Lockfile

`Cargo.lock` is checked into version control because tickborg produces binaries.
This ensures reproducible builds across all environments.

### Git Dependencies

```toml
hubcaps = { git = "https://github.com/ofborg/hubcaps.git", rev = "0d7466e..." }
```

This is pinned to a specific commit for stability. When the upstream fork is
updated, change the `rev` and update the Nix `outputHashes` accordingly.

### Patching Dependencies

The workspace `Cargo.toml` has commented-out patch sections:

```toml
[patch.crates-io]
#hubcaps = { path = "../hubcaps" }
#amq-proto = { path = "rust-amq-proto" }
```

Uncomment these to develop against local checkouts of forked dependencies.

---

## Build Output

After `cargo build --release`, binaries are located at:

```
ofborg/target/release/build-faker
ofborg/target/release/builder
ofborg/target/release/evaluation-filter
ofborg/target/release/github-comment-filter
ofborg/target/release/github-comment-poster
ofborg/target/release/github-webhook-receiver
ofborg/target/release/log-message-collector
ofborg/target/release/logapi
ofborg/target/release/mass-rebuilder
ofborg/target/release/push-filter
ofborg/target/release/stats
```

Each binary is self-contained and takes a single argument: the path to the
configuration JSON file.

```bash
./target/release/builder /etc/tickborg/config.json
```

---

## Cross-Compilation

The flake supports building on:

```nix
supportedSystems = [
    "aarch64-darwin"
    "x86_64-darwin"
    "x86_64-linux"
    "aarch64-linux"
];
```

On macOS, additional build inputs are needed:

```nix
buildInputs = with pkgs; lib.optionals stdenv.isDarwin [
    darwin.Security
    libiconv
];
```

---

## Incremental Compilation Tips

1. **Use `cargo check` for fast feedback**: Skips codegen, only type-checks.
2. **Set `CARGO_INCREMENTAL=1`**: Enabled by default in debug builds.
3. **Use `sccache`**: `RUSTC_WRAPPER=sccache cargo build` for cached
   compilation across clean builds.
4. **Link with `mold`**: On Linux, add to `.cargo/config.toml`:
   ```toml
   [target.x86_64-unknown-linux-gnu]
   linker = "clang"
   rustflags = ["-C", "link-arg=-fuse-ld=mold"]
   ```

---

## Troubleshooting

### `error[E0554]: #![feature] may not be used on the stable release channel`

You're using an older Rust. Tickborg requires Edition 2024 features. Run:
```bash
rustup update stable
```

### `hubcaps` build failure

The git dependency needs network access on first build. Ensure the rev is
reachable:
```bash
git ls-remote https://github.com/ofborg/hubcaps.git 0d7466e
```

### Linking errors on macOS

Ensure Xcode Command Line Tools are installed:
```bash
xcode-select --install
```

### `lapin` connection failures at runtime

This is a runtime issue, not a build issue. Ensure RabbitMQ is running and
the config file points to the correct host. See
[configuration.md](configuration.md).
