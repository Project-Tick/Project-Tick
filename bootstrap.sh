#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Project Tick
#
# MeshMC Bootstrap Script
# Detects distro, installs dependencies, and sets up lefthook.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { printf "${CYAN}[INFO]${NC}  %s\n" "$*"; }
ok()    { printf "${GREEN}[ OK ]${NC}  %s\n" "$*"; }
warn()  { printf "${YELLOW}[WARN]${NC}  %s\n" "$*"; }
err()   { printf "${RED}[ERR]${NC}  %s\n" "$*" >&2; }

detect_distro() {
    case "$(uname -s)" in
        Darwin)
            DISTRO="macos"
            DISTRO_ID="macos"
            ok "Detected platform: macOS"
            return
            ;;
        Linux) ;;
        *)
            err "Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_ID="${ID:-unknown}"
        DISTRO_ID_LIKE="${ID_LIKE:-}"
    else
        err "Cannot detect distribution (/etc/os-release not found)."
        exit 1
    fi

    case "$DISTRO_ID" in
        debian)              DISTRO="debian"  ;;
        ubuntu|linuxmint|pop) DISTRO="ubuntu"  ;;
        fedora)              DISTRO="fedora"  ;;
        rhel|centos|rocky|alma) DISTRO="rhel" ;;
        opensuse*|sles)      DISTRO="suse"    ;;
        arch|manjaro|endeavouros) DISTRO="arch" ;;
        *)
            # Fallback: check ID_LIKE
            case "$DISTRO_ID_LIKE" in
                *debian*|*ubuntu*) DISTRO="ubuntu" ;;
                *fedora*|*rhel*)   DISTRO="fedora" ;;
                *suse*)            DISTRO="suse"   ;;
                *arch*)            DISTRO="arch"   ;;
                *)
                    err "Unsupported distribution: $DISTRO_ID"
                    err "Supported: Debian, Ubuntu, Fedora, RHEL, SUSE, Arch, macOS"
                    exit 1
                    ;;
            esac
            ;;
    esac

    ok "Detected distribution: $DISTRO (${DISTRO_ID})"
}

MISSING_DEPS=()

check_cmd() {
    local name="$1"
    local cmd="$2"
    if command -v "$cmd" &>/dev/null; then
        ok "$name is installed ($(command -v "$cmd"))"
    else
        warn "$name is NOT installed"
        MISSING_DEPS+=("$name")
    fi
}

check_lib() {
    local name="$1"
    local pkg_name="$2"
    if pkg-config --exists "$pkg_name" 2>/dev/null; then
        ok "$name found via pkg-config"
    else
        warn "$name is NOT found"
        MISSING_DEPS+=("$name")
    fi
}

check_llvm() {
    local found=0
    for bin in clang-22 clang++-22 llvm-config-22; do
        if command -v "$bin" &>/dev/null; then
            found=1
            break
        fi
    done
    if [ $found -eq 1 ]; then
        ok "LLVM 22 is installed"
    else
        warn "LLVM 22 is NOT installed"
        MISSING_DEPS+=("LLVM 22")
    fi
}

check_dependencies() {
    info "Checking dependencies..."
    echo

    check_cmd "npm"       "npm"
    check_cmd "Go"        "go"
    check_cmd "lefthook"  "lefthook"
    check_cmd "reuse"     "reuse"
    check_lib "Qt6"       "Qt6Core"
    check_lib "QuaZip"    "quazip1-qt6"
    check_lib "zlib"      "zlib"
    check_lib "ECM"       "ECM"
    check_llvm
    echo
}

install_llvm_debian_ubuntu() {
    info "Installing LLVM 22 via llvm.sh installer..."
    if ! command -v curl &>/dev/null && ! command -v wget &>/dev/null; then
        sudo apt-get install -y curl
    fi
    local tmp
    tmp=$(mktemp)
    curl -fsSL https://apt.llvm.org/llvm.sh -o "$tmp"
    chmod +x "$tmp"
    sudo bash "$tmp" 22
    rm -f "$tmp"
}

install_debian_ubuntu() {
    info "Installing packages via apt..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        npm \
        golang \
        qt6-base-dev \
        libquazip1-qt6-dev \
        zlib1g-dev \
        extra-cmake-modules \
        pkg-config \
        reuse
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        install_llvm_debian_ubuntu
    fi
}

