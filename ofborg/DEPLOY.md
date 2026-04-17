# Tickborg Deployment Guide

Tickborg is a distributed CI bot system for the Project Tick monorepo.
It consists of 10 Rust microservices that communicate over RabbitMQ.

## Architecture

```
GitHub Webhook
      │
      ▼
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ webhook-receiver │────▶│    RabbitMQ       │◀────│     stats       │
└─────────────────┘     │  (message queue)  │     │  (prometheus)   │
                        └────────┬─────────┘     └─────────────────┘
                 ┌───────┬───────┼───────┬────────────┐
                 ▼       ▼       ▼       ▼            ▼
           ┌──────┐ ┌────────┐ ┌──────┐ ┌──────────┐ ┌─────────┐
           │eval  │ │comment │ │build │ │mass      │ │log      │
           │filter│ │filter  │ │er    │ │rebuilder │ │collector│
           └──────┘ └────┬───┘ └──────┘ └──────────┘ └─────────┘
                         ▼
                   ┌──────────┐
                   │comment   │
                   │poster    │
                   └──────────┘
```

## Prerequisites

### 1. Create a GitHub App

1. GitHub → Settings → Developer Settings → GitHub Apps → New GitHub App
2. Settings:
   - **App name**: `tickborg` (or any name you prefer)
   - **Homepage URL**: Your repo URL
   - **Webhook URL**: `https://YOUR_SERVER_ADDRESS:9899/webhook`
   - **Webhook secret**: A strong random string
3. Permissions:
   - **Repository permissions**:
     - Checks: Read & write
     - Commit statuses: Read & write
     - Contents: Read-only
     - Issues: Read & write
     - Pull requests: Read & write
   - **Subscribe to events**:
     - Check run
     - Issue comment
     - Pull request
     - Push
4. Download the private key → save as `github-private-key.pem`
5. Note the App ID and Installation ID

### 2. RabbitMQ

RabbitMQ is used as the message queue. All services communicate through this queue.

### 3. Config File

Copy and edit `example.config.json`:

```bash
cp example.config.json config.json
```

Fields to edit:
```json
{
  "github": {
    "app_id": 12345,
    "installation_id": 67890,
    "private_key": "/path/to/github-private-key.pem",
    "webhook_secret": "your-webhook-secret"
  },
  "rabbitmq": {
    "host": "rabbitmq",
    "username": "tickborg",
    "password": "STRONG_PASSWORD"
  },
  "checkout": {
    "root": "/var/lib/tickborg/checkout"
  },
  "log_storage": {
    "path": "/var/log/tickborg"
  }
}
```

---

## Method 1: RHEL/CentOS/Fedora + Nix (Recommended)

Build with Nix on RHEL-based servers and run as systemd services.
No Docker required — everything runs as native binaries.

### Prerequisites

#### Install Nix

```bash
# Multi-user installation (recommended)
curl -L https://nixos.org/nix/install | sh -s -- --daemon

# Reopen your shell or:
. /etc/profile.d/nix.sh

# Enable Flakes support
mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf

# Test
nix --version
```

### One-Command Setup

```bash
cd ofborg/
sudo ./deploy/deploy.sh install
```

This command will:
1. Create the `tickborg` system user
2. Set up `/var/lib/tickborg/`, `/etc/tickborg/`, `/var/log/tickborg/` directories
3. Install build tools (cmake, meson, gcc, jdk, cargo...) from the RHEL repo
4. Install and configure RabbitMQ
5. Build tickborg with Nix
6. Copy binaries to `/var/lib/tickborg/bin/`
7. Install systemd template services

### Edit Config

```bash
sudo nano /etc/tickborg/config.json
sudo cp github-private-key.pem /etc/tickborg/
sudo chown tickborg:tickborg /etc/tickborg/github-private-key.pem
sudo chmod 600 /etc/tickborg/github-private-key.pem
```

### Start Services

```bash
# Start all services
sudo systemctl start tickborg.target

# Check status
./deploy/deploy.sh status
```

### Service Management

```bash
# Manage individual services
sudo systemctl start tickborg@builder.service
sudo systemctl stop tickborg@mass_rebuilder.service
sudo systemctl restart tickborg@github_webhook_receiver.service

# Logs
./deploy/deploy.sh logs builder
./deploy/deploy.sh logs github_webhook_receiver

# or directly via journalctl
journalctl -u tickborg@builder -f
```

### Update

```bash
# Pull code + rebuild + restart services
sudo ./deploy/deploy.sh update

# or just rebuild + deploy
sudo ./deploy/deploy.sh deploy
```

### Adding Extra Builders

To run only the builder on another machine:

```bash
# On the other server
sudo ./deploy/deploy.sh install

# Enable only the builder
sudo systemctl disable tickborg@github_webhook_receiver.service
sudo systemctl disable tickborg@evaluation_filter.service
sudo systemctl disable tickborg@github_comment_filter.service
sudo systemctl disable tickborg@github_comment_poster.service
sudo systemctl disable tickborg@log_message_collector.service
sudo systemctl disable tickborg@stats.service

# Point the RabbitMQ host to the main server in config.json
sudo systemctl start tickborg@builder.service
```

