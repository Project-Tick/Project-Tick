# Tickborg — Webhook Receiver

## Overview

The **GitHub Webhook Receiver** (`github-webhook-receiver`) is the entry point
for all GitHub events into the tickborg system. It is an HTTP server that:

1. Listens for incoming POST requests from GitHub's webhook delivery system.
2. Validates the HMAC-SHA256 signature of every payload.
3. Extracts the event type from the `X-Github-Event` header.
4. Parses the payload to determine the target repository.
5. Publishes the raw payload to the `github-events` RabbitMQ topic exchange.
6. Declares and binds the downstream queues that other workers consume from.

**Source file:** `tickborg/src/bin/github-webhook-receiver.rs`

---

## HTTP Server

The webhook receiver uses **hyper 1.0** directly — no web framework is
involved. The server is configured to listen on the address specified in the
configuration file:

```rust
let addr: SocketAddr = listen.parse().expect("Invalid listen address");
let listener = TcpListener::bind(addr).await?;
```

The main accept loop:

```rust
loop {
    let (stream, _) = listener.accept().await?;
    let io = TokioIo::new(stream);

    let secret = webhook_secret.clone();
    let chan = chan.clone();

    tokio::task::spawn(async move {
        let service = service_fn(move |req| {
            handle_request(req, secret.clone(), chan.clone())
        });
        http1::Builder::new().serve_connection(io, service).await
    });
}
```

Each incoming connection is spawned as an independent tokio task. The service
function (`handle_request`) processes one request at a time per connection.

---

## Request Handling

### HTTP Method Validation

```rust
if req.method() != Method::POST {
    return Ok(empty_response(StatusCode::METHOD_NOT_ALLOWED));
}
```

Only `POST` requests are accepted. Any other method receives a `405 Method Not
Allowed`.

### Header Extraction

Three headers are extracted before consuming the request body:

```rust
let sig_header = req.headers().get("X-Hub-Signature-256")
    .and_then(|v| v.to_str().ok())
    .map(|s| s.to_string());

let event_type = req.headers().get("X-Github-Event")
    .and_then(|v| v.to_str().ok())
    .map(|s| s.to_string());

let content_type = req.headers().get("Content-Type")
    .and_then(|v| v.to_str().ok())
    .map(|s| s.to_string());
```

### Body Collection

```rust
let raw = match req.collect().await {
    Ok(collected) => collected.to_bytes(),
    Err(e) => {
        warn!("Failed to read body from client: {e}");
        return Ok(response(StatusCode::INTERNAL_SERVER_ERROR, "Failed to read body"));
    }
};
```

The full body is collected into a `Bytes` buffer using `http-body-util`'s
`BodyExt::collect()`.

---

## HMAC-SHA256 Signature Verification

GitHub sends a `X-Hub-Signature-256` header with the format:

```
sha256=<hex-encoded HMAC-SHA256>
```

The webhook receiver verifies this signature against the configured webhook
secret:

### Step 1: Parse the signature header

```rust
let Some(sig) = sig_header else {
    return Ok(response(StatusCode::BAD_REQUEST, "Missing signature header"));
};

let mut components = sig.splitn(2, '=');
let Some(algo) = components.next() else {
    return Ok(response(StatusCode::BAD_REQUEST, "Signature hash method missing"));
};
let Some(hash) = components.next() else {
    return Ok(response(StatusCode::BAD_REQUEST, "Signature hash missing"));
};
let Ok(hash) = hex::decode(hash) else {
    return Ok(response(StatusCode::BAD_REQUEST, "Invalid signature hash hex"));
};
```

### Step 2: Validate the algorithm

```rust
if algo != "sha256" {
    return Ok(response(StatusCode::BAD_REQUEST, "Invalid signature hash method"));
}
```

Only SHA-256 is accepted. GitHub also supports SHA-1 (`X-Hub-Signature`) but
tickborg does not accept it.

### Step 3: Compute and compare

```rust
let Ok(mut mac) = Hmac::<Sha256>::new_from_slice(webhook_secret.as_bytes()) else {
    error!("Unable to create HMAC from secret");
    return Ok(response(StatusCode::INTERNAL_SERVER_ERROR, "Internal error"));
};

mac.update(&raw);

if mac.verify_slice(&hash).is_err() {
    return Ok(response(StatusCode::FORBIDDEN, "Signature verification failed"));
}
```

