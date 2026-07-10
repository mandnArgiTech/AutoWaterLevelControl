#!/usr/bin/env bash
# MQTT (Mosquitto) install / uninstall — standalone (native) or docker
set -euo pipefail

_mqtt_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_mqtt_lib_dir/common.sh"

MQTT_STANDALONE_CONF="$SERVER_ROOT/mosquitto/config/mosquitto-standalone.generated.conf"
MQTT_STANDALONE_TEMPLATE="$SERVER_ROOT/mosquitto/config/mosquitto-standalone.conf.template"
MQTT_DYNSEC_JSON="$SERVER_ROOT/mosquitto/config/dynamic-security.json"
MQTT_PID_FILE="$RUN_DIR/mosquitto.pid"
MQTT_LOG_FILE="$LOG_DIR/mosquitto.log"

mqtt_ensure_certs() {
  export_env_for_certs
  if bash "$SCRIPTS_DIR/generate-mqtt-certs.sh"; then
    log_ok "Certificates ready in mosquitto/certs/"
    log_info "Dynamic Security: mosquitto/config/dynamic-security.json"
    log_info "Device CA file: mosquitto/certs/esp8266_ca.pem"
    log_info "Config snippet: mosquitto/certs/esp8266-mqtt-config.json"
  else
    log_fail "Certificate generation failed — see $FLM_LOG_FILE"
    return 1
  fi
}

mqtt_ensure_certs_force() {
  export_env_for_certs
  bash "$SCRIPTS_DIR/generate-mqtt-certs.sh" --force
}

_mqtt_run_as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    return 1
  fi
}

mqtt_disable_system_mosquitto() {
  if command -v systemctl >/dev/null 2>&1 && systemctl is-active mosquitto &>/dev/null; then
    log_warn "Stopping system mosquitto service (FLM owns ports 1883/8883)"
    _mqtt_run_as_root systemctl stop mosquitto 2>/dev/null || true
    _mqtt_run_as_root systemctl disable mosquitto 2>/dev/null || true
  fi
}

mqtt_fix_standalone_permissions() {
  local log_dir="$SERVER_ROOT/mosquitto/log"
  mkdir -p "$SERVER_ROOT/mosquitto/data" "$log_dir"
  chmod 755 "$SERVER_ROOT/mosquitto/certs" "$log_dir" 2>/dev/null || true
  chmod 644 "$SERVER_ROOT/mosquitto/certs/"*.crt "$SERVER_ROOT/mosquitto/certs/server.key" 2>/dev/null || true
  [[ -f "$MQTT_DYNSEC_JSON" ]] && chmod 600 "$MQTT_DYNSEC_JSON"
  if id mosquitto >/dev/null 2>&1; then
    chown mosquitto:mosquitto "$MQTT_DYNSEC_JSON" 2>/dev/null || true
    chown -R mosquitto:mosquitto "$SERVER_ROOT/mosquitto/data" "$log_dir" 2>/dev/null || true
    touch "$log_dir/mosquitto.log"
    chown mosquitto:mosquitto "$log_dir/mosquitto.log" 2>/dev/null || true
    chmod 640 "$log_dir/mosquitto.log" 2>/dev/null || true
  fi
}

mqtt_render_standalone_conf() {
  local plugin
  plugin="$(find_dynsec_plugin)" || {
    log_fail "Dynamic Security plugin not found — run: sudo apt install mosquitto"
    return 1
  }
  [[ -f "$MQTT_STANDALONE_TEMPLATE" ]] || {
    log_fail "Missing template: $MQTT_STANDALONE_TEMPLATE"
    return 1
  }
  local cert_dir data_dir log_dir
  cert_dir="$SERVER_ROOT/mosquitto/certs"
  data_dir="$SERVER_ROOT/mosquitto/data"
  log_dir="$SERVER_ROOT/mosquitto/log"
  sed \
    -e "s|__PLUGIN_SO__|${plugin//|/\\|}|g" \
    -e "s|__DYNSEC_JSON__|$MQTT_DYNSEC_JSON|g" \
    -e "s|__CERT_DIR__|$cert_dir|g" \
    -e "s|__DATA_DIR__|$data_dir|g" \
    -e "s|__LOG_DIR__|$log_dir|g" \
    "$MQTT_STANDALONE_TEMPLATE" > "$MQTT_STANDALONE_CONF"
  log_ok "Generated $MQTT_STANDALONE_CONF"
}

