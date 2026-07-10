#!/usr/bin/env bash
# Verify Mosquitto TLS is compatible with ESP8266 BearSSL (run after broker is up)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CERT_DIR="$ROOT/mosquitto/certs"
HOST="${MQTT_TLS_TEST_HOST:-31.97.235.233}"
PORT="${MQTT_TLS_TEST_PORT:-8883}"
CN="${MQTT_SERVER_CN:-vivasvan-tech.in}"

[[ -f "$CERT_DIR/ca.crt" ]] || { echo "Run ./scripts/generate-mqtt-certs.sh first" >&2; exit 1; }

echo "==> TLS 1.2 + CA verify (tlsMode=ca)"
echo | openssl s_client -connect "${HOST}:${PORT}" \
  -CAfile "$CERT_DIR/ca.crt" -tls1_2 -servername "$CN" 2>&1 \
  | tee /tmp/flm-tls-verify.log | grep -E "Protocol|Cipher|Verify return code|subject=|issuer="

grep -q "Verify return code: 0 (ok)" /tmp/flm-tls-verify.log \
  || { echo "FAIL: CA verification failed" >&2; exit 1; }

grep -q "Protocol  : TLSv1.2" /tmp/flm-tls-verify.log \
  || { echo "FAIL: TLS 1.2 not negotiated (firmware requires TLS 1.2)" >&2; exit 1; }

echo ""
echo "==> Fingerprint for tlsMode=fingerprint"
cat "$CERT_DIR/esp8266-fingerprint.txt"

echo ""
echo "==> MQTT publish test (devAdmin user)"
if command -v mosquitto_pub >/dev/null 2>&1; then
  mosquitto_pub -h "$HOST" -p "$PORT" \
    --cafile "$CERT_DIR/ca.crt" --tls-version tlsv1.2 \
    -u devAdmin -P 123456 \
    -t 'test/flm/verify' -m '{"ok":true}' -q 1 \
    && echo "MQTT TLS publish OK" \
    || echo "WARN: MQTT publish failed (check Mosquitto users/ACL)"
else
  echo "mosquitto_pub not installed — skip MQTT publish test"
fi

echo ""
echo "PASS: TLS configuration is compatible with ESP8266 FluidLevelMonitor firmware"
