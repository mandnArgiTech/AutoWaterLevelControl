#!/usr/bin/env bash
# Java backend install / uninstall
set -euo pipefail

_be_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_be_lib_dir/common.sh"

backend_jar_path() {
  local jar
  jar="$(find "$SERVER_ROOT/backend/flm-api/target" -maxdepth 1 -name 'flm-api-*.jar' ! -name '*-original.jar' 2>/dev/null | head -1)"
  [[ -n "$jar" && -f "$jar" ]] || return 1
  echo "$jar"
}

backend_install_docker() {
  log_header "Installing Backend (Java API) — Docker"
  # shellcheck source=deps.sh
  source "$SCRIPTS_DIR/lib/deps.sh"
  deps_install_docker || return 1
  ensure_env_file

  log_step 1 3 "Checking dependencies (PostgreSQL + MQTT)"
  if ! is_postgres_running; then
    log_warn "PostgreSQL not running — installing database first"
    # shellcheck source=postgres.sh
    source "$SCRIPTS_DIR/lib/postgres.sh"
    postgres_install || return 1
  fi
  if ! docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
    log_warn "MQTT not running — installing MQTT first"
    # shellcheck source=mqtt.sh
    source "$SCRIPTS_DIR/lib/mqtt.sh"
    mqtt_install || return 1
  fi

  log_step 2 3 "Building and starting backend container"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.backend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"

  log_step 3 3 "Waiting for API"
  if wait_for_port localhost "${API_PORT:-8080}" "REST API" 120; then
    log_ok "Backend API ready"
  else
    log_warn "API not responding yet — check: docker logs flm-backend"
  fi

  log_summary_box "Backend Installation Complete" \
    "API URL:  http://localhost:${API_PORT:-8080}/api" \
    "Health:   http://localhost:${API_PORT:-8080}/actuator/health" \
    "Login:    admin / 123456 (change on first login)"
}

backend_install_standalone() {
  log_header "Installing Backend (Java API) — Standalone (production)"
  # shellcheck source=deps.sh
  source "$SCRIPTS_DIR/lib/deps.sh"
  ensure_java21 || return 1
  ensure_maven || return 1
  ensure_env_file

  if ! is_postgres_running; then
    source "$SCRIPTS_DIR/lib/postgres.sh"
    postgres_install || return 1
  fi
  if ! docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
    source "$SCRIPTS_DIR/lib/mqtt.sh"
    mqtt_install || return 1
  fi

  log_step 1 3 "Building backend JAR (Maven)"
  cd "$SERVER_ROOT/backend"
  mvn -q -pl flm-api -am package -DskipTests 2>&1 | tee -a "$FLM_LOG_FILE"
  local jar
  jar="$(backend_jar_path)" || { log_fail "JAR not found after build"; return 1; }
  log_ok "Built $(basename "$jar")"

  log_step 2 3 "Starting API (java -jar)"
  local pidfile="$RUN_DIR/backend.pid"
  if [[ -f "$pidfile" ]] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
    log_warn "Backend already running (PID $(cat "$pidfile")) — stopping first"
    backend_stop_standalone_process
  fi

  nohup java -jar "$jar" \
    --spring.profiles.active=standalone \
    --server.port="${API_PORT:-8080}" \
    >> "$LOG_DIR/backend.log" 2>&1 &
  echo $! > "$pidfile"
  log_ok "Started (PID $(cat "$pidfile"), log: logs/backend.log)"

  log_step 3 3 "Waiting for API"
  wait_for_port localhost "${API_PORT:-8080}" "REST API" 120 || log_warn "Still starting — check logs/backend.log"

  log_summary_box "Backend (Standalone) Ready" \
    "API: http://localhost:${API_PORT:-8080}/api" \
    "Log: logs/backend.log" \
    "Stop: ./flm-server.sh → Uninstall → Backend"
}

backend_stop_standalone_process() {
  local pidfile="$RUN_DIR/backend.pid"
  if [[ ! -f "$pidfile" ]]; then
    return 0
  fi
  local pid
  pid="$(cat "$pidfile")"
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    sleep 2
    kill -9 "$pid" 2>/dev/null || true
  fi
  rm -f "$pidfile"
}

backend_uninstall() {
  log_header "Uninstalling Backend (Java API)"

  log_step 1 2 "Stopping Docker backend"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.backend.yml down 2>/dev/null || true
  $COMPOSE -f docker-compose.yml stop backend 2>/dev/null || true
  $COMPOSE -f docker-compose.yml rm -f backend 2>/dev/null || true
  log_ok "Docker backend removed"

  log_step 2 2 "Stopping standalone backend"
  if backend_stop_standalone_process; then
    log_ok "Standalone backend stopped"
  else
    log_info "No standalone backend PID file"
  fi

  log_summary_box "Backend Uninstall Complete"
}

backend_status() {
  echo ""
  echo -e "${C_BOLD}Backend (Java API)${C_RESET}"
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi flm-backend; then
    log_ok "Docker container running"
    docker ps --filter name=flm-backend --format '  {{.Names}}  {{.Status}}  {{.Ports}}'
  elif [[ -f "$RUN_DIR/backend.pid" ]] && kill -0 "$(cat "$RUN_DIR/backend.pid")" 2>/dev/null; then
    log_ok "Standalone running (PID $(cat "$RUN_DIR/backend.pid"))"
  else
    log_fail "Not running"
  fi
}

backend_configure() {
  log_header "Configure Backend"
  ensure_env_file
  read -r -p "  API port [${API_PORT}]: " v; [[ -n "$v" ]] && save_env_var API_PORT "$v"
  read -r -p "  JWT secret [hidden]: " v; [[ -n "$v" ]] && save_env_var FLM_JWT_SECRET "$v"
  log_ok "Saved to .env"
}
