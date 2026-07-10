#!/usr/bin/env bash
# Backward-compatible wrapper — use generate-mqtt-certs.sh
exec "$(dirname "$0")/generate-mqtt-certs.sh" "$@"