The HMAC is computed using `hmac::Hmac<sha2::Sha256>` from the `hmac` and `sha2`
crates. `verify_slice` performs a constant-time comparison to prevent timing
attacks.

---

## Event Type Routing

After signature verification, the event type and repository are determined:

```rust
let event_type = event_type.unwrap_or_else(|| "unknown".to_owned());

let body_json: GenericWebhook = match serde_json::from_slice(&raw) {
    Ok(webhook) => webhook,
    Err(_) => {
        // If we can't parse the body, route to the unknown queue
        // ...
    }
};

let routing_key = format!("{}.{}", event_type, body_json.repository.full_name);
```

The `GenericWebhook` struct is minimal — it only extracts the `repository`
field:

```rust
// ghevent/common.rs
#[derive(Serialize, Deserialize, Debug)]
pub struct GenericWebhook {
    pub repository: Repository,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Repository {
    pub owner: User,
    pub name: String,
    pub full_name: String,
    pub clone_url: String,
}
```

### Routing Key Format

```
{event_type}.{owner}/{repo}
```

Examples:
- `pull_request.project-tick/Project-Tick`
- `issue_comment.project-tick/Project-Tick`
- `push.project-tick/Project-Tick`
- `unknown.project-tick/Project-Tick`

---

## AMQP Setup

The `setup_amqp` function declares the exchange and all downstream queues:

### Exchange Declaration

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

The `github-events` exchange is a **topic** exchange. This means routing keys
are matched against binding patterns using `.`-separated segments and `*`/`#`
wildcards.

### Queue Declarations and Bindings

| Queue | Binding Pattern | Consumer |
|-------|----------------|----------|
| `build-inputs` | `issue_comment.*` | github-comment-filter |
| `github-events-unknown` | `unknown.*` | (monitoring/debugging) |
| `mass-rebuild-check-inputs` | `pull_request.*` | evaluation-filter |
| `push-build-inputs` | `push.*` | push-filter |

Each queue is declared with:

```rust
chan.declare_queue(easyamqp::QueueConfig {
    queue: queue_name.clone(),
    passive: false,
    durable: true,     // survive broker restart
    exclusive: false,   // accessible by other connections
    auto_delete: false, // don't delete when last consumer disconnects
    no_wait: false,
}).await?;
```

And bound to the exchange:

```rust
chan.bind_queue(easyamqp::BindQueueConfig {
    queue: queue_name.clone(),
    exchange: "github-events".to_owned(),
    routing_key: Some(String::from("issue_comment.*")),
    no_wait: false,
}).await?;
```

---

## Message Publishing

After validation and routing key construction, the raw GitHub payload is
published:

```rust
let props = BasicProperties::default()
    .with_content_type("application/json".into())
    .with_delivery_mode(2);  // persistent

chan.lock().await.basic_publish(
    "github-events".into(),
    routing_key.into(),
    BasicPublishOptions::default(),
    &raw,
    props,
).await?;
```

Key properties:
- **delivery_mode = 2**: Message is persisted to disk by RabbitMQ.
- **content_type**: `application/json` — the raw GitHub payload.
- The **entire raw body** is published, not a parsed/re-serialized version.
  This preserves all fields that downstream consumers might need, even if the
  webhook receiver itself doesn't parse them.

---

## Configuration

The webhook receiver reads from the `github_webhook_receiver` section of the
config:

```rust
#[derive(Serialize, Deserialize, Debug)]
pub struct GithubWebhookConfig {
    pub listen: String,
    pub webhook_secret_file: String,
    pub rabbitmq: RabbitMqConfig,
}
```

Example configuration:

```json
{
    "github_webhook_receiver": {
        "listen": "0.0.0.0:9899",
        "webhook_secret_file": "/run/secrets/tickborg/webhook-secret",
        "rabbitmq": {
            "ssl": false,
            "host": "rabbitmq:5672",
            "virtualhost": "tickborg",
            "username": "tickborg",
            "password_file": "/run/secrets/tickborg/rabbitmq-password"
        }
    }
}
```

