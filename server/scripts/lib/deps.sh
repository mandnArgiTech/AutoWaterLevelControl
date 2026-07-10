#!/usr/bin/env bash
# System dependency installation for FLM standalone (production on host)
set -euo pipefail

_deps_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_deps_lib_dir/common.sh"

# Set FLM_SKIP_DEPS=1 to skip automatic package installation
# Set FLM_AUTO_INSTALL_DEPS=1 to install without prompting (CI / non-interactive)

_os_has_apt() {
  command -v apt-get >/dev/null 2>&1
}

_run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    log_fail "Need root or sudo to install system packages"
    return 1
  fi
}

_apt_install() {
  local pkgs=("$@")
  log_info "Installing: ${pkgs[*]}"
  _run_as_root apt-get update -qq
  _run_as_root apt-get install -y -qq "${pkgs[@]}"
}

_confirm_deps_install() {
  if [[ "${FLM_SKIP_DEPS:-}" == "1" ]]; then
    log_info "FLM_SKIP_DEPS=1 — skipping automatic package installation"
    return 1
  fi
  if [[ "${FLM_AUTO_INSTALL_DEPS:-}" == "1" ]]; then
    return 0
  fi
  if [[ ! -t 0 ]]; then
    log_warn "Non-interactive shell — set FLM_AUTO_INSTALL_DEPS=1 to install packages automatically"
    return 1
  fi
  echo ""
  echo -e "${C_YELLOW}Some required packages are missing.${C_RESET}"
  echo "The installer can install them with apt (Debian/Ubuntu)."
  read -r -p "Install missing dependencies now? [Y/n] " ans
  [[ -z "$ans" || "${ans,,}" == "y" ]]
}

_ensure_pkg() {
  local pkg="$1"
  if dpkg -s "$pkg" >/dev/null 2>&1; then
    return 0
  fi
  _confirm_deps_install || return 1
  _apt_install "$pkg"
}

ensure_openssl() {
  if command -v openssl >/dev/null 2>&1; then
    log_ok "openssl found"
    return 0
  fi
  log_warn "openssl not found"
  if _os_has_apt; then
    _confirm_deps_install || return 1
    _apt_install openssl
    log_ok "openssl installed"
    return 0
  fi
  log_fail "Install openssl manually"
  return 1
}

ensure_docker() {
  if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    log_ok "Docker found and running"
    COMPOSE="$(bash "$SCRIPTS_DIR/lib/compose.sh")"
    log_ok "Using: $COMPOSE"
    return 0
  fi

  log_warn "Docker not available (required for MQTT + PostgreSQL)"
  if ! _os_has_apt; then
    log_fail "Install Docker manually: https://docs.docker.com/engine/install/"
    return 1
  fi

  _confirm_deps_install || return 1

  if ! command -v docker >/dev/null 2>&1; then
    _apt_install ca-certificates curl gnupg
    if ! dpkg -s docker.io >/dev/null 2>&1; then
      _apt_install docker.io
    fi
    if ! dpkg -s docker-compose-plugin >/dev/null 2>&1 \
        && ! command -v docker-compose >/dev/null 2>&1; then
      _apt_install docker-compose-plugin 2>/dev/null \
        || _apt_install docker-compose 2>/dev/null \
        || true
    fi
    log_info "Adding current user to docker group (log out/in if docker permission denied)"
    _run_as_root usermod -aG docker "${SUDO_USER:-$USER}" 2>/dev/null || true
    _run_as_root systemctl enable --now docker 2>/dev/null || true
  fi

  if ! docker info >/dev/null 2>&1; then
    log_fail "Docker installed but daemon not running — run: sudo systemctl start docker"
    return 1
  fi

  COMPOSE="$(bash "$SCRIPTS_DIR/lib/compose.sh")"
  log_ok "Docker ready ($COMPOSE)"
}

_java_major_version() {
  java -version 2>&1 | awk -F '[ ".]' '/version/ { print $4; exit }'
}

