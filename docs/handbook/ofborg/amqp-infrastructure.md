# Tickborg — AMQP Infrastructure

## Overview

Tickborg uses **AMQP 0-9-1** (RabbitMQ) as the message bus connecting all
services. The Rust crate `lapin` (v4.3.0) provides the low-level protocol
client. Two abstraction layers — `easyamqp` and `easylapin` — provide
higher-level APIs for declaring exchanges, binding queues, and running worker
consumers.

---

## Key Source Files

| File | Purpose |
|------|---------|
| `tickborg/src/easyamqp.rs` | Config types, traits, exchange/queue declarations |
| `tickborg/src/easylapin.rs` | `lapin`-based implementations of the traits |
| `tickborg/src/worker.rs` | `SimpleWorker` trait, `Action` enum |
| `tickborg/src/notifyworker.rs` | `SimpleNotifyWorker`, `NotificationReceiver` |
| `tickborg/src/config.rs` | `RabbitMqConfig` |

---

## Connection Configuration

### `RabbitMqConfig`

```rust
// config.rs
#[derive(Deserialize, Debug)]
pub struct RabbitMqConfig {
    pub ssl: bool,
    pub host: String,
    pub vhost: Option<String>,
    pub username: String,
    pub password_file: PathBuf,
}
```

### Connection URI Construction

```rust
// easylapin.rs
pub async fn from_config(cfg: &RabbitMqConfig) -> Result<lapin::Connection, lapin::Error> {
    let password = std::fs::read_to_string(&cfg.password_file)
        .expect("Failed to read RabbitMQ password file")
        .trim()
        .to_owned();

    let vhost = cfg.vhost
        .as_deref()
        .unwrap_or("/")
        .to_owned();

    let scheme = if cfg.ssl { "amqps" } else { "amqp" };
    let uri = format!(
        "{scheme}://{user}:{pass}@{host}/{vhost}",
        user = urlencoding::encode(&cfg.username),
        pass = urlencoding::encode(&password),
        host = cfg.host,
        vhost = urlencoding::encode(&vhost),
    );

    lapin::Connection::connect(
        &uri,
        lapin::ConnectionProperties::default()
            .with_tokio()
            .with_default_executor(8),
    ).await
}
```

---

## Exchange and Queue Configuration Types

### `ExchangeType`

```rust
#[derive(Clone, Debug)]
pub enum ExchangeType {
    Topic,
    Fanout,
    Headers,
    Direct,
    Custom(String),
}

impl ExchangeType {
    fn as_str(&self) -> &str {
        match self {
            ExchangeType::Topic => "topic",
            ExchangeType::Fanout => "fanout",
            ExchangeType::Headers => "headers",
            ExchangeType::Direct => "direct",
            ExchangeType::Custom(s) => s.as_ref(),
        }
    }
}
```

### `ExchangeConfig`

```rust
#[derive(Clone, Debug)]
pub struct ExchangeConfig {
    pub exchange_name: String,
    pub exchange_type: ExchangeType,
    pub passive: bool,
    pub durable: bool,
    pub exclusive: bool,
    pub auto_delete: bool,
    pub no_wait: bool,
    pub internal: bool,
}

impl Default for ExchangeConfig {
    fn default() -> Self {
        ExchangeConfig {
            exchange_name: String::new(),
            exchange_type: ExchangeType::Topic,
            passive: false,
            durable: true,
            exclusive: false,
            auto_delete: false,
            no_wait: false,
            internal: false,
        }
    }
}
```

### `QueueConfig`

```rust
#[derive(Clone, Debug)]
pub struct QueueConfig {
    pub queue_name: String,
    pub passive: bool,
    pub durable: bool,
    pub exclusive: bool,
    pub auto_delete: bool,
    pub no_wait: bool,
}

impl Default for QueueConfig {
    fn default() -> Self {
        QueueConfig {
            queue_name: String::new(),
            passive: false,
            durable: true,
            exclusive: false,
            auto_delete: false,
            no_wait: false,
        }
    }
}
```

### `BindQueueConfig`

