#!/bin/bash
# End-to-end test of the Songsmith EQ macOS installer. CI runs it on the macOS job.
#
#   installer/macos/test-installer.sh                  # build the .pkg, then test it
#   installer/macos/test-installer.sh --pkg <file>     # test an existing .pkg
#
# WARNING: this installs for real, system-wide (sudo installer -target /), seeds fake legacy
# plug-ins in /Library/Audio/Plug-Ins and ~/Library/Audio/Plug-Ins, and uninstalls at the end.
# That's fine on a throwaway CI runner, not on your own Mac. So it refuses to run unless CI=true
# or GITHUB_ACTIONS is set, or you pass --i-know-this-installs-system-wide.
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
pkg=""
allow=0
while [ $# -gt 0 ]; do
    case "$1" in
        --pkg) pkg="$2"; shift 2 ;;
        --i-know-this-installs-system-wide) allow=1; shift ;;
        -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ "${CI:-}" != "true" ] && [ -z "${GITHUB_ACTIONS:-}" ] && [ "$allow" -ne 1 ]; then
    echo "Refusing to run: this test installs Songsmith EQ system-wide and removes plug-ins." >&2
    echo "It's meant for CI. To run it on this Mac anyway, pass --i-know-this-installs-system-wide." >&2
    exit 2
fi

if [ -z "$pkg" ]; then
    pkg=$("$here/build-installer.sh" | tail -n 1) || { echo "build-installer.sh failed" >&2; exit 1; }
fi
pkg="$(cd "$(dirname "$pkg")" && pwd)/$(basename "$pkg")"

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

VST3_DIR="/Library/Audio/Plug-Ins/VST3"
AU_DIR="/Library/Audio/Plug-Ins/Components"
VST3="$VST3_DIR/Songsmith EQ.vst3"
AU="$AU_DIR/Songsmith EQ.component"
APP="/Applications/Songsmith EQ.app"
IDS="com.songsmithstudio.songsmitheq.vst3 com.songsmithstudio.songsmitheq.au com.songsmithstudio.songsmitheq.app"
UNRELATED="$VST3_DIR/Unrelated Test Plugin.vst3"     # must survive install and uninstall

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
# check "<what>" <command...>: passes when the command succeeds.
check() { local what="$1"; shift; if "$@"; then ok "$what"; else fail "$what"; fi; }
absent()  { [ ! -e "$1" ]; }
receipt() { pkgutil --pkg-info "$1" >/dev/null 2>&1; }
no_receipt() { ! receipt "$1"; }
snapshot() { find "$VST3" "$AU" "$APP" 2>/dev/null | LC_ALL=C sort; }
install_pkg() { $SUDO installer -pkg "$pkg" -target / "$@"; }

# Same rule as the installer scripts: the user at the console, not $USER.
console_user=$(stat -f%Su /dev/console 2>/dev/null)
user_home=""
case "$console_user" in
    ""|root|loginwindow|_mbsetupuser) ;;
    *) user_home=$(dscl . -read "/Users/$console_user" NFSHomeDirectory 2>/dev/null | awk '{print $2}') ;;
esac
U_SUDO=""
[ -n "$user_home" ] && [ "$user_home" != "$HOME" ] && U_SUDO="$SUDO"

presets="$HOME/Library/Songsmith Studio/Songsmith EQ/Presets"
preset_file="$presets/Installer test preset.sspreset"
work=$(mktemp -d -t songsmitheq-installer-test)

cleanup() {
    $SUDO rm -rf "$UNRELATED"
    rm -f "$preset_file"
    rm -rf "$work"
}
trap cleanup EXIT

echo "Installer:    $pkg"
echo "Console user: ${console_user:-none} (home: ${user_home:-none, per-user clean-up not tested})"

# --- 1. The package itself ----------------------------------------------------------------
echo
echo "Package contents"
pkgutil --expand-full "$pkg" "$work/expanded" >/dev/null
for spec in "vst3|/Library/Audio/Plug-Ins/VST3|Songsmith EQ.vst3" \
            "au|/Library/Audio/Plug-Ins/Components|Songsmith EQ.component" \
            "app|/Applications|Songsmith EQ.app"; do
    IFS='|' read -r id location bundle <<< "$spec"
    d="$work/expanded/songsmitheq-$id.pkg"
    info="$d/PackageInfo"
    check "$id: component package present" test -f "$info"
    check "$id: identifier com.songsmithstudio.songsmitheq.$id" grep -q "identifier=\"com.songsmithstudio.songsmitheq.$id\"" "$info"
    check "$id: installs into $location" grep -q "install-location=\"$location\"" "$info"
    check "$id: payload is exactly '$bundle'" test "$(ls -A "$d/Payload" 2>/dev/null)" = "$bundle"
    check "$id: bundle isn't relocatable" bash -c "! grep -q '<relocate' '$info'"
done
check "no .DS_Store or AppleDouble files in any payload" \
    test -z "$(find "$work/expanded" \( -name .DS_Store -o -name '._*' \) -print -quit)"

installer -pkg "$pkg" -target / -showChoicesXML > "$work/choices.xml" 2>/dev/null
for c in vst3 au app; do
    check "choice '$c' offered" grep -q "<string>$c</string>" "$work/choices.xml"
done