ensure_java21() {
  if command -v java >/dev/null 2>&1; then
    local major
    major="$(_java_major_version)"
    if [[ -n "$major" && "$major" -ge 21 ]]; then
      log_ok "Java $major found"
      return 0
    fi
    log_warn "Java $major found — Java 21+ required"
  else
    log_warn "Java not found"
  fi

  if _os_has_apt; then
    _confirm_deps_install || return 1
    _apt_install openjdk-21-jdk
    log_ok "OpenJDK 21 installed"
    return 0
  fi

  log_fail "Install Java 21 manually"
  return 1
}

ensure_maven() {
  if command -v mvn >/dev/null 2>&1; then
    log_ok "Maven found"
    return 0
  fi

  if _os_has_apt; then
    _confirm_deps_install || return 1
    _apt_install maven
    log_ok "Maven installed"
    return 0
  fi

  log_fail "Install Maven manually"
  return 1
}

_node_major_version() {
  node -v 2>/dev/null | sed 's/^v//' | cut -d. -f1
}

ensure_node() {
  if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
    local major
    major="$(_node_major_version)"
    if [[ -n "$major" && "$major" -ge 18 ]]; then
      log_ok "Node.js v$(node -v | sed 's/^v//') found"
      return 0
    fi
    log_warn "Node.js v$(node -v) found — Node 18+ recommended"
  else
    log_warn "Node.js / npm not found"
  fi

  if _os_has_apt; then
    _confirm_deps_install || return 1
    if ! command -v node >/dev/null 2>&1 || [[ "$(_node_major_version)" -lt 18 ]]; then
      if command -v curl >/dev/null 2>&1; then
        log_info "Installing Node.js 20 from NodeSource"
        curl -fsSL https://deb.nodesource.com/setup_20.x | _run_as_root bash -
      fi
    fi
    _apt_install nodejs
    log_ok "Node.js installed ($(node -v))"
    return 0
  fi

  log_fail "Install Node.js 18+ manually: https://nodejs.org/"
  return 1
}

ensure_nginx() {
  if command -v nginx >/dev/null 2>&1; then
    log_ok "nginx found"
    return 0
  fi

  if _os_has_apt; then
    _confirm_deps_install || return 1
    _apt_install nginx
    log_ok "nginx installed"
    return 0
  fi

  log_fail "Install nginx manually"
  return 1
}

# Install everything needed for standalone production (host API + UI, Docker infra)
deps_install_standalone() {
  log_header "Checking / Installing System Dependencies (Standalone)"
  local failed=0

  log_step 1 6 "OpenSSL (MQTT certificates)"
  ensure_openssl || ((failed++)) || true

  log_step 2 6 "Docker (MQTT + PostgreSQL containers)"
  ensure_docker || ((failed++)) || true

  log_step 3 6 "Java 21 (backend API)"
  ensure_java21 || ((failed++)) || true

  log_step 4 6 "Maven (build backend)"
  ensure_maven || ((failed++)) || true

  log_step 5 6 "Node.js + npm (build frontend)"
  ensure_node || ((failed++)) || true

  log_step 6 6 "nginx (serve production web UI)"
  ensure_nginx || ((failed++)) || true

  if [[ $failed -gt 0 ]]; then
    log_fail "$failed dependency group(s) missing — fix above and retry"
    log_info "Tip: on Debian/Ubuntu run with FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install"
    return 1
  fi

  log_ok "All standalone dependencies satisfied"
  return 0
}

# Docker-only full stack
deps_install_docker() {
  log_header "Checking / Installing System Dependencies (Docker)"
  local failed=0

  log_step 1 2 "OpenSSL (MQTT certificates)"
  ensure_openssl || ((failed++)) || true

  log_step 2 2 "Docker (all services)"
  ensure_docker || ((failed++)) || true

  if [[ $failed -gt 0 ]]; then
    log_fail "Prerequisites missing"
    return 1
  fi
  log_ok "Docker install dependencies satisfied"
  return 0
}
