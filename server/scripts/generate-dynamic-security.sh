#!/usr/bin/env bash
# Generate mosquitto/config/dynamic-security.json from server/.env
# Mosquitto 2.0.x requires mosquitto_ctrl for properly hashed passwords.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$ROOT/.env" ]]; then set -a; # shellcheck disable=SC1091
  source "$ROOT/.env"; set +a; fi

OUT="$ROOT/mosquitto/config/dynamic-security.json"
BOOT_PORT="${MQTT_DYNSEC_BOOT_PORT:-11883}"
WORKDIR="$ROOT/mosquitto/.dynsec-bootstrap"
TMPDIR="$WORKDIR"
mkdir -p "$WORKDIR"
chmod 755 "$WORKDIR"
trap 'rm -rf "$WORKDIR"' EXIT

MQTT_DYNSEC_ADMIN="${MQTT_DYNSEC_ADMIN:-flmDynsecAdmin}"
MQTT_DYNSEC_PASS="${MQTT_DYNSEC_PASS:-flmDynsecPass}"
MQTT_USER_BRIDGE="${MQTT_USER_BRIDGE:-flmServerAdmin}"
MQTT_PASS_BRIDGE="${MQTT_PASS_BRIDGE:-flmPass}"
MQTT_USER_VENDOR="${MQTT_USER_VENDOR:-vendor_demo}"
MQTT_PASS_VENDOR="${MQTT_PASS_VENDOR:-vendor_demo_secret}"
MQTT_USER_DEVICE="${MQTT_USER_DEVICE:-devAdmin}"
MQTT_PASS_DEVICE="${MQTT_PASS_DEVICE:-123456}"

log() { printf '==> %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

find_dynsec_plugin() {
  local p
  for p in \
    /usr/lib/x86_64-linux-gnu/mosquitto_dynamic_security.so \
    /usr/lib/aarch64-linux-gnu/mosquitto_dynamic_security.so \
    /usr/lib/mosquitto_dynamic_security.so; do
    [[ -f "$p" ]] && echo "$p" && return 0
  done
  return 1
}

command -v mosquitto_ctrl >/dev/null 2>&1 || die "mosquitto_ctrl not found — install mosquitto-clients"
command -v mosquitto >/dev/null 2>&1 || die "mosquitto not found — install mosquitto package"
PLUGIN="$(find_dynsec_plugin)" || die "mosquitto_dynamic_security.so not found"

log "Building Dynamic Security config via mosquitto_ctrl"
log "  Admin: ${MQTT_DYNSEC_ADMIN} (platform MQTT control API)"
log "  Bridge: ${MQTT_USER_BRIDGE} (Java ingest)"
log "  Device: ${MQTT_USER_DEVICE} (ESP8266)"

printf '%s\n%s\n' "$MQTT_DYNSEC_PASS" "$MQTT_DYNSEC_PASS" \
  | mosquitto_ctrl dynsec init "$TMPDIR/dynamic-security.json" "$MQTT_DYNSEC_ADMIN" >/dev/null

if id mosquitto >/dev/null 2>&1; then
  chown -R mosquitto:mosquitto "$WORKDIR" 2>/dev/null || true
fi
chmod 700 "$WORKDIR"
chmod 600 "$TMPDIR/dynamic-security.json"

cat > "$TMPDIR/mosquitto-bootstrap.conf" <<EOF
listener ${BOOT_PORT} 127.0.0.1
allow_anonymous false
plugin ${PLUGIN}
plugin_opt_config_file ${TMPDIR}/dynamic-security.json
EOF

pkill -f "${WORKDIR}/mosquitto-bootstrap.conf" 2>/dev/null || true
sleep 1

if id mosquitto >/dev/null 2>&1; then
  chown -R mosquitto:mosquitto "$WORKDIR" 2>/dev/null || true
fi

mosquitto -c "$TMPDIR/mosquitto-bootstrap.conf" &
BPID=$!
cleanup_broker() { kill "$BPID" 2>/dev/null || true; wait "$BPID" 2>/dev/null || true; }
trap 'cleanup_broker; rm -rf "$TMPDIR"' EXIT

for _ in $(seq 1 30); do
  if timeout 1 bash -c "echo >/dev/tcp/127.0.0.1/${BOOT_PORT}" 2>/dev/null; then
    break
  fi
  sleep 0.2
done

ctrl() {
  mosquitto_ctrl -h 127.0.0.1 -p "$BOOT_PORT" \
    -u "$MQTT_DYNSEC_ADMIN" -P "$MQTT_DYNSEC_PASS" "$@" 2>/dev/null
}

role_acl() {
  ctrl dynsec addRoleACL "$1" "$2" "$3" allow
}

ensure_role() {
  local role="$1"
  shift
  ctrl dynsec createRole "$role" 2>/dev/null || true
  while [[ $# -ge 2 ]]; do
    role_acl "$role" "$1" "$2"
    shift 2
  done
}

ensure_client() {
  local user="$1" pass="$2" role="$3"
  if ! ctrl dynsec createClient "$user" -p "$pass"; then
    ctrl dynsec setClientPassword "$user" "$pass" >/dev/null
  fi
  ctrl dynsec addClientRole "$user" "$role" || true
}

ensure_role bridge \
  publishClientSend '#' \
  publishClientReceive '#' \
  subscribePattern '#'

ensure_role vendor_demo \
  publishClientReceive '+/water/#' \
  publishClientSend '+/water/command' \
  publishClientReceive '+/motor/#' \
  publishClientSend '+/motor/command' \
  subscribePattern '+/water/#' \
  subscribePattern '+/motor/#' \
  subscribePattern '+/water/command' \
  subscribePattern '+/motor/command'

ensure_role device_dev \
  publishClientSend '+/water/level' \
  publishClientSend '+/water/status' \
  publishClientReceive '+/water/command' \
  publishClientSend '+/motor/status' \
  publishClientReceive '+/motor/command' \
  subscribePattern '+/water/command' \
  subscribePattern '+/motor/command'

ensure_client "$MQTT_USER_BRIDGE" "$MQTT_PASS_BRIDGE" bridge
ensure_client "$MQTT_USER_VENDOR" "$MQTT_PASS_VENDOR" vendor_demo
ensure_client "$MQTT_USER_DEVICE" "$MQTT_PASS_DEVICE" device_dev

if ! mosquitto_sub -h 127.0.0.1 -p "$BOOT_PORT" \
    -u "$MQTT_USER_BRIDGE" -P "$MQTT_PASS_BRIDGE" \
    -t '$SYS/broker/version' -C 1 -W 5 >/dev/null 2>&1; then
  die "Bootstrap broker auth check failed for ${MQTT_USER_BRIDGE}"
fi

cleanup_broker
trap 'rm -rf "$TMPDIR"' EXIT

mkdir -p "$(dirname "$OUT")"
cp "$TMPDIR/dynamic-security.json" "$OUT"
chmod 600 "$OUT"
rm -f "$ROOT/mosquitto/data/mosquitto.db"

if id mosquitto >/dev/null 2>&1; then
  chown mosquitto:mosquitto "$OUT" 2>/dev/null || true
fi

log "Dynamic Security config ready: $OUT"