# --- 2. Seed an old install ----------------------------------------------------------------
echo
echo "Seeding legacy bundles, a stale file, an unrelated plug-in and a preset"
$SUDO mkdir -p "$VST3_DIR" "$AU_DIR"
for b in "$VST3_DIR/TW Pro EQ.vst3" "$VST3_DIR/3-Band EQ.vst3" "$AU_DIR/TW Pro EQ.component" \
         "$AU_DIR/3-Band EQ.component" "$UNRELATED"; do
    $SUDO mkdir -p "$b/Contents"
    $SUDO touch "$b/Contents/Info.plist"
done
$SUDO mkdir -p "$VST3/Contents/Resources"
$SUDO touch "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"

user_bundles=()
if [ -n "$user_home" ]; then
    for b in "VST3/TW Pro EQ.vst3" "VST3/Songsmith EQ.vst3" "Components/TW Pro EQ.component" \
             "Components/Songsmith EQ.component"; do
        $U_SUDO mkdir -p "$user_home/Library/Audio/Plug-Ins/$b/Contents"
        user_bundles+=("$user_home/Library/Audio/Plug-Ins/$b")
    done
else
    # Nobody at the console, so the installer can't clean up a per-user copy. Remove ours
    # (an earlier CI step installs one for auval) so auval below checks the installed one.
    rm -rf "$HOME/Library/Audio/Plug-Ins/Components/Songsmith EQ.component"
fi

mkdir -p "$presets"
echo "dummy preset, must survive" > "$preset_file"
preset_hash=$(shasum "$preset_file")

# --- 3. Install ------------------------------------------------------------------------------
echo
echo "Install"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
for b in "$VST3" "$AU" "$APP"; do check "installed: $b" test -d "$b"; done
for b in "$VST3_DIR/TW Pro EQ.vst3" "$VST3_DIR/3-Band EQ.vst3" "$AU_DIR/TW Pro EQ.component" "$AU_DIR/3-Band EQ.component"; do
    check "legacy bundle removed: $b" absent "$b"
done
for b in "${user_bundles[@]+"${user_bundles[@]}"}"; do check "per-user copy removed: $b" absent "$b"; done
check "stale file from the old version is gone" absent "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"
check "unrelated plug-in untouched" test -d "$UNRELATED"
check "preset file untouched" test "$(shasum "$preset_file" 2>/dev/null)" = "$preset_hash"
check "everything installed is owned by root" test -z "$(find "$VST3" "$AU" "$APP" ! -user root -print -quit)"
check "everything installed has group wheel (or admin, which Installer may use in /Applications)" \
    test -z "$(find "$VST3" "$AU" "$APP" ! -group wheel ! -group admin -print -quit)"
check "nothing world-writable" test -z "$(find "$VST3" "$AU" "$APP" -perm -o+w ! -type l -print -quit)"
check "no quarantine attributes" bash -c "! xattr -r '$VST3' '$AU' '$APP' 2>/dev/null | grep -q com.apple.quarantine"
for id in $IDS; do check "receipt recorded: $id" receipt "$id"; done
first=$(snapshot)
check "installed file list isn't empty" test -n "$first"

# --- 4. auval against the installed component -----------------------------------------------
echo
echo "auval (installed component)"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2
if auval -v aufx Eq3b Tnyw > "$work/auval.txt" 2>&1; then
    ok "auval -v aufx Eq3b Tnyw"
else
    tail -n 30 "$work/auval.txt"
    fail "auval -v aufx Eq3b Tnyw"
fi

# --- 5. Install over itself ------------------------------------------------------------------
echo
echo "Install again over itself"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "same file list as after the first install" test "$(snapshot)" = "$first"

# --- 6. Install without the standalone app ---------------------------------------------------
echo
echo "Install with the standalone app deselected"
$SUDO rm -rf "$APP"
$SUDO pkgutil --forget com.songsmithstudio.songsmitheq.app >/dev/null 2>&1
cat > "$work/no-app.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>attributeSetting</key><integer>0</integer>
        <key>choiceAttribute</key><string>selected</string>
        <key>choiceIdentifier</key><string>app</string>
    </dict>
</array>
</plist>
EOF
install_pkg -applyChoiceChangesXML "$work/no-app.xml"; status=$?
check "installer exits 0" test "$status" -eq 0
check "VST3 installed" test -d "$VST3"
check "AU installed" test -d "$AU"
check "standalone app not added" absent "$APP"
check "no receipt for the app" no_receipt com.songsmithstudio.songsmitheq.app

# --- 7. Uninstall ----------------------------------------------------------------------------
echo
echo "uninstall.sh"
$SUDO /bin/bash "$here/uninstall.sh"; status=$?
check "uninstall.sh exits 0" test "$status" -eq 0
for b in "$VST3" "$AU" "$APP"; do check "removed: $b" absent "$b"; done
for id in $IDS; do check "receipt forgotten: $id" no_receipt "$id"; done
check "preset file kept" test "$(shasum "$preset_file" 2>/dev/null)" = "$preset_hash"
check "VST3 folder kept" test -d "$VST3_DIR"
check "Components folder kept" test -d "$AU_DIR"
check "unrelated plug-in kept" test -d "$UNRELATED"

# pluginval isn't run here: see the PR / README (it needs a separate download and adds minutes).

echo
if [ "$failures" -eq 0 ]; then
    echo "All installer checks passed."
else
    echo "$failures installer check(s) failed."
fi
exit $(( failures > 0 ? 1 : 0 ))
