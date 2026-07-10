#!/usr/bin/env bash
# PostgreSQL install / uninstall
set -euo pipefail

_pg_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_pg_lib_dir/common.sh"

postgres_install() {
  log_header "Installing Database (PostgreSQL)"
  require_docker || return 1

  log_step 1 2 "Starting PostgreSQL container"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.postgres.yml up -d 2>&1 | tee -a "$FLM_LOG_FILE"

  log_step 2 2 "Waiting for database"
  local ok=false
  for i in $(seq 1 30); do
    if $COMPOSE -f docker-compose.postgres.yml exec -T postgres \
      pg_isready -U "${POSTGRES_USER:-flmAdmin}" -d "${POSTGRES_DB:-flmDB}" >/dev/null 2>&1; then
      ok=true
      break
    fi
    sleep 1
  done
  if $ok; then
    log_ok "PostgreSQL ready on port ${POSTGRES_PORT:-5432}"
  else
    log_fail "PostgreSQL failed to start"
    return 1
  fi

  log_summary_box "Database Installation Complete" \
    "Host:     localhost:${POSTGRES_PORT:-5432}" \
    "Database: ${POSTGRES_DB:-flmDB}" \
    "User:     ${POSTGRES_USER:-flmAdmin}"
}

postgres_uninstall() {
  log_header "Uninstalling Database (PostgreSQL)"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.postgres.yml down 2>&1 | tee -a "$FLM_LOG_FILE"
  log_ok "PostgreSQL stopped"

  read -r -p "  Remove database volumes (ALL DATA LOST)? [y/N] " ans
  if [[ "${ans,,}" == "y" ]]; then
    docker volume rm server_postgres_data 2>/dev/null || true
    log_ok "Database volumes removed"
  else
    log_info "Data volumes preserved"
  fi

  log_summary_box "Database Uninstall Complete"
}

postgres_status() {
  echo ""
  echo -e "${C_BOLD}PostgreSQL${C_RESET}"
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi postgres; then
    log_ok "Running on port ${POSTGRES_PORT:-5432}"
  else
    log_fail "Not running"
  fi
}

postgres_configure() {
  log_header "Configure Database"
  ensure_env_file
  read -r -p "  DB name [${POSTGRES_DB}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_DB "$v"
  read -r -p "  DB user [${POSTGRES_USER}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_USER "$v"
  read -r -p "  DB password [${POSTGRES_PASSWORD}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_PASSWORD "$v"
  read -r -p "  Port [${POSTGRES_PORT}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_PORT "$v"
  log_ok "Saved to .env — recreate container to apply"
}
