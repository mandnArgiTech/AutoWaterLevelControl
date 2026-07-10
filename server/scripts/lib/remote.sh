#!/usr/bin/env bash
# Remote VPS deploy — push server/ to /opt/flm and run install over SSH
set -euo pipefail

_remote_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_remote_lib_dir/common.sh"

REMOTE_ENV_FILE="${REMOTE_ENV_FILE:-$CONFIG_DIR/remote.env}"
REMOTE_ENV_EXAMPLE="$CONFIG_DIR/remote.env.example"

load_remote_config() {
  if [[ ! -f "$REMOTE_ENV_FILE" ]]; then
    if [[ -f "$REMOTE_ENV_EXAMPLE" ]]; then
      log_warn "Missing $REMOTE_ENV_FILE"
      log_info "Copy and edit: cp config/remote.env.example config/remote.env"
      return 1
    fi
    log_fail "Remote config not found: $REMOTE_ENV_FILE"
    return 1
  fi
  # shellcheck disable=SC1090
  set -a; source "$REMOTE_ENV_FILE"; set +a
  FLM_REMOTE_HOST="${FLM_REMOTE_HOST:-vivasvan-tech.in}"
  FLM_REMOTE_USER="${FLM_REMOTE_USER:-root}"
  FLM_REMOTE_PATH="${FLM_REMOTE_PATH:-/opt/flm}"
  FLM_REMOTE_PORT="${FLM_REMOTE_PORT:-22}"
  FLM_REMOTE_INSTALL_MODE="${FLM_REMOTE_INSTALL_MODE:-standalone}"
  export FLM_REMOTE_HOST FLM_REMOTE_USER FLM_REMOTE_PATH FLM_REMOTE_PORT FLM_REMOTE_INSTALL_MODE
  if [[ -z "${FLM_REMOTE_PASSWORD:-}" ]] && [[ ! -f "${HOME}/.ssh/id_rsa" ]] && [[ ! -f "${HOME}/.ssh/id_ed25519" ]]; then
    log_warn "No FLM_REMOTE_PASSWORD and no default SSH key — connection may fail"
  fi
  return 0
}

require_remote_tools() {
  require_cmd ssh "Install openssh-client: sudo apt install openssh-client" || return 1
  require_cmd rsync "Install rsync: sudo apt install rsync" || return 1
  if [[ -n "${FLM_REMOTE_PASSWORD:-}" ]]; then
    require_cmd sshpass "Install sshpass for password SSH: sudo apt install sshpass" || return 1
  fi
  return 0
}

_remote_ssh_opts() {
  echo -o StrictHostKeyChecking=accept-new -o ConnectTimeout=30 -p "${FLM_REMOTE_PORT:-22}"
}

remote_ssh() {
  local opts
  opts="$(_remote_ssh_opts)"
  if [[ -n "${FLM_REMOTE_PASSWORD:-}" ]]; then
    export SSHPASS="${FLM_REMOTE_PASSWORD}"
    # shellcheck disable=SC2086
    sshpass -e ssh $opts "${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}" "$@"
  else
    # shellcheck disable=SC2086
    ssh $opts "${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}" "$@"
  fi
}

remote_rsync() {
  local ssh_wrapper
  if [[ -n "${FLM_REMOTE_PASSWORD:-}" ]]; then
    ssh_wrapper="sshpass -e ssh $(_remote_ssh_opts)"
  else
    ssh_wrapper="ssh $(_remote_ssh_opts)"
  fi
  export SSHPASS="${FLM_REMOTE_PASSWORD:-}"
  # shellcheck disable=SC2086
  rsync -az --delete --human-readable \
    -e "$ssh_wrapper" \
    --exclude '.env' \
    --exclude 'config/remote.env' \
    --exclude 'logs/' \
    --exclude '.run/' \
    --exclude 'frontend/node_modules/' \
    --exclude 'frontend/dist/' \
    --exclude 'frontend/tsconfig.tsbuildinfo' \
    --exclude 'backend/flm-api/target/' \
    --exclude 'backend/flm-common/target/' \
    --exclude 'backend/flm-domain/target/' \
    --exclude 'backend/flm-security/target/' \
    --exclude 'backend/flm-mqtt-bridge/target/' \
    --exclude 'mosquitto/certs/*' \
    --exclude 'mosquitto/passwd/passwd' \
    --exclude 'mosquitto/data/' \
    --exclude 'mosquitto/log/' \
    --exclude 'mosquitto/openssl/server.cnf' \
    --exclude 'nginx/flm-standalone.conf' \
    "$SERVER_ROOT/" "${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}:${FLM_REMOTE_PATH}/"
}

