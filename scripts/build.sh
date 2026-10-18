#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
args=()
local_deps="$HOME/.local/share/monnett-qt-deps/usr"
if [[ -d "$local_deps/lib/cmake/QuotientQt6" ]]; then
  args+=("-DCMAKE_PREFIX_PATH=$local_deps")
  export LD_LIBRARY_PATH="$local_deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
cmake -S "$project_dir" -B "$project_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "${args[@]}" "$@"
cmake --build "$project_dir/build" --parallel "${MONNETT_BUILD_JOBS:-2}"
ctest --test-dir "$project_dir/build" --output-on-failure
