#!/usr/bin/env bash
# PostgreSQL install / uninstall — native cluster under $SERVER_ROOT/postgres (standalone)
# or Docker (docker mode only)
set -euo pipefail

_pg_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_pg_lib_dir/common.sh"

PG_DATA_DIR="${PG_DATA_DIR:-$SERVER_ROOT/postgres/data}"
PG_LOG_FILE="${PG_LOG_FILE:-$LOG_DIR/postgres.log}"
PG_RUN_USER="${PG_RUN_USER:-postgres}"

_pg_find_bin() {
  local name="$1"
  local d
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  for d in /usr/lib/postgresql/*/bin; do
    if [[ -x "$d/$name" ]]; then
      echo "$d/$name"
      return 0
    fi
  done
  return 1
}

_pg_bin() {
  local name="$1" path
  path="$(_pg_find_bin "$name")" || {
    log_fail "$name not found — install: sudo apt install postgresql"
    return 1
  }
  echo "$path"
}

_pg_as_postgres() {
  if [[ "$(id -u)" -eq 0 ]]; then
    if id "$PG_RUN_USER" >/dev/null 2>&1; then
      sudo -u "$PG_RUN_USER" env "PATH=$PATH" "$@"
    else
      "$@"
    fi
  elif [[ "$(id -un)" == "$PG_RUN_USER" ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1 && id "$PG_RUN_USER" >/dev/null 2>&1; then
    sudo -u "$PG_RUN_USER" env "PATH=$PATH" "$@"
  else
    "$@"
  fi
}

_pg_run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    return 1
  fi
}

postgres_disable_system_service() {
  if command -v systemctl >/dev/null 2>&1; then
    log_info "Ensuring system PostgreSQL is not holding port ${POSTGRES_PORT:-5432}"
    _pg_run_as_root systemctl stop postgresql 2>/dev/null || true
    _pg_run_as_root systemctl disable postgresql 2>/dev/null || true
    # Versioned clusters (Debian/Ubuntu)
    local u unit
    for u in /lib/systemd/system/postgresql@*.service; do
      [[ -f "$u" ]] || continue
      unit="$(basename "$u" .service)"
      _pg_run_as_root systemctl stop "$unit" 2>/dev/null || true
      _pg_run_as_root systemctl disable "$unit" 2>/dev/null || true
    done
    # pg_ctlcluster if available
    if command -v pg_lsclusters >/dev/null 2>&1; then
      pg_lsclusters 2>/dev/null | awk 'NR>1 && $4=="online" {print $1,$2}' | while read -r ver name; do
        _pg_run_as_root pg_ctlcluster "$ver" "$name" stop 2>/dev/null || true
      done
    fi
  fi
  # Standalone must not share 5432 with a leftover Docker Postgres
  if command -v docker >/dev/null 2>&1; then
    local cid
    for cid in $(docker ps -q --filter name=postgres 2>/dev/null); do
      log_warn "Stopping Docker PostgreSQL container $cid (standalone uses native cluster)"
      docker stop "$cid" 2>/dev/null || true
    done
  fi
  sleep 1
  if command -v ss >/dev/null 2>&1; then
    if ss -tln 2>/dev/null | grep -q ":${POSTGRES_PORT:-5432} "; then
      if ! is_postgres_standalone_running; then
        log_warn "Port ${POSTGRES_PORT:-5432} still in use after stopping system PostgreSQL"
        ss -tlnp 2>/dev/null | grep ":${POSTGRES_PORT:-5432} " | sed 's/^/  /' || true
      fi
    fi
  fi
}

postgres_init_cluster() {
  local initdb_bin pg_ctl_bin pwfile
  initdb_bin="$(_pg_bin initdb)" || return 1
  pg_ctl_bin="$(_pg_bin pg_ctl)" || return 1

  mkdir -p "$(dirname "$PG_DATA_DIR")" "$LOG_DIR"
  if [[ -f "$PG_DATA_DIR/PG_VERSION" ]]; then
    log_info "PostgreSQL data directory already initialized: $PG_DATA_DIR"
    return 0
  fi

  # Guard: updates must never initdb. First-time install sets FLM_ALLOW_DB_INIT=1.
  if [[ "${FLM_ALLOW_DB_INIT:-}" != "1" ]]; then
    log_fail "No PostgreSQL cluster at $PG_DATA_DIR (missing PG_VERSION)"
    log_fail "Refusing initdb — set FLM_ALLOW_DB_INIT=1 for first-time install only"
    log_fail "Routine updates must start an existing cluster, never create a new one"
    return 1
  fi

  # Never wipe a non-empty data dir — missing PG_VERSION can happen after a bad stop;
  # destroying it would erase production history.
  if [[ -d "$PG_DATA_DIR" ]] && [[ -n "$(ls -A "$PG_DATA_DIR" 2>/dev/null || true)" ]]; then
    log_fail "Refusing to initdb: $PG_DATA_DIR is not empty but has no PG_VERSION"
    log_fail "Restore / backup, or move the dir aside. Set FLM_ALLOW_DB_WIPE=1 only if intentional."
    if [[ "${FLM_ALLOW_DB_WIPE:-}" != "1" ]]; then
      return 1
    fi
    log_warn "FLM_ALLOW_DB_WIPE=1 — removing $PG_DATA_DIR"
  fi

  log_info "Initializing PostgreSQL cluster at $PG_DATA_DIR"
  mkdir -p "$(dirname "$PG_DATA_DIR")"
  pwfile="$(dirname "$PG_DATA_DIR")/.pwfile"
  printf '%s\n' "${POSTGRES_PASSWORD:-flmPass}" > "$pwfile"
  chmod 600 "$pwfile"

  # initdb must run as postgres user; prepare empty dir owned by postgres
  if [[ "${FLM_ALLOW_DB_WIPE:-}" == "1" ]] || [[ ! -d "$PG_DATA_DIR" ]] || [[ -z "$(ls -A "$PG_DATA_DIR" 2>/dev/null || true)" ]]; then
    rm -rf "$PG_DATA_DIR"
  else
    log_fail "Cannot create empty data dir without wipe flag"
    return 1
  fi
  mkdir -p "$PG_DATA_DIR"
  if id "$PG_RUN_USER" >/dev/null 2>&1; then
    _pg_run_as_root chown -R "$PG_RUN_USER:$PG_RUN_USER" "$(dirname "$PG_DATA_DIR")"
    _pg_run_as_root chmod 700 "$PG_DATA_DIR"
    _pg_run_as_root chown "$PG_RUN_USER:$PG_RUN_USER" "$pwfile"
    _pg_run_as_root chmod 600 "$pwfile"
  fi

  if ! _pg_as_postgres "$initdb_bin" -D "$PG_DATA_DIR" \
      -U "${POSTGRES_USER:-flmAdmin}" \
      --auth=scram-sha-256 \
      --pwfile="$pwfile" \
      --encoding=UTF8 \
      --locale=C.UTF-8 2>&1 | tee -a "$FLM_LOG_FILE"; then
    # locale fallback
    _pg_as_postgres "$initdb_bin" -D "$PG_DATA_DIR" \
      -U "${POSTGRES_USER:-flmAdmin}" \
      --auth=scram-sha-256 \
      --pwfile="$pwfile" \
      --encoding=UTF8 2>&1 | tee -a "$FLM_LOG_FILE" || {
      rm -f "$pwfile"
      log_fail "initdb failed"
      return 1
    }
  fi
  rm -f "$pwfile"

  # Configure listen + port
  {
    echo "listen_addresses = 'localhost'"
    echo "port = ${POSTGRES_PORT:-5432}"
    echo "password_encryption = scram-sha-256"
    echo "logging_collector = off"
    echo "log_destination = 'stderr'"
  } >> "$PG_DATA_DIR/postgresql.conf"

  cat > "$PG_DATA_DIR/pg_hba.conf" <<EOF
# TYPE  DATABASE        USER            ADDRESS                 METHOD
local   all             all                                     scram-sha-256
host    all             all             127.0.0.1/32            scram-sha-256
host    all             all             ::1/128                 scram-sha-256
EOF

  if id "$PG_RUN_USER" >/dev/null 2>&1; then
    _pg_run_as_root chown -R "$PG_RUN_USER:$PG_RUN_USER" "$PG_DATA_DIR"
  fi
  log_ok "Cluster initialized (superuser: ${POSTGRES_USER:-flmAdmin})"
}

# Prefer common.sh implementation (pidfile + pg_isready fallback).
# Redefine here only if common was not sourced with SERVER_ROOT set — keep in sync.
is_postgres_standalone_running() {
  local pidfile="$PG_DATA_DIR/postmaster.pid"
  if [[ -f "$pidfile" ]]; then
    local pid
    pid="$(head -1 "$pidfile" 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      return 0
    fi
  fi
  if [[ -f "$PG_DATA_DIR/PG_VERSION" ]]; then
    local pg_isready_bin
    pg_isready_bin="$(_pg_find_bin pg_isready 2>/dev/null)" || true
    if [[ -n "${pg_isready_bin:-}" ]] && \
       "$pg_isready_bin" -h localhost -p "${POSTGRES_PORT:-5432}" -q 2>/dev/null; then
      return 0
    fi
  fi
  return 1
}

postgres_start() {
  ensure_env_file
  local pg_ctl_bin
  pg_ctl_bin="$(_pg_bin pg_ctl)" || return 1

  if is_postgres_standalone_running; then
    log_info "PostgreSQL already running"
    return 0
  fi

  # Only touch system Postgres / init when we still need to bring FLM cluster up
  if [[ ! -f "$PG_DATA_DIR/PG_VERSION" ]]; then
    postgres_disable_system_service
    postgres_init_cluster || return 1
  else
    # Existing cluster: free the port if something else holds it, but never initdb
    postgres_disable_system_service
  fi

  if is_postgres_standalone_running; then
    log_info "PostgreSQL already running"
    return 0
  fi

  mkdir -p "$LOG_DIR"
  touch "$PG_LOG_FILE"
  if id "$PG_RUN_USER" >/dev/null 2>&1; then
    _pg_run_as_root chown "$PG_RUN_USER:$PG_RUN_USER" "$PG_LOG_FILE" 2>/dev/null || true
  fi

  _pg_as_postgres "$pg_ctl_bin" -D "$PG_DATA_DIR" -l "$PG_LOG_FILE" -o "-p ${POSTGRES_PORT:-5432}" start \
    2>&1 | tee -a "$FLM_LOG_FILE" || {
    log_fail "pg_ctl start failed — see $PG_LOG_FILE"
    tail -10 "$PG_LOG_FILE" 2>/dev/null | sed 's/^/  /' || true
    return 1
  }

  local ok=false i
  for i in $(seq 1 30); do
    if is_postgres_standalone_running; then
      ok=true
      break
    fi
    sleep 1
  done
  if [[ "$ok" != true ]]; then
    log_fail "PostgreSQL did not become ready"
    return 1
  fi
  log_ok "PostgreSQL started on port ${POSTGRES_PORT:-5432}"
}

postgres_ensure_database() {
  local createdb_bin psql_bin
  createdb_bin="$(_pg_bin createdb)" || return 1
  psql_bin="$(_pg_bin psql)" || return 1

  export PGPASSWORD="${POSTGRES_PASSWORD:-flmPass}"
  local user="${POSTGRES_USER:-flmAdmin}"
  local db="${POSTGRES_DB:-flmDB}"
  local port="${POSTGRES_PORT:-5432}"

  # Check if DB exists
  if "$psql_bin" -h localhost -p "$port" -U "$user" -d postgres -tAc \
      "SELECT 1 FROM pg_database WHERE datname='${db}'" 2>/dev/null | grep -q 1; then
    log_info "Database ${db} already exists"
    unset PGPASSWORD
    return 0
  fi

  if "$createdb_bin" -h localhost -p "$port" -U "$user" -O "$user" "$db" 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "Created database ${db}"
  else
    # Fallback: SQL create
    "$psql_bin" -h localhost -p "$port" -U "$user" -d postgres -c \
      "CREATE DATABASE \"${db}\" OWNER \"${user}\";" 2>&1 | tee -a "$FLM_LOG_FILE" || {
      unset PGPASSWORD
      log_fail "Could not create database ${db}"
      return 1
    }
    log_ok "Created database ${db}"
  fi
  unset PGPASSWORD
}

postgres_stop() {
  if [[ ! -f "$PG_DATA_DIR/PG_VERSION" ]]; then
    log_info "No native PostgreSQL cluster"
    return 0
  fi
  local pg_ctl_bin
  pg_ctl_bin="$(_pg_find_bin pg_ctl)" || return 0
  if is_postgres_standalone_running; then
    _pg_as_postgres "$pg_ctl_bin" -D "$PG_DATA_DIR" stop -m fast 2>&1 | tee -a "$FLM_LOG_FILE" || true
    sleep 1
    if is_postgres_standalone_running; then
      log_warn "PostgreSQL still running — forcing immediate stop"
      _pg_as_postgres "$pg_ctl_bin" -D "$PG_DATA_DIR" stop -m immediate 2>&1 | tee -a "$FLM_LOG_FILE" || true
      sleep 1
    fi
    if is_postgres_standalone_running; then
      local pid
      pid="$(head -1 "$PG_DATA_DIR/postmaster.pid" 2>/dev/null || true)"
      [[ -n "$pid" ]] && kill -9 "$pid" 2>/dev/null || true
      rm -f "$PG_DATA_DIR/postmaster.pid"
    fi
    if is_postgres_standalone_running; then
      log_warn "PostgreSQL may still be running"
    else
      log_ok "PostgreSQL stopped"
    fi
  else
    rm -f "$PG_DATA_DIR/postmaster.pid" 2>/dev/null || true
    log_info "PostgreSQL not running"
  fi
}

postgres_restart() {
  postgres_stop
  postgres_start
}

postgres_install_standalone() {
  log_header "Installing Database (PostgreSQL — native)"
  # shellcheck source=deps.sh
  source "$_pg_lib_dir/deps.sh"
  ensure_postgres || return 1
  ensure_env_file

  # Explicit opt-in for initdb (first install / intentional re-init only)
  export FLM_ALLOW_DB_INIT="${FLM_ALLOW_DB_INIT:-1}"

  log_step 1 3 "Initializing / starting cluster"
  postgres_start || return 1

  log_step 2 3 "Ensuring application database"
  postgres_ensure_database || return 1

  log_step 3 3 "Health check"
  local pg_isready_bin
  pg_isready_bin="$(_pg_bin pg_isready)" || return 1
  if "$pg_isready_bin" -h localhost -p "${POSTGRES_PORT:-5432}" -q; then
    log_ok "PostgreSQL ready on port ${POSTGRES_PORT:-5432}"
  else
    log_fail "pg_isready failed"
    return 1
  fi

  log_summary_box "Database Installation Complete" \
    "Mode:     native cluster" \
    "Data:     $PG_DATA_DIR" \
    "Host:     localhost:${POSTGRES_PORT:-5432}" \
    "Database: ${POSTGRES_DB:-flmDB}" \
    "User:     ${POSTGRES_USER:-flmAdmin}"
}

postgres_install_docker() {
  log_header "Installing Database (PostgreSQL — Docker)"
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

postgres_install() {
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    postgres_install_standalone
  else
    postgres_install_docker
  fi
}

postgres_uninstall() {
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    log_header "Uninstalling Database (PostgreSQL — native)"
    postgres_stop
    local purge=false
    if flm_should_purge; then
      purge=true
    elif [[ -z "${FLM_UNINSTALL_PURGE:-}" ]]; then
      if flm_confirm_or_auto "Remove cluster data at $PG_DATA_DIR (ALL DATA LOST)?"; then
        purge=true
      fi
    fi
    if [[ "$purge" == true ]]; then
      rm -rf "$SERVER_ROOT/postgres"
      log_ok "Cluster data removed"
    else
      log_info "Data preserved"
    fi
    log_summary_box "Database Uninstall Complete"
    return 0
  fi

  log_header "Uninstalling Database (PostgreSQL — Docker)"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.postgres.yml down 2>&1 | tee -a "$FLM_LOG_FILE" || true
  log_ok "PostgreSQL stopped"

  if flm_should_purge || flm_confirm_or_auto "Remove database volumes (ALL DATA LOST)?"; then
    docker volume rm server_postgres_data 2>/dev/null || true
    log_ok "Database volumes removed"
  else
    log_info "Data volumes preserved"
  fi

  log_summary_box "Database Uninstall Complete"
}

postgres_status() {
  ensure_env_file
  echo ""
  echo -e "${C_BOLD}PostgreSQL${C_RESET}"
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    if is_postgres_standalone_running; then
      log_ok "Running (native) on port ${POSTGRES_PORT:-5432}"
      log_info "Data: $PG_DATA_DIR"
    else
      log_fail "Not running"
    fi
  else
    if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi postgres; then
      log_ok "Running (Docker) on port ${POSTGRES_PORT:-5432}"
    else
      log_fail "Not running"
    fi
  fi
}

postgres_configure() {
  log_header "Configure Database"
  ensure_env_file
  read -r -p "  DB name [${POSTGRES_DB}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_DB "$v"
  read -r -p "  DB user [${POSTGRES_USER}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_USER "$v"
  read -r -p "  DB password [${POSTGRES_PASSWORD}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_PASSWORD "$v"
  read -r -p "  Port [${POSTGRES_PORT}]: " v; [[ -n "$v" ]] && save_env_var POSTGRES_PORT "$v"
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    log_ok "Saved to .env — restart PostgreSQL to apply port changes (re-init needed for user/password)"
  else
    log_ok "Saved to .env — recreate container to apply"
  fi
}
