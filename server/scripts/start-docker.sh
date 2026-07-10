#!/usr/bin/env bash
# Wrapper — use ./flm-server.sh instead
exec "$(dirname "$0")/flm-server.sh" install "$@"
