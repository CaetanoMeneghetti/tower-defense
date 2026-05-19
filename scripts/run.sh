#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BIN_DIR="$PROJECT_ROOT/bin/Linux"
EXE="$BIN_DIR/main"

if [[ ! -x "$EXE" ]]; then
    echo "main não encontrado em $BIN_DIR. Rode scripts/build.sh primeiro." >&2
    exit 1
fi

cd "$BIN_DIR"
./main
