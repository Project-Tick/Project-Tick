# Tickborg — Architecture

## Workspace Structure

The tickborg codebase is organized as a Cargo workspace with two member crates:

```toml
# ofborg/Cargo.toml
[workspace]
members = [
    "tickborg",
    "tickborg-simple-build"
]
resolver = "2"

[profile.release]
debug = true
```

The `debug = true` in the release profile ensures that production binaries
include debug symbols, making crash backtraces and profiling useful without
sacrificing optimization.

---

## Crate: `tickborg`

This is the main crate. It compiles into a library (`lib.rs`) and **11 binary
targets** under `src/bin/`.

### Library Root (`src/lib.rs`)

```rust
#![recursion_limit = "512"]
#![allow(clippy::redundant_closure)]

pub mod acl;
pub mod asynccmd;
pub mod buildtool;
pub mod checkout;
pub mod clone;
pub mod commentparser;
pub mod commitstatus;
pub mod config;
pub mod easyamqp;
pub mod easylapin;
pub mod evalchecker;
pub mod files;
pub mod ghevent;
pub mod locks;
pub mod message;
pub mod notifyworker;
pub mod stats;
pub mod systems;
pub mod tagger;
pub mod tasks;
pub mod test_scratch;
pub mod worker;
pub mod writetoline;
```

Additionally, a `tickborg` sub-module re-exports everything for convenient
access:

```rust
pub mod tickborg {
    pub use crate::acl;
    pub use crate::asynccmd;
    pub use crate::buildtool;
    pub use crate::checkout;
    pub use crate::clone;
    pub use crate::commentparser;
    // ... all modules re-exported ...

    pub const VERSION: &str = env!("CARGO_PKG_VERSION");

    pub fn partition_result<A, B>(results: Vec<Result<A, B>>) -> (Vec<A>, Vec<B>) {
        let mut ok = Vec::new();
        let mut err = Vec::new();
        for result in results.into_iter() {
            match result {
                Ok(x) => ok.push(x),
                Err(x) => err.push(x),
            }
        }
        (ok, err)
    }
}
```

### Logging Initialization

```rust
pub fn setup_log() {
    let filter_layer = EnvFilter::try_from_default_env()
        .or_else(|_| EnvFilter::try_new("info"))
        .unwrap();

    let log_json = env::var("RUST_LOG_JSON").is_ok_and(|s| s == "1");

    if log_json {
        let fmt_layer = tracing_subscriber::fmt::layer().json();
        tracing_subscriber::registry()
            .with(filter_layer)
            .with(fmt_layer)
            .init();
    } else {
        let fmt_layer = tracing_subscriber::fmt::layer();
        tracing_subscriber::registry()
            .with(filter_layer)
            .with(fmt_layer)
            .init();
    }
}
```

Every binary calls `tickborg::setup_log()` as its first action. The environment
variable `RUST_LOG` controls the filter level. Setting `RUST_LOG_JSON=1`
switches to JSON-structured output for log aggregation in production.

---

## Module Hierarchy

### Core Worker Pattern

```
worker.rs
├── SimpleWorker trait
├── Action enum (Ack, NackRequeue, NackDump, Publish)
├── QueueMsg struct
└── publish_serde_action() helper

notifyworker.rs
├── SimpleNotifyWorker trait
├── NotificationReceiver trait
└── DummyNotificationReceiver (for testing)
```

### AMQP Layer

```
easyamqp.rs
├── ConsumeConfig struct
├── BindQueueConfig struct
├── ExchangeConfig struct
├── QueueConfig struct
├── ExchangeType enum (Topic, Headers, Fanout, Direct, Custom)
├── ChannelExt trait
└── ConsumerExt trait

easylapin.rs
├── from_config() → Connection
├── impl ChannelExt for Channel
├── impl ConsumerExt for Channel
├── WorkerChannel (with prefetch=1)
├── NotifyChannel (with prefetch=1, for SimpleNotifyWorker)
├── ChannelNotificationReceiver
└── action_deliver() (Ack/Nack/Publish dispatch)
```

### Configuration

```
config.rs
├── Config (top-level)
├── GithubWebhookConfig
├── LogApiConfig
├── EvaluationFilter
├── GithubCommentFilter
├── GithubCommentPoster
├── MassRebuilder
├── Builder
├── PushFilter
├── LogMessageCollector
├── Stats
├── RabbitMqConfig
├── BuildConfig
├── GithubAppConfig
├── RunnerConfig
├── CheckoutConfig
├── GithubAppVendingMachine
└── load() → Config
```