remote_test_connection() {
  log_step 1 1 "Testing SSH to ${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}:${FLM_REMOTE_PORT}"
  if remote_ssh "echo ok" >/dev/null 2>&1; then
    log_ok "SSH connection successful"
    return 0
  fi
  log_fail "Cannot connect via SSH — check host, user, password/key, and firewall (port ${FLM_REMOTE_PORT})"
  return 1
}

remote_prepare_directory() {
  log_info "Ensuring ${FLM_REMOTE_PATH} exists on remote host"
  remote_ssh "mkdir -p '${FLM_REMOTE_PATH}' && chmod 755 '${FLM_REMOTE_PATH}'"
  log_ok "Remote directory ready: ${FLM_REMOTE_PATH}"
}

remote_sync_files() {
  log_header "Syncing FLM platform to remote VPS"
  load_remote_config || return 1
  require_remote_tools || return 1
  remote_test_connection || return 1
  remote_prepare_directory
  log_step 1 1 "Rsync server files → ${FLM_REMOTE_HOST}:${FLM_REMOTE_PATH}"
  remote_rsync 2>&1 | tee -a "$FLM_LOG_FILE"
  log_ok "Files synced"
  remote_ssh "chmod +x '${FLM_REMOTE_PATH}/flm-server.sh' '${FLM_REMOTE_PATH}/scripts/'*.sh '${FLM_REMOTE_PATH}/scripts/lib/'*.sh 2>/dev/null || true"
}

remote_run_install() {
  local mode="${1:-${FLM_REMOTE_INSTALL_MODE:-standalone}}"
  local remote_cmd
  if [[ "$mode" == docker ]]; then
    remote_cmd="cd '${FLM_REMOTE_PATH}' && FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install-docker"
  else
    remote_cmd="cd '${FLM_REMOTE_PATH}' && FLM_AUTO_INSTALL_DEPS=1 ./flm-server.sh install"
  fi
  log_header "Running remote install on VPS (${mode})"
  log_info "This may take 10–20 minutes (Docker images, Maven, npm build)…"
  log_info "Remote: ${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}:${FLM_REMOTE_PATH}"
  if remote_ssh "$remote_cmd" 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "Remote install finished"
  else
    log_fail "Remote install failed — check output above and ${FLM_LOG_FILE}"
    return 1
  fi
  remote_post_install_vps || log_warn "Post-install tuning had warnings (services may still work)"
  log_summary_box "Remote VPS Install Complete" \
    "Host:     ${FLM_REMOTE_HOST}" \
    "Path:     ${FLM_REMOTE_PATH}" \
    "Web UI:   http://${FLM_REMOTE_HOST}/" \
    "API:      http://${FLM_REMOTE_HOST}:${API_PORT:-8080}/api" \
    "MQTT TLS: ${FLM_REMOTE_HOST}:8883" \
    "SSH:      ssh -p ${FLM_REMOTE_PORT} ${FLM_REMOTE_USER}@${FLM_REMOTE_HOST}"
}

