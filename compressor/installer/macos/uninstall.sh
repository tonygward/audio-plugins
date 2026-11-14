#!/bin/bash
# Uninstalls Songsmith Compressor that was installed with the macOS .pkg installer.
#
#   sudo installer/macos/uninstall.sh      (asks for your password if you leave out sudo)
#
# Removes the three bundles the installer puts down and makes macOS forget the package
# receipts. Nothing else in the plug-in folders, and nothing in your home folder, is touched.
# Quit your DAW first.
set -u

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Library and /Applications needs an administrator password."
    exec sudo /bin/bash "$0" "$@"
fi

for bundle in "/Library/Audio/Plug-Ins/VST3/Songsmith Compressor.vst3" \
              "/Library/Audio/Plug-Ins/Components/Songsmith Compressor.component" \
              "/Applications/Songsmith Compressor.app"; do
    if [ -e "$bundle" ]; then
        rm -rf "$bundle" && echo "Removed $bundle"
    fi
done

# The receipts the installer recorded (see build-installer.sh; never change these ids).
for id in com.songsmithstudio.songsmithcompressor.vst3 com.songsmithstudio.songsmithcompressor.au com.songsmithstudio.songsmithcompressor.app; do
    if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
        pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
    fi
done

# Make Audio Unit hosts notice the component has gone.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "Songsmith Compressor is uninstalled."
