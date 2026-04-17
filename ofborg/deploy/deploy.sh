#!/usr/bin/env bash
# deploy.sh — Tickborg deploy script using Nix on RHEL/CentOS/Fedora
set -euo pipefail

TICKBORG_USER="tickborg"
TICKBORG_HOME="/var/lib/tickborg"
TICKBORG_CONFIG="/etc/tickborg"
TICKBORG_LOG="/var/log/tickborg"
SYSTEMD_DIR="/etc/systemd/system"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Path to compiled binaries (after nix build)
NIX_RESULT=""

# Colored output
info()  { echo -e "\033[1;34m[INFO]\033[0m  $*"; }
ok()    { echo -e "\033[1;32m[OK]\033[0m    $*"; }
err()   { echo -e "\033[1;31m[ERROR]\033[0m $*" >&2; }
warn()  { echo -e "\033[1;33m[WARN]\033[0m  $*"; }

usage() {
    cat <<EOF
Usage: $0 <command>

Commands:
  install     Initial setup (user, directories, systemd, RabbitMQ)
  build       Build tickborg with Nix
  deploy      Build + update and start systemd services
  update      Pull code, rebuild, and deploy
  start       Start all services
  status      Show status of all services
  logs <srv>  Show logs for a specific service
  stop        Stop all services
  uninstall   Remove services (data is not deleted)

Examples:
  $0 install          # Initial setup
  $0 deploy           # Build and deploy
  $0 logs builder     # Show builder logs
  $0 status           # Check status
EOF
    exit 1
}

# --- Servisler ---
SERVICES=(
    github-webhook-receiver
    evaluation-filter
    github-comment-filter
    github-comment-poster
    push-filter
    builder
    mass-rebuilder
    log-message-collector
    logapi
    stats
)

# Services to start by default (excluding mass_rebuilder)
DEFAULT_SERVICES=(
    github-webhook-receiver
    evaluation-filter
    github-comment-filter
    github-comment-poster
    push-filter
    builder
    log-message-collector
    logapi
    stats
)

# --- Set Nix PATH ---
ensure_nix_path() {
    # Nix may not be in PATH when running under sudo
    if command -v nix &>/dev/null; then
        return
    fi

    # Try known Nix paths
    local nix_paths=(
        "/nix/var/nix/profiles/default/bin"
        "$HOME/.nix-profile/bin"
        "/root/.nix-profile/bin"
    )

    # Also try the SUDO_USER's profile if set
    if [[ -n "${SUDO_USER:-}" ]]; then
        local sudo_home
        sudo_home=$(getent passwd "$SUDO_USER" | cut -d: -f6)
        if [[ -n "$sudo_home" ]]; then
            nix_paths+=("$sudo_home/.nix-profile/bin")
        fi
    fi

    for p in "${nix_paths[@]}"; do
        if [[ -x "$p/nix" ]]; then
            export PATH="$p:$PATH"
            info "Nix found: $p/nix"
            return
        fi
    done

    # Nix daemon profile
    if [[ -f /etc/profile.d/nix.sh ]]; then
        # shellcheck source=/dev/null
        . /etc/profile.d/nix.sh
        if command -v nix &>/dev/null; then
            return
        fi
    fi

    # Nix daemon (multi-user) profile file
    if [[ -f /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh ]]; then
        # shellcheck source=/dev/null
        . /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh
        if command -v nix &>/dev/null; then
            return
        fi
    fi
}

# --- Check Nix ---
check_nix() {
    ensure_nix_path

    if ! command -v nix &>/dev/null; then
        err "Nix not found. Install with:"
        echo "  curl -L https://nixos.org/nix/install | sh -s -- --daemon"
        echo "  # Reopen your shell after installation"
        exit 1
    fi

    ok "Nix found: $(nix --version)"
}