remote_post_install_vps() {
  log_header "Remote post-install (firewall, port 80, MQTT permissions)"
  remote_ssh "bash -s" <<EOS
set -e
cd '${FLM_REMOTE_PATH}'
# Web UI on standard HTTP port (cloud firewalls usually block 3000)
if grep -q '^FRONTEND_PORT=' .env; then sed -i 's/^FRONTEND_PORT=.*/FRONTEND_PORT=80/' .env
else echo 'FRONTEND_PORT=80' >> .env; fi
# shellcheck disable=SC1091
source .env
bash ./scripts/generate-mqtt-certs.sh 2>/dev/null || true
bash ./scripts/generate-dynamic-security.sh 2>/dev/null || true
chmod 755 mosquitto/certs mosquitto/passwd 2>/dev/null || true
chmod 644 mosquitto/certs/*.crt mosquitto/certs/server.key 2>/dev/null || true
chmod 600 mosquitto/config/dynamic-security.json 2>/dev/null || true
chown mosquitto:mosquitto mosquitto/config/dynamic-security.json mosquitto/data mosquitto/log 2>/dev/null || true
mkdir -p mosquitto/log && touch mosquitto/log/mosquitto.log
chown mosquitto:mosquitto mosquitto/log/mosquitto.log 2>/dev/null || true
rm -f mosquitto/data/mosquitto.db
# Native Mosquitto (standalone): render config and restart
PLUGIN=\$(for p in /usr/lib/x86_64-linux-gnu/mosquitto_dynamic_security.so /usr/lib/aarch64-linux-gnu/mosquitto_dynamic_security.so /usr/lib/mosquitto_dynamic_security.so; do [[ -f "\$p" ]] && echo "\$p" && break; done)
if [[ -n "\$PLUGIN" && -f mosquitto/config/mosquitto-standalone.conf.template ]]; then
  sed -e "s|__PLUGIN_SO__|\${PLUGIN}|g" \\
      -e "s|__DYNSEC_JSON__|${FLM_REMOTE_PATH}/mosquitto/config/dynamic-security.json|g" \\
      -e "s|__CERT_DIR__|${FLM_REMOTE_PATH}/mosquitto/certs|g" \\
      -e "s|__DATA_DIR__|${FLM_REMOTE_PATH}/mosquitto/data|g" \\
      -e "s|__LOG_DIR__|${FLM_REMOTE_PATH}/mosquitto/log|g" \\
      mosquitto/config/mosquitto-standalone.conf.template > mosquitto/config/mosquitto-standalone.generated.conf
  if command -v systemctl >/dev/null 2>&1 && systemctl is-active mosquitto &>/dev/null; then
    systemctl stop mosquitto 2>/dev/null || true
    systemctl disable mosquitto 2>/dev/null || true
  fi
  [[ -f .run/mosquitto.pid ]] && kill "\$(cat .run/mosquitto.pid)" 2>/dev/null || true
  pkill -f 'mosquitto -c ${FLM_REMOTE_PATH}/mosquitto/config/mosquitto-standalone.generated.conf' 2>/dev/null || true
  sleep 1
  nohup mosquitto -c mosquitto/config/mosquitto-standalone.generated.conf >> mosquitto/log/mosquitto.log 2>&1 &
  echo \$! > .run/mosquitto.pid
fi
# Regenerate nginx for port 80 and restart (absolute -c path required)
NGINX_CONF='${FLM_REMOTE_PATH}/nginx/flm-standalone.conf'
sed -e "s|__FRONTEND_PORT__|\${FRONTEND_PORT}|g" \\
    -e "s|__API_PORT__|\${API_PORT:-8080}|g" \\
    -e "s|__FRONTEND_ROOT__|${FLM_REMOTE_PATH}/frontend/dist|g" \\
    -e "s|__NGINX_PID__|${FLM_REMOTE_PATH}/.run/nginx.pid|g" \\
    -e "s|__NGINX_ERROR_LOG__|${FLM_REMOTE_PATH}/logs/nginx-error.log|g" \\
    nginx/flm-standalone.conf.template > nginx/flm-standalone.conf
mkdir -p .run logs
# Ensure native PostgreSQL is up before backend health restart
if [[ -f scripts/lib/postgres.sh ]]; then
  # shellcheck disable=SC1091
  source scripts/lib/common.sh 2>/dev/null || true
  # shellcheck disable=SC1091
  source scripts/lib/postgres.sh 2>/dev/null || true
  postgres_start 2>/dev/null || true
  postgres_ensure_database 2>/dev/null || true
fi
nginx -c "\$NGINX_CONF" -s stop 2>/dev/null || true
pkill -f "nginx -c ${FLM_REMOTE_PATH}/nginx/flm-standalone.conf" 2>/dev/null || true
nginx -c "\$NGINX_CONF" -t
nginx -c "\$NGINX_CONF"
# ufw
if command -v ufw >/dev/null 2>&1; then
  ufw --force reset >/dev/null 2>&1 || true
  ufw default deny incoming >/dev/null
  ufw default allow outgoing >/dev/null
  ufw allow 22/tcp >/dev/null
  ufw allow 80/tcp >/dev/null
  ufw allow 443/tcp >/dev/null
  ufw allow \${API_PORT:-8080}/tcp >/dev/null
  ufw allow 1883/tcp >/dev/null
  ufw allow 8883/tcp >/dev/null
  ufw --force enable >/dev/null
fi
# Restart backend if MQTT was down during first start
if ! curl -sf http://127.0.0.1:\${API_PORT:-8080}/actuator/health >/dev/null 2>&1; then
  [[ -f .run/backend.pid ]] && kill "\$(cat .run/backend.pid)" 2>/dev/null || true
  sleep 2
  JAR=\$(ls backend/flm-api/target/flm-api-*.jar 2>/dev/null | head -1)
  [[ -n "\$JAR" ]] && nohup java -jar "\$JAR" --spring.profiles.active=standalone --server.port=\${API_PORT:-8080} >> logs/backend.log 2>&1 &
  [[ -n "\$JAR" ]] && echo \$! > .run/backend.pid
fi
EOS
  log_ok "Post-install complete"
}

remote_refresh_web_on_vps() {
  remote_ssh "bash -s" <<EOS
set -e
cd '${FLM_REMOTE_PATH}'
if grep -q '^FRONTEND_PORT=' .env; then sed -i 's/^FRONTEND_PORT=.*/FRONTEND_PORT=80/' .env
else echo 'FRONTEND_PORT=80' >> .env; fi
# shellcheck disable=SC1091
source .env
sed -e "s|__FRONTEND_PORT__|\${FRONTEND_PORT}|g" \\
    -e "s|__API_PORT__|\${API_PORT:-8080}|g" \\
    -e "s|__FRONTEND_ROOT__|${FLM_REMOTE_PATH}/frontend/dist|g" \\
    -e "s|__NGINX_PID__|${FLM_REMOTE_PATH}/.run/nginx.pid|g" \\
    -e "s|__NGINX_ERROR_LOG__|${FLM_REMOTE_PATH}/logs/nginx-error.log|g" \\
    nginx/flm-standalone.conf.template > nginx/flm-standalone.conf
mkdir -p .run logs
NGINX_CONF='${FLM_REMOTE_PATH}/nginx/flm-standalone.conf'
nginx -c "\$NGINX_CONF" -s stop 2>/dev/null || true
pkill -f "nginx -c ${FLM_REMOTE_PATH}/nginx/flm-standalone.conf" 2>/dev/null || true
sleep 1
nginx -c "\$NGINX_CONF" -t
nginx -c "\$NGINX_CONF"
EOS
}

