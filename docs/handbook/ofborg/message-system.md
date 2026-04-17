# Tickborg — Message System

## Overview

Tickborg's entire architecture is built on **AMQP 0-9-1** messaging via
**RabbitMQ**. Every component is a standalone binary that communicates
exclusively through message queues. There is no shared database, no direct
RPC between services, and no in-memory coupling.

This document covers:
- The AMQP topology (exchanges, queues, bindings)
- Message types and their serialization
- Publishing and consuming patterns
- The worker abstraction layer

---

## Exchanges

Tickborg uses four RabbitMQ exchanges:

### `github-events` (Topic Exchange)

**Declared by:** `github-webhook-receiver`

The primary ingestion exchange. All GitHub webhook payloads are published here
with routing keys of the form `{event_type}.{owner}/{repo}`.

```rust
chan.declare_exchange(easyamqp::ExchangeConfig {
    exchange: "github-events".to_owned(),
    exchange_type: easyamqp::ExchangeType::Topic,
    passive: false,
    durable: true,
    auto_delete: false,
    no_wait: false,
    internal: false,
}).await?;
```

**Routing key patterns:**

| Pattern | Example | Consumer |
|---------|---------|----------|
| `pull_request.*` | `pull_request.project-tick/Project-Tick` | evaluation-filter |
| `issue_comment.*` | `issue_comment.project-tick/Project-Tick` | github-comment-filter |
| `push.*` | `push.project-tick/Project-Tick` | push-filter |
| `unknown.*` | `unknown.project-tick/Project-Tick` | (monitoring) |

### `build-jobs` (Fanout Exchange)

**Declared by:** `github-comment-filter`, `builder`, `push-filter`

Distributes build jobs to all connected builder instances. As a **fanout**
exchange, every bound queue receives a copy of every message.

```rust
chan.declare_exchange(easyamqp::ExchangeConfig {
    exchange: "build-jobs".to_owned(),
    exchange_type: easyamqp::ExchangeType::Fanout,
    passive: false,
    durable: true,
    auto_delete: false,
    no_wait: false,
    internal: false,
}).await?;
```

### `build-results` (Fanout Exchange)

**Declared by:** `github-comment-filter`, `github-comment-poster`, `push-filter`

Collects build results (both "queued" notifications and "completed" results).
The `github-comment-poster` consumes from this to create GitHub Check Runs.

```rust
chan.declare_exchange(easyamqp::ExchangeConfig {
    exchange: "build-results".to_owned(),
    exchange_type: easyamqp::ExchangeType::Fanout,
    passive: false,
    durable: true,
    auto_delete: false,
    no_wait: false,
    internal: false,
}).await?;
```

### `logs` (Topic Exchange)

**Declared by:** `log-message-collector`

Receives streaming build log messages from builders. The routing key encodes
the repository and PR/push identifier.

```rust
chan.declare_exchange(easyamqp::ExchangeConfig {
    exchange: "logs".to_owned(),
    exchange_type: easyamqp::ExchangeType::Topic,
    passive: false,
    durable: true,
    auto_delete: false,
    no_wait: false,
    internal: false,
}).await?;
```

### `stats` (Fanout Exchange)

**Declared by:** `stats`

Receives operational metric events from all workers. The stats collector
aggregates these into Prometheus-compatible metrics.

```rust
chan.declare_exchange(easyamqp::ExchangeConfig {
    exchange: "stats".to_owned(),
    exchange_type: easyamqp::ExchangeType::Fanout,
    passive: false,
    durable: true,
    auto_delete: false,
    no_wait: false,
    internal: false,
}).await?;
```

---

## Queues

### Durable Queues