# --- Install RabbitMQ ---
install_rabbitmq() {
    info "Checking RabbitMQ..."

    if systemctl is-active --quiet rabbitmq-server 2>/dev/null; then
        ok "RabbitMQ is already running"
        return
    fi

    if ! rpm -q rabbitmq-server &>/dev/null; then
        info "Installing RabbitMQ..."

        # Erlang repo (required for RabbitMQ)
        if ! rpm -q erlang &>/dev/null; then
            sudo dnf install -y https://github.com/rabbitmq/erlang-rpm/releases/download/v26.2.5.6/erlang-26.2.5.6-1.el9.x86_64.rpm \
                || sudo dnf install -y erlang
        fi

        # Add/update the official RabbitMQ RPM repository
        info "Configuring RabbitMQ RPM repository..."
        sudo tee /etc/yum.repos.d/rabbitmq.repo > /dev/null <<'REPOEOF'
[rabbitmq-server]
name=RabbitMQ Server
baseurl=https://packagecloud.io/rabbitmq/rabbitmq-server/el/8/$basearch
gpgcheck=0
repo_gpgcheck=0
enabled=1
sslverify=1
sslcacert=/etc/pki/tls/certs/ca-bundle.crt
metadata_expire=300
REPOEOF

        # Clean old cache
        sudo dnf clean all --disablerepo='*' --enablerepo='rabbitmq-server' 2>/dev/null || true

        # RabbitMQ — the el8 package also works on RHEL 9 (noarch).
        # RHEL 9 disabled SHA1 signatures by default in its crypto policy;
        # gpgcheck is disabled. The package is downloaded over TLS.
        sudo dnf install -y rabbitmq-server || {
            err "RabbitMQ installation failed. Install manually:"
            echo "  See: https://www.rabbitmq.com/docs/install-rpm"
            exit 1
        }
    fi

    sudo systemctl enable --now rabbitmq-server
    sudo rabbitmq-plugins enable rabbitmq_management

    # Create tickborg user
    sudo rabbitmqctl add_user tickborg "$(head -c 32 /dev/urandom | base64 | tr -dc 'a-zA-Z0-9' | head -c 24)" 2>/dev/null || true
    sudo rabbitmqctl set_permissions -p / tickborg ".*" ".*" ".*"
    sudo rabbitmqctl set_user_tags tickborg administrator

    ok "RabbitMQ installed and running"
    warn "Write the RabbitMQ password to config.json!"
    warn "To view/change the password: sudo rabbitmqctl change_password tickborg NEW_PASSWORD"
}

