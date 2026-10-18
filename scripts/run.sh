#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
deps_lib="$HOME/.local/share/monnett-qt-deps/usr/lib"
if [[ -d "$deps_lib" ]]; then
  export LD_LIBRARY_PATH="$deps_lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$project_dir/build/monnett-qt" "$@"