mqtt_health_check() {
  local user pass
  user="${MQTT_USER_BRIDGE:-flmServerAdmin}"
  pass="${MQTT_PASS_BRIDGE:-flmPass}"
  mosquitto_sub -h localhost -p 1883 -u "$user" -P "$pass" \
    -t '$SYS/broker/version' -C 1 -W 3 >/dev/null 2>&1
}

mqtt_start_standalone() {
  ensure_env_file
  require_cmd mosquitto "Install: sudo apt install mosquitto" || return 1
  mqtt_disable_system_mosquitto

  if [[ -f "$MQTT_PID_FILE" ]] && kill -0 "$(cat "$MQTT_PID_FILE" 2>/dev/null)" 2>/dev/null; then
    if mqtt_health_check; then
      log_info "Mosquitto already running (PID $(cat "$MQTT_PID_FILE"))"
      return 0
    fi
    log_warn "Mosquitto PID alive but unhealthy — restarting"
    mqtt_stop_standalone
  elif is_mqtt_standalone_running; then
    # Port open but no valid PID file (stale process)
    log_warn "Mosquitto port in use without valid PID file — clearing"
    mqtt_stop_standalone
  fi

  mqtt_render_standalone_conf || return 1
  mqtt_fix_standalone_permissions
  mkdir -p "$SERVER_ROOT/mosquitto/data" "$SERVER_ROOT/mosquitto/log"
  # Clear persistence so Dynamic Security reloads from JSON (avoids stale clients)
  rm -f "$SERVER_ROOT/mosquitto/data/mosquitto.db"

  nohup mosquitto -c "$MQTT_STANDALONE_CONF" >> "$SERVER_ROOT/mosquitto/log/mosquitto.log" 2>&1 &
  echo $! > "$MQTT_PID_FILE"
  sleep 2

  local i ok=false
  for i in $(seq 1 10); do
    if [[ -f "$MQTT_PID_FILE" ]] && kill -0 "$(cat "$MQTT_PID_FILE")" 2>/dev/null && mqtt_health_check; then
      ok=true
      break
    fi
    sleep 1
  done
  if [[ "$ok" == true ]]; then
    log_ok "Mosquitto started (PID $(cat "$MQTT_PID_FILE"))"
    return 0
  fi

  log_fail "Mosquitto failed to start — see $SERVER_ROOT/mosquitto/log/mosquitto.log"
  if [[ -f "$SERVER_ROOT/mosquitto/log/mosquitto.log" ]]; then
    tail -8 "$SERVER_ROOT/mosquitto/log/mosquitto.log" | sed 's/^/  /' || true
  fi
  return 1
}

mqtt_stop_standalone() {
  if [[ -f "$MQTT_PID_FILE" ]]; then
    local pid
    pid="$(cat "$MQTT_PID_FILE" 2>/dev/null || true)"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
      sleep 1
      kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$MQTT_PID_FILE"
  fi
  pkill -f "mosquitto -c $MQTT_STANDALONE_CONF" 2>/dev/null || true
  sleep 1
  pkill -9 -f "mosquitto -c $MQTT_STANDALONE_CONF" 2>/dev/null || true
  # Wait briefly for ports to release
  local i
  for i in $(seq 1 5); do
    if command -v ss >/dev/null 2>&1; then
      if ! ss -tln 2>/dev/null | grep -qE ':1883 |:8883 '; then
        break
      fi
    else
      break
    fi
    sleep 1
  done
  if command -v ss >/dev/null 2>&1 && ss -tln 2>/dev/null | grep -qE ':1883 |:8883 '; then
    log_warn "MQTT ports 1883/8883 may still be in use"
  else
    log_ok "Mosquitto stopped"
  fi
}

