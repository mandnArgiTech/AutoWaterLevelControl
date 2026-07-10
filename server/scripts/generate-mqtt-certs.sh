#!/usr/bin/env bash
# =============================================================================
# generate-mqtt-certs.sh — ONE script for Mosquitto TLS + ESP8266 compatibility
#
# Generates certificates verified against FluidLevelMonitor firmware:
#   - BearSSL TLS 1.2 only (MQTTManager::setSSLVersion BR_TLS12)
#   - MFLN 1024-byte buffers (FLM_TLS_RX_BUF / FLM_TLS_TX_BUF)
#   - tlsMode=ca    → upload esp8266_ca.pem to device POST /api/mqtt/ca
#   - tlsMode=fingerprint → use esp8266-fingerprint.txt (SHA-1, colon-separated)
#   - tlsMode=insecure  → encrypted only, no cert validation
#
# Usage:
#   ./scripts/generate-mqtt-certs.sh              # create if missing
#   ./scripts/generate-mqtt-certs.sh --force      # regenerate all
#   MQTT_SERVER_CN=vivasvan-tech.in \
#   MQTT_SAN_DNS=vivasvan-tech.in,localhost \
#   MQTT_SAN_IPS=31.97.235.233 \
#     ./scripts/generate-mqtt-certs.sh
#
# Requires: openssl (3.x recommended)
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "$ROOT/.env" ]]; then set -a; # shellcheck disable=SC1091
  source "$ROOT/.env"; set +a; fi
CERT_DIR="$ROOT/mosquitto/certs"
PASSWD_DIR="$ROOT/mosquitto/passwd"
OPENSSL_DIR="$ROOT/mosquitto/openssl"
DAYS="${MQTT_CERT_DAYS:-3650}"
FORCE=false

for arg in "$@"; do
  case "$arg" in
    --force|-f) FORCE=true ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *) echo "Unknown option: $arg" >&2; exit 1 ;;
  esac
done

# ── Deployment defaults (vivasvan-tech.in VPS) ───────────────────────────────
MQTT_SERVER_CN="${MQTT_SERVER_CN:-vivasvan-tech.in}"
MQTT_SAN_DNS="${MQTT_SAN_DNS:-${MQTT_SERVER_CN},localhost}"
MQTT_SAN_IPS="${MQTT_SAN_IPS:-31.97.235.233,127.0.0.1}"

# Mosquitto MQTT users (match docker-compose / application.yml)
MQTT_USER_BRIDGE="${MQTT_USER_BRIDGE:-flmServerAdmin}"
MQTT_PASS_BRIDGE="${MQTT_PASS_BRIDGE:-flmPass}"
MQTT_USER_VENDOR="${MQTT_USER_VENDOR:-vendor_demo}"
MQTT_PASS_VENDOR="${MQTT_PASS_VENDOR:-vendor_demo_secret}"
MQTT_USER_DEVICE="${MQTT_USER_DEVICE:-devAdmin}"
MQTT_PASS_DEVICE="${MQTT_PASS_DEVICE:-123456}"

