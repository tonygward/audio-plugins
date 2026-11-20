#!/bin/bash
# End-to-end test of the Songsmith Plugin Analyzer macOS installer. CI runs it on the macOS job.
#
#   installer/macos/test-installer.sh                  # build the .pkg, then test it
#   installer/macos/test-installer.sh --pkg <file>     # test an existing .pkg
#
# WARNING: this installs for real, system-wide (sudo installer -target /), seeds a fake plug-in
# in /Library/Audio/Plug-Ins and ~/Library/Audio/Plug-Ins, and uninstalls at the end.
# That's fine on a throwaway CI runner, not on your own Mac. So it refuses to run unless CI=true
# or GITHUB_ACTIONS is set, or you pass --i-know-this-installs-system-wide.
#
# The Plugin Analyzer is a VST3 plug-in only (ANALYZER-01), so there is no Audio Unit and no app to check.
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
    echo "Refusing to run: this test installs Songsmith Plugin Analyzer system-wide and removes plug-ins." >&2
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
VST3="$VST3_DIR/Songsmith Plugin Analyzer.vst3"
IDS="com.songsmithstudio.songsmithpluginanalyzer.vst3"
UNRELATED="$VST3_DIR/Unrelated Test Plugin.vst3"     # must survive install and uninstall

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
# check "<what>" <command...>: passes when the command succeeds.
check() { local what="$1"; shift; if "$@"; then ok "$what"; else fail "$what"; fi; }
absent()  { [ ! -e "$1" ]; }
receipt() { pkgutil --pkg-info "$1" >/dev/null 2>&1; }
no_receipt() { ! receipt "$1"; }
snapshot() { find "$VST3" 2>/dev/null | LC_ALL=C sort; }
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

presets="$HOME/Library/Songsmith Studio/Songsmith Plugin Analyzer/Presets"
preset_file="$presets/Installer test preset.sspreset"
work=$(mktemp -d -t songsmithpluginanalyzer-installer-test)

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
for spec in "vst3|/Library/Audio/Plug-Ins/VST3|Songsmith Plugin Analyzer.vst3"; do
    IFS='|' read -r id location bundle <<< "$spec"
    d="$work/expanded/songsmithpluginanalyzer-$id.pkg"
    info="$d/PackageInfo"
    check "$id: component package present" test -f "$info"
    check "$id: identifier com.songsmithstudio.songsmithpluginanalyzer.$id" grep -q "identifier=\"com.songsmithstudio.songsmithpluginanalyzer.$id\"" "$info"
    check "$id: installs into $location" grep -q "install-location=\"$location\"" "$info"
    check "$id: payload is exactly '$bundle'" test "$(ls -A "$d/Payload" 2>/dev/null)" = "$bundle"
    # Relocatable bundles are listed as <relocate><bundle id=.../></relocate>; an empty <relocate/> is fine.
    check "$id: bundle isn't relocatable" test "$(xmllint --xpath 'count(//relocate/bundle)' "$info" 2>/dev/null)" = "0"
done
check "only the VST3 is packaged (no Audio Unit, no app)" test "$(ls "$work/expanded" | grep -c '\.pkg$')" = "1"
check "no .DS_Store or AppleDouble files in any payload" \
    test -z "$(find "$work/expanded" \( -name .DS_Store -o -name '._*' \) -print -quit)"

installer -pkg "$pkg" -target / -showChoicesXML > "$work/choices.xml" 2>/dev/null
check "choice 'vst3' offered" grep -q "<string>vst3</string>" "$work/choices.xml"

# --- 2. Seed an old install ----------------------------------------------------------------
echo
echo "Seeding a stale file, an unrelated plug-in (stands in for Songsmith EQ) and a user file"
$SUDO mkdir -p "$VST3_DIR"
$SUDO mkdir -p "$UNRELATED/Contents"
$SUDO touch "$UNRELATED/Contents/Info.plist"
$SUDO mkdir -p "$VST3/Contents/Resources"
$SUDO touch "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"

user_bundles=()
if [ -n "$user_home" ]; then
    b="VST3/Songsmith Plugin Analyzer.vst3"
    $U_SUDO mkdir -p "$user_home/Library/Audio/Plug-Ins/$b/Contents"
    user_bundles+=("$user_home/Library/Audio/Plug-Ins/$b")
fi

mkdir -p "$presets"
echo "a user file, must survive" > "$preset_file"
preset_hash=$(shasum "$preset_file")

# --- 3. Install ------------------------------------------------------------------------------
echo
echo "Install"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "installed: $VST3" test -d "$VST3"
for b in "${user_bundles[@]+"${user_bundles[@]}"}"; do check "per-user copy removed: $b" absent "$b"; done
check "stale file from the old version is gone" absent "$VST3/Contents/Resources/stale-file-from-an-old-version.txt"
check "unrelated plug-in untouched" test -d "$UNRELATED"
check "user file untouched" test "$(shasum "$preset_file" 2>/dev/null)" = "$preset_hash"
check "everything installed is owned by root" test -z "$(find "$VST3" ! -user root -print -quit)"
check "everything installed has group wheel (or admin)" \
    test -z "$(find "$VST3" ! -group wheel ! -group admin -print -quit)"
check "nothing world-writable" test -z "$(find "$VST3" -perm -o+w ! -type l -print -quit)"
check "no quarantine attributes" bash -c "! xattr -r '$VST3' 2>/dev/null | grep -q com.apple.quarantine"
for id in $IDS; do check "receipt recorded: $id" receipt "$id"; done
first=$(snapshot)
check "installed file list isn't empty" test -n "$first"

# --- 4. Install over itself ------------------------------------------------------------------
echo
echo "Install again over itself"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "same file list as after the first install" test "$(snapshot)" = "$first"

# --- 5. Uninstall ----------------------------------------------------------------------------
echo
echo "uninstall.sh"
$SUDO /bin/bash "$here/uninstall.sh"; status=$?
check "uninstall.sh exits 0" test "$status" -eq 0
check "removed: $VST3" absent "$VST3"
for id in $IDS; do check "receipt forgotten: $id" no_receipt "$id"; done
check "user file kept" test "$(shasum "$preset_file" 2>/dev/null)" = "$preset_hash"
check "VST3 folder kept" test -d "$VST3_DIR"
check "unrelated plug-in kept" test -d "$UNRELATED"

echo
if [ "$failures" -eq 0 ]; then
    echo "All installer checks passed."
else
    echo "$failures installer check(s) failed."
fi
exit $(( failures > 0 ? 1 : 0 ))
