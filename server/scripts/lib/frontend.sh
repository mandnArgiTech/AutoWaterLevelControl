#!/usr/bin/env bash
# React frontend install / uninstall
set -euo pipefail

_fe_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_fe_lib_dir/common.sh"

NGINX_CONF="$SERVER_ROOT/nginx/flm-standalone.conf"
NGINX_TEMPLATE="$SERVER_ROOT/nginx/flm-standalone.conf.template"

frontend_write_nginx_conf() {
  ensure_env_file
  if [[ ! -f "$NGINX_TEMPLATE" ]]; then
    log_fail "Missing nginx template: $NGINX_TEMPLATE"
    return 1
  fi
  mkdir -p "$SERVER_ROOT/nginx"
  sed \
    -e "s|__FRONTEND_PORT__|${FRONTEND_PORT:-3000}|g" \
    -e "s|__API_PORT__|${API_PORT:-8080}|g" \
    -e "s|__FRONTEND_ROOT__|${SERVER_ROOT}/frontend/dist|g" \
    "$NGINX_TEMPLATE" > "$NGINX_CONF"
  log_ok "Wrote $NGINX_CONF"
}

frontend_stop_standalone_nginx() {
  if [[ -f "$RUN_DIR/nginx.pid" ]]; then
    local pid
    pid="$(cat "$RUN_DIR/nginx.pid")"
    if kill -0 "$pid" 2>/dev/null; then
      nginx -c "$NGINX_CONF" -s stop 2>/dev/null || kill "$pid" 2>/dev/null || true
      sleep 1
      kill -9 "$pid" 2>/dev/null || true
      log_ok "Stopped standalone nginx (PID $pid)"
    fi
    rm -f "$RUN_DIR/nginx.pid"
  fi
}

frontend_install_docker() {
  log_header "Installing Frontend (React UI) — Docker"
  # shellcheck source=deps.sh
  source "$SCRIPTS_DIR/lib/deps.sh"
  deps_install_docker || return 1
  ensure_env_file

  log_step 1 2 "Building and starting frontend container"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.frontend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"

  log_step 2 2 "Waiting for web UI"
  if wait_for_port localhost "${FRONTEND_PORT:-3000}" "Web UI" 60; then
    log_ok "Frontend ready"
  else
    log_warn "UI not responding yet — check: docker logs flm-frontend"
  fi

  log_summary_box "Frontend Installation Complete" \
    "Web UI:   http://localhost:${FRONTEND_PORT:-3000}" \
    "Vendor:   vendor / 123456  (code: demo)" \
    "Admin:    admin / 123456"
}

frontend_install_standalone() {
  log_header "Installing Frontend (React UI) — Standalone (production)"
  # shellcheck source=deps.sh
  source "$SCRIPTS_DIR/lib/deps.sh"
  ensure_node || return 1
  ensure_nginx || return 1
  ensure_env_file

  log_step 1 4 "Installing npm packages"
  cd "$SERVER_ROOT/frontend"
  npm install 2>&1 | tee -a "$FLM_LOG_FILE"
  log_ok "Dependencies installed"

  log_step 2 4 "Building production bundle (npm run build)"
  VITE_API_BASE="${VITE_API_BASE:-/api}" npm run build 2>&1 | tee -a "$FLM_LOG_FILE"
  [[ -f "$SERVER_ROOT/frontend/dist/index.html" ]] || { log_fail "Build failed — dist/index.html missing"; return 1; }
  log_ok "Production build in frontend/dist/"

  log_step 3 4 "Configuring nginx"
  frontend_write_nginx_conf || return 1
  frontend_stop_standalone_nginx

  log_step 4 4 "Starting nginx"
  if ! nginx -c "$NGINX_CONF" -t 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_fail "nginx config test failed"
    return 1
  fi
  nginx -c "$NGINX_CONF"
  sleep 1
  if [[ -f "$RUN_DIR/nginx.pid" ]] && kill -0 "$(cat "$RUN_DIR/nginx.pid")" 2>/dev/null; then
    log_ok "nginx running (PID $(cat "$RUN_DIR/nginx.pid"))"
  else
    log_warn "nginx started but PID file not found — check logs/nginx-error.log"
  fi
  wait_for_port localhost "${FRONTEND_PORT:-3000}" "Web UI" 30 || log_warn "UI not reachable yet"

  log_summary_box "Frontend (Standalone) Ready" \
    "Web UI: http://localhost:${FRONTEND_PORT:-3000}" \
    "Static: frontend/dist/ (served by nginx)" \
    "Log:    logs/nginx-error.log"
}

frontend_uninstall() {
  log_header "Uninstalling Frontend (React UI)"

  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.frontend.yml down 2>/dev/null || true
  $COMPOSE -f docker-compose.yml stop frontend 2>/dev/null || true
  log_ok "Docker frontend removed"

  frontend_stop_standalone_nginx

  local pidfile="$RUN_DIR/frontend.pid"
  if [[ -f "$pidfile" ]]; then
    local pid
    pid="$(cat "$pidfile")"
    kill "$pid" 2>/dev/null || true
    rm -f "$pidfile"
    log_ok "Legacy dev server process stopped (if any)"
  fi

  log_summary_box "Frontend Uninstall Complete"
}

frontend_status() {
  echo ""
  echo -e "${C_BOLD}Frontend (React UI)${C_RESET}"
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi flm-frontend; then
    log_ok "Docker running on port ${FRONTEND_PORT:-3000}"
  elif [[ -f "$RUN_DIR/nginx.pid" ]] && kill -0 "$(cat "$RUN_DIR/nginx.pid")" 2>/dev/null; then
    log_ok "Standalone nginx on port ${FRONTEND_PORT:-3000} (PID $(cat "$RUN_DIR/nginx.pid"))"
  elif [[ -f "$RUN_DIR/frontend.pid" ]] && kill -0 "$(cat "$RUN_DIR/frontend.pid")" 2>/dev/null; then
    log_warn "Legacy Vite dev server (PID $(cat "$RUN_DIR/frontend.pid")) — reinstall frontend for production nginx"
  else
    log_fail "Not running"
  fi
}

frontend_configure() {
  log_header "Configure Frontend"
  ensure_env_file
  read -r -p "  Standalone / Docker UI port [${FRONTEND_PORT}]: " v; [[ -n "$v" ]] && save_env_var FRONTEND_PORT "$v"
  read -r -p "  Vite dev port (manual npm run dev only) [${FRONTEND_DEV_PORT}]: " v; [[ -n "$v" ]] && save_env_var FRONTEND_DEV_PORT "$v"
  log_ok "Saved to .env — reinstall frontend if nginx port changed"
}