# --- Build tools (from RHEL repo) ---
install_build_tools() {
    info "Checking build tools..."

    local needed=()
    command -v podman  &>/dev/null || needed+=(podman)
    command -v cmake   &>/dev/null || needed+=(cmake)
    command -v meson   &>/dev/null || needed+=(meson)
    command -v ninja   &>/dev/null || needed+=(ninja-build)
    command -v gcc     &>/dev/null || needed+=(gcc gcc-c++)
    command -v make    &>/dev/null || needed+=(make)
    command -v autoconf &>/dev/null || needed+=(autoconf automake libtool)
    command -v javac   &>/dev/null || needed+=(java-17-openjdk-devel)
    command -v cargo   &>/dev/null || needed+=(cargo rust)
    command -v pkg-config &>/dev/null || needed+=(pkg-config pkgconf)
    command -v git     &>/dev/null || needed+=(git)

    if [[ ${#needed[@]} -gt 0 ]]; then
        info "Installing missing packages: ${needed[*]}"
        sudo dnf install -y "${needed[@]}"
    fi

    ok "Build tools ready"

    # Build the isolated builder container image
    build_container_image
}

build_container_image() {
    info "Building builder container image..."

    local containerfile="$SCRIPT_DIR/Containerfile.builder"
    if [[ ! -f "$containerfile" ]]; then
        err "Containerfile not found: $containerfile"
        return 1
    fi

    sudo podman build \
        -t localhost/tickborg-builder:latest \
        -f "$containerfile" \
        "$SCRIPT_DIR"

    ok "Container image ready: localhost/tickborg-builder:latest"
}

# --- System User ---
create_user() {
    if id "$TICKBORG_USER" &>/dev/null; then
        ok "User '$TICKBORG_USER' already exists"
        return
    fi

    info "Creating system user: $TICKBORG_USER"
    sudo useradd --system --create-home \
        --home-dir "$TICKBORG_HOME" \
        --shell /sbin/nologin \
        --comment "Tickborg CI Bot" \
        "$TICKBORG_USER"

    ok "User created: $TICKBORG_USER"
}

# --- Directories ---
create_dirs() {
    info "Creating directories..."

    sudo mkdir -p "$TICKBORG_HOME"/{bin,checkout}
    sudo mkdir -p "$TICKBORG_CONFIG"
    sudo mkdir -p "$TICKBORG_LOG"

    sudo chown -R "$TICKBORG_USER:$TICKBORG_USER" "$TICKBORG_HOME"
    sudo chown -R "$TICKBORG_USER:$TICKBORG_USER" "$TICKBORG_LOG"
    sudo chown "root:$TICKBORG_USER" "$TICKBORG_CONFIG"
    sudo chmod 750 "$TICKBORG_HOME" "$TICKBORG_LOG"
    sudo chmod 750 "$TICKBORG_CONFIG"

    ok "Directories ready"
}

# --- Nix Build ---
do_build() {
    info "Building Tickborg with Nix..."
    check_nix

    cd "$REPO_ROOT"

    NIX_RESULT=$(
        nix build .#tickborg --no-link --print-out-paths 2>/dev/null \
        || nix build .#tickborg --no-link --print-out-paths \
            --extra-experimental-features "nix-command flakes" 2>/dev/null
    ) || true

    if [[ -z "$NIX_RESULT" ]]; then
        err "Build failed!"
        exit 1
    fi

    ok "Build complete: $NIX_RESULT"
}

# --- Copy Binaries ---
install_binaries() {
    if [[ -z "$NIX_RESULT" ]]; then
        err "Must build first"
        exit 1
    fi

    info "Copying binaries..."

    # Copy from Nix store to /var/lib/tickborg/bin/
    sudo rm -rf "$TICKBORG_HOME/bin/"*

    for f in "$NIX_RESULT"/bin/*; do
        local bn
        bn=$(basename "$f")
        # Follow symlinks to copy the real file
        sudo cp -L "$f" "$TICKBORG_HOME/bin/$bn"
    done

    sudo chown -R "$TICKBORG_USER:$TICKBORG_USER" "$TICKBORG_HOME/bin"
    sudo chmod 755 "$TICKBORG_HOME/bin/"*

    ok "Binaries installed: $TICKBORG_HOME/bin/"
    ls -la "$TICKBORG_HOME/bin/"
}

# --- Viewer Build ---
build_viewer() {
    local viewer_dir="$REPO_ROOT/ofborg-viewer"
    if [[ ! -d "$viewer_dir" ]]; then
        warn "ofborg-viewer directory not found, skipping"
        return
    fi

    info "Building viewer..."
    cd "$viewer_dir"

    if ! command -v node &>/dev/null; then
        warn "Node.js not found, skipping viewer"
        return
    fi

    if [[ ! -d "node_modules" ]]; then
        npm install --production=false 2>&1 | tail -3
    fi

    npm run build 2>&1

    sudo mkdir -p "$TICKBORG_HOME/viewer"
    sudo cp -r dist/* "$TICKBORG_HOME/viewer/"
    sudo chown -R "$TICKBORG_USER:$TICKBORG_USER" "$TICKBORG_HOME/viewer"
    # Allow the nginx user to access viewer files
    sudo chmod o+x "$TICKBORG_HOME"
    sudo chmod o+x "$TICKBORG_HOME/viewer"
    sudo chmod -R o+r "$TICKBORG_HOME/viewer"

    ok "Viewer installed: $TICKBORG_HOME/viewer/"
    cd "$REPO_ROOT"
}

# --- Systemd Services ---
install_systemd() {
    info "Installing systemd services..."

    # Template unit
    sudo cp "$SCRIPT_DIR/tickborg@.service" "$SYSTEMD_DIR/"
    sudo cp "$SCRIPT_DIR/tickborg.target" "$SYSTEMD_DIR/"

    sudo systemctl daemon-reload

    # Enable default services
    for svc in "${DEFAULT_SERVICES[@]}"; do
        sudo systemctl enable "tickborg@${svc}.service"
    done

    sudo systemctl enable tickborg.target

    ok "Systemd services installed"
}

# --- Config ---
install_config() {
    if [[ -f "$TICKBORG_CONFIG/config.json" ]]; then
        ok "Config already exists: $TICKBORG_CONFIG/config.json"
        return
    fi

    info "Copying example config..."
    sudo cp "$REPO_ROOT/example.config.json" "$TICKBORG_CONFIG/config.json"
    sudo chown "$TICKBORG_USER:$TICKBORG_USER" "$TICKBORG_CONFIG/config.json"
    sudo chmod 600 "$TICKBORG_CONFIG/config.json"

    warn "Edit the config file: sudo nano $TICKBORG_CONFIG/config.json"
    warn "Place the GitHub App private key under /etc/tickborg/"
}

# --- Git Config (for builder) ---
setup_git() {
    sudo -u "$TICKBORG_USER" git config --global user.email "tickborg@project-tick.dev"
    sudo -u "$TICKBORG_USER" git config --global user.name "TickBorg"
    ok "Git config set"
}

# --- Commands ---
cmd_install() {
    info "=== Tickborg Initial Setup (RHEL + Nix) ==="
    check_nix
    create_user
    create_dirs
    install_build_tools
    install_rabbitmq
    do_build
    install_binaries
    install_config
    setup_git
    build_viewer
    install_systemd

    echo ""
    ok "=== Setup complete! ==="
    echo ""
    echo "Next steps:"
    echo "  1. Edit config:          sudo nano $TICKBORG_CONFIG/config.json"
    echo "  2. Place private key:    sudo cp key.pem $TICKBORG_CONFIG/github-private-key.pem"
    echo "  3. Start services:       sudo systemctl start tickborg.target"
    echo "  4. Check status:         $0 status"
}

cmd_build() {
    do_build
}

cmd_deploy() {
    do_build
    install_binaries
    build_viewer

    info "Restarting services..."
    sudo systemctl daemon-reload
    for svc in "${DEFAULT_SERVICES[@]}"; do
        sudo systemctl enable "tickborg@${svc}.service" 2>/dev/null
        sudo systemctl restart "tickborg@${svc}.service"
    done

    sleep 2
    cmd_status
}

cmd_start() {
    info "Starting all services..."
    sudo systemctl daemon-reload
    for svc in "${DEFAULT_SERVICES[@]}"; do
        sudo systemctl enable "tickborg@${svc}.service" 2>/dev/null
        sudo systemctl start "tickborg@${svc}.service"
    done

    sleep 2
    cmd_status
}

cmd_update() {
    info "Updating code..."
    cd "$REPO_ROOT"
    git pull --ff-only

    cmd_deploy
}

cmd_status() {
    echo ""
    info "=== Tickborg Service Status ==="
    echo ""

    for svc in "${SERVICES[@]}"; do
        local state
        state=$(systemctl is-active "tickborg@${svc}.service" 2>/dev/null || echo "inactive")
        local icon="❌"
        [[ "$state" == "active" ]] && icon="✅"
        printf "  %s  %-35s %s\n" "$icon" "tickborg@${svc}" "$state"
    done

    echo ""

    # RabbitMQ status
    local rmq_state
    rmq_state=$(systemctl is-active rabbitmq-server 2>/dev/null || echo "inactive")
    local rmq_icon="❌"
    [[ "$rmq_state" == "active" ]] && rmq_icon="✅"
    printf "  %s  %-35s %s\n" "$rmq_icon" "rabbitmq-server" "$rmq_state"

    echo ""
}

cmd_logs() {
    local svc="${1:-}"
    if [[ -z "$svc" ]]; then
        err "Specify a service name: $0 logs builder"
        echo "Available services: ${SERVICES[*]}"
        exit 1
    fi
    journalctl -u "tickborg@${svc}.service" -f --no-pager
}

cmd_stop() {
    info "Stopping all services..."
    sudo systemctl stop tickborg.target
    ok "Services stopped"
}

cmd_uninstall() {
    warn "Removing services (data will not be deleted)..."

    for svc in "${SERVICES[@]}"; do
        sudo systemctl disable "tickborg@${svc}.service" 2>/dev/null || true
    done
    sudo systemctl disable tickborg.target 2>/dev/null || true
    sudo systemctl stop tickborg.target 2>/dev/null || true

    sudo rm -f "$SYSTEMD_DIR/tickborg@.service"
    sudo rm -f "$SYSTEMD_DIR/tickborg.target"
    sudo systemctl daemon-reload

    ok "Services removed. Data still exists at: $TICKBORG_HOME"
}

# --- Main ---
case "${1:-}" in
    install)   cmd_install ;;
    build)     cmd_build ;;
    deploy)    cmd_deploy ;;
    update)    cmd_update ;;
    start)     cmd_start ;;
    status)    cmd_status ;;
    logs)      cmd_logs "${2:-}" ;;
    stop)      cmd_stop ;;
    uninstall) cmd_uninstall ;;
    *)         usage ;;
esac
