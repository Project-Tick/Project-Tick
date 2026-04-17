# Tickborg — Code Style & Conventions

## Rust Edition and Toolchain

- **Edition**: 2024
- **Resolver**: Cargo workspace resolver v2
- **MSRV**: Not pinned — follows latest stable

---

## Module Organization

### Top-Level Layout

```
tickborg/src/
├── lib.rs              # Public API, module declarations, setup_log()
├── config.rs           # Configuration loading and types
├── worker.rs           # SimpleWorker trait, Action enum
├── notifyworker.rs     # SimpleNotifyWorker trait
├── easyamqp.rs         # AMQP abstraction types
├── easylapin.rs        # lapin-based AMQP implementations
├── acl.rs              # Access control
├── systems.rs          # Platform/architecture definitions
├── commentparser.rs    # @tickbot command parser (nom)
├── checkout.rs         # Git clone/checkout/merge
├── buildtool.rs        # Build system detection
├── commitstatus.rs     # GitHub commit status wrapper
├── tagger.rs           # PR label generation
├── clone.rs            # Low-level git operations
├── locks.rs            # File-based locking
├── asynccmd.rs         # Async subprocess execution
├── evalchecker.rs      # Generic command runner
├── stats.rs            # Metrics collection trait
├── writetoline.rs      # Line-targeted file writing
├── bin/                # Binary entry points (11 files)
├── tasks/              # Worker implementations
├── message/            # AMQP message types
├── ghevent/            # GitHub webhook event types
└── eval/               # Evaluation strategies
```

### Convention: One Trait Per File

Worker-related traits each get their own file:
- `worker.rs` → `SimpleWorker`
- `notifyworker.rs` → `SimpleNotifyWorker`

### Convention: `mod.rs` in Sub-Modules

Sub-directories use `mod.rs` for re-exports:

```rust
// message/mod.rs
pub mod buildjob;
pub mod buildresult;
pub mod evaluationjob;
pub mod buildlogmsg;
pub mod common;
```

---

## Naming Conventions

### Types

| Pattern | Example |
|---------|---------|
| Worker structs | `BuildWorker`, `EvaluationFilterWorker` |
| Config structs | `RabbitMqConfig`, `BuilderConfig` |
| Message structs | `BuildJob`, `BuildResult`, `EvaluationJob` |
| Event structs | `PullRequestEvent`, `IssueComment`, `PushEvent` |
| Enums | `BuildStatus`, `ExchangeType`, `System` |

### Functions

| Pattern | Example |
|---------|---------|
| Constructors | `new()`, `from_config()` |
| Predicates | `is_tag()`, `is_delete()`, `is_zero_sha()` |
| Accessors | `branch()`, `name()` |
| Actions | `set_with_description()`, `analyze_changes()` |

### Constants

```rust
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
```

Upper-case `SCREAMING_SNAKE_CASE` for constants.

---

## Async Patterns

### `async fn` in Traits

Tickborg uses Rust 2024 edition which supports `async fn` in traits natively
via `impl Future` return types:

```rust
pub trait SimpleWorker: Send {
    type J: Send;

    fn msg_to_job(/* ... */) -> impl Future<Output = Result<Self::J, String>> + Send;
    fn consumer(&mut self, job: &Self::J) -> impl Future<Output = Actions> + Send;
}
```

### Tokio Runtime

All binaries use the multi-threaded Tokio runtime:

```rust
#[tokio::main]
async fn main() {
    // ...
}
```

### `RwLock` for Shared State

The `GithubAppVendingMachine` is wrapped in `tokio::sync::RwLock` to allow
concurrent read access to cached tokens:

```rust
pub struct EvaluationWorker<E> {
    github_vend: tokio::sync::RwLock<GithubAppVendingMachine>,
    // ...
}
```

---

## Error Handling

### Pattern: Enum-Based Errors

