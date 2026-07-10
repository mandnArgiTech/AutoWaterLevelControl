#!/usr/bin/env bash
# Wrapper — default production install: ./flm-server.sh install
exec "$(dirname "$0")/../flm-server.sh" install "$@"