| Queue Name | Exchange | Routing Key | Consumer |
|------------|----------|-------------|----------|
| `build-inputs` | `github-events` | `issue_comment.*` | github-comment-filter |
| `github-events-unknown` | `github-events` | `unknown.*` | (monitoring) |
| `mass-rebuild-check-inputs` | `github-events` | `pull_request.*` | evaluation-filter |
| `push-build-inputs` | `github-events` | `push.*` | push-filter |
| `mass-rebuild-check-jobs` | (direct publish) | — | mass-rebuilder |
| `build-inputs-x86_64-linux` | `build-jobs` | — | builder (x86_64-linux) |
| `build-inputs-aarch64-linux` | `build-jobs` | — | builder (aarch64-linux) |
| `build-inputs-x86_64-darwin` | `build-jobs` | — | builder (x86_64-darwin) |
| `build-inputs-aarch64-darwin` | `build-jobs` | — | builder (aarch64-darwin) |
| `build-inputs-x86_64-windows` | `build-jobs` | — | builder (x86_64-windows) |
| `build-inputs-aarch64-windows` | `build-jobs` | — | builder (aarch64-windows) |
| `build-inputs-x86_64-freebsd` | `build-jobs` | — | builder (x86_64-freebsd) |
| `build-results` | `build-results` | — | github-comment-poster |
| `stats-events` | `stats` | — | stats |

### Ephemeral Queues

| Queue Name | Exchange | Routing Key | Consumer |
|------------|----------|-------------|----------|
| `logs` | `logs` | `*.*` | log-message-collector |

The `logs` queue is declared `durable: false, exclusive: true, auto_delete:
true`. This means:
- It only exists while the log collector is connected.
- If the log collector disconnects, the queue is deleted.
- Log messages published while no collector is connected are lost.
- This is intentional: logs are not critical path data and the exchange itself
  is durable.

---

## Message Types

All messages are serialized as JSON using `serde_json`.

### `EvaluationJob`

**Published by:** evaluation-filter, github-comment-filter  
**Consumed by:** mass-rebuilder  
**Queue:** `mass-rebuild-check-jobs`

```rust
// message/evaluationjob.rs
#[derive(Serialize, Deserialize, Debug)]
pub struct EvaluationJob {
    pub repo: Repo,
    pub pr: Pr,
}
```

Example JSON:

```json
{
    "repo": {
        "owner": "project-tick",
        "name": "Project-Tick",
        "full_name": "project-tick/Project-Tick",
        "clone_url": "https://github.com/project-tick/Project-Tick.git"
    },
    "pr": {
        "number": 42,
        "head_sha": "abc123def456...",
        "target_branch": "main"
    }
}
```

### `BuildJob`

**Published by:** github-comment-filter, mass-rebuilder, push-filter  
**Consumed by:** builder  
**Queue:** `build-inputs-{system}`

```rust
// message/buildjob.rs
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

The `logs` and `statusreport` fields are tuples of `(Option<Exchange>,
Option<RoutingKey>)` that tell the builder where to send log messages and
build results.

Two constructors exist:

```rust
// For PR-triggered builds
impl BuildJob {
    pub fn new(
        repo: Repo, pr: Pr, subset: Subset, attrs: Vec<String>,
        logs: Option<ExchangeQueue>, statusreport: Option<ExchangeQueue>,
        request_id: String,
    ) -> BuildJob;

    // For push-triggered builds
    pub fn new_push(
        repo: Repo, push: PushTrigger, attrs: Vec<String>,
        request_id: String,
    ) -> BuildJob;

