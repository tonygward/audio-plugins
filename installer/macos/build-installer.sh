#!/bin/bash
# Builds the Songsmith Studio macOS installer: one unsigned flat .pkg with every plugin, each one
# selected by default (Apple's pkgutil and productbuild, installed with Xcode).
#
#   installer/macos/build-installer.sh       # build each plugin's own .pkg first, then the suite
#   installer/macos/build-installer.sh --eq-pkg <pkg> --fet-pkg <pkg> --max-pkg <pkg>   # CI
#   installer/macos/build-installer.sh --suite-version <older version> --out-dir <folder>         # tests
#
# The suite is made from the SAME component packages as each plugin's own installer: it expands
# each plugin's .pkg (made by <plugin>/installer/macos/build-installer.sh) and takes its VST3,
# Audio Unit and app packages as they are, with their preinstall/postinstall scripts. So the two
# installers can't drift apart, and installing from either updates the same receipts.
#
# Every plugin and the suite share the one version in VERSION at the repository root. The last
# line of the output is the path of the .pkg (default: build/suite-installer/Songsmith-Studio-<version>-macOS.pkg).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
eq_pkg="" fet_pkg="" max_pkg="" suite_version="" out_dir="$repo/build/suite-installer"

while [ $# -gt 0 ]; do
    case "$1" in
        --eq-pkg)        eq_pkg="$2"; shift 2 ;;
        --fet-pkg)       fet_pkg="$2"; shift 2 ;;
        --max-pkg)       max_pkg="$2"; shift 2 ;;
        --suite-version) suite_version="$2"; shift 2 ;;
        --out-dir)       out_dir="$2"; shift 2 ;;
        -h|--help)       sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

shared_version=$(tr -d '[:space:]' < "$repo/VERSION")
if ! printf '%s' "$shared_version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "VERSION must hold one version as x.y.z (found '$shared_version')" >&2
    exit 1
fi

# Tests may build an "older" suite with --suite-version; the plugins inside always carry VERSION.
[ -z "$suite_version" ] && suite_version="$shared_version"
if ! printf '%s' "$suite_version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Suite version '$suite_version' isn't x.y.z" >&2
    exit 1
fi
eq_version="$shared_version"
fet_version="$shared_version"
max_version="$shared_version"

# Each plugin's own .pkg: given, or built now from that plugin's Release build.
[ -z "$eq_pkg" ]  && eq_pkg=$("$repo/eq/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$fet_pkg" ] && fet_pkg=$("$repo/analog-compressor/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$max_pkg" ] && max_pkg=$("$repo/maximiser/installer/macos/build-installer.sh" | tail -n 1)

mkdir -p "$out_dir"
out_dir="$(cd "$out_dir" && pwd)"
work="$out_dir/macos-suite-work"
pkg="$out_dir/Songsmith-Studio-$suite_version-macOS.pkg"
rm -rf "$work"
mkdir -p "$work/pkgs" "$work/resources"
rm -f "$pkg"

# Takes the three component packages out of one plugin's .pkg, flat again, as they were built.
#   take <plugin pkg> <component file prefix>
min_macos=""
take() {
    local product="$1" prefix="$2" expanded="$work/expanded-$2"
    if [ ! -f "$product" ]; then
        echo "Missing $product" >&2
        exit 1
    fi
    pkgutil --expand "$product" "$expanded"
    for id in vst3 au app; do
        local component="$expanded/$prefix-$id.pkg"
        if [ ! -d "$component" ]; then
            echo "$product has no $prefix-$id.pkg component package" >&2
            exit 1
        fi
        pkgutil --flatten "$component" "$work/pkgs/$prefix-$id.pkg"
    done
    # Its minimum macOS: the suite needs the highest of the three.
    local m
    m=$(sed -nE 's/.*<os-version[^>]*min="([0-9.]+)".*/\1/p' "$expanded/Distribution" | head -n 1)
    [ -n "$m" ] && min_macos=$(printf '%s\n%s\n' "$min_macos" "$m" | sed '/^$/d' | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1)
    return 0
}
take "$eq_pkg"  songsmitheq
take "$fet_pkg" songsmithanalogcompressor
take "$max_pkg" songsmithmaximiser
[ -z "$min_macos" ] && min_macos="10.13"

fill() {
    sed -e "s/@SUITE_VERSION@/$suite_version/g" -e "s/@EQ_VERSION@/$eq_version/g" \
        -e "s/@FET_VERSION@/$fet_version/g" -e "s/@MAX_VERSION@/$max_version/g" \
        -e "s/@MIN_MACOS@/$min_macos/g" "$1"
}
fill "$here/distribution.xml" > "$work/distribution.xml"
for f in "$here/resources/"*; do
    fill "$f" > "$work/resources/$(basename "$f")"
done
# The licence page carries the font licence word for word (shared/Resources/Fonts/OFL.txt).
licence="$work/resources/licence.html"
{
    sed '/@OFL@/,$d' "$licence"
    sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' "$repo/shared/Resources/Fonts/OFL.txt"
    sed '1,/@OFL@/d' "$licence"
} > "$work/licence.html"
mv "$work/licence.html" "$licence"

productbuild --distribution "$work/distribution.xml" \
             --package-path "$work/pkgs" \
             --resources "$work/resources" \
             "$pkg"

echo "Suite version: $suite_version (Songsmith EQ $eq_version, Songsmith Analog Compressor $fet_version, Songsmith Maximiser $max_version)"
echo "Minimum macOS: $min_macos"
echo "Installer:     $pkg"
echo "$pkg"