remote_update() {
  local component="${1:-all}"
  load_remote_config || return 1
  require_remote_tools || return 1

  log_header "Remote update on VPS (${component})"
  log_info "Sync code → rebuild on ${FLM_REMOTE_HOST} (keeps .env, DB, MQTT certs)"

  remote_sync_files || return 1

  local remote_cmd="cd '${FLM_REMOTE_PATH}' && FLM_SKIP_DEPS=1 ./flm-server.sh update ${component}"
  if remote_ssh "$remote_cmd" 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "Remote update finished"
  else
    log_fail "Remote update failed — see ${FLM_LOG_FILE}"
    return 1
  fi

  # Refresh nginx on port 80 after frontend rebuild (no full post-install / ufw reset)
  if [[ "$component" == all || "$component" == frontend ]]; then
    remote_refresh_web_on_vps 2>/dev/null || log_warn "nginx refresh skipped"
  fi

  log_summary_box "Remote Update Complete" \
    "Host:   ${FLM_REMOTE_HOST}" \
    "Web UI: http://${FLM_REMOTE_HOST}/" \
    "API:    http://${FLM_REMOTE_HOST}:${API_PORT:-8080}/api" \
    "Check:  ./flm-server.sh remote-status"
}

remote_install() {
  local mode="${1:-}"
  load_remote_config || return 1
  require_remote_tools || return 1
  [[ -n "$mode" ]] && FLM_REMOTE_INSTALL_MODE="$mode"
  remote_sync_files || return 1
  remote_run_install "${FLM_REMOTE_INSTALL_MODE}" || return 1
}

