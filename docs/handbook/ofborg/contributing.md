# Tickborg — Contributing Guide

## Getting Started

### Prerequisites

- **Rust** (latest stable) — via `rustup` or Nix
- **RabbitMQ** — local instance for integration testing
- **Git** — recent version with submodule support
- **Nix** (optional) — provides a reproducible dev environment

### Quick Setup with Nix

```bash
# Enter the development shell
nix develop

# This provides: cargo, rustc, clippy, rustfmt, pkg-config, openssl
```

### Manual Setup

```bash
# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Install system dependencies (Debian/Ubuntu)
sudo apt install pkg-config libssl-dev

# Install system dependencies (Fedora)
sudo dnf install pkg-config openssl-devel

# Install RabbitMQ (for integration testing)
sudo apt install rabbitmq-server
sudo systemctl start rabbitmq-server
```

---

## Building

```bash
# Debug build (fast compilation)
cargo build

# Release build (optimized, includes debug symbols)
cargo build --release

# Build a specific binary
cargo build --bin github-webhook-receiver
```

All 11 binaries are built from the `tickborg` crate. The workspace also
includes `tickborg-simple-build` as a secondary crate.

---

## Running Tests

```bash
# Run all tests
cargo test

# Run tests for a specific module
cargo test --lib commentparser

# Run tests with output
cargo test -- --nocapture

# Run a specific test
cargo test test_parse_build_command
```

---

## Code Quality

### Formatting

```bash
# Check formatting
cargo fmt --all -- --check

# Apply formatting
cargo fmt --all
```

### Linting

```bash
# Run clippy with warnings as errors
cargo clippy --all-targets --all-features -- -D warnings
```

Both checks run in CI. PRs with formatting or clippy violations will fail.

---

## Project Structure

See [architecture.md](architecture.md) for the full module hierarchy.

Key directories:

| Directory | What goes here |
|-----------|---------------|
| `tickborg/src/bin/` | Binary entry points — one file per service |
| `tickborg/src/tasks/` | Worker implementations |
| `tickborg/src/message/` | AMQP message type definitions |
| `tickborg/src/ghevent/` | GitHub webhook event types |
| `tickborg/src/eval/` | Evaluation strategies |
| `docs/handbook/ofborg/` | This documentation |

---

## Making Changes

### Adding a New Worker

1. Create the task implementation in `tickborg/src/tasks/`:

```rust
// tasks/myworker.rs
pub struct MyWorker { /* ... */ }

impl worker::SimpleWorker for MyWorker {
    type J = MyMessageType;

    async fn consumer(&mut self, job: &Self::J) -> worker::Actions {
        // Process the job
        vec![worker::Action::Ack]
    }
}
```

2. Create the binary entry point in `tickborg/src/bin/`:

```rust
// bin/my-worker.rs
#[tokio::main]
async fn main() {
    tickborg::setup_log();
    let cfg = tickborg::config::load();
    // Connect to AMQP, declare queues, start consumer
}
```

3. Add the binary to `tickborg/Cargo.toml`:

```toml
[[bin]]
name = "my-worker"
path = "src/bin/my-worker.rs"
```

4. Add any necessary config fields to `Config` in `config.rs`.

5. Add the service to `service.nix` and `docker-compose.yml`.

### Adding a New Message Type

1. Create the message type in `tickborg/src/message/`:

```rust
// message/mymessage.rs
#[derive(Serialize, Deserialize, Debug)]
pub struct MyMessage {
    pub field: String,
}
```

2. Add the module to `message/mod.rs`:

```rust
pub mod mymessage;
```

### Adding a New GitHub Event Type

1. Create the event type in `tickborg/src/ghevent/`:

```rust
// ghevent/myevent.rs
#[derive(Deserialize, Debug)]
pub struct MyEvent {
    pub action: String,
    pub repository: Repository,
}
```

2. Add the module to `ghevent/mod.rs`.

3. Add routing in the webhook receiver's `route_event` function.

---

## Testing Locally

### With `build-faker`

The `build-faker` binary simulates a builder without running actual builds:

```bash
# Terminal 1: Start RabbitMQ
sudo systemctl start rabbitmq-server

# Terminal 2: Start the webhook receiver
CONFIG_PATH=example.config.json cargo run --bin github-webhook-receiver

# Terminal 3: Start the build faker
CONFIG_PATH=example.config.json cargo run --bin build-faker
```

### Sending Test Webhooks

```bash
# Compute HMAC signature
BODY='{"action":"opened","pull_request":{...}}'
SIG=$(echo -n "$BODY" | openssl dgst -sha256 -hmac "your-webhook-secret" | awk '{print $2}')

# Send webhook
curl -X POST http://localhost:8080/github-webhook \
  -H "Content-Type: application/json" \
  -H "X-GitHub-Event: pull_request" \
  -H "X-Hub-Signature-256: sha256=$SIG" \
  -d "$BODY"
```

---

## Commit Messages

Follow **Conventional Commits** format:

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | When to use |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation changes |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `test` | Adding or correcting tests |
| `chore` | Maintenance tasks |
| `ci` | CI/CD changes |

### Scopes

Use the sub-project or module name:

```
feat(meshmc): add block renderer
fix(builder): handle timeout correctly
docs(ofborg): add deployment guide
ci(github): update workflow matrix
```

The evaluation system uses commit scopes to detect changed projects — see
[evaluation-system.md](evaluation-system.md).

---

## Pull Request Workflow

1. **Fork & branch** — Create a feature branch from `main`.
2. **Develop** — Make changes, run tests locally.
3. **Push** — Push to your fork.
4. **Open PR** — Target the `main` branch.
5. **CI** — Tickborg automatically evaluates the PR:
   - Detects changed projects
   - Adds `project: <name>` labels
   - Schedules builds on eligible platforms
6. **Review** — Maintainers review the code and build results.
7. **Merge** — Squash-merge into `main`.

### Bot Commands

Maintainers can use `@tickbot` commands on PRs:

```
@tickbot build meshmc          Build meshmc on all platforms
@tickbot build meshmc neozip   Build multiple projects
@tickbot test mnv              Run tests for mnv
@tickbot eval                  Re-run evaluation
```

---

## Documentation

Documentation lives in `docs/handbook/ofborg/`. When making changes to
tickborg:

- Update relevant docs if the change affects architecture or configuration.
- Reference real struct names, function signatures, and module paths.
- Include code snippets from the actual source.

---

## Release Process

Releases are built via the Nix flake:

```bash
nix build .#tickborg
```

The output includes all 11 binaries in a single package. Deploy by updating
the NixOS module's `package` option or rebuilding the Docker image.

---

## Getting Help

- Read the [overview](overview.md) for a high-level understanding.
- Check [architecture](architecture.md) for the module structure.
- See [data-flow](data-flow.md) for end-to-end message tracing.
- Review [configuration](configuration.md) for config file reference.