### Message Types

```
message/
├── mod.rs (re-exports)
├── common.rs
│   ├── Repo
│   ├── Pr
│   └── PushTrigger
├── buildjob.rs
│   ├── BuildJob
│   ├── QueuedBuildJobs
│   └── Actions
├── buildresult.rs
│   ├── BuildStatus enum
│   ├── BuildResult enum (V1, Legacy)
│   ├── LegacyBuildResult
│   └── V1Tag
├── buildlogmsg.rs
│   ├── BuildLogMsg
│   └── BuildLogStart
└── evaluationjob.rs
    ├── EvaluationJob
    └── Actions
```

### GitHub Event Types

```
ghevent/
├── mod.rs (re-exports)
├── common.rs
│   ├── Comment
│   ├── User
│   ├── Repository
│   ├── Issue
│   └── GenericWebhook
├── issuecomment.rs
│   ├── IssueComment
│   └── IssueCommentAction enum
├── pullrequestevent.rs
│   ├── PullRequestEvent
│   ├── PullRequest
│   ├── PullRequestRef
│   ├── PullRequestState enum
│   ├── PullRequestAction enum
│   ├── PullRequestChanges
│   └── BaseChange, ChangeWas
└── pushevent.rs
    ├── PushEvent
    ├── Pusher
    └── HeadCommit
```

### Task Implementations

```
tasks/
├── mod.rs
├── build.rs
│   ├── BuildWorker (SimpleNotifyWorker)
│   └── JobActions (log streaming helper)
├── eval/
│   ├── mod.rs
│   │   ├── EvaluationStrategy trait
│   │   ├── EvaluationComplete
│   │   └── Error enum
│   └── monorepo.rs
│       ├── MonorepoStrategy
│       ├── label_from_title()
│       └── parse_commit_scopes()
├── evaluate.rs
│   ├── EvaluationWorker (SimpleWorker)
│   ├── OneEval (per-job evaluation context)
│   └── update_labels()
├── evaluationfilter.rs
│   └── EvaluationFilterWorker (SimpleWorker)
├── githubcommentfilter.rs
│   └── GitHubCommentWorker (SimpleWorker)
├── githubcommentposter.rs
│   ├── GitHubCommentPoster (SimpleWorker)
│   ├── PostableEvent enum
│   ├── job_to_check()
│   └── result_to_check()
├── log_message_collector.rs
│   ├── LogMessageCollector (SimpleWorker)
│   ├── LogFrom
│   └── LogMessage
├── pushfilter.rs
│   └── PushFilterWorker (SimpleWorker)
└── statscollector.rs
    └── StatCollectorWorker (SimpleWorker)
```

### Utility Modules

```
acl.rs           — Access control (repos, trusted users, arch mapping)
asynccmd.rs      — Async subprocess execution with streaming output
buildtool.rs     — Build system detection and execution
checkout.rs      — Git checkout caching (CachedCloner, CachedProject)
clone.rs         — Git clone trait (GitClonable, file locking)
commentparser.rs — @tickbot command parser (nom combinators)
commitstatus.rs  — GitHub commit status abstraction
evalchecker.rs   — Generic command execution checker
files.rs         — File utility functions
locks.rs         — File-based locking (fs2)
stats.rs         — Metrics events and RabbitMQ publisher
systems.rs       — Platform/architecture enum
tagger.rs        — PR label generation from changed files
writetoline.rs   — Random-access line writer for log files
```

---

## Binary Targets

### `github-webhook-receiver`

**File:** `src/bin/github-webhook-receiver.rs`

- Starts an HTTP server using `hyper 1.0`.
- Validates `X-Hub-Signature-256` using HMAC-SHA256.
- Reads the `X-Github-Event` header to determine the event type.
- Parses the body as `GenericWebhook` to extract the repository name.
- Publishes to the `github-events` topic exchange with routing key
  `{event_type}.{owner}/{repo}`.
- Declares queues: `build-inputs`, `github-events-unknown`,
  `mass-rebuild-check-inputs`, `push-build-inputs`.

### `evaluation-filter`

**File:** `src/bin/evaluation-filter.rs`

- Consumes from `mass-rebuild-check-inputs`.
- Deserializes `PullRequestEvent`.
- Checks if the repo is eligible via ACL.
- Filters by action (Opened, Synchronize, Reopened, Edited with base change).
- Produces `EvaluationJob` to `mass-rebuild-check-jobs`.

### `github-comment-filter`

**File:** `src/bin/github-comment-filter.rs`