install_fedora() {
    info "Installing packages via dnf..."
    sudo dnf install -y \
        npm \
        golang \
        qt6-qtbase-devel \
        quazip-qt6-devel \
        zlib-devel \
        extra-cmake-modules \
        pkgconf \
        reuse
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        info "Installing LLVM 22 via dnf..."
        sudo dnf install -y clang22 llvm22
    fi
}

install_rhel() {
    info "Installing packages via dnf..."
    sudo dnf install -y epel-release || true
    sudo dnf install -y \
        npm \
        golang \
        qt6-qtbase-devel \
        quazip-qt6-devel \
        zlib-devel \
        extra-cmake-modules \
        pkgconf \
        reuse
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        info "Installing LLVM 22 via dnf..."
        sudo dnf install -y clang22 llvm22 || install_llvm_debian_ubuntu
    fi
}

install_suse() {
    info "Installing packages via zypper..."
    sudo zypper install -y \
        npm \
        go \
        qt6-base-devel \
        quazip-qt6-devel \
        zlib-devel \
        extra-cmake-modules \
        pkg-config \
        python3-reuse
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        info "Installing LLVM 22 via zypper..."
        sudo zypper install -y clang22 llvm22
    fi
}

install_arch() {
    info "Installing packages via pacman..."
    sudo pacman -Sy --needed --noconfirm \
        npm \
        go \
        qt6-base \
        quazip-qt6 \
        zlib \
        extra-cmake-modules \
        pkgconf \
        reuse
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        info "Installing LLVM 22 via pacman..."
        sudo pacman -Sy --needed --noconfirm llvm clang
    fi
}

install_macos() {
    if ! command -v brew &>/dev/null; then
        err "Homebrew is not installed. Install it from https://brew.sh"
        exit 1
    fi

    info "Installing packages via Homebrew..."
    brew install \
        node \
        go \
        qt@6 \
        quazip \
        zlib \
        extra-cmake-modules \
        reuse \
        lefthook
    if [[ " ${MISSING_DEPS[*]} " == *" LLVM 22 "* ]]; then
        info "Installing LLVM 22 via Homebrew..."
        brew install llvm@22
    fi
}

install_lefthook() {
    if command -v lefthook &>/dev/null; then
        return
    fi

    info "Installing lefthook via Go..."
    go install github.com/evilmartians/lefthook@latest

    export PATH="${GOPATH:-$HOME/go}/bin:$PATH"

    if ! command -v lefthook &>/dev/null; then
        err "lefthook installation failed. Please install it manually."
        exit 1
    fi
    ok "lefthook installed successfully"
}

install_missing() {
    if [ ${#MISSING_DEPS[@]} -eq 0 ]; then
        ok "All dependencies are already installed!"
        return
    fi

    info "Missing dependencies: ${MISSING_DEPS[*]}"
    echo

    case "$DISTRO" in
        debian|ubuntu) install_debian_ubuntu ;;
        fedora)        install_fedora        ;;
        rhel)          install_rhel           ;;
        suse)          install_suse           ;;
        arch)          install_arch           ;;
        macos)         install_macos          ;;
    esac

    install_lefthook

    echo
    ok "Package installation complete"
}

setup_lefthook() {
    if [ ! -d ".git" ]; then
        err "Not a git repository. Please run this script from the project root."
        exit 1
    fi

    info "Setting up lefthook..."
    lefthook install
    ok "Lefthook hooks installed into .git/hooks"
}

init_submodules() {
    if [ ! -d ".git" ]; then
        err "Not a git repository. Please run this script from the project root."
        exit 1
    fi

    info "Setting up submodules..."
    git submodule update --init --recursive
    ok "Submodules initialized"
}

main() {
    echo
    info "MeshMC Bootstrap"
    echo "─────────────────────────────────────────────"
    echo

    init_submodules
    echo

    detect_distro
    echo

    check_dependencies

    install_missing
    echo

    setup_lefthook
    echo

    echo "─────────────────────────────────────────────"
    ok "Bootstrap successful! You're all set."
    echo
}

cd "$(dirname "$(readlink -f "$0")")"

main "$@"