remote_status() {
  load_remote_config || return 1
  require_remote_tools || return 1
  log_header "Remote VPS Status"
  remote_ssh "cd '${FLM_REMOTE_PATH}' && ./flm-server.sh status" 2>&1 | tee -a "$FLM_LOG_FILE" || {
    log_fail "Remote status failed — is FLM installed at ${FLM_REMOTE_PATH}?"
    return 1
  }
}

remote_uninstall() {
  load_remote_config || return 1
  require_remote_tools || return 1
  log_header "Remote VPS Uninstall (FULL WIPE)"
  echo -e "${C_YELLOW}This FULLY removes FLM on ${FLM_REMOTE_HOST}${C_RESET}"
  echo "  • Syncs latest uninstall scripts, then:"
  echo "  • Stops nginx / Java / Mosquitto / PostgreSQL"
  echo "  • Purges FLM data (certs, DB cluster, dynsec)"
  echo "  • apt-purges Mosquitto + PostgreSQL (removes /etc/mosquitto)"
  echo "  • Deletes install directory: ${FLM_REMOTE_PATH}"
  echo -e "${C_YELLOW}  WARNING: all FLM data and that folder are destroyed.${C_RESET}"
  if [[ "${FLM_AUTO_YES:-}" != "1" ]]; then
    read -r -p "Continue? [y/N] " ans
    [[ "${ans,,}" == "y" ]] || { log_info "Cancelled"; return 0; }
  fi

  # Push fixed uninstall logic before running it (VPS may still have old "keep tree" script)
  if remote_ssh "test -d '${FLM_REMOTE_PATH}'"; then
    log_info "Syncing uninstall scripts to remote…"
    remote_sync_files || return 1
  else
    log_info "${FLM_REMOTE_PATH} already absent — cleaning leftover packages if any"
  fi

  # FLM_UNINSTALL_FULL=1 → data + packages + delete tree
  if remote_ssh "test -x '${FLM_REMOTE_PATH}/flm-server.sh'"; then
    remote_ssh "cd '${FLM_REMOTE_PATH}' && FLM_UNINSTALL_FULL=1 FLM_AUTO_YES=1 ./flm-server.sh uninstall" 2>&1 | tee -a "$FLM_LOG_FILE" || true
  else
    log_warn "${FLM_REMOTE_PATH}/flm-server.sh missing — cleaning leftovers only"
    # Still try to purge packages from a one-liner if tree is gone but pkgs remain
    remote_ssh '
      systemctl stop mosquitto postgresql 2>/dev/null || true
      apt-get purge -y mosquitto mosquitto-clients 2>/dev/null || true
      pkgs=$(dpkg-query -W -f="${Package}\n" "postgresql*" 2>/dev/null || true)
      [ -n "$pkgs" ] && apt-get purge -y $pkgs 2>/dev/null || true
      rm -rf /etc/mosquitto /var/lib/mosquitto /var/log/mosquitto /etc/postgresql /var/lib/postgresql /var/log/postgresql
      apt-get autoremove -y -qq 2>/dev/null || true
    ' 2>&1 | tee -a "$FLM_LOG_FILE" || true
  fi
  # Safety net if tree delete failed mid-script
  remote_ssh "rm -rf '${FLM_REMOTE_PATH}'" 2>&1 | tee -a "$FLM_LOG_FILE" || true
  log_ok "Remote uninstall finished"
  log_info "Verifying remote is clean…"
  remote_ssh "
    if [[ -d '${FLM_REMOTE_PATH}' ]]; then echo 'FAIL: ${FLM_REMOTE_PATH} still exists'; else echo 'OK: ${FLM_REMOTE_PATH} removed'; fi
    if [[ -d /etc/mosquitto ]]; then echo 'FAIL: /etc/mosquitto still exists'; else echo 'OK: /etc/mosquitto removed'; fi
    command -v mosquitto >/dev/null 2>&1 && echo 'WARN: mosquitto binary still present' || echo 'OK: mosquitto not installed'
    command -v psql >/dev/null 2>&1 && echo 'WARN: psql still present' || echo 'OK: postgresql client not installed'
    ss -tln 2>/dev/null | grep -E ':80 |:8080 |:1883 |:8883 |:5432 ' || echo 'OK: FLM ports look free'
    pgrep -af 'mosquitto|flm-api-|postgres' 2>/dev/null | head -20 || echo 'OK: no matching processes'
  " 2>&1 | tee -a "$FLM_LOG_FILE" || true
}