### Systemd Services

All services use the `tickborg@.service` template unit.
They are grouped under `tickborg.target`.

| Unit | Description |
|------|-------------|
| `tickborg@github_webhook_receiver` | Receives GitHub webhooks |
| `tickborg@evaluation_filter` | Processes the PR evaluation queue |
| `tickborg@github_comment_filter` | Parses @tickbot commands |
| `tickborg@github_comment_poster` | Posts build results to PRs |
| `tickborg@builder` | Builds and tests projects |
| `tickborg@mass_rebuilder` | Bulk rebuild jobs (optional) |
| `tickborg@log_message_collector` | Collects build logs |
| `tickborg@stats` | Prometheus metrics (:9898) |

---

## Method 2: NixOS

Run as systemd services on NixOS-based servers.

### configuration.nix

```nix
{ config, pkgs, ... }:
{
  imports = [
    /path/to/ofborg/service.nix
  ];

  # RabbitMQ
  services.rabbitmq = {
    enable = true;
    listenAddress = "127.0.0.1";
  };

  # Tickborg
  services.tickborg = {
    enable = true;
    configFile = "/etc/tickborg/config.json";

    # Enable whichever services you need
    enableBuilder = true;
    enableWebhook = true;
    enableEvaluationFilter = true;
    enableCommentFilter = true;
    enableCommentPoster = true;
    enableLogCollector = true;
    enableStats = true;
    enableMassRebuilder = false;  # Optional
  };

  # Firewall for webhook
  networking.firewall.allowedTCPPorts = [ 9899 ];
}
```

### Place the Config File

```bash
sudo mkdir -p /etc/tickborg
sudo cp config.json /etc/tickborg/config.json
sudo chmod 600 /etc/tickborg/config.json
sudo chown tickborg:tickborg /etc/tickborg/config.json
```

### Manage Services

```bash
# View status
systemctl status tickborg-*

# View logs
journalctl -u tickborg-builder -f
journalctl -u tickborg-webhook-receiver -f

# Restart
sudo systemctl restart tickborg-builder
```

---

## Method 3: Docker Compose

For those who prefer Docker (`podman-compose` also works on RHEL).

```bash
cd ofborg/

# 1. Set environment variables
cp .env.example .env
nano .env

# 2. Create the config file
cp example.config.json config.json
nano config.json

# 3. Place the GitHub private key
cp key.pem ./github-private-key.pem

# 4. Start
docker compose up -d

# 5. Check status
docker compose ps
docker compose logs -f builder
```

Builder scaling: `docker compose up -d --scale builder=4`

| Port | Service |
|------|---------|
| 15672 | RabbitMQ Management |
| 9899 | Webhook Receiver |
| 8082 | Log API |
| 9898 | Prometheus Metrics |

---

## Method 4: Manual (For Development)

For local development and testing.

```bash
cd ofborg/

# 1. Build
cargo build --release

# 2. Start RabbitMQ (via Docker)
docker run -d --name rabbitmq \
  -p 5672:5672 -p 15672:15672 \
  rabbitmq:3-management

# 3. Start each service in a separate terminal
./target/release/github_webhook_receiver config.json &
./target/release/evaluation_filter config.json &
./target/release/github_comment_filter config.json &
./target/release/github_comment_poster config.json &
./target/release/builder config.json &
./target/release/log_message_collector config.json &
./target/release/stats config.json &
```

---

## Reverse Proxy (Nginx)

To expose the webhook receiver to the internet:

```nginx
server {
    listen 443 ssl;
    server_name tickborg.your-domain.com;

    ssl_certificate /etc/letsencrypt/live/tickborg.your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/tickborg.your-domain.com/privkey.pem;

    location /webhook {
        proxy_pass http://127.0.0.1:9899;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

---

## Prometheus Monitoring

The stats service exposes Prometheus metrics on port `:9898`.

`prometheus.yml`:
```yaml
scrape_configs:
  - job_name: 'tickborg'
    static_configs:
      - targets: ['localhost:9898']
    scrape_interval: 15s
```

Example metrics:
- `tickborg_queue_builder_waiting` — Number of jobs waiting in the builder queue
- `tickborg_queue_evaluator_waiting` — Number of jobs waiting in the evaluator queue

---

## Troubleshooting

### Cannot connect to RabbitMQ

```bash
# Is RabbitMQ running?
systemctl status rabbitmq-server
# or
sudo rabbitmqctl status

# Is the port open?
ss -tlnp | grep 5672
```

### Webhooks not arriving

1. Make sure the webhook URL in the GitHub App settings is correct
2. Verify that the webhook secret matches config.json
3. Confirm that port 9899 on the server is accessible from the internet
4. Check webhook status via GitHub App → Advanced → Recent Deliveries

### Builder failing

```bash
# Check builder logs
journalctl -u tickborg@builder -f
# or if using Docker
# docker compose logs builder

# Are the required build tools installed?
which cmake meson make gcc cargo javac
```

### To see verbose logs

```bash
RUST_LOG=tickborg=debug ./target/release/builder config.json
```
