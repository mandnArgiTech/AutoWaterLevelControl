#!/usr/bin/env bash
# Resolve docker compose command (v2 plugin or standalone binary)
if docker compose version >/dev/null 2>&1; then
  echo "docker compose"
elif command -v docker-compose >/dev/null 2>&1; then
  echo "docker-compose"
else
  echo "ERROR: docker compose not found" >&2
  exit 1
fi
