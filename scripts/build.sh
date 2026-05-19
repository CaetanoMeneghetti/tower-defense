#!/usr/bin/env bash
set -euo pipefail

# Resolve o diretório raiz do projeto (um nível acima de scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

cmake --preset default-config -G "Unix Makefiles"
cmake --build --preset default-build -j"$(nproc)"
