#!/usr/bin/env bash
# Shared helpers for FLM server manager scripts
set -euo pipefail

SERVER_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPTS_DIR="$SERVER_ROOT/scripts"
CONFIG_DIR="$SERVER_ROOT/config"
LOG_DIR="$SERVER_ROOT/logs"
RUN_DIR="$SERVER_ROOT/.run"
ENV_FILE="$SERVER_ROOT/.env"
DEFAULTS_FILE="$CONFIG_DIR/defaults.env"

mkdir -p "$LOG_DIR" "$RUN_DIR" "$SERVER_ROOT/mosquitto/data" "$SERVER_ROOT/mosquitto/log"
COMPOSE="docker-compose"

if [[ -z "${FLM_LOG_FILE:-}" ]]; then
  FLM_LOG_FILE="$LOG_DIR/flm-server-$(date +%Y%m%d).log"
fi

# ── Colours (disabled when not a terminal) ─────────────────────────────────────
if [[ -t 1 ]]; then
  C_RESET='\033[0m' C_BOLD='\033[1m' C_DIM='\033[2m'
  C_GREEN='\033[0;32m' C_RED='\033[0;31m' C_YELLOW='\033[1;33m'
  C_CYAN='\033[0;36m' C_BLUE='\033[0;34m'
else
  C_RESET= C_BOLD= C_DIM= C_GREEN= C_RED= C_YELLOW= C_CYAN= C_BLUE=
fi

_ts() { date '+%Y-%m-%d %H:%M:%S'; }

_log_plain() {
  local level="$1"; shift
  local msg="[$(_ts)] [$level] $*"
  echo "$msg" >> "$FLM_LOG_FILE"
  echo -e "$msg"
}

log_header() {
  local title="$1"
  echo ""
  echo -e "${C_BOLD}${C_CYAN}╔══════════════════════════════════════════════════════════════╗${C_RESET}"
  printf "${C_BOLD}${C_CYAN}║${C_RESET} %-60s ${C_BOLD}${C_CYAN}║${C_RESET}\n" "$title"
  echo -e "${C_BOLD}${C_CYAN}╚══════════════════════════════════════════════════════════════╝${C_RESET}"
  echo ""
  _log_plain "INFO" "=== $title ==="
}

log_step() {
  local n="$1" total="$2" msg="$3"
  echo -e "${C_BLUE}[$n/$total]${C_RESET} $msg"
  _log_plain "STEP" "[$n/$total] $msg"
}

log_ok()   { echo -e "  ${C_GREEN}✓${C_RESET} $*"; _log_plain "OK" "$*"; }
log_fail() { echo -e "  ${C_RED}✗${C_RESET} $*"; _log_plain "FAIL" "$*"; }
log_warn() { echo -e "  ${C_YELLOW}!${C_RESET} $*"; _log_plain "WARN" "$*"; }
log_info() { echo -e "  ${C_DIM}$*${C_RESET}"; _log_plain "INFO" "$*"; }

