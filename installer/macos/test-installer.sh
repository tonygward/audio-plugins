#!/bin/bash
# End-to-end test of the Songsmith Studio macOS installer. CI runs it in the suite-macos job.
#
#   installer/macos/test-installer.sh --i-know-this-installs-system-wide \
#       --pkg <suite pkg> --eq-pkg <pkg> --fet-pkg <pkg> --max-pkg <pkg>
#
# --eq-pkg, --fet-pkg and --max-pkg are the plugins' own installers: the EQ's is installed before
# the suite to check that the suite replaces it, and all three are used to build an "older"
# suite (version 0.9.0) for the upgrade test. Without --pkg, the suite is built from them first.
#
# WARNING: this installs for real, system-wide (sudo installer -target /), removes every
# Songsmith plugin with uninstall.sh, and leaves a dummy preset in each preset folder only while
# it runs. Fine on a throwaway CI runner, not on your own Mac. So it refuses to run unless you
# pass --i-know-this-installs-system-wide.
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
pkg="" eq_pkg="" fet_pkg="" max_pkg="" allow=0
while [ $# -gt 0 ]; do
    case "$1" in
        --pkg)     pkg="$2"; shift 2 ;;
        --eq-pkg)  eq_pkg="$2"; shift 2 ;;
        --fet-pkg) fet_pkg="$2"; shift 2 ;;
        --max-pkg) max_pkg="$2"; shift 2 ;;
        --i-know-this-installs-system-wide) allow=1; shift ;;
        -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ "$allow" -ne 1 ]; then
    echo "Refusing to run: this test installs every Songsmith plugin system-wide and removes them again." >&2
    echo "It's meant for CI. To run it anyway, pass --i-know-this-installs-system-wide." >&2
    exit 2
fi
for p in "$eq_pkg" "$fet_pkg" "$max_pkg"; do
    if [ -z "$p" ] || [ ! -f "$p" ]; then
        echo "Pass the plugins' own installers: --eq-pkg, --fet-pkg and --max-pkg." >&2
        exit 2
    fi
done

work=$(mktemp -d -t songsmith-studio-installer-test)
build_args=(--eq-pkg "$eq_pkg" --fet-pkg "$fet_pkg" --max-pkg "$max_pkg")
if [ -z "$pkg" ]; then
    pkg=$("$here/build-installer.sh" "${build_args[@]}" | tail -n 1) || { echo "build-installer.sh failed" >&2; exit 1; }
fi
pkg="$(cd "$(dirname "$pkg")" && pwd)/$(basename "$pkg")"
older=$("$here/build-installer.sh" "${build_args[@]}" --suite-version 0.9.0 --out-dir "$work/older" | tail -n 1) ||
    { echo "building the older suite failed" >&2; exit 1; }

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

VST3_DIR="/Library/Audio/Plug-Ins/VST3"
AU_DIR="/Library/Audio/Plug-Ins/Components"
keys=(eq fet max)
name_of()   { case "$1" in eq) echo "Songsmith EQ" ;; fet) echo "Songsmith Analog Compressor" ;; max) echo "Songsmith Maximiser" ;; esac; }
prefix_of() { case "$1" in eq) echo "com.songsmithstudio.songsmitheq" ;; fet) echo "com.songsmithstudio.songsmithanalogcompressor" ;; max) echo "com.songsmithstudio.songsmithmaximiser" ;; esac; }
auval_of()  { case "$1" in eq) echo "aufx Eq3b Tnyw" ;; fet) echo "aufx Sfc1 Tnyw" ;; max) echo "aufx Mxlm Tnyw" ;; esac; }
version_of() {
    local folder; case "$1" in eq) folder=eq ;; fet) folder=analog-compressor ;; max) folder=maximiser ;; esac
    sed -nE 's/^[[:space:]]*project\([[:space:]]*[A-Za-z0-9_]+[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "$here/../../$folder/CMakeLists.txt" | head -n 1
}
bundles_of() {
    local n; n=$(name_of "$1")
    echo "$VST3_DIR/$n.vst3"; echo "$AU_DIR/$n.component"; echo "/Applications/$n.app"
}

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }
check() { local what="$1"; shift; if "$@"; then ok "$what"; else fail "$what"; fi; }
receipt() { pkgutil --pkg-info "$1" >/dev/null 2>&1; }
no_receipt() { ! receipt "$1"; }
receipt_version() { pkgutil --pkg-info "$1" 2>/dev/null | awk '/^version:/ { print $2 }'; }
install_pkg() { local p="$1"; shift; $SUDO installer -pkg "$p" -target / "$@"; }
uninstall_all() { "$here/uninstall.sh" --yes >/dev/null; }

# expect_installed "<keys installed>" "<context>": the listed plugins' three bundles and receipts
# are there, the others' aren't.
expect_installed() {
    local want=" $1 " context="$2" k b part
    for k in "${keys[@]}"; do
        if [[ "$want" == *" $k "* ]]; then
            while read -r b; do check "$context: installed $b" test -d "$b"; done < <(bundles_of "$k")
            for part in vst3 au app; do check "$context: receipt $(prefix_of "$k").$part" receipt "$(prefix_of "$k").$part"; done
        else
            while read -r b; do check "$context: not installed $b" test ! -e "$b"; done < <(bundles_of "$k")
            for part in vst3 au app; do check "$context: no receipt $(prefix_of "$k").$part" no_receipt "$(prefix_of "$k").$part"; done
        fi
    done
}

