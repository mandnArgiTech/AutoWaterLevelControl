#!/usr/bin/env bash
# =============================================================================
# FLM Platform Server Manager — single menu for install / uninstall / configure
#
# Usage:
#   cd server
#   chmod +x flm-server.sh
#   ./flm-server.sh
# =============================================================================
set -euo pipefail

SERVER_ROOT="$(cd "$(dirname "$0")" && pwd)"
export SERVER_ROOT
export FLM_LOG_FILE="${FLM_LOG_FILE:-$SERVER_ROOT/logs/flm-server-$(date +%Y%m%d).log}"

# shellcheck source=scripts/lib/common.sh
source "$SERVER_ROOT/scripts/lib/common.sh"
# shellcheck source=scripts/lib/mqtt.sh
source "$SERVER_ROOT/scripts/lib/mqtt.sh"
# shellcheck source=scripts/lib/postgres.sh
source "$SERVER_ROOT/scripts/lib/postgres.sh"
# shellcheck source=scripts/lib/backend.sh
source "$SERVER_ROOT/scripts/lib/backend.sh"
# shellcheck source=scripts/lib/frontend.sh
source "$SERVER_ROOT/scripts/lib/frontend.sh"
# shellcheck source=scripts/lib/deps.sh
source "$SERVER_ROOT/scripts/lib/deps.sh"

