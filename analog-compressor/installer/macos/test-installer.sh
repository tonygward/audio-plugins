#!/bin/bash
# End-to-end test of the Songsmith Analog Compressor macOS installer. CI runs it on macOS.
#
#   installer/macos/test-installer.sh                  # build the .pkg, then test it
#   installer/macos/test-installer.sh --pkg <file>     # test an existing .pkg
#
# WARNING: like eq/installer/macos/test-installer.sh, this installs for real, system-wide, and
# uninstalls at the end. It refuses to run outside CI unless you pass
# --i-know-this-installs-system-wide.
#
# On top of the EQ test, it seeds a Songsmith EQ install and Songsmith EQ presets next to this
# plug-in's own presets, and checks that install, upgrade and uninstall leave them untouched.
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
pkg=""
allow=0
while [ $# -gt 0 ]; do
    case "$1" in
        --pkg) pkg="$2"; shift 2 ;;
        --i-know-this-installs-system-wide) allow=1; shift ;;
        -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ "${CI:-}" != "true" ] && [ -z "${GITHUB_ACTIONS:-}" ] && [ "$allow" -ne 1 ]; then
    echo "Refusing to run: this test installs Songsmith Analog Compressor system-wide." >&2
    echo "It's meant for CI. To run it on this Mac anyway, pass --i-know-this-installs-system-wide." >&2
    exit 2
fi

if [ -z "$pkg" ]; then
    pkg=$("$here/build-installer.sh" | tail -n 1) || { echo "build-installer.sh failed" >&2; exit 1; }
fi
pkg="$(cd "$(dirname "$pkg")" && pwd)/$(basename "$pkg")"

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

name="Songsmith Analog Compressor"
VST3_DIR="/Library/Audio/Plug-Ins/VST3"
AU_DIR="/Library/Audio/Plug-Ins/Components"
VST3="$VST3_DIR/$name.vst3"
AU="$AU_DIR/$name.component"
APP="/Applications/$name.app"
IDS="com.songsmithstudio.songsmithanalogcompressor.vst3 com.songsmithstudio.songsmithanalogcompressor.au com.songsmithstudio.songsmithanalogcompressor.app"

# Songsmith EQ, seeded as if installed (unless this runner already has it): must survive.
EQ_VST3="$VST3_DIR/Songsmith EQ.vst3"
EQ_AU="$AU_DIR/Songsmith EQ.component"
seeded_eq=0

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
check() { local what="$1"; shift; if "$@"; then ok "$what"; else fail "$what"; fi; }
absent()  { [ ! -e "$1" ]; }
receipt() { pkgutil --pkg-info "$1" >/dev/null 2>&1; }
no_receipt() { ! receipt "$1"; }
snapshot() { find "$VST3" "$AU" "$APP" 2>/dev/null | LC_ALL=C sort; }
install_pkg() { $SUDO installer -pkg "$pkg" -target / "$@"; }

presets="$HOME/Library/Songsmith Studio/$name/Presets"
eq_presets="$HOME/Library/Songsmith Studio/Songsmith EQ/Presets"
preset_file="$presets/Installer test preset.sspreset"
eq_preset_file="$eq_presets/Installer test EQ preset.sspreset"
work=$(mktemp -d -t songsmithfet-installer-test)

others() {
    { find "$EQ_VST3" "$EQ_AU" -type f 2>/dev/null; echo "$preset_file"; echo "$eq_preset_file"; } |
        LC_ALL=C sort | while read -r f; do [ -f "$f" ] && shasum "$f"; done
}

cleanup() {
    rm -f "$preset_file" "$eq_preset_file"
    if [ "$seeded_eq" -eq 1 ]; then $SUDO rm -rf "$EQ_VST3" "$EQ_AU"; fi
    rm -rf "$work"
}
trap cleanup EXIT

echo "Installer: $pkg"