# Exactly one bundle per plugin in each folder (no "Songsmith EQ 2.vst3" and the like).
one_copy_each() {
    test "$(ls -d "$VST3_DIR"/Songsmith* 2>/dev/null | wc -l | tr -d ' ')" -le 3 &&
    test "$(ls -d "$AU_DIR"/Songsmith* 2>/dev/null | wc -l | tr -d ' ')" -le 3 &&
    test "$(ls -d /Applications/Songsmith* 2>/dev/null | wc -l | tr -d ' ')" -le 3
}

presets=()
cleanup() {
    for f in "${presets[@]+"${presets[@]}"}"; do rm -f "$f"; done
    rm -rf "$work"
}
trap cleanup EXIT

echo "Installer:        $pkg"
echo "Older installer:  $older"
echo "Plugin installers: $eq_pkg, $fet_pkg, $max_pkg"

# --- The package itself ---------------------------------------------------------------------
echo
echo "Package contents"
pkgutil --expand "$pkg" "$work/expanded" >/dev/null
for k in "${keys[@]}"; do
    prefix=$(prefix_of "$k")
    file="${prefix##*.}"   # com.songsmithstudio.songsmitheq -> songsmitheq
    for part in vst3 au app; do
        check "$k: component package $file-$part.pkg present" test -f "$work/expanded/$file-$part.pkg/PackageInfo"
        check "$k: $file-$part.pkg is $prefix.$part" grep -q "identifier=\"$prefix.$part\"" "$work/expanded/$file-$part.pkg/PackageInfo"
    done
done
installer -pkg "$pkg" -target / -showChoicesXML > "$work/choices.xml" 2>/dev/null
for c in eq eq_vst3 eq_au eq_app fet fet_vst3 fet_au fet_app max max_vst3 max_au max_app; do
    check "choice '$c' offered" grep -q "<string>$c</string>" "$work/choices.xml"
done
for k in "${keys[@]}"; do
    check "the welcome page names $(name_of "$k") $(version_of "$k")" grep -rq "$(name_of "$k") $(version_of "$k")" "$work/expanded/Resources"
done

# A dummy preset in each plugin's preset folder: nothing may touch these.
for k in "${keys[@]}"; do
    d="$HOME/Library/Songsmith Studio/$(name_of "$k")/Presets"
    mkdir -p "$d"
    f="$d/Suite installer test preset.sspreset"
    echo "dummy preset, must survive" > "$f"
    presets+=("$f")
done
uninstall_all   # start clean (an earlier CI step may have installed something)

# --- 1. Everything -------------------------------------------------------------------------------
echo
echo "1. Install everything"
install_pkg "$pkg"; check "installer exits 0" test $? -eq 0
expect_installed "eq fet max" "everything"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2
for k in "${keys[@]}"; do
    # shellcheck disable=SC2046
    if auval -v $(auval_of "$k") > "$work/auval-$k.txt" 2>&1; then
        ok "auval -v $(auval_of "$k") ($(name_of "$k"))"
    else
        tail -n 30 "$work/auval-$k.txt"
        fail "auval -v $(auval_of "$k") ($(name_of "$k"))"
    fi
done

# --- 2. Only the maximiser -------------------------------------------------------------------
echo
echo "2. Install with only Songsmith Maximiser selected"
uninstall_all
{
    echo '<?xml version="1.0" encoding="UTF-8"?>'
    echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
    echo '<plist version="1.0"><array>'
    for c in eq eq_vst3 eq_au eq_app fet fet_vst3 fet_au fet_app max max_vst3 max_au max_app; do
        setting=0; [[ "$c" == max* ]] && setting=1
        echo "<dict><key>attributeSetting</key><integer>$setting</integer><key>choiceAttribute</key><string>selected</string><key>choiceIdentifier</key><string>$c</string></dict>"
    done
    echo '</array></plist>'
} > "$work/only-max.xml"
install_pkg "$pkg" -applyChoiceChangesXML "$work/only-max.xml"; check "installer exits 0" test $? -eq 0
expect_installed "max" "only the maximiser"

# --- 3. The EQ's own installer first, then the suite ---------------------------------------
echo
echo "3. Songsmith EQ's own installer, then the suite"
uninstall_all
install_pkg "$eq_pkg"; check "Songsmith EQ's own installer exits 0" test $? -eq 0
expect_installed "eq" "the EQ on its own"
install_pkg "$pkg"; check "suite installer exits 0" test $? -eq 0
expect_installed "eq fet max" "after the suite"
check "one copy of each plugin" one_copy_each
for part in vst3 au app; do
    id="$(prefix_of eq).$part"
    check "receipt $id is version $(version_of eq)" test "$(receipt_version "$id")" = "$(version_of eq)"
done

# --- 4. Upgrade from an older suite ------------------------------------------------------------
echo
echo "4. Upgrade from suite 0.9.0"
uninstall_all
install_pkg "$older"; check "older suite installer exits 0" test $? -eq 0
expect_installed "eq fet max" "older suite"
install_pkg "$pkg"; check "newer suite installer exits 0" test $? -eq 0
expect_installed "eq fet max" "upgraded"
check "one copy of each plugin (upgraded in place)" one_copy_each

# --- 5. Uninstall --------------------------------------------------------------------------------
echo
echo "5. uninstall.sh"
"$here/uninstall.sh" --yes; check "uninstall.sh exits 0" test $? -eq 0
expect_installed "" "after uninstall"
for f in "${presets[@]}"; do check "preset kept: $f" test -f "$f"; done
install_pkg "$pkg" >/dev/null
echo n | "$here/uninstall.sh" >/dev/null
check "uninstall.sh asks first: answering no removes nothing" test -d "$VST3_DIR/Songsmith EQ.vst3"
uninstall_all

echo
if [ "$failures" -gt 0 ]; then
    echo "$failures suite installer check(s) failed"
    exit 1
fi
echo "All suite installer checks passed"