- Consumes from `build-inputs`.
- Deserializes `IssueComment`.
- Parses the comment body for `@tickbot` commands.
- Looks up the PR via GitHub API to get the head SHA.
- Produces `BuildJob` messages to architecture-specific queues.
- Also produces `QueuedBuildJobs` to `build-results` for the comment poster.

### `github-comment-poster`

**File:** `src/bin/github-comment-poster.rs`

- Consumes from `build-results`.
- Accepts both `QueuedBuildJobs` (build queued) and `BuildResult` (build
  finished).
- Creates GitHub Check Runs via the Checks API.
- Maps `BuildStatus` to `Conclusion` (Success, Failure, Skipped, Neutral).

### `mass-rebuilder`

**File:** `src/bin/mass-rebuilder.rs`

- Consumes from `mass-rebuild-check-jobs`.
- Uses `EvaluationWorker` with `MonorepoStrategy`.
- Clones the repository, checks out the PR, detects changed files.
- Uses build system detection to discover affected projects.
- Creates `BuildJob` messages for each affected project/architecture.
- Updates GitHub commit statuses throughout the process.

### `builder`

**File:** `src/bin/builder.rs`

- Consumes from `build-inputs-{system}` (e.g., `build-inputs-x86_64-linux`).
- Creates one channel per configured system.
- Uses `BuildWorker` (a `SimpleNotifyWorker`) to execute builds.
- Streams build log lines to the `logs` exchange in real-time.
- Publishes `BuildResult` to `build-results` when done.

### `push-filter`

**File:** `src/bin/push-filter.rs`

- Consumes from `push-build-inputs`.
- Deserializes `PushEvent`.
- Skips tag pushes, branch deletions, and zero-SHA events.
- Detects changed projects from the push event's commit info.
- Falls back to `default_attrs` when no projects are detected.
- Creates `BuildJob::new_push()` and schedules on primary platforms.

### `log-message-collector`

**File:** `src/bin/log-message-collector.rs`

- Consumes from `logs` (ephemeral queue bound to the `logs` exchange).
- Writes build log lines to `{logs_path}/{routing_key}/{attempt_id}`.
- Uses `LineWriter` for random-access line writing.
- Also writes `.metadata.json` and `.result.json` files.

### `logapi`

**File:** `src/bin/logapi.rs`

- HTTP server that serves build log metadata.
- Endpoint: `GET /logs/{routing_key}`.
- Returns JSON with attempt IDs, metadata, results, and log URLs.
- Path traversal prevention via `canonicalize()` and `validate_path_segment()`.

### `stats`

**File:** `src/bin/stats.rs`

- Consumes from `stats-events` (bound to the `stats` fanout exchange).
- Collects `EventMessage` payloads.
- Exposes Prometheus-compatible metrics on `0.0.0.0:9898`.
- Runs an HTTP server in a separate thread.

### `build-faker`

**File:** `src/bin/build-faker.rs`

- Development tool that publishes fake `BuildJob` messages.
- Useful for testing the builder without a real GitHub webhook.

---

## The Worker Pattern in Detail

### `SimpleWorker`

```rust
pub trait SimpleWorker: Send {
    type J: Send;

    fn consumer(&mut self, job: &Self::J) -> impl Future<Output = Actions>;

    fn msg_to_job(
        &mut self,
        method: &str,
        headers: &Option<String>,
        body: &[u8],
    ) -> impl Future<Output = Result<Self::J, String>>;
}
```

Workers that implement `SimpleWorker` receive a message, process it, and return
a `Vec<Action>`. The actions are applied in order:

```rust
pub enum Action {
    Ack,          // Acknowledge message (remove from queue)
    NackRequeue,  // Negative ack, requeue (retry later)
    NackDump,     // Negative ack, discard
    Publish(Arc<QueueMsg>),  // Publish a new message
}
```

The `ConsumerExt` implementation on `Channel` drives the loop:

```rust
impl<'a, W: SimpleWorker + 'a> ConsumerExt<'a, W> for Channel {
    async fn consume(self, mut worker: W, config: ConsumeConfig)
        -> Result<Self::Handle, Self::Error>
    {
        let mut consumer = self.basic_consume(/* ... */).await?;
        Ok(Box::pin(async move {
            while let Some(Ok(deliver)) = consumer.next().await {
                let job = worker.msg_to_job(/* ... */).await.expect("...");
                for action in worker.consumer(&job).await {
                    action_deliver(&self, &deliver, action).await.expect("...");
                }
            }
        }))
    }
}
```

### `SimpleNotifyWorker`