```rust
#[derive(Clone, Debug)]
pub struct BindQueueConfig {
    pub queue_name: String,
    pub exchange_name: String,
    pub routing_key: Option<String>,
    pub no_wait: bool,
    pub headers: Option<Vec<(String, String)>>,
}
```

### `ConsumeConfig`

```rust
#[derive(Clone, Debug)]
pub struct ConsumeConfig {
    pub queue: String,
    pub consumer_tag: String,
    pub no_local: bool,
    pub no_ack: bool,
    pub no_wait: bool,
    pub exclusive: bool,
}
```

---

## The `ChannelExt` Trait

```rust
// easyamqp.rs
pub trait ChannelExt {
    fn declare_exchange(
        &mut self,
        config: ExchangeConfig,
    ) -> impl Future<Output = Result<(), String>>;

    fn declare_queue(
        &mut self,
        config: QueueConfig,
    ) -> impl Future<Output = Result<(), String>>;

    fn bind_queue(
        &mut self,
        config: BindQueueConfig,
    ) -> impl Future<Output = Result<(), String>>;
}
```

### `lapin` Implementation

```rust
// easylapin.rs
impl ChannelExt for lapin::Channel {
    async fn declare_exchange(&mut self, config: ExchangeConfig) -> Result<(), String> {
        let opts = ExchangeDeclareOptions {
            passive: config.passive,
            durable: config.durable,
            auto_delete: config.auto_delete,
            internal: config.internal,
            nowait: config.no_wait,
        };
        self.exchange_declare(
            &config.exchange_name,
            lapin::ExchangeKind::Custom(
                config.exchange_type.as_str().to_owned()
            ),
            opts,
            FieldTable::default(),
        ).await
        .map_err(|e| format!("Failed to declare exchange: {e}"))?;
        Ok(())
    }

    async fn declare_queue(&mut self, config: QueueConfig) -> Result<(), String> {
        let opts = QueueDeclareOptions {
            passive: config.passive,
            durable: config.durable,
            exclusive: config.exclusive,
            auto_delete: config.auto_delete,
            nowait: config.no_wait,
        };
        self.queue_declare(
            &config.queue_name,
            opts,
            FieldTable::default(),
        ).await
        .map_err(|e| format!("Failed to declare queue: {e}"))?;
        Ok(())
    }

    async fn bind_queue(&mut self, config: BindQueueConfig) -> Result<(), String> {
        let routing_key = config.routing_key
            .as_deref()
            .unwrap_or("#");

        let mut headers = FieldTable::default();
        if let Some(hdr_vec) = &config.headers {
            for (k, v) in hdr_vec {
                headers.insert(
                    k.clone().into(),
                    AMQPValue::LongString(v.clone().into()),
                );
            }
        }

        self.queue_bind(
            &config.queue_name,
            &config.exchange_name,
            routing_key,
            QueueBindOptions { nowait: config.no_wait },
            headers,
        ).await
        .map_err(|e| format!("Failed to bind queue: {e}"))?;
        Ok(())
    }
}
```

---

## The `ConsumerExt` Trait

```rust
// easyamqp.rs
pub trait ConsumerExt {
    fn consume<W: worker::SimpleWorker + 'static>(
        &mut self,
        worker: W,
        config: ConsumeConfig,
    ) -> impl Future<Output = Result<(), String>>;
}
```

Three implementations exist in `easylapin.rs`:

### 1. `Channel` — Simple Workers

```rust
impl ConsumerExt for lapin::Channel {
    async fn consume<W: worker::SimpleWorker + 'static>(
        &mut self,
        mut worker: W,
        config: ConsumeConfig,
    ) -> Result<(), String> {
        let consumer = self.basic_consume(
            &config.queue,
            &config.consumer_tag,
            BasicConsumeOptions {
                no_local: config.no_local,
                no_ack: config.no_ack,
                exclusive: config.exclusive,
                nowait: config.no_wait,
            },
            FieldTable::default(),
        ).await
        .map_err(|e| format!("Failed to start consumer: {e}"))?;

        // Message processing loop
        while let Some(delivery) = consumer.next().await {
            let delivery = delivery
                .map_err(|e| format!("Consumer error: {e}"))?;

            // Decode the message
            let job = match worker.msg_to_job(
                &delivery.routing_key,
                &delivery.exchange,
                &delivery.data,
            ).await {
                Ok(job) => job,
                Err(err) => {
                    tracing::error!("Failed to decode message: {}", err);
                    delivery.ack(BasicAckOptions::default()).await?;
                    continue;
                }
            };

            // Process the job
            let actions = worker.consumer(&job).await;

            // Execute resulting actions
            for action in actions {
                action_deliver(&self, &delivery, action).await?;
            }
        }
        Ok(())
    }
}
```

