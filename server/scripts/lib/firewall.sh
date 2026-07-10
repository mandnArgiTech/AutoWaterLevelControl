#!/usr/bin/env bash
# Host firewall (ufw) — open FLM ports on production VPS
set -euo pipefail

_fw_lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$_fw_lib_dir/common.sh"

_os_has_apt() { command -v apt-get >/dev/null 2>&1; }

firewall_configure_standalone() {
  if ! command -v ufw >/dev/null 2>&1; then
    if _os_has_apt 2>/dev/null || command -v apt-get >/dev/null 2>&1; then
      log_info "Installing ufw"
      if [[ "$(id -u)" -eq 0 ]]; then apt-get install -y -qq ufw
      elif command -v sudo >/dev/null 2>&1; then sudo apt-get install -y -qq ufw
      else return 0; fi
    else
      log_warn "ufw not found — open ports 80, 8080, 1883, 8883 in your cloud provider firewall"
      return 0
    fi
  fi

  log_step 1 1 "Configuring ufw (SSH + HTTP + FLM services)"
  local ufw_cmd=(ufw)
  [[ "$(id -u)" -ne 0 ]] && command -v sudo >/dev/null 2>&1 && ufw_cmd=(sudo ufw)

  "${ufw_cmd[@]}" --force reset >/dev/null 2>&1 || true
  "${ufw_cmd[@]}" default deny incoming >/dev/null
  "${ufw_cmd[@]}" default allow outgoing >/dev/null
  "${ufw_cmd[@]}" allow 22/tcp comment 'SSH' >/dev/null
  "${ufw_cmd[@]}" allow 80/tcp comment 'FLM Web UI' >/dev/null
  "${ufw_cmd[@]}" allow 443/tcp comment 'HTTPS' >/dev/null
  "${ufw_cmd[@]}" allow "${API_PORT:-8080}"/tcp comment 'FLM API' >/dev/null
  "${ufw_cmd[@]}" allow 1883/tcp comment 'MQTT' >/dev/null
  "${ufw_cmd[@]}" allow 8883/tcp comment 'MQTT TLS' >/dev/null
  if [[ "${FRONTEND_PORT:-3000}" != "80" && "${FRONTEND_PORT:-3000}" != "443" ]]; then
    "${ufw_cmd[@]}" allow "${FRONTEND_PORT:-3000}"/tcp comment 'FLM UI alt' >/dev/null
  fi
  "${ufw_cmd[@]}" --force enable >/dev/null
  log_ok "ufw enabled — ports 22, 80, ${API_PORT:-8080}, 1883, 8883 open"
}