mqtt_restart_standalone() {
  mqtt_stop_standalone
  mqtt_start_standalone
}

mqtt_install_standalone() {
  log_header "Installing MQTT Server (Mosquitto — native standalone)"
  local total=5 failed=0

  log_step 1 "$total" "Checking prerequisites"
  require_cmd openssl "Install: sudo apt install openssl" || ((failed++)) || true
  # shellcheck source=deps.sh
  source "$_mqtt_lib_dir/deps.sh"
  ensure_mosquitto || ((failed++)) || true
  [[ $failed -eq 0 ]] || { log_fail "Prerequisites missing — install aborted"; return 1; }

  log_step 2 "$total" "TLS certificates + Dynamic Security config"
  mqtt_ensure_certs || return 1

  log_step 3 "$total" "Rendering Mosquitto config"
  mqtt_render_standalone_conf || return 1

  log_step 4 "$total" "Starting Mosquitto (ports 1883 plain, 8883 TLS)"
  mqtt_disable_system_mosquitto
  mqtt_start_standalone || return 1

  log_step 5 "$total" "Verifying TLS (ESP8266 compatibility)"
  if bash "$SCRIPTS_DIR/verify-mqtt-tls.sh" 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "TLS verification passed"
  else
    log_warn "TLS verification skipped or failed (broker may still work locally)"
  fi

  log_summary_box "MQTT Installation Complete" \
    "Plain MQTT:  port 1883" \
    "TLS MQTT:    port 8883  (for ESP8266 devices)" \
    "Dynsec admin: ${MQTT_DYNSEC_ADMIN:-flmDynsecAdmin}" \
    "Hostname:    ${MQTT_SERVER_CN:-vivasvan-tech.in}" \
    "Bridge user: ${MQTT_USER_BRIDGE:-flmServerAdmin}" \
    "Device user: ${MQTT_USER_DEVICE:-devAdmin}" \
    "CA cert:     mosquitto/certs/esp8266_ca.pem" \
    "Config:      mosquitto-standalone.generated.conf"
  return 0
}

mqtt_install_docker() {
  log_header "Installing MQTT Server (Mosquitto — Docker)"
  local total=4 failed=0

  log_step 1 "$total" "Checking prerequisites"
  require_cmd openssl "Install: sudo apt install openssl" || ((failed++)) || true
  require_docker || ((failed++)) || true
  [[ $failed -eq 0 ]] || { log_fail "Prerequisites missing — install aborted"; return 1; }

  export_env_for_certs
  FLM_DEPLOY_MODE=docker bash "$SCRIPTS_DIR/generate-mqtt-certs.sh" || return 1

  log_step 2 "$total" "Starting Mosquitto container (ports 1883 plain, 8883 TLS)"
  cd "$SERVER_ROOT"
  if $COMPOSE -f docker-compose.mqtt.yml up -d 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "Mosquitto container started"
  else
    log_fail "Failed to start Mosquitto — is port 1883 or 8883 already in use?"
    log_info "Run: sudo ss -tlnp | grep -E '1883|8883'"
    return 1
  fi

  log_step 3 "$total" "Waiting for MQTT broker"
  sleep 3
  local ok=false
  for _ in $(seq 1 20); do
    if $COMPOSE -f docker-compose.mqtt.yml exec -T mosquitto \
      mosquitto_sub -h localhost -p 1883 -u "${MQTT_USER_BRIDGE:-flmServerAdmin}" \
      -P "${MQTT_PASS_BRIDGE:-flmPass}" \
      -t '$SYS/broker/version' -C 1 -W 2 >/dev/null 2>&1; then
      ok=true
      break
    fi
    sleep 1
  done
  if $ok; then
    log_ok "MQTT broker accepting connections"
  else
    log_warn "Broker started but health check timed out — check logs"
  fi

  log_step 4 "$total" "Verifying TLS (ESP8266 compatibility)"
  if bash "$SCRIPTS_DIR/verify-mqtt-tls.sh" 2>&1 | tee -a "$FLM_LOG_FILE"; then
    log_ok "TLS verification passed"
  else
    log_warn "TLS verification skipped or failed (broker may still work locally)"
  fi

  log_summary_box "MQTT Installation Complete" \
    "Plain MQTT:  port 1883" \
    "TLS MQTT:    port 8883  (for ESP8266 devices)" \
    "Hostname:    ${MQTT_SERVER_CN:-vivasvan-tech.in}" \
    "Device user: ${MQTT_USER_DEVICE:-devAdmin}" \
    "CA cert:     mosquitto/certs/esp8266_ca.pem"
  return 0
}