    pub fn is_push(&self) -> bool;
}
```

### `QueuedBuildJobs`

**Published by:** github-comment-filter, push-filter  
**Consumed by:** github-comment-poster  
**Exchange/Queue:** `build-results`

```rust
#[derive(Serialize, Deserialize, Debug)]
pub struct QueuedBuildJobs {
    pub job: BuildJob,
    pub architectures: Vec<String>,
}
```

This message tells the comment poster that builds have been queued so it can
create "Queued" check runs on GitHub.

### `BuildResult`

**Published by:** builder  
**Consumed by:** github-comment-poster, log-message-collector  
**Exchange/Queue:** `build-results`, `logs`

```rust
// message/buildresult.rs
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
    Legacy { /* backward compat */ },
}
```

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

### `BuildLogMsg`

**Published by:** builder  
**Consumed by:** log-message-collector  
**Exchange:** `logs`

```rust
// message/buildlogmsg.rs
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct BuildLogMsg {
    pub system: String,
    pub identity: String,
    pub attempt_id: String,
    pub line_number: u64,
    pub output: String,
}
```

### `BuildLogStart`

**Published by:** builder  
**Consumed by:** log-message-collector  
**Exchange:** `logs`

```rust
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct BuildLogStart {
    pub system: String,
    pub identity: String,
    pub attempt_id: String,
    pub attempted_attrs: Option<Vec<String>>,
    pub skipped_attrs: Option<Vec<String>>,
}
```

### `EventMessage`

**Published by:** all workers (via `stats::RabbitMq`)  
**Consumed by:** stats  
**Exchange:** `stats`

```rust
// stats.rs
#[derive(Serialize, Deserialize, Debug)]
pub struct EventMessage {
    pub sender: String,
    pub events: Vec<Event>,
}
```

---

## Common Message Structures

### `Repo`

```rust
// message/common.rs
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Repo {
    pub owner: String,
    pub name: String,
    pub full_name: String,
    pub clone_url: String,
}
```

### `Pr`

```rust
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct Pr {
    pub target_branch: Option<String>,
    pub number: u64,
    pub head_sha: String,
}
```

For push-triggered builds, `pr.number` is set to `0` and `pr.head_sha`
contains the push commit SHA.

### `PushTrigger`

```rust
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Eq)]
pub struct PushTrigger {
    pub head_sha: String,
    pub branch: String,
    pub before_sha: Option<String>,
}
```

---

## Publishing Messages

### The `publish_serde_action` Helper

```rust
// worker.rs
pub fn publish_serde_action<T: Serialize + ?Sized>(
    exchange: Option<String>,
    routing_key: Option<String>,
    msg: &T,
) -> Action {
    Action::Publish(Arc::new(QueueMsg {
        exchange,
        routing_key,
        mandatory: false,
        immediate: false,
        content_type: Some("application/json".to_owned()),
        content: serde_json::to_string(&msg).unwrap().into_bytes(),
    }))
}
```

This is the primary way workers produce outgoing messages. The message is
serialized to JSON and wrapped in a `QueueMsg` which is then wrapped in an
`Action::Publish`.

### Message Delivery

The `action_deliver` function in `easylapin.rs` handles all action types:

```rust
async fn action_deliver(
    chan: &Channel, deliver: &Delivery, action: Action,
) -> Result<(), lapin::Error> {
    match action {
        Action::Ack => {
            chan.basic_ack(deliver.delivery_tag, BasicAckOptions::default()).await
        }
        Action::NackRequeue => {
            chan.basic_nack(deliver.delivery_tag,
                BasicNackOptions { requeue: true, ..Default::default() }).await
        }
        Action::NackDump => {
            chan.basic_nack(deliver.delivery_tag,
                BasicNackOptions::default()).await
        }
        Action::Publish(msg) => {
            let exch = msg.exchange.as_deref().unwrap_or("");
            let key = msg.routing_key.as_deref().unwrap_or("");

            let mut props = BasicProperties::default()
                .with_delivery_mode(2);  // persistent

            if let Some(s) = msg.content_type.as_deref() {
                props = props.with_content_type(s.into());
            }

            chan.basic_publish(
                exch.into(), key.into(),
                BasicPublishOptions::default(),
                &msg.content, props,
            ).await?.await?;
            Ok(())
        }
    }
}
```

Key details:
- **delivery_mode = 2**: All published messages are persistent.
- The double `.await` on `basic_publish`: the first await sends the message,
  the second awaits the publisher confirm from the broker.
- When `exchange` is `None`, an empty string is used (the default exchange).
- When `routing_key` is `None`, an empty string is used.

---

## Consuming Messages

### Consumer Loop (SimpleWorker)

```rust
// easylapin.rs
impl<'a, W: SimpleWorker + 'a> ConsumerExt<'a, W> for Channel {
    async fn consume(self, mut worker: W, config: ConsumeConfig)
        -> Result<Self::Handle, Self::Error>
    {
        let mut consumer = self.basic_consume(
            config.queue.into(),
            config.consumer_tag.into(),
            BasicConsumeOptions::default(),
            FieldTable::default(),
        ).await?;

        Ok(Box::pin(async move {
            while let Some(Ok(deliver)) = consumer.next().await {
                let job = worker.msg_to_job(
                    deliver.routing_key.as_str(),
                    &content_type,
                    &deliver.data,
                ).await.expect("worker unexpected message consumed");

                for action in worker.consumer(&job).await {
                    action_deliver(&self, &deliver, action)
                        .await.expect("action deliver failure");
                }
            }
        }))
    }
}
```

### Consumer Loop (SimpleNotifyWorker)

```rust
impl<'a, W: SimpleNotifyWorker + 'a + Send> ConsumerExt<'a, W> for NotifyChannel {
    async fn consume(self, worker: W, config: ConsumeConfig)
        -> Result<Self::Handle, Self::Error>
    {
        self.0.basic_qos(1, BasicQosOptions::default()).await?;

        let mut consumer = self.0.basic_consume(/* ... */).await?;

        Ok(Box::pin(async move {
            while let Some(Ok(deliver)) = consumer.next().await {
                let receiver = ChannelNotificationReceiver {
                    channel: chan.clone(),
                    deliver,
                };

                let job = worker.msg_to_job(
                    receiver.deliver.routing_key.as_str(),
                    &content_type,
                    &receiver.deliver.data,
                ).expect("worker unexpected message consumed");

                worker.consumer(job, Arc::new(receiver)).await;
            }
        }))
    }
}
```

### Prefetch (QoS)

- **`WorkerChannel`** and **`NotifyChannel`** both set `basic_qos(1)`.
  This means the broker will only deliver one unacknowledged message at a time
  to each consumer. This provides fair dispatch when multiple instances consume
  from the same queue.
- **Raw `Channel`** has no prefetch limit set. This is used by the log
  collector which benefits from prefetching many small messages.

---

## Message Routing Diagram

```
                    github-events (Topic)
                    ┌───────────────────────────────────────────┐
                    │                                           │
                    │  issue_comment.*  ──► build-inputs        │
                    │  pull_request.*   ──► mass-rebuild-check- │
                    │                      inputs               │
                    │  push.*           ──► push-build-inputs   │
                    │  unknown.*        ──► github-events-      │
                    │                      unknown              │
                    └───────────────────────────────────────────┘

                    build-jobs (Fanout)
                    ┌───────────────────────────────────────────┐
                    │                                           │
                    │  ──► build-inputs-x86_64-linux            │
                    │  ──► build-inputs-aarch64-linux           │
                    │  ──► build-inputs-x86_64-darwin           │
                    │  ──► build-inputs-aarch64-darwin          │
                    │  ──► build-inputs-x86_64-windows          │
                    │  ──► build-inputs-aarch64-windows         │
                    │  ──► build-inputs-x86_64-freebsd          │
                    └───────────────────────────────────────────┘

                    build-results (Fanout)
                    ┌───────────────────────────────────────────┐
                    │  ──► build-results                        │
                    └───────────────────────────────────────────┘

                    logs (Topic)
                    ┌───────────────────────────────────────────┐
                    │  *.*  ──► logs (ephemeral)                │
                    └───────────────────────────────────────────┘

                    stats (Fanout)
                    ┌───────────────────────────────────────────┐
                    │  ──► stats-events                         │
                    └───────────────────────────────────────────┘
