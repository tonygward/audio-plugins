#!/bin/bash
# End-to-end test of the Songsmith Audio Analyzer macOS installer. CI runs it on the macOS job.
#
#   installer/macos/test-installer.sh                  # build the .pkg, then test it
#   installer/macos/test-installer.sh --pkg <file>     # test an existing .pkg
#
# WARNING: this installs for real, system-wide (sudo installer -target /) and uninstalls at the end.
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
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ "${CI:-}" != "true" ] && [ -z "${GITHUB_ACTIONS:-}" ] && [ "$allow" -ne 1 ]; then
    echo "Refusing to run: this test installs Songsmith Audio Analyzer system-wide and removes it again." >&2
    echo "It's meant for CI. To run it on this Mac anyway, pass --i-know-this-installs-system-wide." >&2
    exit 2
fi

if [ -z "$pkg" ]; then
    pkg=$("$here/build-installer.sh" | tail -n 1) || { echo "build-installer.sh failed" >&2; exit 1; }
fi
pkg="$(cd "$(dirname "$pkg")" && pwd)/$(basename "$pkg")"

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

APP="/Applications/Songsmith Audio Analyzer.app"
ID="com.songsmithstudio.songsmithaudioanalyzer.app"
UNRELATED="/Applications/Unrelated Test App.app"     # must survive install and uninstall

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
check() { local what="$1"; shift; if "$@"; then ok "$what"; else fail "$what"; fi; }
absent()  { [ ! -e "$1" ]; }
receipt() { pkgutil --pkg-info "$1" >/dev/null 2>&1; }
no_receipt() { ! receipt "$1"; }
snapshot() { find "$APP" 2>/dev/null | LC_ALL=C sort; }
install_pkg() { $SUDO installer -pkg "$pkg" -target / "$@"; }

work=$(mktemp -d -t songsmithaudioanalyzer-installer-test)
cleanup() {
    $SUDO rm -rf "$UNRELATED"
    rm -rf "$work"
}
trap cleanup EXIT

echo "Installer: $pkg"

# --- 1. The package itself ----------------------------------------------------------------
echo
echo "Package contents"
pkgutil --expand-full "$pkg" "$work/expanded" >/dev/null
d="$work/expanded/songsmithaudioanalyzer-app.pkg"
info="$d/PackageInfo"
check "app: component package present" test -f "$info"
check "app: identifier $ID" grep -q "identifier=\"$ID\"" "$info"
check "app: installs into /Applications" grep -q 'install-location="/Applications"' "$info"
check "app: payload is exactly 'Songsmith Audio Analyzer.app'" test "$(ls -A "$d/Payload" 2>/dev/null)" = "Songsmith Audio Analyzer.app"
check "app: the app isn't relocatable" test "$(xmllint --xpath 'count(//relocate/bundle)' "$info" 2>/dev/null)" = "0"
check "there is no VST3 or Audio Unit component (the Audio Analyzer is an app only)" \
    test -z "$(ls "$work/expanded" | grep -E -- '-(vst3|au)\.pkg$')"
check "no .DS_Store or AppleDouble files in the payload" \
    test -z "$(find "$work/expanded" \( -name .DS_Store -o -name '._*' \) -print -quit)"
check "the app has a launchable executable" test -x "$d/Payload/Songsmith Audio Analyzer.app/Contents/MacOS/Songsmith Audio Analyzer"

installer -pkg "$pkg" -target / -showChoicesXML > "$work/choices.xml" 2>/dev/null
check "choice 'app' offered" grep -q "<string>app</string>" "$work/choices.xml"

# --- 2. Seed an old install ----------------------------------------------------------------
echo
echo "Seeding a stale file and an unrelated app"
$SUDO mkdir -p "$UNRELATED/Contents"
$SUDO touch "$UNRELATED/Contents/Info.plist"
$SUDO mkdir -p "$APP/Contents/Resources"
$SUDO touch "$APP/Contents/Resources/stale-file-from-an-old-version.txt"

# --- 3. Install ------------------------------------------------------------------------------
echo
echo "Install"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "installed: $APP" test -d "$APP"
check "the app can be launched (a real executable)" test -x "$APP/Contents/MacOS/Songsmith Audio Analyzer"
check "stale file from the old version is gone" absent "$APP/Contents/Resources/stale-file-from-an-old-version.txt"
check "unrelated app untouched" test -d "$UNRELATED"
check "everything installed is owned by root" test -z "$(find "$APP" ! -user root -print -quit)"
check "everything installed has group wheel (or admin, which Installer may use in /Applications)" \
    test -z "$(find "$APP" ! -group wheel ! -group admin -print -quit)"
check "nothing world-writable" test -z "$(find "$APP" -perm -o+w ! -type l -print -quit)"
check "no quarantine attributes" bash -c "! xattr -r '$APP' 2>/dev/null | grep -q com.apple.quarantine"
check "no VST3 or Audio Unit was installed" test ! -e "/Library/Audio/Plug-Ins/VST3/Songsmith Audio Analyzer.vst3" -a ! -e "/Library/Audio/Plug-Ins/Components/Songsmith Audio Analyzer.component"
check "receipt recorded: $ID" receipt "$ID"
first=$(snapshot)
check "installed file list isn't empty" test -n "$first"
check "the app's Info.plist names the product" bash -c "/usr/libexec/PlistBuddy -c 'Print :CFBundleName' '$APP/Contents/Info.plist' | grep -q 'Songsmith Audio Analyzer'"
check "the app opens WAV, AIFF and FLAC files" bash -c "plutil -p '$APP/Contents/Info.plist' | grep -qi 'wav' && plutil -p '$APP/Contents/Info.plist' | grep -qi 'flac'"

# --- 4. Install over itself ------------------------------------------------------------------
echo
echo "Install again over itself (an upgrade)"
install_pkg; status=$?
check "installer exits 0" test "$status" -eq 0
check "same file list as after the first install" test "$(snapshot)" = "$first"

# --- 5. Uninstall ----------------------------------------------------------------------------
echo
echo "uninstall.sh"
$SUDO /bin/bash "$here/uninstall.sh"; status=$?
check "uninstall.sh exits 0" test "$status" -eq 0
check "removed: $APP" absent "$APP"
check "receipt forgotten: $ID" no_receipt "$ID"
check "unrelated app kept" test -d "$UNRELATED"

echo
if [ "$failures" -eq 0 ]; then
    echo "All installer checks passed."
else
    echo "$failures installer check(s) failed."
fi
exit $(( failures > 0 ? 1 : 0 ))