# --- 1. The package itself ----------------------------------------------------------------
echo
echo "Package contents"
pkgutil --expand-full "$pkg" "$work/expanded" >/dev/null
for spec in "vst3|/Library/Audio/Plug-Ins/VST3|$name.vst3" \
            "au|/Library/Audio/Plug-Ins/Components|$name.component" \
            "app|/Applications|$name.app"; do
    IFS='|' read -r id location bundle <<< "$spec"
    d="$work/expanded/songsmithanalogcompressor-$id.pkg"
    info="$d/PackageInfo"
    check "$id: component package present" test -f "$info"
    check "$id: identifier com.songsmithstudio.songsmithanalogcompressor.$id" grep -q "identifier=\"com.songsmithstudio.songsmithanalogcompressor.$id\"" "$info"
    check "$id: installs into $location" grep -q "install-location=\"$location\"" "$info"
    check "$id: payload is exactly '$bundle'" test "$(ls -A "$d/Payload" 2>/dev/null)" = "$bundle"
    check "$id: bundle isn't relocatable" test "$(xmllint --xpath 'count(//relocate/bundle)' "$info" 2>/dev/null)" = "0"
done
check "no .DS_Store or AppleDouble files in any payload" \
    test -z "$(find "$work/expanded" \( -name .DS_Store -o -name '._*' \) -print -quit)"

# --- 2. Seed Songsmith EQ, presets and a stale file --------------------------------------------
echo
echo "Seeding Songsmith EQ, both plug-ins' presets and a stale file"
$SUDO mkdir -p "$VST3_DIR" "$AU_DIR"
if [ ! -e "$EQ_VST3" ]; then
    seeded_eq=1
    for b in "$EQ_VST3" "$EQ_AU"; do
        $SUDO mkdir -p "$b/Contents"
        echo "songsmith eq" | $SUDO tee "$b/Contents/Info.plist" >/dev/null
    done
fi
$SUDO mkdir -p "$VST3/Contents/Resources"
$SUDO touch "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/$name.component"   # CI's auval copy, if any
mkdir -p "$presets" "$eq_presets"
echo "compressor preset, must survive" > "$preset_file"
echo "eq preset, must survive" > "$eq_preset_file"
others_before=$(others)

# --- 3. Install ------------------------------------------------------------------------------
echo
echo "Install"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
for b in "$VST3" "$AU" "$APP"; do check "installed: $b" test -d "$b"; done
check "stale file from the old version is gone" absent "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"
check "Songsmith EQ and all presets untouched" test "$(others)" = "$others_before"
check "everything installed is owned by root" test -z "$(find "$VST3" "$AU" "$APP" ! -user root -print -quit)"
check "nothing world-writable" test -z "$(find "$VST3" "$AU" "$APP" -perm -o+w ! -type l -print -quit)"
for id in $IDS; do check "receipt recorded: $id" receipt "$id"; done
first=$(snapshot)

# --- 4. auval with the new identifiers ------------------------------------------------------
echo
echo "auval (installed component)"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2
if auval -v aufx Sfc1 Tnyw > "$work/auval.txt" 2>&1; then
    ok "auval -v aufx Sfc1 Tnyw"
else
    tail -n 30 "$work/auval.txt"
    fail "auval -v aufx Sfc1 Tnyw"
fi

# --- 5. Upgrade (install over itself) -------------------------------------------------------
echo
echo "Install again over itself"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "same file list as after the first install" test "$(snapshot)" = "$first"
check "Songsmith EQ and all presets untouched" test "$(others)" = "$others_before"

# --- 6. Uninstall ---------------------------------------------------------------------------
echo
echo "uninstall.sh"
$SUDO /bin/bash "$here/uninstall.sh"; status=$?
check "uninstall.sh exits 0" test "$status" -eq 0
for b in "$VST3" "$AU" "$APP"; do check "removed: $b" absent "$b"; done
for id in $IDS; do check "receipt forgotten: $id" no_receipt "$id"; done
check "Songsmith EQ and all presets untouched" test "$(others)" = "$others_before"
check "VST3 folder kept" test -d "$VST3_DIR"
check "Components folder kept" test -d "$AU_DIR"
check "Songsmith EQ receipts not touched" bash -c "! grep -q songsmitheq '$here/uninstall.sh'"

echo
if [ "$failures" -eq 0 ]; then
    echo "All installer checks passed."
else
    echo "$failures installer check(s) failed."
fi
exit $(( failures > 0 ? 1 : 0 ))
