#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
install_root="${MONNETT_INSTALL_PREFIX:-$HOME/.local}"
app_dir="$install_root/libexec/monnett-qt"
if [[ ! -x "$project_dir/build/monnett-qt" ]]; then
  printf '%s\n' 'Сначала выполните ./scripts/build.sh' >&2
  exit 1
fi
install -Dm755 "$project_dir/build/monnett-qt" "$app_dir/monnett-qt"
local_lib="$HOME/.local/share/monnett-qt-deps/usr/lib"
if [[ -d "$local_lib" ]]; then
  mkdir -p "$app_dir/lib"
  cp -a "$local_lib"/libQuotientQt6.so* "$local_lib"/libqt6keychain.so* "$local_lib"/libolm.so* "$app_dir/lib/"
fi
mkdir -p "$install_root/bin"
cat > "$install_root/bin/monnett-qt" <<'LAUNCHER'
#!/usr/bin/env bash
set -euo pipefail
prefix=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
app_dir="$prefix/libexec/monnett-qt"
if [[ -d "$app_dir/lib" ]]; then
  export LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
exec "$app_dir/monnett-qt" "$@"
LAUNCHER
chmod +x "$install_root/bin/monnett-qt"
install -Dm644 "$project_dir/packaging/social.monnett.MonnettQt.desktop" "$install_root/share/applications/social.monnett.MonnettQt.desktop"
install -Dm644 "$project_dir/resources/monnett.svg" "$install_root/share/icons/hicolor/scalable/apps/social.monnett.MonnettQt.svg"
install -Dm644 "$project_dir/LICENSE" "$app_dir/LICENSE"
mkdir -p "$app_dir/licenses"
cp -a "$project_dir/packaging/licenses/." "$app_dir/licenses/"
install -Dm644 "$project_dir/packaging/THIRD_PARTY.md" "$app_dir/THIRD_PARTY.md"
if command -v update-desktop-database >/dev/null; then
  update-desktop-database "$install_root/share/applications"
fi
printf 'Установлено: %s/bin/monnett-qt\n' "$install_root"