log_summary_box() {
  local title="$1"
  shift
  echo ""
  echo -e "${C_BOLD}${C_GREEN}┌─ $title ─────────────────────────────────────────${C_RESET}"
  while [[ $# -gt 0 ]]; do
    echo -e "${C_GREEN}│${C_RESET} $1"
    shift
  done
  echo -e "${C_BOLD}${C_GREEN}└────────────────────────────────────────────────────${C_RESET}"
  echo ""
}

ensure_env_file() {
  if [[ ! -f "$ENV_FILE" ]]; then
    cp "$DEFAULTS_FILE" "$ENV_FILE"
    log_ok "Created $ENV_FILE from defaults"
  fi
  # shellcheck disable=SC1090
  set -a; source "$ENV_FILE"; set +a
}

save_env_var() {
  local key="$1" val="$2"
  ensure_env_file
  if grep -q "^${key}=" "$ENV_FILE" 2>/dev/null; then
    sed -i "s|^${key}=.*|${key}=${val}|" "$ENV_FILE"
  else
    echo "${key}=${val}" >> "$ENV_FILE"
  fi
  export "$key=$val"
}

require_cmd() {
  local cmd="$1" hint="${2:-}"
  if command -v "$cmd" >/dev/null 2>&1; then
    log_ok "$cmd found"
    return 0
  fi
  log_fail "$cmd not found${hint:+ — $hint}"
  return 1
}

require_docker() {
  require_cmd docker "Install Docker: https://docs.docker.com/engine/install/" || return 1
  if ! docker info >/dev/null 2>&1; then
    log_fail "Docker daemon not running — start Docker and retry"
    return 1
  fi
  log_ok "Docker daemon running"
  COMPOSE="$(bash "$SCRIPTS_DIR/lib/compose.sh")"
  log_ok "Using: $COMPOSE"
}

wait_for_port() {
  local host="$1" port="$2" label="$3" max="${4:-30}"
  local i=1
  while [[ $i -le $max ]]; do
    if (echo >/dev/tcp/"$host"/"$port") 2>/dev/null; then
      log_ok "$label is reachable on $host:$port"
      return 0
    fi
    sleep 1
    ((i++)) || true
  done
  log_fail "$label not reachable on $host:$port after ${max}s"
  return 1
}

is_service_running() {
  local service="$1"
  local cid
  cid="$($COMPOSE -f "$SERVER_ROOT/docker-compose.yml" ps -q "$service" 2>/dev/null || true)"
  [[ -n "$cid" ]] && docker inspect -f '{{.State.Running}}' "$cid" 2>/dev/null | grep -q true
}

is_mqtt_standalone_running() {
  local pidfile="$RUN_DIR/mosquitto.pid"
  if [[ -f "$pidfile" ]] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
    return 0
  fi
  if command -v ss >/dev/null 2>&1; then
    ss -tln 2>/dev/null | grep -q ':1883 '
    return $?
  fi
  return 1
}

is_mqtt_running() {
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    is_mqtt_standalone_running
    return $?
  fi
  is_service_running mosquitto || \
    $COMPOSE -f "$SERVER_ROOT/docker-compose.infra.yml" ps -q mosquitto 2>/dev/null | grep -q .
}

is_postgres_standalone_running() {
  local pgdata="${PG_DATA_DIR:-$SERVER_ROOT/postgres/data}"
  local pidfile="$pgdata/postmaster.pid"
  if [[ -f "$pidfile" ]]; then
    local pid
    pid="$(head -1 "$pidfile" 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      return 0
    fi
  fi
  return 1
}

is_postgres_running() {
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    is_postgres_standalone_running
    return $?
  fi
  is_service_running postgres || \
    $COMPOSE -f "$SERVER_ROOT/docker-compose.infra.yml" ps -q postgres 2>/dev/null | grep -q .
}

# Resolve Mosquitto Dynamic Security plugin path on host
find_dynsec_plugin() {
  local p
  for p in \
    /usr/lib/x86_64-linux-gnu/mosquitto_dynamic_security.so \
    /usr/lib/aarch64-linux-gnu/mosquitto_dynamic_security.so \
    /usr/lib/mosquitto_dynamic_security.so; do
    if [[ -f "$p" ]]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

export_env_for_certs() {
  ensure_env_file
  export MQTT_SERVER_CN MQTT_SAN_DNS MQTT_SAN_IPS
  export MQTT_USER_BRIDGE MQTT_PASS_BRIDGE MQTT_USER_DEVICE MQTT_PASS_DEVICE
  export MQTT_USER_VENDOR MQTT_PASS_VENDOR
  export MQTT_DYNSEC_ADMIN MQTT_DYNSEC_PASS
}

# Returns 0 (yes) if FLM_AUTO_YES=1, FLM_UNINSTALL_PURGE=1, or user answers y
flm_confirm_or_auto() {
  local prompt="${1:-Continue?}"
  if [[ "${FLM_AUTO_YES:-}" == "1" || "${FLM_UNINSTALL_PURGE:-}" == "1" ]]; then
    return 0
  fi
  if [[ ! -t 0 ]]; then
    return 1
  fi
  local ans
  read -r -p "  ${prompt} [y/N] " ans
  [[ "${ans,,}" == "y" ]]
}

# True when full uninstall already decided to purge (skip nested prompts)
flm_should_purge() {
  [[ "${FLM_UNINSTALL_PURGE:-}" == "1" || "${FLM_AUTO_YES:-}" == "1" || "${FLM_UNINSTALL_FULL:-}" == "1" ]]
}

# apt-purge Mosquitto + PostgreSQL and remove /etc/mosquitto etc.
flm_should_remove_packages() {
  [[ "${FLM_UNINSTALL_FULL:-}" == "1" || "${FLM_UNINSTALL_REMOVE_PACKAGES:-}" == "1" ]]
}

# Delete the install directory (e.g. /opt/flm) after services stop
flm_should_remove_tree() {
  [[ "${FLM_UNINSTALL_FULL:-}" == "1" || "${FLM_UNINSTALL_REMOVE_TREE:-}" == "1" ]]
}

uninstall_verify_ports() {
  log_step 1 1 "Checking FLM ports are free"
  local port still=0
  if ! command -v ss >/dev/null 2>&1; then
    log_warn "ss not available — skip port check"
    return 0
  fi
  for port in 80 8080 1883 8883 5432; do
    if ss -tln 2>/dev/null | grep -q ":${port} "; then
      log_warn "Port ${port} still has a listener"
      ss -tlnp 2>/dev/null | grep ":${port} " | sed 's/^/  /' || true
      still=1
    else
      log_ok "Port ${port} free"
    fi
  done
  if [[ $still -eq 0 ]]; then
    log_ok "All checked ports are free"
  else
    log_warn "Some ports still in use — may be system services or leftover processes"
  fi
}

pause_enter() {
  echo ""
  read -r -p "Press Enter to continue..." _
}