mqtt_install() {
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    mqtt_install_standalone
  else
    mqtt_install_docker
  fi
}

mqtt_uninstall() {
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    log_header "Uninstalling MQTT Server (Mosquitto — native)"
    log_step 1 3 "Stopping Mosquitto"
    mqtt_stop_standalone

    local purge=false
    if flm_should_purge; then
      purge=true
    elif [[ -z "${FLM_UNINSTALL_PURGE:-}" ]]; then
      # Standalone component uninstall (not driven by uninstall_all)
      if flm_confirm_or_auto "Remove generated Mosquitto config, certs, and dynamic-security.json?"; then
        purge=true
      fi
    fi

    if [[ "$purge" == true ]]; then
      log_step 2 3 "Removing generated config"
      rm -f "$MQTT_STANDALONE_CONF" 2>/dev/null || true
      log_ok "Generated config removed"
      log_step 3 3 "Removing TLS certificates + Dynamic Security"
      rm -f "$SERVER_ROOT/mosquitto/certs/"*.crt "$SERVER_ROOT/mosquitto/certs/"*.key \
            "$SERVER_ROOT/mosquitto/certs/"*.pem "$SERVER_ROOT/mosquitto/certs/"*.txt \
            "$SERVER_ROOT/mosquitto/certs/"*.json "$SERVER_ROOT/mosquitto/certs/"*.srl 2>/dev/null || true
      rm -f "$MQTT_DYNSEC_JSON" 2>/dev/null || true
      rm -rf "$SERVER_ROOT/mosquitto/data/"* "$SERVER_ROOT/mosquitto/.dynsec-bootstrap" 2>/dev/null || true
      log_ok "Certificates and Dynamic Security removed"
    else
      log_step 2 3 "Generated config"
      log_info "Kept (set FLM_UNINSTALL_PURGE=1 to remove)"
      log_step 3 3 "TLS certificates + Dynamic Security"
      log_info "Kept in mosquitto/certs/"
    fi
    log_summary_box "MQTT Uninstall Complete" \
      "Native Mosquitto: stopped" \
      "Reinstall anytime: ./flm-server.sh → Install → MQTT only"
    return 0
  fi

  log_header "Uninstalling MQTT Server (Mosquitto — Docker)"
  log_step 1 4 "Stopping Mosquitto container"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.mqtt.yml down 2>&1 | tee -a "$FLM_LOG_FILE" || true
  $COMPOSE -f docker-compose.infra.yml stop mosquitto 2>/dev/null || true
  $COMPOSE -f docker-compose.yml stop mosquitto 2>/dev/null || true
  log_ok "Mosquitto stopped"

  log_step 2 4 "Removing Mosquitto container"
  $COMPOSE -f docker-compose.mqtt.yml rm -f 2>/dev/null || true
  log_ok "Container removed"

  log_step 3 4 "MQTT data volumes"
  if flm_should_purge || flm_confirm_or_auto "Remove MQTT persistence volumes?"; then
    docker volume rm server_mosquitto_data server_mosquitto_log 2>/dev/null || true
    log_ok "MQTT volumes removed"
  else
    log_info "Volumes kept (data preserved for reinstall)"
  fi

  log_step 4 4 "TLS certificates"
  if flm_should_purge || flm_confirm_or_auto "Remove generated certificates?"; then
    rm -f "$SERVER_ROOT/mosquitto/certs/"*.crt "$SERVER_ROOT/mosquitto/certs/"*.key \
          "$SERVER_ROOT/mosquitto/certs/"*.pem "$SERVER_ROOT/mosquitto/certs/"*.txt \
          "$SERVER_ROOT/mosquitto/certs/"*.json "$SERVER_ROOT/mosquitto/certs/"*.srl 2>/dev/null || true
    rm -f "$SERVER_ROOT/mosquitto/passwd/passwd" 2>/dev/null || true
    log_ok "Certificates removed (will regenerate on next install)"
  else
    log_info "Certificates kept in mosquitto/certs/"
  fi

  log_summary_box "MQTT Uninstall Complete" \
    "Mosquitto container: removed" \
    "Reinstall anytime: ./flm-server.sh → Install → MQTT only"
  return 0
}

