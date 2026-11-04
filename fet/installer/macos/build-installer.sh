#!/bin/bash
# Builds the Songsmith Analog Compressor macOS installer (an unsigned flat .pkg) from a Release
# build, with Apple's pkgbuild and productbuild. Same pattern as eq/installer/macos.
#
#   installer/macos/build-installer.sh                         # -> build/installer/Songsmith-Analog-Compressor-<version>-macOS.pkg
#   installer/macos/build-installer.sh --build-dir other/dir   # a different CMake build folder
#   installer/macos/build-installer.sh --config Debug
#
# Build the plug-in first (from fet/): cmake --build build --config Release
# The version comes from project(... VERSION x.y.z) in fet/CMakeLists.txt. The last line of the
# output is the path of the .pkg.
#
#   VST3  -> /Library/Audio/Plug-Ins/VST3/Songsmith Analog Compressor.vst3
#   AU    -> /Library/Audio/Plug-Ins/Components/Songsmith Analog Compressor.component
#   App   -> /Applications/Songsmith Analog Compressor.app
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fet_dir="$(cd "$here/../.." && pwd)"
build_dir="$fet_dir/build"
config="Release"
name="Songsmith Analog Compressor"

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) build_dir="$2"; shift 2 ;;
        --config)    config="$2"; shift 2 ;;
        -h|--help)   sed -n '2,15p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
build_dir="$(cd "$build_dir" && pwd)"

version=$(sed -nE 's/^[[:space:]]*project\([[:space:]]*[A-Za-z0-9_]+[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
          "$fet_dir/CMakeLists.txt" | head -n 1)
if [ -z "$version" ]; then
    echo "Couldn't find 'project(... VERSION x.y.z)' in fet/CMakeLists.txt" >&2
    exit 1
fi

artefacts="$build_dir/SongsmithAnalogCompressor_artefacts/$config"
vst3="$artefacts/VST3/$name.vst3"
au="$artefacts/AU/$name.component"
app="$artefacts/Standalone/$name.app"
for b in "$vst3" "$au" "$app"; do
    if [ ! -d "$b" ]; then
        echo "Missing '$b'. Build first: cmake --build build --config $config" >&2
        exit 1
    fi
done

# NEVER CHANGE THESE package identifiers (receipts). They differ from Songsmith EQ's
# (com.songsmithstudio.songsmitheq.*), so the two products never replace each other.
id_prefix="com.songsmithstudio.songsmithanalogcompressor"

min_macos=""
for bin in "$vst3/Contents/MacOS/$name" "$au/Contents/MacOS/$name" "$app/Contents/MacOS/$name"; do
    for arch in $(lipo -archs "$bin"); do
        m=$(otool -arch "$arch" -l "$bin" | awk '/LC_BUILD_VERSION/ { f = 1 } f && $1 == "minos" { print $2; exit }')
        [ -z "$m" ] && m=$(otool -arch "$arch" -l "$bin" | awk '/LC_VERSION_MIN_MACOSX/ { f = 1 } f && $1 == "version" { print $2; exit }')
        [ -n "$m" ] && min_macos=$(printf '%s\n%s\n' "$min_macos" "$m" | sed '/^$/d' | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1)
    done
done
[ -z "$min_macos" ] && min_macos="10.13"

out_dir="$build_dir/installer"
work="$out_dir/macos-work"
pkg="$out_dir/Songsmith-Analog-Compressor-$version-macOS.pkg"
rm -rf "$work"
mkdir -p "$work/pkgs" "$out_dir"
rm -f "$pkg"

component() {
    local id="$1" bundle="$2" location="$3"
    local root="$work/root-$id" scripts="$work/scripts-$id" plist="$work/$id.plist"

    mkdir -p "$root" "$scripts"
    ditto --norsrc --noextattr --noqtn "$bundle" "$root/$(basename "$bundle")"
    find "$root" -name .DS_Store -delete
    xattr -cr "$root"
    chmod -R u+rwX,go+rX,go-w "$root"

    cp "$here/scripts/common.sh" "$here/scripts/$id/"* "$scripts/"
    chmod 755 "$scripts"/*

    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist" 2>/dev/null ||
            /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
        /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$plist" 2>/dev/null ||
            /usr/libexec/PlistBuddy -c "Add :$i:BundleOverwriteAction string upgrade" "$plist"
        i=$((i + 1))
    done

    pkgbuild --root "$root" \
             --component-plist "$plist" \
             --identifier "$id_prefix.$id" \
             --version "$version" \
             --install-location "$location" \
             --scripts "$scripts" \
             --ownership recommended \
             "$work/pkgs/songsmithanalogcompressor-$id.pkg"
}

component vst3 "$vst3" "/Library/Audio/Plug-Ins/VST3"
component au   "$au"   "/Library/Audio/Plug-Ins/Components"
component app  "$app"  "/Applications"

sed -e "s/@VERSION@/$version/g" -e "s/@MIN_MACOS@/$min_macos/g" "$here/distribution.xml" > "$work/distribution.xml"

productbuild --distribution "$work/distribution.xml" \
             --package-path "$work/pkgs" \
             --resources "$here/resources" \
             "$pkg"

echo "Version:       $version"
echo "Minimum macOS: $min_macos"
echo "Installer:     $pkg"
echo "$pkg"