### 2. `WorkerChannel` — Workers on a Dedicated Channel

```rust
pub struct WorkerChannel {
    pub channel: lapin::Channel,
    pub prefetch_count: u16,
}

impl ConsumerExt for WorkerChannel {
    async fn consume<W: worker::SimpleWorker + 'static>(
        &mut self,
        worker: W,
        config: ConsumeConfig,
    ) -> Result<(), String> {
        // Set QoS (prefetch count)
        self.channel.basic_qos(
            self.prefetch_count,
            BasicQosOptions::default(),
        ).await?;

        // Delegate to Channel implementation
        self.channel.consume(worker, config).await
    }
}
```

### 3. `NotifyChannel` — Notify Workers

```rust
pub struct NotifyChannel {
    pub channel: lapin::Channel,
}

impl NotifyChannel {
    pub async fn consume<W: notifyworker::SimpleNotifyWorker + 'static>(
        &mut self,
        mut worker: W,
        config: ConsumeConfig,
    ) -> Result<(), String> {
        // Similar to Channel but creates a ChannelNotificationReceiver
        // that allows the worker to report progress back to AMQP
        let consumer = self.channel.basic_consume(/* ... */).await?;

        while let Some(delivery) = consumer.next().await {
            let delivery = delivery?;
            let receiver = ChannelNotificationReceiver {
                channel: self.channel.clone(),
                delivery: &delivery,
            };

            let job = worker.msg_to_job(/* ... */).await?;
            let actions = worker.consumer(&job, &receiver).await;

            for action in actions {
                action_deliver(&self.channel, &delivery, action).await?;
            }
        }
        Ok(())
    }
}
```

---

## Action Delivery

```rust
// easylapin.rs
async fn action_deliver(
    channel: &lapin::Channel,
    delivery: &lapin::message::Delivery,
    action: worker::Action,
) -> Result<(), String> {
    match action {
        worker::Action::Ack => {
            delivery.ack(BasicAckOptions::default()).await
                .map_err(|e| format!("Failed to ack: {e}"))?;
        }
        worker::Action::NackRequeue => {
            delivery.nack(BasicNackOptions {
                requeue: true,
                ..Default::default()
            }).await
                .map_err(|e| format!("Failed to nack: {e}"))?;
        }
        worker::Action::NackDump => {
            delivery.nack(BasicNackOptions {
                requeue: false,
                ..Default::default()
            }).await
                .map_err(|e| format!("Failed to nack-dump: {e}"))?;
        }
        worker::Action::Publish(msg) => {
            channel.basic_publish(
                msg.exchange.as_deref().unwrap_or(""),
                msg.routing_key.as_deref().unwrap_or(""),
                BasicPublishOptions::default(),
                &msg.content,
                BasicProperties::default()
                    .with_delivery_mode(2),  // persistent
            ).await
                .map_err(|e| format!("Failed to publish: {e}"))?;
        }
    }
    Ok(())
}
```

---

## Notification Receiver

```rust
// easylapin.rs
pub struct ChannelNotificationReceiver<'a> {
    channel: lapin::Channel,
    delivery: &'a lapin::message::Delivery,
}

impl<'a> notifyworker::NotificationReceiver for ChannelNotificationReceiver<'a> {
    async fn tell(&mut self, action: worker::Action) {
        if let Err(e) = action_deliver(&self.channel, self.delivery, action).await {
            tracing::error!("Failed to deliver notification action: {}", e);
        }
    }
}
```

Used by `BuildWorker` (which implements `SimpleNotifyWorker`) to publish
incremental log messages while a build is in progress, without waiting for the
build to complete.

---

## Exchange Topology

### Declarations

Every binary declares its own required exchanges/queues at startup.
Here is the complete topology used across the system:

| Exchange | Type | Purpose |
|----------|------|---------|
| `github-events` | Topic | GitHub webhooks → routing by event type |
| `build-jobs` | Fanout | Evaluation → builders |
| `build-results` | Fanout | Builder results → poster + stats |
| `logs` | Topic | Build log lines → collector |
| `stats` | Fanout | Metrics events → stats collector |

### Queue Bindings

| Queue | Exchange | Routing Key | Consumer |
|-------|----------|-------------|----------|
| `mass-rebuild-check-inputs` | `github-events` | `pull_request.*` | EvaluationFilterWorker |
| `mass-rebuild-check-jobs` | _(direct publish)_ | — | EvaluationWorker |
| `build-inputs-{identity}` | `build-jobs` | — | BuildWorker |
| `build-results` | `build-results` | — | GitHubCommentPoster |
| `build-logs` | `logs` | `logs.*` | LogMessageCollector |
| `comment-jobs` | `github-events` | `issue_comment.*` | GitHubCommentWorker |
| `push-jobs` | `github-events` | `push.*` | PushFilterWorker |
| `stats-events` | `stats` | — | StatCollectorWorker |

### Topic Routing Keys

For the `github-events` exchange, the routing key follows the pattern:

```
{event_type}.{action}
```

Examples:
- `pull_request.opened`
- `pull_request.synchronize`
- `issue_comment.created`
- `push.push`

For the `logs` exchange:
- `logs.{build_id}` — Each build's log lines are tagged with the build ID

---

## Message Persistence

All published messages use `delivery_mode = 2` (persistent), which means
messages survive RabbitMQ restarts:

```rust
BasicProperties::default()
    .with_delivery_mode(2)  // persistent
```

---

## Prefetch / QoS

Worker binaries configure `basic_qos` (prefetch count) to control how many
messages are delivered to a consumer before it must acknowledge them:

```rust
let mut chan = WorkerChannel {
    channel,
    prefetch_count: 1,  // Process one job at a time
};
```

Setting `prefetch_count = 1` ensures fair dispatching across multiple worker
instances and prevents a single slow worker from hoarding messages.

---

## Error Recovery

### Message Processing Failures

| Scenario | Action | Effect |
|----------|--------|--------|
| Decode error | `Ack` | Message discarded |
| Processing error (retryable) | `NackRequeue` | Message requeued |
| Processing error (permanent) | `NackDump` | Message dead-lettered |
| Processing success | `Ack` | Message removed |
| Worker publish | `Publish` | New message to exchange |

### Connection Recovery

`lapin` supports automatic connection recovery. If the TCP connection drops,
the library will attempt to reconnect. However, tickborg binaries are designed
to be restarted by their process supervisor (systemd) if the connection
cannot be re-established.

---

## Usage Example: Declaring a Full Stack

A typical binary does:

```rust
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tickborg::setup_log();
    let cfg = tickborg::config::load();

    // 1. Connect to RabbitMQ
    let conn = easylapin::from_config(&cfg.rabbitmq).await?;
    let mut chan = conn.create_channel().await?;

    // 2. Declare exchange
    chan.declare_exchange(ExchangeConfig {
        exchange_name: "github-events".to_owned(),
        exchange_type: ExchangeType::Topic,
        durable: true,
        ..Default::default()
    }).await?;

    // 3. Declare queue
    chan.declare_queue(QueueConfig {
        queue_name: "mass-rebuild-check-inputs".to_owned(),
        durable: true,
        ..Default::default()
    }).await?;

    // 4. Bind queue to exchange
    chan.bind_queue(BindQueueConfig {
        queue_name: "mass-rebuild-check-inputs".to_owned(),
        exchange_name: "github-events".to_owned(),
        routing_key: Some("pull_request.*".to_owned()),
        ..Default::default()
    }).await?;

    // 5. Start consume loop
    let worker = EvaluationFilterWorker::new(cfg.acl());
    chan.consume(worker, ConsumeConfig {
        queue: "mass-rebuild-check-inputs".to_owned(),
        consumer_tag: format!("evaluation-filter-{}", cfg.identity),
        ..Default::default()
    }).await?;

    Ok(())
}
```