mqtt_status() {
  ensure_env_file
  echo ""
  echo -e "${C_BOLD}MQTT (Mosquitto)${C_RESET}"
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    if is_mqtt_standalone_running; then
      log_ok "Running (native)"
      [[ -f "$MQTT_PID_FILE" ]] && log_info "PID: $(cat "$MQTT_PID_FILE")"
      if command -v ss >/dev/null 2>&1; then
        ss -tln 2>/dev/null | grep -E ':1883 |:8883 ' | sed 's/^/  /' || true
      fi
    else
      log_fail "Not running"
    fi
    if [[ -f "$MQTT_DYNSEC_JSON" ]]; then
      log_ok "Dynamic Security config present"
    else
      log_warn "No dynamic-security.json — run Install → MQTT"
    fi
  else
    if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
      log_ok "Running (Docker)"
      docker ps --filter name=mosquitto --format '  Container: {{.Names}}  Status: {{.Status}}  Ports: {{.Ports}}'
    else
      log_fail "Not running"
    fi
  fi
  if [[ -f "$SERVER_ROOT/mosquitto/certs/esp8266_ca.pem" ]]; then
    log_ok "TLS certificates present"
  else
    log_warn "No certificates — run Install → MQTT"
  fi
}

mqtt_verify_plain() {
  local host="${1:-127.0.0.1}"
  local port="${2:-1883}"
  local user="${MQTT_USER_DEVICE:-devAdmin}"
  local pass="${MQTT_PASS_DEVICE:-123456}"
  if ! command -v mosquitto_pub >/dev/null 2>&1; then
    log_warn "mosquitto_pub not installed — skip plain MQTT test"
    return 0
  fi
  if mosquitto_pub -h "$host" -p "$port" -u "$user" -P "$pass" \
      -t 'testtank/water/level' -m '{"ok":true,"tls":false}' -q 0 >/dev/null 2>&1; then
    log_ok "Plain MQTT OK (${host}:${port} as ${user})"
    return 0
  fi
  log_fail "Plain MQTT failed on ${host}:${port} (user ${user})"
  return 1
}

mqtt_print_device_snippets() {
  local host="${MQTT_SERVER_CN:-vivasvan-tech.in}"
  local user="${MQTT_USER_DEVICE:-devAdmin}"
  local pass="${MQTT_PASS_DEVICE:-123456}"
  echo ""
  echo "── ESP8266 config.json (plain MQTT, no TLS) ──"
  cat <<EOF
  "mqtt": {
    "enabled": true,
    "server": "${host}",
    "port": 1883,
    "username": "${user}",
    "password": "${pass}",
    "tls": false
  }
EOF
  echo ""
  echo "── ESP8266 config.json (TLS MQTT) ──"
  cat <<EOF
  "mqtt": {
    "enabled": true,
    "server": "${host}",
    "port": 8883,
    "username": "${user}",
    "password": "${pass}",
    "tls": true,
    "tlsMode": "fingerprint"
  }
EOF
  echo ""
  log_warn "Cloud firewall (Hostinger/VPS panel) must allow TCP 1883 (plain) and/or 8883 (TLS)."
  log_warn "ufw alone is not enough — open the same ports in the provider firewall panel."
}

