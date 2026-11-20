#!/bin/bash
# Uninstalls Songsmith Plugin Analyzer that was installed with the macOS .pkg installer.
#
#   sudo installer/macos/uninstall.sh      (asks for your password if you leave out sudo)
#
# Removes the VST3 plug-in the installer puts down and makes macOS forget the package
# receipt. Nothing else in the plug-in folders, and nothing in your home folder, is touched.
# Quit your DAW first.
set -u

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Library needs an administrator password."
    exec sudo /bin/bash "$0" "$@"
fi

for bundle in "/Library/Audio/Plug-Ins/VST3/Songsmith Plugin Analyzer.vst3"; do
    if [ -e "$bundle" ]; then
        rm -rf "$bundle" && echo "Removed $bundle"
    fi
done

# The receipt the installer recorded (see build-installer.sh; never change this id).
for id in com.songsmithstudio.songsmithpluginanalyzer.vst3; do
    if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
        pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
    fi
done

echo "Songsmith Plugin Analyzer is uninstalled."
