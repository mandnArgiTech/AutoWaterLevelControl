#!/usr/bin/env bash
# In-place software update (rebuild + restart) — no full reinstall
set -euo pipefail

_upd_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_upd_lib_dir/common.sh"
# shellcheck source=backend.sh
source "$_upd_lib_dir/backend.sh"
# shellcheck source=frontend.sh
source "$_upd_lib_dir/frontend.sh"

update_standalone() {
  local component="${1:-all}"
  ensure_env_file
  FLM_SKIP_DEPS=1
  export FLM_SKIP_DEPS
  # Updates must never initdb / wipe Postgres. Existing cluster only.
  unset FLM_ALLOW_DB_INIT
  unset FLM_ALLOW_DB_WIPE

  log_header "Updating FLM Platform (Standalone) — ${component}"

  case "$component" in
    backend)
      backend_install_standalone || return 1
      ;;
    frontend)
      frontend_install_standalone || return 1
      ;;
    all|*)
      backend_install_standalone || return 1
      frontend_install_standalone || return 1
      ;;
  esac

  log_summary_box "Update Complete (Standalone)" \
    "Web UI: http://localhost:${FRONTEND_PORT:-80}/" \
    "API:    http://localhost:${API_PORT:-8080}/api" \
    "Logs:   logs/backend.log, logs/nginx-error.log"
}

update_docker() {
  local component="${1:-all}"
  ensure_env_file
  require_docker || return 1
  cd "$SERVER_ROOT"

  log_header "Updating FLM Platform (Docker) — ${component}"

  case "$component" in
    backend)
      $COMPOSE -f docker-compose.backend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
      ;;
    frontend)
      $COMPOSE -f docker-compose.frontend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
      ;;
    all|*)
      $COMPOSE -f docker-compose.backend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
      $COMPOSE -f docker-compose.frontend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
      ;;
  esac

  log_summary_box "Update Complete (Docker)" \
    "Web UI: http://localhost:${FRONTEND_PORT:-3000}" \
    "API:    http://localhost:${API_PORT:-8080}/api"
}

update_platform() {
  local component="${1:-all}"
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == docker ]]; then
    update_docker "$component"
  else
    update_standalone "$component"
  fi
}
