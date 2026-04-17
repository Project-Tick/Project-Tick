# Tickborg — Configuration Reference

## Overview

Tickborg is configured via a single JSON file, typically located at
`config.json` or specified via the `CONFIG_PATH` environment variable.
The file maps to the top-level `Config` struct in `tickborg/src/config.rs`.

---

## Loading Configuration

```rust
// config.rs
pub fn load() -> Config {
    let config_path = env::var("CONFIG_PATH")
        .unwrap_or_else(|_| "config.json".to_owned());

    let config_str = std::fs::read_to_string(&config_path)
        .unwrap_or_else(|e| panic!("Failed to read config file {config_path}: {e}"));

    serde_json::from_str(&config_str)
        .unwrap_or_else(|e| panic!("Failed to parse config file {config_path}: {e}"))
}
```

---

## Top-Level `Config`

```rust
#[derive(Deserialize, Debug)]
pub struct Config {
    pub identity: String,
    pub rabbitmq: RabbitMqConfig,
    pub github_app: Option<GithubAppConfig>,

    // Per-service configs — only the relevant one needs to be present
    pub github_webhook: Option<GithubWebhookConfig>,
    pub log_api: Option<LogApiConfig>,
    pub evaluation_filter: Option<EvaluationFilterConfig>,
    pub mass_rebuilder: Option<MassRebuilderConfig>,
    pub builder: Option<BuilderConfig>,
    pub github_comment_filter: Option<GithubCommentFilterConfig>,
    pub github_comment_poster: Option<GithubCommentPosterConfig>,
    pub log_message_collector: Option<LogMessageCollectorConfig>,
    pub push_filter: Option<PushFilterConfig>,
    pub stats: Option<StatsConfig>,
}
```

### `identity`

A unique string identifying this instance. Used as:
- AMQP consumer tags (`evaluation-filter-{identity}`)
- Exclusive queue names (`build-inputs-{identity}`)
- GitHub Check Run external ID

```json
{
  "identity": "prod-worker-01"
}
```

---

## `RabbitMqConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct RabbitMqConfig {
    pub ssl: bool,
    pub host: String,
    pub vhost: Option<String>,
    pub username: String,
    pub password_file: PathBuf,
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `ssl` | bool | yes | Use `amqps://` instead of `amqp://` |
| `host` | string | yes | RabbitMQ hostname (may include port) |
| `vhost` | string | no | Virtual host (default: `/`) |
| `username` | string | yes | AMQP username |
| `password_file` | path | yes | File containing the password (not the password itself) |

```json
{
  "rabbitmq": {
    "ssl": true,
    "host": "rabbitmq.example.com",
    "vhost": "tickborg",
    "username": "tickborg",
    "password_file": "/run/secrets/rabbitmq-password"
  }
}
```

> **Security**: The password is read from a file rather than stored directly
> in the config, allowing secure credential injection via systemd credentials,
> Docker secrets, or similar mechanisms.

---

## `GithubAppConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct GithubAppConfig {
    pub app_id: u64,
    pub private_key_file: PathBuf,
    pub owner: String,
    pub repo: String,
    pub installation_id: Option<u64>,
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `app_id` | u64 | yes | GitHub App ID |
| `private_key_file` | path | yes | PEM-encoded RSA private key |
| `owner` | string | yes | Repository owner |
| `repo` | string | yes | Repository name |
| `installation_id` | u64 | no | Installation ID (auto-detected if omitted) |

```json
{
  "github_app": {
    "app_id": 12345,
    "private_key_file": "/run/secrets/github-app-key.pem",
    "owner": "project-tick",
    "repo": "Project-Tick",
    "installation_id": 67890
  }
}
```

---

## Service-Specific Configs

### `GithubWebhookConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct GithubWebhookConfig {
    pub bind_address: Option<String>,
    pub port: u16,
    pub webhook_secret: String,
}
```

```json
{
  "github_webhook": {
    "bind_address": "0.0.0.0",
    "port": 8080,
    "webhook_secret": "your-webhook-secret-here"
  }
}
```

### `LogApiConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct LogApiConfig {
    pub bind_address: Option<String>,
    pub port: u16,
    pub log_storage_path: PathBuf,
}
```

```json
{
  "log_api": {
    "port": 8081,
    "log_storage_path": "/var/log/tickborg/builds"
  }
}
```

### `EvaluationFilterConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct EvaluationFilterConfig {
    pub repos: Vec<String>,
}
```

```json
{
  "evaluation_filter": {
    "repos": [
      "project-tick/Project-Tick"
    ]
  }
}
```

### `MassRebuilderConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct MassRebuilderConfig {
    pub checkout: CheckoutConfig,
}
```

```json
{
  "mass_rebuilder": {
    "checkout": {
      "root": "/var/cache/tickborg/checkout"
    }
  }
}
```

### `BuilderConfig` / `RunnerConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct BuilderConfig {
    pub runner: RunnerConfig,
    pub checkout: CheckoutConfig,
    pub build: BuildConfig,
}

