#!/bin/bash
# Uninstalls Songsmith EQ that was installed with the macOS .pkg installer.
#
#   sudo installer/macos/uninstall.sh      (asks for your password if you leave out sudo)
#
# Removes the three bundles the installer puts down and makes macOS forget the package
# receipts. Your presets in ~/Library/Songsmith Studio/Songsmith EQ/Presets (and an old
# TW Pro EQ presets folder) are NOT touched, and nothing else in the plug-in folders is.
# Quit your DAW first.
set -u

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Library and /Applications needs an administrator password."
    exec sudo /bin/bash "$0" "$@"
fi

for bundle in "/Library/Audio/Plug-Ins/VST3/Songsmith EQ.vst3" \
              "/Library/Audio/Plug-Ins/Components/Songsmith EQ.component" \
              "/Applications/Songsmith EQ.app"; do
    if [ -e "$bundle" ]; then
        rm -rf "$bundle" && echo "Removed $bundle"
    fi
done

# The receipts the installer recorded (see build-installer.sh; never change these ids).
for id in com.songsmithstudio.songsmitheq.vst3 com.songsmithstudio.songsmitheq.au com.songsmithstudio.songsmitheq.app; do
    if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
        pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
    fi
done

# Make Audio Unit hosts notice the component has gone.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "Songsmith EQ is uninstalled. Your presets were kept."