```rust
#[async_trait]
pub trait SimpleNotifyWorker {
    type J;

    async fn consumer(
        &self,
        job: Self::J,
        notifier: Arc<dyn NotificationReceiver + Send + Sync>,
    );

    fn msg_to_job(
        &self,
        routing_key: &str,
        content_type: &Option<String>,
        body: &[u8],
    ) -> Result<Self::J, String>;
}
```

The key difference: instead of returning `Actions`, the worker receives a
`NotificationReceiver` that it can `tell()` at any point during processing.
This enables streaming log lines back to RabbitMQ while a build is still
running.

```rust
#[async_trait]
pub trait NotificationReceiver {
    async fn tell(&self, action: Action);
}
```

The `ChannelNotificationReceiver` bridges this to a real AMQP channel:

```rust
pub struct ChannelNotificationReceiver {
    channel: lapin::Channel,
    deliver: Delivery,
}

#[async_trait]
impl NotificationReceiver for ChannelNotificationReceiver {
    async fn tell(&self, action: Action) {
        action_deliver(&self.channel, &self.deliver, action)
            .await
            .expect("action deliver failure");
    }
}
```

### Channel Variants

| Wrapper | Prefetch | Use Case |
|---------|----------|----------|
| `Channel` (raw) | None | Services with a single instance or that want prefetching |
| `WorkerChannel(Channel)` | 1 | Multi-instance workers (fair dispatch) |
| `NotifyChannel(Channel)` | 1 | Long-running workers with streaming notifications |

---

## Message Flow Through the System

### PR Opened/Synchronised

```
GitHub ──POST──► webhook-receiver
                       │
                       ▼ publish to github-events
                       │ routing_key: pull_request.{owner}/{repo}
                       │
            ┌──────────┴──────────┐
            ▼                     ▼
    evaluation-filter     (other consumers)
            │
            ▼ publish to mass-rebuild-check-jobs
            │
    mass-rebuilder
            │
            ├─► clone repo
            ├─► checkout PR branch
            ├─► detect changed files
            ├─► map to projects
            ├─► create BuildJob per project/arch
            │
            ├─► publish BuildJob to build-inputs-{system}
            ├─► publish QueuedBuildJobs to build-results
            └─► update commit status
                       │
            ┌──────────┴──────────┐
            ▼                     ▼
        builder               comment-poster
            │                     │
            ├─► clone & merge     ├─► create check run (Queued)
            ├─► build project     │
            ├─► stream logs ──►   │
            │   logs exchange     │
            │        │            │
            │   log-collector     │
            │                     │
            ├─► publish result    │
            │   to build-results  │
            │         │           │
            │         └──────────►├─► create check run (Completed)
            └─► Ack               └─► Ack
```

### Comment Command (`@tickbot build meshmc`)

```
GitHub ──POST──► webhook-receiver
                       │
                       ▼ publish to github-events
                       │ routing_key: issue_comment.{owner}/{repo}
                       │
            comment-filter
                       │
                       ├─► parse @tickbot commands
                       ├─► lookup PR via GitHub API
                       ├─► determine build architectures from ACL
                       │
                       ├─► publish BuildJob to build-inputs-{system}
                       ├─► publish QueuedBuildJobs to build-results
                       └─► Ack
```

### Push to Branch

```
GitHub ──POST──► webhook-receiver
                       │
                       ▼ publish to github-events
                       │ routing_key: push.{owner}/{repo}
                       │
            push-filter
                       │
                       ├─► check if branch push (not tag/delete)
                       ├─► detect changed projects from commit info
                       ├─► fallback to default_attrs if needed
                       │
                       ├─► create BuildJob::new_push()
                       ├─► publish to build-inputs-{system} (primary)
                       ├─► publish QueuedBuildJobs to build-results
                       └─► Ack
```

---

## Concurrency Model

Tickborg uses **Tokio** as its async runtime with multi-threaded scheduling:

```rust
#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    // ...
}
```

Within the builder, multiple systems can be served simultaneously:

```rust
// builder.rs — main()
let mut handles: Vec<Pin<Box<dyn Future<Output = ()> + Send>>> = Vec::new();
for system in &cfg.build.system {
    handles.push(self::create_handle(&conn, &cfg, system.to_string()).await?);
}
future::join_all(handles).await;
```

Each handle is a `Pin<Box<dyn Future>>` that runs a consumer loop for one
architecture. The `basic_qos(1)` prefetch setting ensures that each builder
instance only works on one job at a time from each queue, preventing resource
starvation.

