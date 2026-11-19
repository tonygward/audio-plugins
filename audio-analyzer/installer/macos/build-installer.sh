#!/bin/bash
# Builds the Songsmith Audio Analyzer macOS installer (an unsigned flat .pkg) from an existing Release build,
# with Apple's own pkgbuild and productbuild (installed with Xcode).
#
#   installer/macos/build-installer.sh                         # -> build/installer/Songsmith-Audio-Analyzer-<version>-macOS.pkg
#   installer/macos/build-installer.sh --build-dir other/dir   # a different CMake build folder
#   installer/macos/build-installer.sh --config Debug
#
# Build the app first (from audio-analyzer/): cmake --build build --config Release
# The version comes from VERSION in the repository root. The last line of the output is the path of the .pkg.
#
# The Audio Analyzer is an application, not a plugin, so there is one component package only:
#   App  -> /Applications/Songsmith Audio Analyzer.app
# (No VST3 and no Audio Unit.) The suite installer takes this same component package, as it does for every plugin.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
ana_dir="$(cd "$here/../.." && pwd)"
build_dir="$ana_dir/build"
config="Release"

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) build_dir="$2"; shift 2 ;;
        --config)    config="$2"; shift 2 ;;
        -h|--help)   sed -n '2,13p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
build_dir="$(cd "$build_dir" && pwd)"

# The one version number for every Songsmith product is in VERSION at the repository root.
version=$(tr -d '[:space:]' < "$here/../../../VERSION")
if ! printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "VERSION must hold one version as x.y.z (found '$version')" >&2
    exit 1
fi

app="$build_dir/SongsmithAudioAnalyzer_artefacts/$config/Songsmith Audio Analyzer.app"
if [ ! -d "$app" ]; then
    echo "Missing '$app'. Build first: cmake --build build --config $config" >&2
    exit 1
fi

# NEVER CHANGE this package identifier prefix. They're the receipts pkgutil keeps, which is how macOS (and
# uninstall.sh) recognise an existing install. It isn't the app's BUNDLE_ID.
id_prefix="com.songsmithstudio.songsmithaudioanalyzer"

# Minimum macOS: the highest deployment target ("minos") of any architecture of the app's binary, so the
# installer refuses a Mac the app couldn't run on anyway.
min_macos=""
bin="$app/Contents/MacOS/Songsmith Audio Analyzer"
for arch in $(lipo -archs "$bin"); do
    m=$(otool -arch "$arch" -l "$bin" | awk '/LC_BUILD_VERSION/ { f = 1 } f && $1 == "minos" { print $2; exit }')
    [ -z "$m" ] && m=$(otool -arch "$arch" -l "$bin" | awk '/LC_VERSION_MIN_MACOSX/ { f = 1 } f && $1 == "version" { print $2; exit }')
    [ -n "$m" ] && min_macos=$(printf '%s\n%s\n' "$min_macos" "$m" | sed '/^$/d' | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1)
done
[ -z "$min_macos" ] && min_macos="10.13"

out_dir="$build_dir/installer"
work="$out_dir/macos-work"
pkg="$out_dir/Songsmith-Audio-Analyzer-$version-macOS.pkg"
rm -rf "$work"
mkdir -p "$work/pkgs" "$out_dir"
rm -f "$pkg"

# Builds the component package: stages the app on its own, strips Finder and quarantine metadata, makes it
# non-relocatable and owned by root:wheel.
id="app"
root="$work/root-$id" scripts="$work/scripts-$id" plist="$work/$id.plist"
mkdir -p "$root" "$scripts"
ditto --norsrc --noextattr --noqtn "$app" "$root/$(basename "$app")"
find "$root" -name .DS_Store -delete
xattr -cr "$root"
chmod -R u+rwX,go+rX,go-w "$root"

cp "$here/scripts/common.sh" "$here/scripts/$id/"* "$scripts/"
chmod 755 "$scripts"/*

# Without this, Installer looks for the app anywhere on disk (by its bundle ID) and "upgrades" a copy the user
# moved elsewhere instead of installing to the right folder.
pkgbuild --analyze --root "$root" "$plist" >/dev/null
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist" 2>/dev/null ||
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$plist" 2>/dev/null ||
        /usr/libexec/PlistBuddy -c "Add :$i:BundleOverwriteAction string upgrade" "$plist"
    # A release numbered below an installed build must still replace it: do not skip bundles by version.
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsVersionChecked false" "$plist" 2>/dev/null ||
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsVersionChecked bool false" "$plist"
    i=$((i + 1))
done

pkgbuild --root "$root" \
         --component-plist "$plist" \
         --identifier "$id_prefix.$id" \
         --version "$version" \
         --install-location "/Applications" \
         --scripts "$scripts" \
         --ownership recommended \
         "$work/pkgs/songsmithaudioanalyzer-$id.pkg"

sed -e "s/@VERSION@/$version/g" -e "s/@MIN_MACOS@/$min_macos/g" "$here/distribution.xml" > "$work/distribution.xml"

productbuild --distribution "$work/distribution.xml" \
             --package-path "$work/pkgs" \
             --resources "$here/resources" \
             "$pkg"

echo "Version:       $version"
echo "Minimum macOS: $min_macos"
echo "Installer:     $pkg"
echo "$pkg"