#[derive(Deserialize, Debug)]
pub struct RunnerConfig {
    pub identity: Option<String>,
    pub architectures: Vec<String>,
}

#[derive(Deserialize, Debug)]
pub struct BuildConfig {
    pub timeout_seconds: u64,
    pub log_tail_lines: usize,
}

#[derive(Deserialize, Debug)]
pub struct CheckoutConfig {
    pub root: PathBuf,
}
```

```json
{
  "builder": {
    "runner": {
      "identity": "builder-x86_64-linux",
      "architectures": ["x86_64-linux"]
    },
    "checkout": {
      "root": "/var/cache/tickborg/checkout"
    },
    "build": {
      "timeout_seconds": 3600,
      "log_tail_lines": 100
    }
  }
}
```

### `GithubCommentFilterConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct GithubCommentFilterConfig {
    pub repos: Vec<String>,
    pub trusted_users: Option<Vec<String>>,
}
```

```json
{
  "github_comment_filter": {
    "repos": ["project-tick/Project-Tick"],
    "trusted_users": ["maintainer1", "maintainer2"]
  }
}
```

### `LogMessageCollectorConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct LogMessageCollectorConfig {
    pub log_storage_path: PathBuf,
}
```

```json
{
  "log_message_collector": {
    "log_storage_path": "/var/log/tickborg/builds"
  }
}
```

### `StatsConfig`

```rust
#[derive(Deserialize, Debug)]
pub struct StatsConfig {
    pub bind_address: Option<String>,
    pub port: u16,
}
```

```json
{
  "stats": {
    "port": 9090
  }
}
```

---

## Complete Example

Based on `example.config.json`:

```json
{
  "identity": "prod-01",
  "rabbitmq": {
    "ssl": false,
    "host": "localhost",
    "vhost": "tickborg",
    "username": "tickborg",
    "password_file": "/run/secrets/rabbitmq-password"
  },
  "github_app": {
    "app_id": 12345,
    "private_key_file": "/run/secrets/github-app-key.pem",
    "owner": "project-tick",
    "repo": "Project-Tick"
  },
  "github_webhook": {
    "port": 8080,
    "webhook_secret": "change-me"
  },
  "evaluation_filter": {
    "repos": ["project-tick/Project-Tick"]
  },
  "mass_rebuilder": {
    "checkout": {
      "root": "/var/cache/tickborg/checkout"
    }
  },
  "builder": {
    "runner": {
      "architectures": ["x86_64-linux"]
    },
    "checkout": {
      "root": "/var/cache/tickborg/checkout"
    },
    "build": {
      "timeout_seconds": 3600,
      "log_tail_lines": 100
    }
  },
  "github_comment_filter": {
    "repos": ["project-tick/Project-Tick"]
  },
  "log_message_collector": {
    "log_storage_path": "/var/log/tickborg/builds"
  },
  "log_api": {
    "port": 8081,
    "log_storage_path": "/var/log/tickborg/builds"
  },
  "stats": {
    "port": 9090
  }
}
```

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CONFIG_PATH` | `config.json` | Path to the JSON config file |
| `RUST_LOG` | `info` | `tracing` filter directive |
| `RUST_LOG_JSON` | (unset) | Set to `1` for structured JSON log output |

### `RUST_LOG` Examples

```bash
# Default — info for everything
RUST_LOG=info

# Debug for tickborg, info for everything else
RUST_LOG=info,tickborg=debug

# Trace AMQP operations
RUST_LOG=info,tickborg=debug,lapin=trace

# Only errors
RUST_LOG=error
```

### Logging Initialization

```rust
// lib.rs
pub fn setup_log() {
    let json = std::env::var("RUST_LOG_JSON").is_ok();

    let subscriber = tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env());

    if json {
        subscriber.json().init();
    } else {
        subscriber.init();
    }
}
```

---

## ACL Configuration

The ACL (Access Control List) is derived from the configuration and controls:

- **Repository eligibility** — Which repos tickborg responds to
- **Architecture access** — Which platforms a user can build on
- **Unrestricted builds** — Whether a user can bypass project restrictions

```rust
// acl.rs
pub struct Acl {
    repos: Vec<String>,
    trusted_users: Vec<String>,
}

impl Acl {
    pub fn is_repo_eligible(&self, repo: &str) -> bool;
    pub fn build_job_architectures_for_user_repo(
        &self, user: &str, repo: &str
    ) -> Vec<System>;
    pub fn can_build_unrestricted(&self, user: &str, repo: &str) -> bool;
}
```

---

## Secrets Management

Files containing secrets should be readable only by the tickborg service user:

```bash
# RabbitMQ password
echo -n "secret-password" > /run/secrets/rabbitmq-password
chmod 600 /run/secrets/rabbitmq-password

# GitHub App private key
cp github-app.pem /run/secrets/github-app-key.pem
chmod 600 /run/secrets/github-app-key.pem
```

With NixOS and systemd `DynamicUser`, secrets can be placed in
`/run/credentials/tickborg-*` using systemd's `LoadCredential` or
`SetCredential` directives.