```rust
#[derive(Debug)]
pub enum CommitStatusError {
    ExpiredCreds(String),
    MissingSha(String),
    InternalError(String),
    Error(String),
}
```

### Pattern: String Errors for Worker Actions

Worker methods return `Result<_, String>` for simplicity — the error message
is logged and the job is acked or nacked.

### Pattern: `unwrap_or_else` with `panic!` for Config

```rust
let config_str = std::fs::read_to_string(&path)
    .unwrap_or_else(|e| panic!("Failed to read: {e}"));
```

Configuration errors are unrecoverable — panic is appropriate at startup.

---

## Serialization

### Serde Conventions

```rust
// snake_case field renaming
#[derive(Deserialize, Debug)]
#[serde(rename_all = "snake_case")]
pub enum PullRequestAction {
    Opened,
    Closed,
    Synchronize,
    // ...
}

// Optional fields
#[derive(Deserialize, Debug)]
pub struct Config {
    pub builder: Option<BuilderConfig>,
    // ...
}

// Default values
#[derive(Deserialize, Debug)]
pub struct QueueConfig {
    #[serde(default = "default_true")]
    pub durable: bool,
}
```

### JSON Message Format

All AMQP messages are `serde_json::to_vec()`:

```rust
pub fn publish_serde_action<T: Serialize>(
    exchange: Option<String>,
    routing_key: Option<String>,
    msg: &T,
) -> Action {
    Action::Publish(QueueMsg {
        exchange,
        routing_key,
        content: serde_json::to_vec(msg).unwrap(),
    })
}
```

---

## Testing Patterns

### Unit Tests in Module Files

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_build_command() {
        let result = parse("@tickbot build meshmc");
        assert_eq!(result, vec![Instruction::Build(
            vec!["meshmc".to_owned()],
            Subset::Project,
        )]);
    }
}
```

### The `build-faker` Binary

```rust
// bin/build-faker.rs
```

A test utility that simulates a builder without actually running builds.
Useful for testing the AMQP pipeline end-to-end.

---

## Logging

### `tracing` Macros

```rust
use tracing::{info, warn, error, debug, trace};

info!("Starting webhook receiver on port {}", port);
warn!("Token expired, refreshing");
error!("Failed to decode message: {}", err);
debug!(routing_key = %key, "Received message");
```

### Structured Fields

```rust
tracing::info!(
    pr = %job.pr.number,
    repo = %job.repo.full_name,
    project = %project_name,
    "Starting build"
);
```

---

## Git Operations

### `CachedCloner` Pattern

All git operations go through the `CachedCloner` → `CachedProject` →
`CachedProjectCo` chain:

```rust
let cloner = CachedCloner::new(checkout_root, 3);  // 3 concurrent clones max
let project = cloner.project("owner/repo", clone_url);
let co = project.clone_for("purpose".into(), identity.into())?;
co.fetch_pr(42)?;
co.merge_commit(OsStr::new("pr"))?;
```

### File Locking

```rust
// locks.rs
pub struct LockFile {
    path: PathBuf,
    file: Option<File>,
}

impl LockFile {
    pub fn lock(path: &Path) -> Result<Self, io::Error>;
}

impl Drop for LockFile {
    fn drop(&mut self) {
        // Release lock automatically
    }
}
```

---

## Clippy and Formatting

```bash
# Format
cargo fmt --all

# Lint
cargo clippy --all-targets --all-features -- -D warnings
```

The CI pipeline enforces both. The workspace `Cargo.toml` does not set custom
clippy lints — the defaults plus `-D warnings` are used.

---

## Dependencies Policy

- **Minimal external crates** — only well-maintained crates with clear purpose.
- **Pinned git dependencies** — the `hubcaps` fork is pinned to a specific rev.
- **Feature-gated Tokio** — only `rt-multi-thread`, `net`, `macros`, `sync`.
- **No `unwrap()` in library code** — except config loading at startup.
- **Release profile**: `debug = true` is set to include debug symbols in
  release builds for better crash diagnostics.
