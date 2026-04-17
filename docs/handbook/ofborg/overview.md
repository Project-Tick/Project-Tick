# Tickborg (ofborg) — Overview

## What is Tickborg?

Tickborg is the distributed Continuous Integration (CI) bot purpose-built for the
**Project Tick monorepo**. It is a Rust-based system derived from the original
[ofborg](https://github.com/NixOS/ofborg) — a CI system created for the NixOS
project — and adapted for the multi-project, multi-language, multi-platform
reality of Project Tick.

Where the original ofborg was tightly coupled to Nix package evaluation, tickborg
has been generalised to handle arbitrary build systems (CMake, Meson, Autotools,
Cargo, Gradle, Make, and custom commands) while retaining the proven AMQP-based
distributed worker architecture that made ofborg reliable at scale.

The crate name remains **`tickborg`** in code, the workspace lives under
`ofborg/` in the Project Tick tree, and the bot responds to the handle
**`@tickbot`** in GitHub comments.

---

## High-Level Goals

| Goal | How Tickborg achieves it |
|------|--------------------------|
| **Automated PR evaluation** | Every opened / synchronised PR is evaluated for which sub-projects changed and builds are scheduled automatically. |
| **On-demand builds** | Maintainers comment `@tickbot build <attr>` or `@tickbot eval` on a PR to trigger builds or re-evaluations. |
| **Push-triggered CI** | Direct pushes to protected branches (`main`, `staging`, etc.) are detected and build jobs are dispatched. |
| **Multi-platform builds** | Builds can be fanned out to `x86_64-linux`, `aarch64-linux`, `x86_64-darwin`, `aarch64-darwin`, `x86_64-windows`, `aarch64-windows`, and `x86_64-freebsd`. |
| **GitHub Check Runs** | Build results are reported back via the GitHub Checks API, giving inline status on every PR. |
| **Build log collection** | Build output is streamed over AMQP to a central log collector and served via a log viewer web UI. |
| **Prometheus metrics** | Operational statistics are published to RabbitMQ and exposed on a `/metrics`-compatible HTTP endpoint. |

---

## Design Principles

### 1. Message-Oriented Architecture

Every component communicates exclusively through **RabbitMQ (AMQP 0-9-1)**
messages. There is no shared database, no direct RPC between services, and no
in-memory coupling between workers. This means:

- Each worker binary can be deployed, scaled, and restarted independently.
- Work is durable — RabbitMQ queues are declared `durable: true` and messages
  are published with `delivery_mode: 2` (persistent).
- Load balancing is implicit: multiple builder instances consuming from the same
  queue will each receive a fair share of jobs via `basic_qos(1)`.

### 2. Worker Trait Abstraction

All business logic is expressed through two traits:

```rust
// tickborg/src/worker.rs
pub trait SimpleWorker: Send {
    type J: Send;
    fn consumer(&mut self, job: &Self::J) -> impl Future<Output = Actions>;
    fn msg_to_job(
        &mut self, method: &str, headers: &Option<String>, body: &[u8],
    ) -> impl Future<Output = Result<Self::J, String>>;
}
```

```rust
// tickborg/src/notifyworker.rs
#[async_trait]
pub trait SimpleNotifyWorker {
    type J;
    async fn consumer(
        &self, job: Self::J,
        notifier: Arc<dyn NotificationReceiver + Send + Sync>,
    );
    fn msg_to_job(
        &self, routing_key: &str, content_type: &Option<String>, body: &[u8],
    ) -> Result<Self::J, String>;
}
```

`SimpleWorker` is for purely functional message processors: receive a message,
return a list of `Action`s. `SimpleNotifyWorker` is for long-running tasks (like
builds) that need to stream intermediate results back during processing.

### 3. One Binary per Concern

Each responsibility is compiled into its own binary target under
`tickborg/src/bin/`:

| Binary | Role |
|--------|------|
| `github-webhook-receiver` | HTTP server that validates GitHub webhook payloads, verifies HMAC-SHA256 signatures, and publishes them to the `github-events` exchange. |
| `evaluation-filter` | Consumes `pull_request.*` events and decides whether a PR warrants evaluation. Publishes `EvaluationJob` to `mass-rebuild-check-jobs`. |
| `github-comment-filter` | Consumes `issue_comment.*` events, parses `@tickbot` commands, and publishes `BuildJob` messages. |
| `github-comment-poster` | Consumes `build-results` and creates GitHub Check Runs. |
| `mass-rebuilder` | Performs full monorepo evaluation on a PR checkout: detects changed projects, schedules builds. |
| `builder` | Executes actual builds using the configured build system (CMake, Cargo, etc.) and reports results. |
| `push-filter` | Consumes `push.*` events and creates build jobs for pushes to tracked branches. |
| `log-message-collector` | Collects streaming build log messages and writes them to disk. |
| `logapi` | HTTP server that serves collected build logs via a REST API. |
| `stats` | Collects stat events from RabbitMQ and exposes Prometheus metrics on port 9898. |
| `build-faker` | Development/testing tool that publishes fake build jobs. |

---

## Key Data Structures

### Repo

```rust
// tickborg/src/message/common.rs
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Repo {
    pub owner: String,
    pub name: String,
    pub full_name: String,
    pub clone_url: String,
}
```

### Pr

```rust
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct Pr {
    pub target_branch: Option<String>,
    pub number: u64,
    pub head_sha: String,
}
```

### PushTrigger

```rust
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct PushTrigger {
    pub head_sha: String,
    pub branch: String,
    pub before_sha: Option<String>,
}
```

### BuildJob

```rust
// tickborg/src/message/buildjob.rs
#[derive(Serialize, Deserialize, Debug)]
pub struct BuildJob {
    pub repo: Repo,
    pub pr: Pr,
    pub subset: Option<Subset>,
    pub attrs: Vec<String>,
    pub request_id: String,
    pub logs: Option<ExchangeQueue>,
    pub statusreport: Option<ExchangeQueue>,
    pub push: Option<PushTrigger>,
}
```

### BuildResult

```rust
// tickborg/src/message/buildresult.rs
#[derive(Serialize, Deserialize, Debug)]
pub enum BuildResult {
    V1 {
        tag: V1Tag,
        repo: Repo,
        pr: Pr,
        system: String,
        output: Vec<String>,
        attempt_id: String,
        request_id: String,
        status: BuildStatus,
        skipped_attrs: Option<Vec<String>>,
        attempted_attrs: Option<Vec<String>>,
        push: Option<PushTrigger>,
    },
    Legacy { /* ... backward compat ... */ },
}
```

### BuildStatus

```rust
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, Eq)]
pub enum BuildStatus {
    Skipped,
    Success,
    Failure,
    TimedOut,
    HashMismatch,
    UnexpectedError { err: String },
}
```

---

## Supported Build Systems

The `BuildExecutor` struct in `tickborg/src/buildtool.rs` supports:

```rust
pub enum BuildSystem {
    CMake,
    Meson,
    Autotools,
    Cargo,
    Gradle,
    Make,
    Custom { command: String },
}
```

For each build system, tickborg knows how to invoke the configure, build, and
test phases. A `ProjectBuildConfig` ties a sub-project to its build system:

```rust
pub struct ProjectBuildConfig {
    pub name: String,
    pub path: String,
    pub build_system: BuildSystem,
    pub build_timeout_seconds: u16,
    pub configure_args: Vec<String>,
    pub build_args: Vec<String>,
    pub test_command: Option<Vec<String>>,
}
```

---

## Supported Platforms (Systems)

```rust
// tickborg/src/systems.rs
pub enum System {
    X8664Linux,
    Aarch64Linux,
    X8664Darwin,
    Aarch64Darwin,
    X8664Windows,
    Aarch64Windows,
    X8664FreeBSD,
}
```

Primary CI platforms (used for untrusted users):

- `x86_64-linux`
- `x86_64-darwin`
- `x86_64-windows`

Trusted users get access to all seven platforms, including ARM and FreeBSD.

---

## Comment Parser

Users interact with tickborg by posting comments on GitHub PRs/issues:

```
@tickbot build meshmc
@tickbot eval
@tickbot test mnv
@tickbot build meshmc json4cpp neozip
```

The parser is implemented in `tickborg/src/commentparser.rs` using the `nom`
parser combinator library. It produces:

```rust
pub enum Instruction {
    Build(Subset, Vec<String>),
    Test(Vec<String>),
    Eval,
}

pub enum Subset {
    Project,
}
```

Multiple commands can appear in a single comment, even interspersed with prose:

```markdown
I noticed the target was broken — let's re-eval:
@tickbot eval

Also, try building meshmc:
@tickbot build meshmc
```

---

## Access Control (ACL)

```rust
// tickborg/src/acl.rs
pub struct Acl {
    trusted_users: Option<Vec<String>>,
    repos: Vec<String>,
}
```

- `repos` — list of GitHub repositories tickborg is responsible for.
- `trusted_users` — users who can build on *all* architectures (including ARM,
  FreeBSD). When `None` (disabled), everyone gets unrestricted access.
- Non-trusted users only build on primary platforms.

```rust
impl Acl {
    pub fn is_repo_eligible(&self, name: &str) -> bool;
    pub fn build_job_architectures_for_user_repo(
        &self, user: &str, repo: &str
    ) -> Vec<System>;
    pub fn can_build_unrestricted(&self, user: &str, repo: &str) -> bool;
}
```

---

## Project Tagger

The `ProjectTagger` in `tickborg/src/tagger.rs` analyses changed files in a PR
and generates labels:

```rust
pub struct ProjectTagger {
    selected: Vec<String>,
}

impl ProjectTagger {
    pub fn analyze_changes(&mut self, changed_files: &[String]);
    pub fn tags_to_add(&self) -> Vec<String>;
}
```

It produces labels like:
- `project: meshmc`
- `project: mnv`
- `scope: ci`
- `scope: docs`
- `scope: root`

---

## The Monorepo Evaluation Strategy

When a PR is evaluated, the `MonorepoStrategy` in
`tickborg/src/tasks/eval/monorepo.rs` implements the `EvaluationStrategy` trait:

```rust
pub trait EvaluationStrategy {
    fn pre_clone(&mut self) -> impl Future<Output = StepResult<()>>;
    fn on_target_branch(&mut self, co: &Path, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<()>>;
    fn after_fetch(&mut self, co: &CachedProjectCo) -> StepResult<()>;
    fn after_merge(&mut self, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<()>>;
    fn evaluation_checks(&self) -> Vec<EvalChecker>;
    fn all_evaluations_passed(&mut self, status: &mut CommitStatus)
        -> impl Future<Output = StepResult<EvaluationComplete>>;
}
```

The strategy:

1. Labels the PR from its title (extracting project names like `meshmc`,
   `mnv`, etc. using regex word boundaries).
2. Parses Conventional Commit messages to find affected scopes.
3. Uses file-change detection to identify which sub-projects changed.
4. Returns an `EvaluationComplete` containing `BuildJob`s to be dispatched.

---

## How It All Fits Together

```
GitHub Webhook
     │
     ▼
┌──────────────────┐
│ Webhook Receiver  │──► github-events (Topic Exchange)
└──────────────────┘         │
           ┌─────────────────┼──────────────────┐
           ▼                 ▼                   ▼
  ┌─────────────┐   ┌───────────────┐   ┌──────────────┐
  │ Eval Filter  │   │ Comment Filter│   │ Push Filter   │
  └──────┬──────┘   └──────┬────────┘   └──────┬───────┘
         │                 │                    │
         ▼                 ▼                    ▼
  mass-rebuild-     build-jobs           build-inputs-*
  check-jobs        (Fanout)             queues
         │                 │                    │
         ▼                 ▼                    ▼
  ┌──────────────┐  ┌──────────────┐    ┌──────────────┐
  │Mass Rebuilder │  │   Builder    │    │   Builder    │
  └──────┬───────┘  └──────┬───────┘    └──────┬───────┘
         │                 │                    │
         └────────┬────────┘                    │
                  ▼                             ▼
           build-results                  build-results
           (Fanout Exchange)              (Fanout Exchange)
                  │
                  ▼
         ┌────────────────┐     ┌──────────────────┐
         │ Comment Poster  │     │ Log Collector     │
         └────────────────┘     └──────────────────┘
                  │                       │
                  ▼                       ▼
           GitHub Checks API        /var/log/tickborg/
```

---

## Repository Layout

```
ofborg/
├── Cargo.toml                  # Workspace root
├── Cargo.lock                  # Pinned dependency versions
├── docker-compose.yml          # Full stack for local dev / production
├── Dockerfile                  # Multi-stage build for all binaries
├── service.nix                 # NixOS module for systemd services
├── flake.nix                   # Nix flake for dev shell & building
├── example.config.json         # Example configuration file
├── config.production.json      # Production config template
├── config.public.json          # Public (non-secret) config
├── deploy/                     # Deployment scripts
├── doc/                        # Legacy upstream docs
├── ofborg/                     # Original ofborg crate (deprecated)
├── ofborg-simple-build/        # Original simple build (deprecated)
├── ofborg-viewer/              # Log viewer web UI (JavaScript)
├── tickborg/                   # Main crate
│   ├── Cargo.toml              # Crate manifest with all dependencies
│   ├── build.rs                # Build script (generates events.rs)
│   ├── src/
│   │   ├── lib.rs              # Library root — module declarations
│   │   ├── bin/                # Binary entry points (11 binaries)
│   │   ├── acl.rs              # Access control lists
│   │   ├── asynccmd.rs         # Async command execution
│   │   ├── buildtool.rs        # Build system abstraction
│   │   ├── checkout.rs         # Git checkout / caching
│   │   ├── clone.rs            # Git clone trait
│   │   ├── commentparser.rs    # @tickbot command parser (nom)
│   │   ├── commitstatus.rs     # GitHub commit status wrapper
│   │   ├── config.rs           # Configuration types & loading
│   │   ├── easyamqp.rs         # AMQP config types & traits
│   │   ├── easylapin.rs        # lapin (AMQP) integration layer
│   │   ├── evalchecker.rs      # Generic command checker
│   │   ├── files.rs            # File utilities
│   │   ├── ghevent/            # GitHub event type definitions
│   │   ├── locks.rs            # File-based locking
│   │   ├── message/            # Message types (jobs, results, logs)
│   │   ├── notifyworker.rs     # Streaming notification worker trait
│   │   ├── stats.rs            # Metrics / event system
│   │   ├── systems.rs          # Platform / architecture enum
│   │   ├── tagger.rs           # PR label tagger
│   │   ├── tasks/              # Task implementations
│   │   ├── worker.rs           # Core worker trait
│   │   └── writetoline.rs      # Line-based file writer
│   ├── test-nix/               # Test fixtures (Nix-era, kept)
│   ├── test-scratch/           # Scratch test data
│   └── test-srcs/              # Test source data (JSON events)
└── tickborg-simple-build/      # Simplified build tool crate
    ├── Cargo.toml
    └── src/
```

---

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | Rust (Edition 2024) |
| Async runtime | Tokio (multi-thread) |
| AMQP client | lapin 4.3 |
| HTTP server | hyper 1.0 + hyper-util |
| JSON | serde + serde_json |
| GitHub API | hubcaps (custom fork) |
| Logging | tracing + tracing-subscriber |
| Parser | nom 8 |
| Cryptography | hmac + sha2 (webhook verification) |
| Concurrency | parking_lot, tokio::sync |
| UUID | uuid v4 |
| Caching | lru-cache |
| File locking | fs2 |
| Date/time | chrono |

---

## Versioning

The crate version is declared in `tickborg/Cargo.toml`:

```toml
[package]
name = "tickborg"
version = "0.1.0"
```

The version is accessible at runtime via:

```rust
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
```

It is also embedded in the RabbitMQ connection properties:

```rust
let opts = ConnectionProperties::default()
    .with_client_property("tickborg_version".into(), tickborg::VERSION.into());
```

---

## Relation to the Original ofborg

Tickborg was forked from ofborg (NixOS/ofborg) and adapted:

| Aspect | ofborg | tickborg |
|--------|--------|----------|
| Purpose | Nix package evaluation for nixpkgs | Monorepo CI for Project Tick |
| Build system | `nix-build` only | CMake, Meson, Cargo, Gradle, Make, Custom |
| Bot handle | `@ofborg` | `@tickbot` |
| Platforms | Linux, macOS | Linux, macOS, Windows, FreeBSD |
| Evaluation | Nix expression evaluation | File-change detection + project mapping |
| Package crate | `ofborg` | `tickborg` |

The `ofborg/` and `ofborg-simple-build/` directories are kept for reference but
are no longer compiled as part of the workspace.

---

## Quick Start (for developers)

```bash
# Enter the dev shell (requires Nix)
nix develop ./ofborg

# Or without Nix, ensure Rust 2024+ is installed
cd ofborg
cargo build --workspace

# Run tests
cargo test --workspace

# Start local infra
docker compose up -d rabbitmq
```

See [building.md](building.md) for comprehensive build instructions and
[configuration.md](configuration.md) for setting up a config file.

---

## Further Reading

- [architecture.md](architecture.md) — Crate structure, module hierarchy, worker pattern
- [building.md](building.md) — Cargo build, dependencies, features, build targets
- [webhook-receiver.md](webhook-receiver.md) — GitHub webhook handling
- [message-system.md](message-system.md) — AMQP/RabbitMQ messaging
- [build-executor.md](build-executor.md) — Build execution, build system abstraction
- [evaluation-system.md](evaluation-system.md) — Monorepo evaluation, project detection
- [github-integration.md](github-integration.md) — GitHub API interaction
- [amqp-infrastructure.md](amqp-infrastructure.md) — RabbitMQ connection management
- [deployment.md](deployment.md) — NixOS module, Docker Compose
- [configuration.md](configuration.md) — Config file format, environment variables
- [data-flow.md](data-flow.md) — End-to-end data flow
- [code-style.md](code-style.md) — Rust coding conventions
- [contributing.md](contributing.md) — Contribution guide
