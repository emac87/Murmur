#!/bin/bash
# Builds a universal (Apple Silicon + Intel) Release of Murmur and packages it as
# dist/Murmur-<version>-mac.pkg. The installer puts:
#   Murmur.component -> /Library/Audio/Plug-Ins/Components   (AU, for Logic/GarageBand)
#   Murmur.vst3      -> /Library/Audio/Plug-Ins/VST3         (Ableton, Reaper, Bitwig...)
#   Murmur.app       -> /Applications                        (standalone)
# Each one is a separate, optional choice in the installer.
# Optional environment: SIGN_APP_ID / SIGN_INSTALLER_ID (Developer ID identities) to sign.
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$(pwd)"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

VERSION="$(sed -n 's/^project(Murmur VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
BUILD="$ROOT/build-universal"
WORK="$ROOT/build-pkg"
DIST="$ROOT/dist"
ART="$BUILD/Murmur_artefacts/Release"

echo "==> Building universal Release $VERSION"
cmake -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "$BUILD" --config Release -j "$(sysctl -n hw.ncpu)"

for b in "$ART/AU/Murmur.component" "$ART/VST3/Murmur.vst3" "$ART/Standalone/Murmur.app"; do
  [ -d "$b" ] || { echo "Missing build product: $b"; exit 1; }
done
lipo -info "$ART/AU/Murmur.component/Contents/MacOS/Murmur"

# Optional Developer ID signing of the bundles (otherwise they keep the ad-hoc signature)
if [ -n "${SIGN_APP_ID:-}" ]; then
  for b in "$ART/AU/Murmur.component" "$ART/VST3/Murmur.vst3" "$ART/Standalone/Murmur.app"; do
    codesign --force --deep --options runtime --timestamp --sign "$SIGN_APP_ID" "$b"
  done
fi

rm -rf "$WORK"; mkdir -p "$WORK/scripts" "$DIST"

# postinstall for the AU: make macOS re-scan Audio Units
cat > "$WORK/scripts/postinstall" <<'EOS'
#!/bin/bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOS
chmod +x "$WORK/scripts/postinstall"

make_component() { # name bundle-path install-location [scripts]
  local name="$1" bundle="$2" loc="$3" scripts="${4:-}"
  local root="$WORK/root-$name"
  mkdir -p "$root"
  cp -R "$bundle" "$root/"
  local args=(--root "$root" --identifier "com.murmuraudio.murmur.$name" --version "$VERSION"
              --install-location "$loc")
  # Keep each bundle at its install location even if another copy exists elsewhere.
  # Best effort only: pkgbuild doesn't list every bundle type, so never fail on this.
  if pkgbuild --analyze --root "$root" "$WORK/$name.plist" >/dev/null 2>&1 \
     && /usr/libexec/PlistBuddy -c "Print :0" "$WORK/$name.plist" >/dev/null 2>&1; then
    /usr/libexec/PlistBuddy -c "Delete :0:BundleIsRelocatable" "$WORK/$name.plist" >/dev/null 2>&1 || true
    if /usr/libexec/PlistBuddy -c "Add :0:BundleIsRelocatable bool false" "$WORK/$name.plist" >/dev/null 2>&1; then
      args+=(--component-plist "$WORK/$name.plist")
    fi
  fi
  [ -n "$scripts" ] && args+=(--scripts "$scripts")
  pkgbuild "${args[@]}" "$WORK/Murmur-$name.pkg"
}

echo "==> Packaging"
make_component au   "$ART/AU/Murmur.component"     "/Library/Audio/Plug-Ins/Components" "$WORK/scripts"
make_component vst3 "$ART/VST3/Murmur.vst3"         "/Library/Audio/Plug-Ins/VST3"
make_component app  "$ART/Standalone/Murmur.app"    "/Applications"

cat > "$WORK/welcome.html" <<EOS
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>Murmur $VERSION</h2>
<p>Live audio to MIDI for voice, guitar and bass.</p>
<p>This installs the <b>Audio Unit</b> (Logic Pro, GarageBand), the <b>VST3</b>
(Ableton Live, Reaper, Bitwig, Cubase) and the <b>standalone app</b>.
Use <i>Customize</i> on the next screens to skip any of them.</p>
<p>In Logic, Murmur sends its notes through the <b>Murmur Out</b> MIDI port.
See LOGIC-SETUP.md for the two-track setup.</p>
</body></html>
EOS

cat > "$WORK/distribution.xml" <<EOS
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Murmur $VERSION</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
  <os-version min="12.0"/>
  <choices-outline>
    <line choice="au"/><line choice="vst3"/><line choice="app"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (Logic Pro, GarageBand)" description="Installs Murmur.component into /Library/Audio/Plug-Ins/Components.">
    <pkg-ref id="com.murmuraudio.murmur.au"/>
  </choice>
  <choice id="vst3" title="VST3 (Ableton, Reaper, Bitwig, Cubase)" description="Installs Murmur.vst3 into /Library/Audio/Plug-Ins/VST3.">
    <pkg-ref id="com.murmuraudio.murmur.vst3"/>
  </choice>
  <choice id="app" title="Standalone app" description="Installs Murmur.app into /Applications.">
    <pkg-ref id="com.murmuraudio.murmur.app"/>
  </choice>
  <pkg-ref id="com.murmuraudio.murmur.au"   version="$VERSION">Murmur-au.pkg</pkg-ref>
  <pkg-ref id="com.murmuraudio.murmur.vst3" version="$VERSION">Murmur-vst3.pkg</pkg-ref>
  <pkg-ref id="com.murmuraudio.murmur.app"  version="$VERSION">Murmur-app.pkg</pkg-ref>
</installer-gui-script>
EOS

OUT="$DIST/Murmur-$VERSION-mac.pkg"
args=(--distribution "$WORK/distribution.xml" --package-path "$WORK" --resources "$WORK")
[ -n "${SIGN_INSTALLER_ID:-}" ] && args+=(--sign "$SIGN_INSTALLER_ID")
productbuild "${args[@]}" "$OUT"

echo
echo "==> Installer ready: $OUT"
pkgutil --check-signature "$OUT" 2>/dev/null | head -2 || true
