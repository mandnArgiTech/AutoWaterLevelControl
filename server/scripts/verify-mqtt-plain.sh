#!/usr/bin/env bash
# Verify Mosquitto plain MQTT (no TLS) on port 1883
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$ROOT/.env" ]]; then set -a; # shellcheck disable=SC1091
  source "$ROOT/.env"; set +a; fi

HOST="${MQTT_PLAIN_TEST_HOST:-127.0.0.1}"
PORT="${MQTT_PLAIN_TEST_PORT:-1883}"
USER="${MQTT_USER_DEVICE:-devAdmin}"
PASS="${MQTT_PASS_DEVICE:-123456}"

echo "==> Plain MQTT connect/publish (${HOST}:${PORT} as ${USER})"
if ! command -v mosquitto_pub >/dev/null 2>&1; then
  echo "FAIL: mosquitto_pub not installed (apt install mosquitto-clients)" >&2
  exit 1
fi

if mosquitto_pub -h "$HOST" -p "$PORT" -u "$USER" -P "$PASS" \
    -t 'testtank/water/level' -m '{"ok":true,"tls":false}' -q 0; then
  echo "PASS: Plain MQTT publish OK"
else
  echo "FAIL: Plain MQTT publish failed" >&2
  echo "  Check: mosquitto listening on ${PORT}, credentials, Dynamic Security ACL" >&2
  echo "  If testing a remote host, also open TCP ${PORT} in the cloud firewall panel" >&2
  exit 1
fi

echo ""
echo "ESP8266 config (no TLS):"
cat <<EOF
  "mqtt": {
    "enabled": true,
    "server": "${MQTT_SERVER_CN:-vivasvan-tech.in}",
    "port": 1883,
    "username": "${USER}",
    "password": "${PASS}",
    "tls": false
  }
EOF
