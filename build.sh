#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
exec uvx --with requests --with intelhex --from platformio pio run -e cardputer "$@"
