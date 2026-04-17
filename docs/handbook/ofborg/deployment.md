# Tickborg — Deployment

## Overview

Tickborg can be deployed via **NixOS modules**, **Docker Compose**, or manual
systemd units. The preferred method is the NixOS module defined in
`service.nix`, which orchestrates all eight binaries as individual systemd
services.

---

## Key Files

| File | Purpose |
|------|---------|
| `service.nix` | NixOS module — systemd services |
| `docker-compose.yml` | Full-stack Docker Compose |
| `flake.nix` | Nix flake — package + dev shell |
| `example.config.json` | Reference configuration file |

---

## NixOS Deployment

### Module Structure (`service.nix`)

```nix
{ config, pkgs, lib, ... }:
let
  cfg = config.services.tickborg;
  tickborg = cfg.package;
in
{
  options.services.tickborg = {
    enable = lib.mkEnableOption "Enable tickborg CI services";

    package = lib.mkOption {
      type = lib.types.package;
      description = "The tickborg package to use";
    };

    configFile = lib.mkOption {
      type = lib.types.path;
      description = "Path to the tickborg config.json";
    };

    logConfig = lib.mkOption {
      type = lib.types.str;
      default = "info";
      description = "RUST_LOG filter string";
    };

    services = {
      github-webhook-receiver = lib.mkEnableOption "webhook receiver";
      evaluation-filter = lib.mkEnableOption "evaluation filter";
      mass-rebuilder = lib.mkEnableOption "mass rebuilder (evaluation)";
      builder = lib.mkEnableOption "build executor";
      github-comment-filter = lib.mkEnableOption "comment filter";
      github-comment-poster = lib.mkEnableOption "comment poster";
      log-message-collector = lib.mkEnableOption "log collector";
      stats = lib.mkEnableOption "stats collector";
    };
  };
}
```

### Per-Service Configuration

Each service is toggled independently. A common template generates systemd
units:

```nix
commonServiceConfig = binary: {
  description = "tickborg ${binary}";
  wantedBy = [ "multi-user.target" ];
  after = [ "network-online.target" "rabbitmq.service" ];
  wants = [ "network-online.target" ];

  environment = {
    RUST_LOG = cfg.logConfig;
    RUST_LOG_JSON = "1";
    CONFIG_PATH = toString cfg.configFile;
  };

  serviceConfig = {
    ExecStart = "${tickborg}/bin/${binary}";
    Restart = "always";
    RestartSec = "10s";
    DynamicUser = true;

    # Hardening
    NoNewPrivileges = true;
    ProtectSystem = "strict";
    ProtectHome = true;
    PrivateTmp = true;
    PrivateDevices = true;
    ProtectKernelTunables = true;
    ProtectKernelModules = true;
    ProtectKernelLogs = true;
    ProtectControlGroups = true;
    RestrictNamespaces = true;
    LockPersonality = true;
    MemoryDenyWriteExecute = true;
    RestrictRealtime = true;
    SystemCallFilter = [ "@system-service" "~@mount" ];
  };
};
```

### Applying the Module

```nix
# In your NixOS configuration.nix or flake:
{
  imports = [ ./service.nix ];

  services.tickborg = {
    enable = true;
    package = tickborg-pkg;
    configFile = /etc/tickborg/config.json;
    logConfig = "info,tickborg=debug";

    services = {
      github-webhook-receiver = true;
      evaluation-filter = true;
      mass-rebuilder = true;
      builder = true;
      github-comment-filter = true;
      github-comment-poster = true;
      log-message-collector = true;
      stats = true;
    };
  };
}
```

### Service Management

```bash
# View all tickborg services
systemctl list-units 'tickborg-*'

# Restart a single service
systemctl restart tickborg-builder

# View logs
journalctl -u tickborg-builder -f

# Structured JSON logs (when RUST_LOG_JSON=1)
journalctl -u tickborg-builder -o cat | jq .
```

---

## Docker Compose Deployment

### `docker-compose.yml`