remote_configure() {
  log_header "Configure Remote VPS Connection"
  ensure_env_file
  local example="$REMOTE_ENV_EXAMPLE"
  [[ -f "$REMOTE_ENV_FILE" ]] && example="$REMOTE_ENV_FILE"
  local host user path port mode pass
  # shellcheck disable=SC1090
  [[ -f "$example" ]] && source "$example"
  read -r -p "  Remote host [${FLM_REMOTE_HOST:-vivasvan-tech.in}]: " host
  host="${host:-${FLM_REMOTE_HOST:-vivasvan-tech.in}}"
  read -r -p "  SSH user [${FLM_REMOTE_USER:-root}]: " user
  user="${user:-${FLM_REMOTE_USER:-root}}"
  read -r -p "  Install path [${FLM_REMOTE_PATH:-/opt/flm}]: " path
  path="${path:-${FLM_REMOTE_PATH:-/opt/flm}}"
  read -r -p "  SSH port [${FLM_REMOTE_PORT:-22}]: " port
  port="${port:-${FLM_REMOTE_PORT:-22}}"
  read -r -p "  Install mode (standalone|docker) [${FLM_REMOTE_INSTALL_MODE:-standalone}]: " mode
  mode="${mode:-${FLM_REMOTE_INSTALL_MODE:-standalone}}"
  echo -n "  SSH password (empty = use SSH key): "
  read -r -s pass
  echo ""
  mkdir -p "$CONFIG_DIR"
  cat > "$REMOTE_ENV_FILE" <<EOF
# Remote VPS — DO NOT COMMIT (gitignored)
FLM_REMOTE_HOST=${host}
FLM_REMOTE_USER=${user}
FLM_REMOTE_PASSWORD=${pass}
FLM_REMOTE_PATH=${path}
FLM_REMOTE_PORT=${port}
FLM_REMOTE_INSTALL_MODE=${mode}
EOF
  chmod 600 "$REMOTE_ENV_FILE"
  log_ok "Saved to $REMOTE_ENV_FILE (mode 600)"
  log_info "Test: ./flm-server.sh remote-status"
}
