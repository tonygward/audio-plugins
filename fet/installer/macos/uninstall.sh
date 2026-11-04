#!/bin/bash
# Uninstalls Songsmith Analog Compressor that was installed with the macOS .pkg installer.
#
#   sudo installer/macos/uninstall.sh      (asks for your password if you leave out sudo)
#
# Removes the three bundles the installer puts down and makes macOS forget the package receipts.
# Your presets in ~/Library/Songsmith Studio/Songsmith Analog Compressor/Presets are NOT touched,
# and neither is Songsmith EQ. Quit your DAW first.
set -u

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Library and /Applications needs an administrator password."
    exec sudo /bin/bash "$0" "$@"
fi

for bundle in "/Library/Audio/Plug-Ins/VST3/Songsmith Analog Compressor.vst3" \
              "/Library/Audio/Plug-Ins/Components/Songsmith Analog Compressor.component" \
              "/Applications/Songsmith Analog Compressor.app"; do
    if [ -e "$bundle" ]; then
        rm -rf "$bundle" && echo "Removed $bundle"
    fi
done

for id in com.songsmithstudio.songsmithanalogcompressor.vst3 \
          com.songsmithstudio.songsmithanalogcompressor.au \
          com.songsmithstudio.songsmithanalogcompressor.app; do
    if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
        pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
    fi
done

killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "Songsmith Analog Compressor is uninstalled. Your presets (and Songsmith EQ) were kept."