chmod +x "$SCRIPTS_DIR"/*.sh "$SCRIPTS_DIR/lib"/*.sh 2>/dev/null || true
ensure_env_file

show_banner() {
  clear 2>/dev/null || true
  echo -e "${C_BOLD}${C_CYAN}"
  echo "  ╔═══════════════════════════════════════════════════════════╗"
  echo "  ║     FLM Platform Server Manager                           ║"
  echo "  ║     FluidLevelMonitor — MQTT · API · Web UI               ║"
  echo "  ╚═══════════════════════════════════════════════════════════╝"
  echo -e "${C_RESET}"
  log_info "Log file: $FLM_LOG_FILE"
  log_info "Hostname: ${MQTT_SERVER_CN:-vivasvan-tech.in}  |  Device MQTT: ${MQTT_USER_DEVICE:-devAdmin}"
  echo ""
}

install_all_standalone() {
  log_header "Installing FULL Platform (Standalone — default production)"
  local total=6

  log_step 1 "$total" "System dependencies (Java, Node, nginx, Docker, OpenSSL)"
  deps_install_standalone || return 1
  save_env_var FLM_DEPLOY_MODE standalone

  log_step 2 "$total" "MQTT + TLS certificates"
  mqtt_install || return 1

  log_step 3 "$total" "PostgreSQL database"
  postgres_install || return 1

  log_step 4 "$total" "Java backend API (production JAR)"
  backend_install_standalone || return 1

  log_step 5 "$total" "React web UI (production build + nginx)"
  frontend_install_standalone || return 1

  log_step 6 "$total" "Final verification"
  bash "$SCRIPTS_DIR/verify-mqtt-tls.sh" 2>&1 | tee -a "$FLM_LOG_FILE" || true

  log_summary_box "Full Platform Installed (Standalone)" \
    "Web UI:      http://localhost:${FRONTEND_PORT:-3000}" \
    "API:         http://localhost:${API_PORT:-8080}/api" \
    "MQTT plain:  port 1883" \
    "MQTT TLS:    port 8883  (ESP8266)" \
    "MQTT host:   ${MQTT_SERVER_CN}" \
    "Device user: ${MQTT_USER_DEVICE} / (see .env)" \
    "CA cert:     mosquitto/certs/esp8266_ca.pem" \
    "Admin login: admin / 123456 (change on first login)" \
    "Vendor login: vendor / 123456  (code: demo)"
}

install_all_docker() {
  log_header "Installing FULL Platform (Docker)"
  local total=6

  log_step 1 "$total" "System dependencies (Docker, OpenSSL)"
  deps_install_docker || return 1

  log_step 2 "$total" "MQTT + TLS certificates (zero configuration)"
  mqtt_install || return 1

  log_step 3 "$total" "PostgreSQL database"
  postgres_install || return 1

  log_step 4 "$total" "Java backend API"
  cd "$SERVER_ROOT"
  $COMPOSE -f docker-compose.backend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
  wait_for_port localhost "${API_PORT:-8080}" "REST API" 120 || log_warn "API still starting"

  log_step 5 "$total" "React web UI"
  $COMPOSE -f docker-compose.frontend.yml up -d --build 2>&1 | tee -a "$FLM_LOG_FILE"
  wait_for_port localhost "${FRONTEND_PORT:-3000}" "Web UI" 60 || log_warn "UI still starting"

  log_step 6 "$total" "Final verification"
  bash "$SCRIPTS_DIR/verify-mqtt-tls.sh" 2>&1 | tee -a "$FLM_LOG_FILE" || true

  save_env_var FLM_DEPLOY_MODE docker
  log_summary_box "Full Platform Installed (Docker)" \
    "Web UI:      http://localhost:${FRONTEND_PORT:-3000}" \
    "API:         http://localhost:${API_PORT:-8080}/api" \
    "MQTT plain:  port 1883" \
    "MQTT TLS:    port 8883  (ESP8266)" \
    "MQTT host:   ${MQTT_SERVER_CN}" \
    "Device user: ${MQTT_USER_DEVICE} / (see .env)" \
    "CA cert:     mosquitto/certs/esp8266_ca.pem" \
    "Admin login: admin / 123456 (change on first login)" \
    "Vendor login: vendor / 123456  (code: demo)"
}

uninstall_all() {
  log_header "Uninstalling FULL Platform"
  echo -e "${C_YELLOW}This stops all FLM services.${C_RESET}"
  read -r -p "Continue? [y/N] " ans
  [[ "${ans,,}" == "y" ]] || { log_info "Cancelled"; return 0; }

  frontend_uninstall
  backend_uninstall
  mqtt_uninstall
  postgres_uninstall

  log_summary_box "Full Platform Uninstalled" \
    "All services stopped." \
    "Reinstall: ./flm-server.sh → Install"
}

show_status() {
  log_header "Service Status"
  mqtt_status
  postgres_status
  backend_status
  frontend_status
  echo ""
  log_info "Config file: $ENV_FILE"
  pause_enter
}

show_config() {
  log_header "Current Configuration"
  ensure_env_file
  echo ""
  grep -v '^#' "$ENV_FILE" | grep -v '^$' | while read -r line; do
    key="${line%%=*}"
    if [[ "$key" == *PASSWORD* || "$key" == *SECRET* ]]; then
      echo "  ${key}=********"
    else
      echo "  $line"
    fi
  done
  echo ""
  pause_enter
}

view_logs() {
  log_header "View Logs"
  echo "  1) Main install log (today)"
  echo "  2) Backend log"
  echo "  3) Frontend log (manual npm run dev)"
  echo "  4) nginx error log (standalone UI)"
  echo "  5) Mosquitto container log"
  echo "  0) Back"
  read -r -p "Choice: " c
  case "$c" in
    1) less +G "$FLM_LOG_FILE" 2>/dev/null || tail -100 "$FLM_LOG_FILE" ;;
    2) less +G "$LOG_DIR/backend.log" 2>/dev/null || tail -100 "$LOG_DIR/backend.log" 2>/dev/null || log_warn "No backend log" ;;
    3) less +G "$LOG_DIR/frontend.log" 2>/dev/null || tail -100 "$LOG_DIR/frontend.log" 2>/dev/null || log_warn "No frontend log" ;;
    4) less +G "$LOG_DIR/nginx-error.log" 2>/dev/null || tail -100 "$LOG_DIR/nginx-error.log" 2>/dev/null || log_warn "No nginx log" ;;
    5) docker logs --tail 100 flm-mosquitto 2>/dev/null || log_warn "Mosquitto not running" ;;
  esac
}

menu_install() {
  while true; do
    show_banner
    echo -e "${C_BOLD}Install${C_RESET}"
    echo "  1) Install ALL — Standalone (default / production on VPS)"
    echo "  2) Install ALL — Docker (everything in containers)"
    echo "  3) Install MQTT only (certificates + Mosquitto, zero config)"
    echo "  4) Install Database only (PostgreSQL)"
    echo "  5) Install Backend only (Java API)"
    echo "  6) Install Frontend only (React UI)"
    echo "  0) Back"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) install_all_standalone; pause_enter ;;
      2) install_all_docker; pause_enter ;;
      3) mqtt_install; pause_enter ;;
      4) postgres_install; pause_enter ;;
      5)
        if [[ "${FLM_DEPLOY_MODE:-standalone}" == standalone ]]; then backend_install_standalone
        else backend_install_docker; fi
        pause_enter ;;
      6)
        if [[ "${FLM_DEPLOY_MODE:-standalone}" == standalone ]]; then frontend_install_standalone
        else frontend_install_docker; fi
        pause_enter ;;
      0) return ;;
      *) log_warn "Invalid choice" ;;
    esac
  done
}

menu_uninstall() {
  while true; do
    show_banner
    echo -e "${C_BOLD}Uninstall${C_RESET}"
    echo "  1) Uninstall ALL"
    echo "  2) Uninstall MQTT only"
    echo "  3) Uninstall Database only"
    echo "  4) Uninstall Backend only"
    echo "  5) Uninstall Frontend only"
    echo "  0) Back"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) uninstall_all; pause_enter ;;
      2) mqtt_uninstall; pause_enter ;;
      3) postgres_uninstall; pause_enter ;;
      4) backend_uninstall; pause_enter ;;
      5) frontend_uninstall; pause_enter ;;
      0) return ;;
      *) log_warn "Invalid choice" ;;
    esac
  done
}

menu_configure() {
  while true; do
    show_banner
    echo -e "${C_BOLD}Configure${C_RESET}"
    echo "  1) Configure MQTT (hostname, IP, device credentials)"
    echo "  2) Configure Database"
    echo "  3) Configure Backend"
    echo "  4) Configure Frontend"
    echo "  5) Show current configuration"
    echo "  0) Back"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) mqtt_configure; pause_enter ;;
      2) postgres_configure; pause_enter ;;
      3) backend_configure; pause_enter ;;
      4) frontend_configure; pause_enter ;;
      5) show_config ;;
      0) return ;;
      *) log_warn "Invalid choice" ;;
    esac
  done
}

main_menu() {
  while true; do
    show_banner
    echo -e "${C_BOLD}Main Menu${C_RESET}"
    echo "  1) Install"
    echo "  2) Uninstall"
    echo "  3) Configure"
    echo "  4) Status (what is running?)"
    echo "  5) Verify MQTT TLS (ESP8266 check)"
    echo "  6) Regenerate MQTT certificates"
    echo "  7) View logs"
    echo "  8) Install system dependencies only"
    echo "  0) Exit"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) menu_install ;;
      2) menu_uninstall ;;
      3) menu_configure ;;
      4) show_status ;;
      5) bash "$SCRIPTS_DIR/verify-mqtt-tls.sh"; pause_enter ;;
      6) mqtt_regenerate_certs; pause_enter ;;
      7) view_logs ;;
      8)
        if [[ "${FLM_DEPLOY_MODE:-standalone}" == docker ]]; then deps_install_docker
        else deps_install_standalone; fi
        pause_enter
        ;;
      0) echo "Goodbye."; exit 0 ;;
      *) log_warn "Invalid choice — enter 0-8" ; sleep 1 ;;
    esac
  done
}

# Non-interactive shortcuts: ./flm-server.sh install|install-docker|uninstall|status|...
if [[ "${1:-}" == "install" || "${1:-}" == "install-standalone" ]]; then install_all_standalone; exit $?; fi
if [[ "${1:-}" == "install-docker" ]]; then install_all_docker; exit $?; fi
if [[ "${1:-}" == "install-deps" ]]; then
  ensure_env_file
  if [[ "${FLM_DEPLOY_MODE:-standalone}" == docker ]]; then deps_install_docker
  else deps_install_standalone; fi
  exit $?
fi
if [[ "${1:-}" == "uninstall" ]]; then uninstall_all; exit $?; fi
if [[ "${1:-}" == "status" ]]; then show_status; exit 0; fi
if [[ "${1:-}" == "mqtt-install" ]]; then mqtt_install; exit $?; fi
if [[ "${1:-}" == "mqtt-uninstall" ]]; then mqtt_uninstall; exit $?; fi

main_menu
