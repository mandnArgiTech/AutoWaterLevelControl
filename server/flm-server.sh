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
# shellcheck source=scripts/lib/remote.sh
source "$SERVER_ROOT/scripts/lib/remote.sh"
# shellcheck source=scripts/lib/update.sh
source "$SERVER_ROOT/scripts/lib/update.sh"

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
  # First-time install may create the Postgres cluster; updates must not.
  export FLM_ALLOW_DB_INIT=1

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
  ensure_env_file
  if [[ "${FLM_AUTO_YES:-}" != "1" && "${FLM_UNINSTALL_FULL:-}" != "1" ]]; then
    echo -e "${C_YELLOW}This stops all FLM services and can remove data, packages, and the install folder.${C_RESET}"
    read -r -p "Continue? [y/N] " ans
    [[ "${ans,,}" == "y" ]] || { log_info "Cancelled"; return 0; }
  fi

  # Full wipe only when FLM_UNINSTALL_FULL=1 (remote-uninstall sets this).
  # FLM_AUTO_YES alone still purges generated data but keeps packages + install tree
  # so a local non-interactive uninstall cannot delete your git checkout.
  if [[ "${FLM_UNINSTALL_FULL:-}" == "1" ]]; then
    export FLM_UNINSTALL_PURGE=1
    export FLM_UNINSTALL_REMOVE_PACKAGES="${FLM_UNINSTALL_REMOVE_PACKAGES:-1}"
    export FLM_UNINSTALL_REMOVE_TREE="${FLM_UNINSTALL_REMOVE_TREE:-1}"
    log_info "Full uninstall — purge data, apt-remove Mosquitto+PostgreSQL, delete install tree"
  elif [[ "${FLM_AUTO_YES:-}" == "1" ]]; then
    export FLM_UNINSTALL_PURGE=1
    export FLM_UNINSTALL_REMOVE_PACKAGES="${FLM_UNINSTALL_REMOVE_PACKAGES:-0}"
    export FLM_UNINSTALL_REMOVE_TREE="${FLM_UNINSTALL_REMOVE_TREE:-0}"
    log_info "FLM_AUTO_YES=1 — purging generated data (set FLM_UNINSTALL_FULL=1 for packages + folder wipe)"
  else
    if [[ -z "${FLM_UNINSTALL_PURGE:-}" ]]; then
      if flm_confirm_or_auto "Remove generated data (certs, DB cluster, dynsec, nginx conf)?"; then
        export FLM_UNINSTALL_PURGE=1
      else
        export FLM_UNINSTALL_PURGE=0
      fi
    fi
    if [[ -z "${FLM_UNINSTALL_REMOVE_PACKAGES:-}" ]]; then
      if flm_confirm_or_auto "apt-purge Mosquitto + PostgreSQL (removes /etc/mosquitto and system PG data)?"; then
        export FLM_UNINSTALL_REMOVE_PACKAGES=1
      else
        export FLM_UNINSTALL_REMOVE_PACKAGES=0
      fi
    fi
    if [[ -z "${FLM_UNINSTALL_REMOVE_TREE:-}" ]]; then
      if flm_confirm_or_auto "Delete install directory entirely (${SERVER_ROOT})?"; then
        export FLM_UNINSTALL_REMOVE_TREE=1
      else
        export FLM_UNINSTALL_REMOVE_TREE=0
      fi
    fi
  fi

  local total_steps=5
  flm_should_remove_packages && total_steps=$((total_steps + 1))
  flm_should_remove_tree && total_steps=$((total_steps + 1))
  local step=1

  log_step $step $total_steps "Stopping frontend (nginx)"; step=$((step + 1))
  frontend_uninstall
  log_step $step $total_steps "Stopping backend (Java API)"; step=$((step + 1))
  backend_uninstall
  log_step $step $total_steps "Stopping MQTT (Mosquitto)"; step=$((step + 1))
  mqtt_uninstall
  log_step $step $total_steps "Stopping PostgreSQL"; step=$((step + 1))
  postgres_uninstall
  log_step $step $total_steps "Clearing PID files + verifying ports"; step=$((step + 1))
  rm -f "$RUN_DIR"/*.pid 2>/dev/null || true
  uninstall_verify_ports

  if flm_should_remove_packages; then
    log_step $step $total_steps "Purging Mosquitto + PostgreSQL packages"; step=$((step + 1))
    deps_purge_standalone_packages
  fi

  local purge_msg="generated data kept"
  [[ "${FLM_UNINSTALL_PURGE:-}" == "1" ]] && purge_msg="generated data purged"
  local pkg_msg="packages kept"
  flm_should_remove_packages && pkg_msg="Mosquitto+PostgreSQL apt-purged"
  local tree_msg="install tree kept (${SERVER_ROOT})"
  local remove_tree=0
  if flm_should_remove_tree; then
    tree_msg="install tree removed (${SERVER_ROOT})"
    remove_tree=1
  fi

  log_summary_box "Full Platform Uninstalled" \
    "All FLM services stopped." \
    "Data: ${purge_msg}" \
    "Packages: ${pkg_msg}" \
    "Tree: ${tree_msg}" \
    "Reinstall: ./flm-server.sh remote-install  (or local install)"

  # Delete last so logging/summary still works; scripts are already in memory
  if [[ "$remove_tree" -eq 1 ]]; then
    log_warn "Removing install directory: ${SERVER_ROOT}"
    local root_to_remove="$SERVER_ROOT"
    cd / || true
    if [[ "$(id -u)" -eq 0 ]]; then
      rm -rf "$root_to_remove"
    elif command -v sudo >/dev/null 2>&1; then
      sudo rm -rf "$root_to_remove"
    else
      rm -rf "$root_to_remove" || log_fail "Could not remove ${root_to_remove} — run as root"
    fi
    if [[ -d "$root_to_remove" ]]; then
      log_fail "Install directory still present: ${root_to_remove}"
    else
      log_ok "Install directory removed"
    fi
  fi
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
  echo "  5) Mosquitto log"
  echo "  6) PostgreSQL log"
  echo "  0) Back"
  read -r -p "Choice: " c
  case "$c" in
    1) less +G "$FLM_LOG_FILE" 2>/dev/null || tail -100 "$FLM_LOG_FILE" ;;
    2) less +G "$LOG_DIR/backend.log" 2>/dev/null || tail -100 "$LOG_DIR/backend.log" 2>/dev/null || log_warn "No backend log" ;;
    3) less +G "$LOG_DIR/frontend.log" 2>/dev/null || tail -100 "$LOG_DIR/frontend.log" 2>/dev/null || log_warn "No frontend log" ;;
    4) less +G "$LOG_DIR/nginx-error.log" 2>/dev/null || tail -100 "$LOG_DIR/nginx-error.log" 2>/dev/null || log_warn "No nginx log" ;;
    5)
      if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
        less +G "$SERVER_ROOT/mosquitto/log/mosquitto.log" 2>/dev/null \
          || tail -100 "$SERVER_ROOT/mosquitto/log/mosquitto.log" 2>/dev/null \
          || log_warn "No Mosquitto log"
      else
        docker logs --tail 100 flm-mosquitto 2>/dev/null || log_warn "Mosquitto not running"
      fi
      ;;
    6)
      if [[ "${FLM_DEPLOY_MODE:-standalone}" == "standalone" ]]; then
        less +G "$LOG_DIR/postgres.log" 2>/dev/null \
          || tail -100 "$LOG_DIR/postgres.log" 2>/dev/null \
          || log_warn "No PostgreSQL log"
      else
        docker logs --tail 100 flm-postgres 2>/dev/null || log_warn "PostgreSQL not running"
      fi
      ;;
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
    echo "  7) Remote install to VPS (sync → /opt/flm + install)"
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
      7) remote_install; pause_enter ;;
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
    echo "  1) Configure MQTT (plain 1883 + TLS 8883, hostname, credentials)"
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

menu_remote() {
  while true; do
    load_remote_config 2>/dev/null || true
    show_banner
    echo -e "${C_BOLD}Remote VPS${C_RESET}"
    echo "  Host: ${FLM_REMOTE_HOST:-vivasvan-tech.in}  Path: ${FLM_REMOTE_PATH:-/opt/flm}"
    echo ""
    echo "  1) Configure remote connection (host, user, password, path)"
    echo "  2) Remote install — Standalone (sync + install at /opt/flm)"
    echo "  3) Remote install — Docker"
    echo "  4) Sync files only (no install)"
    echo "  5) Remote update (sync + rebuild — after bug fixes)"
    echo "  6) Remote status"
    echo "  7) Remote uninstall"
    echo "  0) Back"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) remote_configure; pause_enter ;;
      2) remote_install standalone; pause_enter ;;
      3) remote_install docker; pause_enter ;;
      4) remote_sync_files; pause_enter ;;
      5)
        echo "  a) Update ALL (backend + frontend)"
        echo "  b) Update backend only"
        echo "  c) Update frontend only"
        read -r -p "Choice [a/b/c]: " u
        case "${u,,}" in
          b) remote_update backend; pause_enter ;;
          c) remote_update frontend; pause_enter ;;
          *) remote_update all; pause_enter ;;
        esac
        ;;
      6) remote_status; pause_enter ;;
      7) remote_uninstall; pause_enter ;;
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
    echo "  5) Verify MQTT (plain 1883 + TLS 8883)"
    echo "  6) Regenerate MQTT certificates"
    echo "  7) View logs"
    echo "  8) Install system dependencies only"
    echo "  9) Remote VPS (install to /opt/flm over SSH)"
    echo "  0) Exit"
    echo ""
    read -r -p "Choice: " c
    case "$c" in
      1) menu_install ;;
      2) menu_uninstall ;;
      3) menu_configure ;;
      4) show_status ;;
      5)
        bash "$SCRIPTS_DIR/verify-mqtt-plain.sh" || true
        echo ""
        bash "$SCRIPTS_DIR/verify-mqtt-tls.sh" || true
        pause_enter
        ;;
      6) mqtt_regenerate_certs; pause_enter ;;
      7) view_logs ;;
      8)
        if [[ "${FLM_DEPLOY_MODE:-standalone}" == docker ]]; then deps_install_docker
        else deps_install_standalone; fi
        pause_enter
        ;;
      9) menu_remote ;;
      0) echo "Goodbye."; exit 0 ;;
      *) log_warn "Invalid choice — enter 0-9" ; sleep 1 ;;
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
if [[ "${1:-}" == "remote-install" ]]; then remote_install standalone; exit $?; fi
if [[ "${1:-}" == "remote-install-docker" ]]; then remote_install docker; exit $?; fi
if [[ "${1:-}" == "update" ]]; then update_platform "${2:-all}"; exit $?; fi
if [[ "${1:-}" == "update-backend" ]]; then update_platform backend; exit $?; fi
if [[ "${1:-}" == "update-frontend" ]]; then update_platform frontend; exit $?; fi
if [[ "${1:-}" == "remote-sync" ]]; then remote_sync_files; exit $?; fi
if [[ "${1:-}" == "remote-update" ]]; then remote_update "${2:-all}"; exit $?; fi
if [[ "${1:-}" == "remote-update-backend" ]]; then remote_update backend; exit $?; fi
if [[ "${1:-}" == "remote-update-frontend" ]]; then remote_update frontend; exit $?; fi
if [[ "${1:-}" == "remote-status" ]]; then remote_status; exit $?; fi
if [[ "${1:-}" == "remote-uninstall" ]]; then remote_uninstall; exit $?; fi
if [[ "${1:-}" == "remote-configure" ]]; then remote_configure; exit $?; fi

main_menu