Build subprocesses themselves are spawned via `std::process::Command` and
monitored through the `AsyncCmd` abstraction which uses OS threads for I/O
multiplexing:

```rust
pub struct AsyncCmd {
    command: Command,
}

pub struct SpawnedAsyncCmd {
    waiter: JoinHandle<Option<Result<ExitStatus, io::Error>>>,
    rx: Receiver<String>,
}
```

---

## Git Operations

### CachedCloner

```rust
pub struct CachedCloner {
    root: PathBuf,
}

impl CachedCloner {
    pub fn project(&self, name: &str, clone_url: String) -> CachedProject;
}
```

The cached cloner maintains a local mirror of repositories under:
```
{root}/repo/{md5(name)}/clone       — bare clone (shared by all checkouts)
{root}/repo/{md5(name)}/{category}/  — working checkouts
```

### CachedProjectCo (Checkout)

```rust
pub struct CachedProjectCo {
    root: PathBuf,
    id: String,
    clone_url: String,
    local_reference: PathBuf,
}

impl CachedProjectCo {
    pub fn checkout_origin_ref(&self, git_ref: &OsStr) -> Result<String, Error>;
    pub fn checkout_ref(&self, git_ref: &OsStr) -> Result<String, Error>;
    pub fn fetch_pr(&self, pr_id: u64) -> Result<(), Error>;
    pub fn commit_exists(&self, commit: &OsStr) -> bool;
    pub fn merge_commit(&self, commit: &OsStr) -> Result<(), Error>;
    pub fn commit_messages_from_head(&self, commit: &str) -> Result<Vec<String>, Error>;
    pub fn files_changed_from_head(&self, commit: &str) -> Result<Vec<String>, Error>;
}
```

All git operations use file-based locking via `fs2::FileExt::lock_exclusive()`
to prevent concurrent access to the same checkout directory.

---

## File Locking

Two locking mechanisms exist:

### `clone.rs` — Git-level locks

```rust
pub trait GitClonable {
    fn lock_path(&self) -> PathBuf;
    fn lock(&self) -> Result<Lock, Error>;
    fn clone_repo(&self) -> Result<(), Error>;
    fn fetch_repo(&self) -> Result<(), Error>;
}
```

### `locks.rs` — Generic file locks

```rust
pub trait Lockable {
    fn lock_path(&self) -> PathBuf;
    fn lock(&self) -> Result<Lock, Error>;
}

pub struct Lock {
    lock: Option<fs::File>,
}

impl Lock {
    pub fn unlock(&mut self) { self.lock = None }
}
```

Both use `fs2`'s `lock_exclusive()` which maps to `flock(2)` on Unix.

---

## Error Handling Strategy

### CommitStatusError

```rust
pub enum CommitStatusError {
    ExpiredCreds(hubcaps::Error),
    MissingSha(hubcaps::Error),
    Error(hubcaps::Error),
    InternalError(String),
}
```

This is used to determine retry behavior:
- `ExpiredCreds` → `NackRequeue` (retry after token refresh)
- `MissingSha` → `Ack` (commit was force-pushed away, skip)
- `InternalError` → `Ack` + label `tickborg-internal-error`

### EvalWorkerError

```rust
enum EvalWorkerError {
    EvalError(eval::Error),
    CommitStatusWrite(CommitStatusError),
}
```

### eval::Error

```rust
pub enum Error {
    CommitStatusWrite(CommitStatusError),
    Fail(String),
}
```

---

## Testing Strategy

- Unit tests are embedded in modules using `#[cfg(test)]`.
- Test fixtures (JSON event payloads) are stored in `test-srcs/events/`.
- Tests use `include_str!()` to load test data at compile time.
- The `DummyNotificationReceiver` captures actions for assertion:

```rust
#[derive(Default)]
pub struct DummyNotificationReceiver {
    pub actions: parking_lot::Mutex<Vec<Action>>,
}
```

Example test from `evaluationfilter.rs`:

```rust
#[tokio::test]
async fn changed_base() {
    let data = include_str!("../../test-srcs/events/pr-changed-base.json");
    let job: PullRequestEvent = serde_json::from_str(data).expect("...");

    let mut worker = EvaluationFilterWorker::new(
        acl::Acl::new(vec!["project-tick/Project-Tick".to_owned()], Some(vec![]))
    );

    assert_eq!(worker.consumer(&job).await, vec![
        worker::publish_serde_action(
            None,
            Some("mass-rebuild-check-jobs".to_owned()),
            &evaluationjob::EvaluationJob { /* ... */ }
        ),
        worker::Action::Ack,
    ]);
}
```