```

---

## Direct Queue Publishing

Some messages bypass exchanges and are published directly to queues:

| Source | Target Queue | Method |
|--------|-------------|--------|
| evaluation-filter | `mass-rebuild-check-jobs` | `publish_serde_action(None, Some("mass-rebuild-check-jobs"))` |
| github-comment-filter | `build-inputs-{system}` | `publish_serde_action(None, Some("build-inputs-x86_64-linux"))` |
| push-filter | `build-inputs-{system}` | `publish_serde_action(None, Some("build-inputs-x86_64-linux"))` |

When the exchange is `None` (empty string `""`), AMQP uses the **default
exchange**, which routes messages directly to the queue named by the routing key.

---

## Message Acknowledgment Patterns

### Typical Worker Flow

```
1. Receive message from queue
2. Deserialize (msg_to_job)
3. Process (consumer)
4. Return [Action::Publish(...), Action::Publish(...), Action::Ack]
5. All Publish actions are executed
6. Final Ack removes the message from the queue
```

### Error Handling

| Situation | Action | Effect |
|-----------|--------|--------|
| Job decoded, processed successfully | `Ack` | Message removed from queue |
| Temporary error (e.g., expired creds) | `NackRequeue` | Message returned to queue for retry |
| Permanent error (e.g., force-pushed) | `Ack` | Message discarded (no point retrying) |
| Decode failure | `panic!` or `Err` | Consumer thread crashes (message stays in queue) |

### Builder Flow (Notify Worker)

```
1. Receive message
2. Deserialize (msg_to_job)
3. Begin build
4. notifier.tell(Publish(BuildLogStart)) → logs exchange
5. For each line of build output:
   notifier.tell(Publish(BuildLogMsg)) → logs exchange