mqtt_configure() {
  log_header "Configure MQTT (plain 1883 + TLS 8883)"
  ensure_env_file

  echo "Broker listens on BOTH:"
  echo "  • Plain MQTT (no TLS) → port 1883  — recommended for ESP8266 when heap is tight"
  echo "  • TLS MQTT            → port 8883  — encrypted (fingerprint or CA)"
  echo ""
  echo "Current values (press Enter to keep default):"
  read -r -p "  Hostname [${MQTT_SERVER_CN}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SERVER_CN "$v"
  read -r -p "  SAN DNS [${MQTT_SAN_DNS}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SAN_DNS "$v"
  read -r -p "  SAN IPs [${MQTT_SAN_IPS}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SAN_IPS "$v"
  read -r -p "  Dynsec admin [${MQTT_DYNSEC_ADMIN:-flmDynsecAdmin}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_DYNSEC_ADMIN "$v"
  read -r -p "  Dynsec password [${MQTT_DYNSEC_PASS:-flmDynsecPass}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_DYNSEC_PASS "$v"
  read -r -p "  Bridge username [${MQTT_USER_BRIDGE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_USER_BRIDGE "$v"
  read -r -p "  Bridge password [${MQTT_PASS_BRIDGE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_PASS_BRIDGE "$v"
  read -r -p "  Device username [${MQTT_USER_DEVICE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_USER_DEVICE "$v"
  read -r -p "  Device password [${MQTT_PASS_DEVICE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_PASS_DEVICE "$v"

  # Reload .env so snippets / verify use new values
  # shellcheck disable=SC1091
  set -a; source "$SERVER_ROOT/.env"; set +a

  log_step 1 3 "Regenerating certificates and Dynamic Security config"
  mqtt_ensure_certs_force
  log_ok "Configuration saved to .env"

  log_step 2 3 "Rendering Mosquitto config (plain 1883 + TLS 8883)"
  read -r -p "Restart Mosquitto now? [Y/n] " restart
  if [[ "${restart,,}" != "n" ]]; then
    if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
      mqtt_render_standalone_conf
      mqtt_restart_standalone
      log_ok "Native Mosquitto restarted"
    else
      cd "$SERVER_ROOT"
      $COMPOSE -f docker-compose.mqtt.yml restart 2>/dev/null || mqtt_install_docker
      log_ok "Mosquitto container restarted"
    fi
  else
    mqtt_render_standalone_conf || true
  fi

  log_step 3 3 "Verifying plain MQTT (no TLS) on localhost:1883"
  mqtt_verify_plain 127.0.0.1 1883 || true
  if command -v ss >/dev/null 2>&1; then
    ss -tln 2>/dev/null | grep -E ':1883 |:8883 ' | sed 's/^/  /' || true
  fi

  mqtt_print_device_snippets
  log_summary_box "MQTT Configure Complete" \
    "Plain MQTT:  ${MQTT_SERVER_CN}:1883  (tls=false)" \
    "TLS MQTT:    ${MQTT_SERVER_CN}:8883  (tls=true)" \
    "Device user: ${MQTT_USER_DEVICE:-devAdmin}" \
    "Open in cloud firewall: 1883 and 8883"
}

mqtt_uninstall_quiet() {
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    mqtt_stop_standalone
    return 0
  fi
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.mqtt.yml down 2>/dev/null || true
}

mqtt_regenerate_certs() {
  log_header "Regenerate MQTT TLS Certificates"
  export_env_for_certs
  mqtt_ensure_certs_force
  log_ok "Certificates and Dynamic Security config regenerated"
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
    if is_mqtt_standalone_running; then
      log_info "Restarting native Mosquitto to load new certs"
      mqtt_render_standalone_conf
      mqtt_restart_standalone
    fi
  elif docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
    log_info "Restart Mosquitto container to load new certs"
  fi
}
