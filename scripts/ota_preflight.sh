#!/usr/bin/env bash
# OTA pre-flight: ping device, check HTTP /api/info, hint on reverse-TCP/UFW,
# then optionally run PlatformIO espota upload.
#
# Usage:
#   scripts/ota_preflight.sh <device-ip> [upload|uploadfs]
# Examples:
#   scripts/ota_preflight.sh 192.168.68.68
#   scripts/ota_preflight.sh 192.168.68.68 upload
#   scripts/ota_preflight.sh 192.168.68.68 uploadfs
#
set -euo pipefail

DEVICE_IP="${1:-}"
ACTION="${2:-}"  # empty = check only; upload | uploadfs
ENV_NAME="${FLM_OTA_ENV:-sensor_only_d1mini_ota}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

fail() { echo "ERROR: $*" >&2; exit 1; }
ok()   { echo "OK: $*"; }
info() { echo "INFO: $*"; }

[[ -n "$DEVICE_IP" ]] || fail "usage: $0 <device-ip> [upload|uploadfs]"

# --- 1. Ping ---
info "Ping $DEVICE_IP ..."
if ! ping -c 2 -W 2 "$DEVICE_IP" >/dev/null 2>&1; then
  fail "device $DEVICE_IP does not respond to ping (wrong IP / offline / different subnet)"
fi
ok "ping"

# --- 2. HTTP /api/info ---
info "GET http://$DEVICE_IP/api/info ..."
INFO_JSON="$(curl -fsS -m 5 "http://${DEVICE_IP}/api/info" 2>/dev/null)" \
  || fail "HTTP /api/info failed — device may be reboot-looping or web UI not started"
ok "HTTP /api/info"
echo "     $INFO_JSON" | head -c 200
echo

# --- 3. Host LAN IP (same subnet hint) ---
HOST_IP=""
if command -v ip >/dev/null 2>&1; then
  HOST_IP="$(ip -4 route get "$DEVICE_IP" 2>/dev/null | awk '{for(i=1;i<=NF;i++) if($i=="src"){print $(i+1); exit}}' || true)"
fi
if [[ -z "$HOST_IP" ]]; then
  HOST_IP="$(hostname -I 2>/dev/null | awk '{print $1}' || true)"
fi
[[ -n "$HOST_IP" ]] || fail "could not determine host LAN IP"
ok "host LAN IP = $HOST_IP (espota must use --host_ip=$HOST_IP)"

# --- 4. Firewall / reverse-TCP hint ---
# ArduinoOTA: PC sends UDP invite → ESP ACKs → ESP opens TCP back to PC.
# UFW blocking inbound from the LAN is the usual "No response from device" cause.
if command -v ufw >/dev/null 2>&1; then
  UFW_STATUS="$(ufw status 2>/dev/null | head -1 || true)"
  info "ufw: $UFW_STATUS"
  if echo "$UFW_STATUS" | grep -qi "active"; then
    info "If OTA fails with 'No response from device', allow reverse TCP from the device LAN:"
    echo "     sudo ufw allow from ${DEVICE_IP%.*}.0/24 comment 'FLM ArduinoOTA'"
    echo "     sudo ufw reload"
  fi
else
  info "ufw not installed — if OTA fails, check firewalld/nftables for inbound TCP from $DEVICE_IP"
fi

# Optional: quick listen test (binds a random high port; ESP won't connect unless invited)
info "ArduinoOTA reverse path: ESP must TCP-connect back to $HOST_IP after UDP invite"
info "Pre-flight checks passed."

if [[ -z "$ACTION" ]]; then
  echo
  echo "Checks only. To upload:"
  echo "  $0 $DEVICE_IP upload      # firmware"
  echo "  $0 $DEVICE_IP uploadfs    # LittleFS"
  exit 0
fi

cd "$ROOT"
case "$ACTION" in
  upload)
    info "Uploading firmware via espota to $DEVICE_IP (env=$ENV_NAME) ..."
    exec pio run -e "$ENV_NAME" -t upload --upload-port "$DEVICE_IP"
    ;;
  uploadfs)
    info "Uploading LittleFS via espota to $DEVICE_IP (env=$ENV_NAME) ..."
    exec pio run -e "$ENV_NAME" -t uploadfs --upload-port "$DEVICE_IP"
    ;;
  *)
    fail "unknown action '$ACTION' (use upload or uploadfs)"
    ;;
esac