The webhook secret is read from a file (not inline in the config) to prevent
accidental exposure in version control.

---

## Response Codes

| Code | Meaning |
|------|---------|
| `200 OK` | Webhook received and published successfully |
| `400 Bad Request` | Missing or malformed signature header |
| `403 Forbidden` | Signature verification failed |
| `405 Method Not Allowed` | Non-POST request |
| `500 Internal Server Error` | Body read failure or HMAC creation failure |

---

## GitHub Webhook Configuration

### Required Events

The GitHub App or webhook should be configured to send:

| Event | Used By |
|-------|---------|
| `pull_request` | evaluation-filter (auto-eval on PR open/sync) |
| `issue_comment` | github-comment-filter (@tickbot commands) |
| `push` | push-filter (branch push CI) |
| `check_run` | (optional, for re-run triggers) |

### Required Permissions (GitHub App)

| Permission | Level | Purpose |
|------------|-------|---------|
| Pull requests | Read & Write | Read PR details, post comments |
| Commit statuses | Read & Write | Set commit status checks |
| Issues | Read & Write | Read comments, manage labels |
| Contents | Read | Clone repository, read files |
| Checks | Read & Write | Create/update check runs |

### Webhook URL

```
https://<your-domain>:9899/github-webhooks
```

The receiver accepts POSTs on any path — the path segment is not validated.
However, conventionally `/github-webhooks` is used.

---

## Security Considerations

### Signature Verification

**Every** request must have a valid `X-Hub-Signature-256` header. Requests
without this header, or with an invalid signature, are rejected before any
processing occurs. The HMAC comparison uses `verify_slice` which is
constant-time.

### Secret File

The webhook secret is read from a file rather than an environment variable or
inline config value. This:
- Prevents accidental exposure in process listings (`/proc/*/environ`)
- Allows secrets management via Docker secrets, Kubernetes secrets, or
  NixOS `sops-nix`

### No Path Traversal

The webhook receiver does not serve files or interact with the filesystem beyond
reading the config and secret files. There is no path traversal risk.

### Rate Limiting

The webhook receiver does **not** implement application-level rate limiting.
This should be handled by:
- An upstream reverse proxy (nginx, Caddy)
- GitHub's own delivery rate limiting
- RabbitMQ's flow control mechanisms

---

## Deployment

### Docker Compose

```yaml
webhook-receiver:
    build:
        context: .
        dockerfile: Dockerfile
    command: ["github-webhook-receiver", "/etc/tickborg/config.json"]
    ports:
        - "9899:9899"
    volumes:
        - ./config.json:/etc/tickborg/config.json:ro
        - ./secrets:/run/secrets/tickborg:ro
    depends_on:
        rabbitmq:
            condition: service_healthy
    restart: unless-stopped
```

### NixOS (`service.nix`)

```nix
systemd.services."tickborg-webhook-receiver" = mkTickborgService "Webhook Receiver" {
    binary = "github_webhook_receiver";
};
```

Note: The binary name uses underscores (`github_webhook_receiver`) while the
Cargo target uses hyphens (`github-webhook-receiver`). Cargo generates both
forms but the NixOS service uses the underscore variant.

---

## Monitoring

The webhook receiver logs:
- Every accepted webhook (event type, routing key)
- Signature verification failures (at `warn` level)
- AMQP publish errors (at `error` level)
- Body read failures (at `warn` level)

Check the `github-events-unknown` queue for events that couldn't be routed to
a handler — these indicate new event types that may need new consumers.

---

## Event Type Reference

| GitHub Event | Routing Key Pattern | Queue | Handler |
|-------------|--------------------|---------|---------| 
| `pull_request` | `pull_request.{owner}/{repo}` | `mass-rebuild-check-inputs` | evaluation-filter |
| `issue_comment` | `issue_comment.{owner}/{repo}` | `build-inputs` | github-comment-filter |
| `push` | `push.{owner}/{repo}` | `push-build-inputs` | push-filter |
| (any other) | `unknown.{owner}/{repo}` | `github-events-unknown` | none (monitoring) |