log()  { printf '==> %s\n' "$*"; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

need_openssl() {
  command -v openssl >/dev/null 2>&1 || die "openssl not found — install openssl package"
}

write_server_cnf() {
  local cnf="$OPENSSL_DIR/server.cnf"
  local dns_list="$1"
  local ip_list="$2"

  {
    echo "[ req ]"
    echo "default_bits       = 2048"
    echo "default_md         = sha256"
    echo "prompt             = no"
    echo "distinguished_name = dn"
    echo "req_extensions     = v3_req"
    echo ""
    echo "[ dn ]"
    echo "CN = ${MQTT_SERVER_CN}"
    echo "O  = FluidLevelMonitor"
    echo "C  = IN"
    echo ""
    echo "[ v3_req ]"
    echo "basicConstraints = critical, CA:false"
    echo "keyUsage         = critical, digitalSignature, keyEncipherment"
    echo "extendedKeyUsage = serverAuth"
    echo "subjectAltName   = @alt_names"
    echo ""
    echo "[ alt_names ]"
    local i=1
    IFS=',' read -ra DNS_ARR <<< "$dns_list"
    for d in "${DNS_ARR[@]}"; do
      d="$(echo "$d" | xargs)"
      [[ -n "$d" ]] && echo "DNS.${i} = ${d}" && ((i++)) || true
    done
    i=1
    IFS=',' read -ra IP_ARR <<< "$ip_list"
    for ip in "${IP_ARR[@]}"; do
      ip="$(echo "$ip" | xargs)"
      [[ -n "$ip" ]] && echo "IP.${i} = ${ip}" && ((i++)) || true
    done
  } > "$cnf"
}

generate_ca() {
  log "Creating CA (RSA 2048, SHA-256) — ESP8266 trust anchor"
  openssl req -new -x509 -days "$DAYS" -nodes \
    -newkey rsa:2048 \
    -keyout "$CERT_DIR/ca.key" \
    -out "$CERT_DIR/ca.crt" \
    -config "$OPENSSL_DIR/ca.cnf"
  chmod 600 "$CERT_DIR/ca.key"
}

generate_server() {
  log "Creating server certificate (RSA 2048, SAN: ${MQTT_SAN_DNS} + ${MQTT_SAN_IPS})"
  write_server_cnf "$MQTT_SAN_DNS" "$MQTT_SAN_IPS"
  openssl req -new -nodes -newkey rsa:2048 \
    -keyout "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.csr" \
    -config "$OPENSSL_DIR/server.cnf"
  openssl x509 -req -days "$DAYS" \
    -in "$CERT_DIR/server.csr" \
    -CA "$CERT_DIR/ca.crt" -CAkey "$CERT_DIR/ca.key" -CAcreateserial \
    -out "$CERT_DIR/server.crt" \
    -extensions v3_req -extfile "$OPENSSL_DIR/server.cnf"
  rm -f "$CERT_DIR/server.csr"
  # server.key chmod applied in fix_mosquitto_cert_permissions()
}

mosquitto_passwd_bin() {
  if command -v mosquitto_passwd >/dev/null 2>&1; then
    echo mosquitto_passwd
  elif command -v docker >/dev/null 2>&1; then
    echo "docker"
  else
    die "mosquitto_passwd not found and docker unavailable — install mosquitto-clients or docker"
  fi
}

generate_passwd() {
  log "Creating Mosquitto password file"
  local passwd_file="$PASSWD_DIR/passwd"
  rm -f "$passwd_file"
  touch "$passwd_file"

  _mp() {
    local user="$1" pass="$2"
    local bin
    bin="$(mosquitto_passwd_bin)"
    if [[ "$bin" == "docker" ]]; then
      docker run --rm -v "$PASSWD_DIR:/mosquitto/passwd" eclipse-mosquitto:2 \
        mosquitto_passwd -b "/mosquitto/passwd/passwd" "$user" "$pass"
    else
      "$bin" -b "$passwd_file" "$user" "$pass"
    fi
  }

  _mp "$MQTT_USER_BRIDGE" "$MQTT_PASS_BRIDGE"
  _mp "$MQTT_USER_VENDOR" "$MQTT_PASS_VENDOR"
  _mp "$MQTT_USER_DEVICE" "$MQTT_PASS_DEVICE"
  fix_mosquitto_cert_permissions
}

# Host-native Mosquitto and Docker mounts must read certs (and legacy passwd if used)
fix_mosquitto_cert_permissions() {
  chmod 755 "$CERT_DIR" "$PASSWD_DIR"
  [[ -f "$PASSWD_DIR/passwd" ]] && chmod 644 "$PASSWD_DIR/passwd"
  chmod 644 "$CERT_DIR/ca.crt" "$CERT_DIR/server.crt" "$CERT_DIR/server.key" 2>/dev/null || true
  [[ -f "$CERT_DIR/esp8266_ca.pem" ]] && chmod 644 "$CERT_DIR/esp8266_ca.pem"
  # CA private key stays host-only (not mounted into container)
  [[ -f "$CERT_DIR/ca.key" ]] && chmod 600 "$CERT_DIR/ca.key"
}

# Format SHA-1 fingerprint exactly as ESP8266 WiFiClientSecure::setFingerprint() expects:
# 20 bytes as hex pairs, colon-separated (also accepts spaces in firmware).
format_esp8266_fingerprint() {
  openssl x509 -in "$CERT_DIR/server.crt" -noout -fingerprint -sha1 \
    | sed 's/sha1 Fingerprint=//' \
    | tr 'a-f' 'A-F'
}

verify_certs() {
  log "Verifying certificate chain"
  openssl verify -CAfile "$CERT_DIR/ca.crt" "$CERT_DIR/server.crt" >/dev/null \
    || die "server.crt is not signed by ca.crt"

  local ca_bits srv_bits
  ca_bits="$(openssl rsa -in "$CERT_DIR/ca.key" -text -noout 2>/dev/null | grep -oP 'Private-Key: \(\K[0-9]+')"
  srv_bits="$(openssl rsa -in "$CERT_DIR/server.key" -text -noout 2>/dev/null | grep -oP 'Private-Key: \(\K[0-9]+')"
  [[ "$ca_bits" == "2048" && "$srv_bits" == "2048" ]] \
    || die "Keys must be RSA 2048 for ESP8266 BearSSL (got CA=${ca_bits} server=${srv_bits})"

  # PEM readable by BearSSL X509List
  openssl x509 -in "$CERT_DIR/ca.crt" -noout -subject -dates >/dev/null \
    || die "ca.crt is not valid PEM"

  # Fingerprint must parse to exactly 20 bytes
  local fp
  fp="$(format_esp8266_fingerprint)"
  local count
  count="$(echo "$fp" | tr -cd ':' | wc -c)"
  [[ "$count" -eq 19 ]] || die "Fingerprint format invalid (expected 20 bytes): $fp"
}

write_esp8266_artifacts() {
  log "Writing ESP8266 device artifacts"
  # Firmware loads THIS file as trust anchor (tlsMode=ca) → MQTT_CA_FILE /mqtt_ca.pem
  cp "$CERT_DIR/ca.crt" "$CERT_DIR/esp8266_ca.pem"

  local fp
  fp="$(format_esp8266_fingerprint)"
  printf '%s\n' "$fp" > "$CERT_DIR/esp8266-fingerprint.txt"

  cat > "$CERT_DIR/esp8266-mqtt-config.json" <<EOF
{
  "_comment": "Paste into device data/config.json mqtt section",
  "enabled": true,
  "server": "${MQTT_SERVER_CN}",
  "port": 8883,
  "username": "${MQTT_USER_DEVICE}",
  "password": "${MQTT_PASS_DEVICE}",
  "tls": true,
  "tlsMode": "ca",
  "fingerprint": "${fp}",
  "_tlsMode_ca": "Upload esp8266_ca.pem via: curl -X POST http://DEVICE_IP/api/mqtt/ca -H 'Content-Type: text/plain' --data-binary @esp8266_ca.pem",
  "_tlsMode_fingerprint": "Set tlsMode to fingerprint and fingerprint to value in esp8266-fingerprint.txt",
  "_tlsMode_insecure": "Set tlsMode to insecure — encrypted only, no validation (dev only)",
  "_requirements": [
    "NTP must sync before TLS connect when tlsMode=ca (firmware waits automatically)",
    "Broker must be Mosquitto 2.x with OpenSSL (MFLN for 1KB BearSSL buffers)",
    "If connecting by IP with tlsMode=ca, that IP must be in MQTT_SAN_IPS when generating certs",
    "deviceTag must be registered in platform after first MQTT connect"
  ]
}
EOF
}

verify_tls_listener() {
  local host="${MQTT_TLS_TEST_HOST:-31.97.235.233}"
  local port="${MQTT_TLS_TEST_PORT:-8883}"
  if ! command -v timeout >/dev/null 2>&1; then return 0; fi
  if ! timeout 2 bash -c "echo >/dev/tcp/${host}/${port}" 2>/dev/null; then
    log "TLS listener not reachable on ${host}:${port} — skip live handshake test (start Mosquitto first)"
    return 0
  fi
  log "Testing TLS 1.2 handshake against ${host}:${port}"
  # Non-fatal: during install the broker may still be running with previous certs.
  # Full verification runs after Mosquitto restarts (verify-mqtt-tls.sh).
  if echo | timeout 5 openssl s_client -connect "${host}:${port}" \
    -CAfile "$CERT_DIR/ca.crt" -tls1_2 -servername "$MQTT_SERVER_CN" 2>/dev/null \
    | grep -q "Verify return code: 0 (ok)"; then
    log "TLS 1.2 handshake OK"
  else
    log "TLS handshake not OK yet (broker may still use old certs) — will re-check after Mosquitto restart"
  fi
  return 0
}

# ── Main ──────────────────────────────────────────────────────────────────────
need_openssl
mkdir -p "$CERT_DIR" "$PASSWD_DIR"

if [[ "$FORCE" == true ]]; then
  rm -f "$CERT_DIR/ca.key" "$CERT_DIR/ca.crt" "$CERT_DIR/server.key" "$CERT_DIR/server.crt" \
        "$CERT_DIR/ca.srl" "$CERT_DIR"/*.pem "$CERT_DIR"/*.txt "$CERT_DIR"/*.json
fi

if [[ ! -f "$CERT_DIR/ca.crt" ]]; then generate_ca; else log "CA exists (use --force to regenerate)"; fi
if [[ ! -f "$CERT_DIR/server.crt" ]]; then generate_server; else log "Server cert exists (use --force to regenerate)"; fi

FLM_DEPLOY_MODE="${FLM_DEPLOY_MODE:-standalone}"
if [[ "$FLM_DEPLOY_MODE" == "docker" ]]; then
  generate_passwd
else
  bash "$ROOT/scripts/generate-dynamic-security.sh"
fi
fix_mosquitto_cert_permissions
verify_certs
write_esp8266_artifacts
verify_tls_listener

FP="$(format_esp8266_fingerprint)"
cat <<EOF

================================================================================
MQTT TLS certificates ready — ESP8266 FluidLevelMonitor compatible
================================================================================
Files:
  CA (upload to device)     $CERT_DIR/esp8266_ca.pem
  Server cert (Mosquitto)   $CERT_DIR/server.crt
  Fingerprint (SHA-1)       $CERT_DIR/esp8266-fingerprint.txt
  Config snippet            $CERT_DIR/esp8266-mqtt-config.json

ESP8266 tlsMode=fingerprint:
  "fingerprint": "${FP}"

ESP8266 tlsMode=ca:
  curl -X POST http://<device-ip>/api/mqtt/ca \\
    -H 'Content-Type: text/plain' \\
    --data-binary @$CERT_DIR/esp8266_ca.pem

Mosquitto users:
  ${MQTT_USER_BRIDGE} / ${MQTT_PASS_BRIDGE}  (Java bridge)
  ${MQTT_USER_DEVICE} / ${MQTT_PASS_DEVICE}  (ESP8266 devices)

If device connects by IP with tlsMode=ca, regenerate with that IP in SAN:
  MQTT_SAN_IPS=<broker-ip> MQTT_SERVER_CN=<broker-ip> $0 --force

Next:
  Standalone:  ./flm-server.sh mqtt-install  (native Mosquitto + Dynamic Security)
  Docker:      ./scripts/start-docker.sh
================================================================================
EOF