```yaml
services:
  rabbitmq:
    image: rabbitmq:3-management
    ports:
      - "5672:5672"
      - "15672:15672"
    environment:
      RABBITMQ_DEFAULT_USER: tickborg
      RABBITMQ_DEFAULT_PASS: tickborg
    volumes:
      - rabbitmq-data:/var/lib/rabbitmq

  webhook-receiver:
    build: .
    command: github-webhook-receiver
    ports:
      - "8080:8080"
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
    depends_on:
      - rabbitmq

  evaluation-filter:
    build: .
    command: evaluation-filter
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
    depends_on:
      - rabbitmq

  mass-rebuilder:
    build: .
    command: mass-rebuilder
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
      - checkout-cache:/var/cache/tickborg
    depends_on:
      - rabbitmq

  builder:
    build: .
    command: builder
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
      - checkout-cache:/var/cache/tickborg
    depends_on:
      - rabbitmq

  comment-filter:
    build: .
    command: github-comment-filter
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
    depends_on:
      - rabbitmq

  comment-poster:
    build: .
    command: github-comment-poster
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
    depends_on:
      - rabbitmq

  log-collector:
    build: .
    command: log-message-collector
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
      - log-data:/var/log/tickborg
    depends_on:
      - rabbitmq

  stats:
    build: .
    command: stats
    ports:
      - "9090:9090"
    environment:
      CONFIG_PATH: /config/config.json
      RUST_LOG: info
    volumes:
      - ./config:/config:ro
    depends_on:
      - rabbitmq

volumes:
  rabbitmq-data:
  checkout-cache:
  log-data:
```

### Running

```bash
# Start all services
docker compose up -d

# View webhook receiver logs
docker compose logs -f webhook-receiver

# Scale builders
docker compose up -d --scale builder=3

# Stop everything
docker compose down
```

---

## Nix Flake

### `flake.nix` Outputs

```nix
{
  outputs = { self, nixpkgs, ... }: {
    packages.x86_64-linux.default = /* tickborg cargo build */ ;
    packages.x86_64-linux.tickborg = self.packages.x86_64-linux.default;

    devShells.x86_64-linux.default = pkgs.mkShell {
      nativeBuildInputs = with pkgs; [
        cargo
        rustc
        clippy
        rustfmt
        pkg-config
        openssl
      ];
      RUST_SRC_PATH = "${pkgs.rust.packages.stable.rustPlatform.rustLibSrc}";
    };

    nixosModules.default = import ./service.nix;
  };
}
```

### Building with Nix

```bash
# Build the package
nix build

# Enter dev shell
nix develop

# Run directly
nix run .#tickborg -- github-webhook-receiver
```

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CONFIG_PATH` | `./config.json` | Path to configuration file |
| `RUST_LOG` | `info` | tracing filter directive |
| `RUST_LOG_JSON` | (unset) | Set to `1` for JSON-formatted logs |

---

## Reverse Proxy

The webhook receiver requires an HTTPS endpoint exposed to GitHub. Typical
setup with nginx:

```nginx
server {
    listen 443 ssl;
    server_name ci.example.com;

    ssl_certificate /etc/letsencrypt/live/ci.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/ci.example.com/privkey.pem;

    location /github-webhook {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # GitHub sends large payloads
        client_max_body_size 25m;
    }

    location /logs/ {
        proxy_pass http://127.0.0.1:8081/;
    }
}
```

---

## RabbitMQ Setup

### Required Configuration

```bash
# Create vhost
rabbitmqctl add_vhost tickborg

# Create user
rabbitmqctl add_user tickborg <password>

# Grant permissions
rabbitmqctl set_permissions -p tickborg tickborg ".*" ".*" ".*"
```

### Management UI

Available at `http://localhost:15672` when using Docker Compose. Useful for
monitoring queue depths and consumer counts.

---

## Health Checks

Monitor these indicators:

| Check | Healthy | Problem |
|-------|---------|---------|
| Queue depth `mass-rebuild-check-inputs` | < 50 | Evaluation filter slow/down |
| Queue depth `build-inputs-*` | < 20 | Builder slow/down |
| Consumer count per queue | ≥ 1 | No consumers (service down) |
| `stats` HTTP endpoint | 200 OK | Stats collector down |
| Webhook receiver `/health` | 200 OK | Webhook receiver down |

### Systemd Watchdog

Services configured with `Restart = "always"` will be automatically restarted
on crash. The 10-second `RestartSec` prevents restart loops on persistent
failures.
