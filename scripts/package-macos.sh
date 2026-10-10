#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 2 ]; then
  printf '%s\n' 'Usage: package-macos.sh /path/OpenLoch.app /path/OpenLoch-macOS.dmg' >&2
  exit 2
fi
openloch_app="$1"
openloch_image="$2"
if [ ! -d "$openloch_app/Contents/MacOS" ] || [ -e "$openloch_image" ]; then
  printf '%s\n' 'App bundle missing or output already exists.' >&2
  exit 1
fi
codesign --verify --deep --strict "$openloch_app"
openloch_staging="$(mktemp -d "${TMPDIR:-/tmp}/openloch-dmg.XXXXXX")"
trap 'rm -rf "$openloch_staging"' EXIT
ditto "$openloch_app" "$openloch_staging/OpenLoch.app"
ln -s /Applications "$openloch_staging/Applications"
printf '%s\n' 'OpenLoch installieren: OpenLoch.app auf den Programme-Ordner ziehen.' > "$openloch_staging/Installation.txt"
# hdiutil on build machines sometimes fails with "Resource busy"; a later try works.
for openloch_try in 1 2 3; do
  if hdiutil create -volname OpenLoch -srcfolder "$openloch_staging" -format UDZO -fs HFS+ "$openloch_image"; then
    break
  fi
  rm -f "$openloch_image"
  if [ "$openloch_try" -eq 3 ]; then
    exit 1
  fi
  sleep 10
done
hdiutil verify "$openloch_image"
