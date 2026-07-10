#!/usr/bin/env bash
# MQTT (Mosquitto) install / uninstall — zero-config, certs included
set -euo pipefail

_mqtt_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_mqtt_lib_dir/common.sh"

mqtt_ensure_certs() {
  export_env_for_certs
  if bash "$SCRIPTS_DIR/generate-mqtt-certs.sh"; then
    log_ok "Certificates ready in mosquitto/certs/"
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

mqtt_install() {
  log_header "Installing MQTT Server (Mosquitto)"
  local total=4 failed=0

  log_step 1 "$total" "Checking prerequisites"
  require_cmd openssl "Install: sudo apt install openssl" || ((failed++)) || true
  require_docker || ((failed++)) || true
  [[ $failed -eq 0 ]] || { log_fail "Prerequisites missing — install aborted"; return 1; }

  mqtt_ensure_certs || return 1

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
  for i in $(seq 1 20); do
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
    "CA cert:     mosquitto/certs/esp8266_ca.pem" \
    "Fingerprint: mosquitto/certs/esp8266-fingerprint.txt"
  return 0
}

mqtt_uninstall() {
  log_header "Uninstalling MQTT Server (Mosquitto)"

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
  read -r -p "  Remove MQTT persistence volumes? [y/N] " ans
  if [[ "${ans,,}" == "y" ]]; then
    docker volume rm server_mosquitto_data server_mosquitto_log 2>/dev/null || true
    log_ok "MQTT volumes removed"
  else
    log_info "Volumes kept (data preserved for reinstall)"
  fi

  log_step 4 4 "TLS certificates"
  read -r -p "  Remove generated certificates? [y/N] " ans2
  if [[ "${ans2,,}" == "y" ]]; then
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
  echo ""
  echo -e "${C_BOLD}MQTT (Mosquitto)${C_RESET}"
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
    log_ok "Running"
    docker ps --filter name=mosquitto --format '  Container: {{.Names}}  Status: {{.Status}}  Ports: {{.Ports}}'
  else
    log_fail "Not running"
  fi
  if [[ -f "$SERVER_ROOT/mosquitto/certs/esp8266_ca.pem" ]]; then
    log_ok "TLS certificates present"
  else
    log_warn "No certificates — run Install → MQTT"
  fi
}

mqtt_configure() {
  log_header "Configure MQTT"
  ensure_env_file

  echo "Current values (press Enter to keep default):"
  read -r -p "  Hostname [${MQTT_SERVER_CN}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SERVER_CN "$v"
  read -r -p "  SAN DNS [${MQTT_SAN_DNS}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SAN_DNS "$v"
  read -r -p "  SAN IPs [${MQTT_SAN_IPS}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_SAN_IPS "$v"
  read -r -p "  Device username [${MQTT_USER_DEVICE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_USER_DEVICE "$v"
  read -r -p "  Device password [${MQTT_PASS_DEVICE}]: " v
  [[ -n "$v" ]] && save_env_var MQTT_PASS_DEVICE "$v"

  log_step 1 1 "Regenerating certificates with new settings"
  mqtt_ensure_certs_force
  log_ok "Configuration saved to .env and certificates updated"

  read -r -p "Restart Mosquitto now? [Y/n] " restart
  if [[ "${restart,,}" != "n" ]]; then
    cd "$SERVER_ROOT"
    $COMPOSE -f docker-compose.mqtt.yml restart 2>/dev/null || mqtt_install
    log_ok "Mosquitto restarted with new certificates"
  fi
}

mqtt_uninstall_quiet() {
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.mqtt.yml down 2>/dev/null || true
}

mqtt_regenerate_certs() {
  log_header "Regenerate MQTT TLS Certificates"
  export_env_for_certs
  mqtt_ensure_certs_force
  log_ok "Certificates regenerated"
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qi mosquitto; then
    log_info "Restart Mosquitto to load new certs: Uninstall + Install MQTT, or restart container"
  fi
}
