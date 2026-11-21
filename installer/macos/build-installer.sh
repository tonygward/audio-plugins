#!/bin/bash
# Builds the Songsmith Plugins macOS installer: one unsigned flat .pkg with every plugin, the Plugin Analyzer (a VST3 only) and the Audio Analyzer app, each one
# selected by default (Apple's pkgutil and productbuild, installed with Xcode).
#
#   installer/macos/build-installer.sh       # build each plugin's own .pkg first, then the suite
#   installer/macos/build-installer.sh --eq-pkg <pkg> --fet-pkg <pkg> --max-pkg <pkg> --comp-pkg <pkg> --opt-pkg <pkg> --peq-pkg <pkg> --bus-pkg <pkg> --desk-pkg <pkg> --pla-pkg <pkg> --ana-pkg <pkg>   # CI
#   installer/macos/build-installer.sh --suite-version <older version> --out-dir <folder>         # tests
#
# The suite is made from the SAME component packages as each plugin's own installer: it expands
# each plugin's .pkg (made by <plugin>/installer/macos/build-installer.sh) and takes its VST3,
# Audio Unit and app packages as they are, with their preinstall/postinstall scripts. The Songsmith Audio
# Analyzer (REC-02) is an application only, so only its app package is taken, and the Songsmith Plugin Analyzer
# (ANALYZER-01) is a VST3 only, so only its VST3 package is taken. So the two
# installers can't drift apart, and installing from either updates the same receipts.
#
# Every plugin and the suite share the one version in VERSION at the repository root. The last
# line of the output is the path of the .pkg (default: build/suite-installer/Songsmith-Plugins-<version>-macOS.pkg).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
eq_pkg="" fet_pkg="" max_pkg="" comp_pkg="" opt_pkg="" peq_pkg="" bus_pkg="" desk_pkg="" pla_pkg="" ana_pkg="" suite_version="" out_dir="$repo/build/suite-installer"

while [ $# -gt 0 ]; do
    case "$1" in
        --eq-pkg)        eq_pkg="$2"; shift 2 ;;
        --fet-pkg)       fet_pkg="$2"; shift 2 ;;
        --max-pkg)       max_pkg="$2"; shift 2 ;;
        --comp-pkg)      comp_pkg="$2"; shift 2 ;;
        --opt-pkg)       opt_pkg="$2"; shift 2 ;;
        --peq-pkg)       peq_pkg="$2"; shift 2 ;;
        --bus-pkg)       bus_pkg="$2"; shift 2 ;;
        --desk-pkg)      desk_pkg="$2"; shift 2 ;;
        --pla-pkg)       pla_pkg="$2"; shift 2 ;;
        --ana-pkg)       ana_pkg="$2"; shift 2 ;;
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
comp_version="$shared_version"
opt_version="$shared_version"
peq_version="$shared_version"
bus_version="$shared_version"
desk_version="$shared_version"
pla_version="$shared_version"
ana_version="$shared_version"

# Each plugin's own .pkg: given, or built now from that plugin's Release build.
[ -z "$eq_pkg" ]  && eq_pkg=$("$repo/eq/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$fet_pkg" ] && fet_pkg=$("$repo/analog-compressor/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$max_pkg" ] && max_pkg=$("$repo/maximiser/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$comp_pkg" ] && comp_pkg=$("$repo/compressor/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$opt_pkg" ] && opt_pkg=$("$repo/optical-leveller/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$peq_pkg" ] && peq_pkg=$("$repo/pultec-eq/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$bus_pkg" ] && bus_pkg=$("$repo/bus-compressor/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$desk_pkg" ] && desk_pkg=$("$repo/analog-desk/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$pla_pkg" ] && pla_pkg=$("$repo/plugin-analyzer/installer/macos/build-installer.sh" | tail -n 1)
[ -z "$ana_pkg" ] && ana_pkg=$("$repo/audio-analyzer/installer/macos/build-installer.sh" | tail -n 1)

mkdir -p "$out_dir"
out_dir="$(cd "$out_dir" && pwd)"
work="$out_dir/macos-suite-work"
pkg="$out_dir/Songsmith-Plugins-$suite_version-macOS.pkg"
rm -rf "$work"
mkdir -p "$work/pkgs" "$work/resources"
rm -f "$pkg"

# Takes the component packages out of one product's .pkg, flat again, as they were built: VST3, Audio Unit and app for
# a plugin, the app alone for the Audio Analyzer.
#   take <product pkg> <component file prefix> [component ids, default "vst3 au app"]
min_macos=""
take() {
    local product="$1" prefix="$2" expanded="$work/expanded-$2" ids="${3:-vst3 au app}"
    if [ ! -f "$product" ]; then
        echo "Missing $product" >&2
        exit 1
    fi
    pkgutil --expand "$product" "$expanded"
    for id in $ids; do
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
take "$comp_pkg" songsmithcompressor
take "$opt_pkg" songsmithopticalleveller
take "$peq_pkg" songsmithpassiveeq
take "$bus_pkg" songsmithbuscompressor
take "$desk_pkg" songsmithanalogdesk
take "$pla_pkg" songsmithpluginanalyzer vst3
take "$ana_pkg" songsmithaudioanalyzer app
[ -z "$min_macos" ] && min_macos="10.13"

fill() {
    sed -e "s/@SUITE_VERSION@/$suite_version/g" -e "s/@EQ_VERSION@/$eq_version/g" \
        -e "s/@FET_VERSION@/$fet_version/g" -e "s/@MAX_VERSION@/$max_version/g" -e "s/@COMP_VERSION@/$comp_version/g" -e "s/@OPT_VERSION@/$opt_version/g" -e "s/@PEQ_VERSION@/$peq_version/g" -e "s/@BUS_VERSION@/$bus_version/g" -e "s/@DESK_VERSION@/$desk_version/g" -e "s/@PLA_VERSION@/$pla_version/g" -e "s/@ANA_VERSION@/$ana_version/g" \
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

echo "Suite version: $suite_version (Songsmith EQ $eq_version, Songsmith Analog Compressor $fet_version, Songsmith Maximiser $max_version, Songsmith Compressor $comp_version, Songsmith Optical Leveller $opt_version, Songsmith Passive EQ $peq_version, Songsmith Bus Compressor $bus_version, Songsmith Analog Desk $desk_version, Songsmith Plugin Analyzer $pla_version, Songsmith Audio Analyzer $ana_version)"
echo "Minimum macOS: $min_macos"
echo "Installer:     $pkg"
echo "$pkg"