6. notifier.tell(Publish(BuildResult)) → build-results exchange
7. notifier.tell(Ack) → acknowledge original message
```

---

## Connection Management

### Creating a Connection

```rust
// easylapin.rs
pub async fn from_config(cfg: &RabbitMqConfig) -> Result<Connection, lapin::Error> {
    let opts = ConnectionProperties::default()
        .with_client_property("tickborg_version".into(), tickborg::VERSION.into());
    Connection::connect(&cfg.as_uri()?, opts).await
}
```

The connection URI is constructed from the config:

```rust
impl RabbitMqConfig {
    pub fn as_uri(&self) -> Result<String, std::io::Error> {
        let password = std::fs::read_to_string(&self.password_file)?;
        Ok(format!(
            "{}://{}:{}@{}/{}",
            if self.ssl { "amqps" } else { "amqp" },
            self.username, password, self.host,
            self.virtualhost.clone().unwrap_or_else(|| "/".to_owned()),
        ))
    }
}
```

### Channel Creation

Each binary creates one or more channels from its connection:

```rust
let conn = easylapin::from_config(&cfg.rabbitmq).await?;
let mut chan = conn.create_channel().await?;
```

The builder creates one channel per system architecture:

```rust
for system in &cfg.build.system {
    handles.push(create_handle(&conn, &cfg, system.to_string()).await?);
}
// Each create_handle call does: conn.create_channel().await?
```

### Connection Lifecycle

Connections are held for the lifetime of the process. When the main consumer
future completes (all messages consumed or an error), the connection is dropped:

```rust
handle.await;
drop(conn); // Close connection.
info!("Closed the session... EOF");
```

---

## Consumer Tags

Each consumer is identified by a unique tag derived from the runner identity:

```rust
easyamqp::ConsumeConfig {
    queue: queue_name.clone(),
    consumer_tag: format!("{}-builder", cfg.whoami()),
    // ...
}
```

Where `whoami()` returns `"{identity}-{system}"`:

```rust
impl Config {
    pub fn whoami(&self) -> String {
        format!("{}-{}", self.runner.identity, self.build.system.join(","))
    }
}
```

This ensures that consumer tags are unique across multiple instances and
architectures.
