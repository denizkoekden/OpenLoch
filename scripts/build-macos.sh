#!/usr/bin/env bash
set -euo pipefail
openloch_root="$(cd "$(dirname "$0")/.." && pwd)"
openloch_qt_prefix="${OPENLOCH_QT_PREFIX:-$(qmake6 -query QT_INSTALL_PREFIX)}"
cmake -S "$openloch_root" -B "$openloch_root/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$openloch_qt_prefix"
cmake --build "$openloch_root/build" --parallel 4
ctest --test-dir "$openloch_root/build" --output-on-failure
mkdir -p "$openloch_root/dist"
if [ -e "$openloch_root/dist/OpenLoch.app" ]; then
  printf '%s\n' 'dist/OpenLoch.app already exists; choose a new output directory or move that build first.' >&2
  exit 1
fi
cp -R "$openloch_root/build/OpenLoch.app" "$openloch_root/dist/OpenLoch.app"
# Widgets needs Cocoa, the native style, the offscreen renderer used in checks and the JPEG and GIF readers.
# Avoid copying unrelated QML, PDF-image and virtual keyboard plugins.
openloch_plugins="${OPENLOCH_QT_PLUGINS:-$(qmake6 -query QT_INSTALL_PLUGINS)}"
openloch_bundle="$openloch_root/dist/OpenLoch.app"
mkdir -p "$openloch_bundle/Contents/PlugIns/platforms" "$openloch_bundle/Contents/PlugIns/styles" "$openloch_bundle/Contents/PlugIns/imageformats" "$openloch_bundle/Contents/Resources"
cp "$openloch_plugins/platforms/libqcocoa.dylib" "$openloch_bundle/Contents/PlugIns/platforms/"
cp "$openloch_plugins/platforms/libqoffscreen.dylib" "$openloch_bundle/Contents/PlugIns/platforms/"
cp "$openloch_plugins/styles/libqmacstyle.dylib" "$openloch_bundle/Contents/PlugIns/styles/"
# JPEG for "Als BMP/JPG exportieren" and JPEG bitmap fills, GIF for GIF bitmap fills; BMP and PNG are built into Qt.
cp "$openloch_plugins/imageformats/libqjpeg.dylib" "$openloch_bundle/Contents/PlugIns/imageformats/"
cp "$openloch_plugins/imageformats/libqgif.dylib" "$openloch_bundle/Contents/PlugIns/imageformats/"
"$openloch_qt_prefix/bin/macdeployqt" "$openloch_bundle" -always-overwrite -no-plugins -no-codesign \
  "-executable=$openloch_bundle/Contents/PlugIns/platforms/libqcocoa.dylib" \
  "-executable=$openloch_bundle/Contents/PlugIns/platforms/libqoffscreen.dylib" \
  "-executable=$openloch_bundle/Contents/PlugIns/styles/libqmacstyle.dylib" \
  "-executable=$openloch_bundle/Contents/PlugIns/imageformats/libqjpeg.dylib" \
  "-executable=$openloch_bundle/Contents/PlugIns/imageformats/libqgif.dylib"
cat > "$openloch_bundle/Contents/Resources/qt.conf" <<'QTCONF'
[Paths]
Plugins = PlugIns
QTCONF
codesign --force --deep --sign - "$openloch_root/dist/OpenLoch.app"
codesign --verify --deep --strict "$openloch_root/dist/OpenLoch.app"
python3 "$openloch_root/tools/check_macos_bundle.py" "$openloch_bundle"
bash "$openloch_root/scripts/package-macos.sh" "$openloch_bundle" "$openloch_root/dist/OpenLoch-macOS.dmg"
