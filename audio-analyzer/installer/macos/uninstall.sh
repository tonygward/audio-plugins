#!/bin/bash
# Uninstalls Songsmith Audio Analyzer that was installed with the macOS .pkg installer.
#
#   sudo installer/macos/uninstall.sh      (asks for your password if you leave out sudo)
#
# Removes the app the installer puts down and makes macOS forget the package receipt. Nothing else in
# /Applications, and nothing in your home folder, is touched. Quit the app first.
set -u

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Applications needs an administrator password."
    exec sudo /bin/bash "$0" "$@"
fi

app="/Applications/Songsmith Audio Analyzer.app"
if [ -e "$app" ]; then
    rm -rf "$app" && echo "Removed $app"
fi

# The receipt the installer recorded (see build-installer.sh; never change this id).
id="com.songsmithstudio.songsmithaudioanalyzer.app"
if pkgutil --pkg-info "$id" >/dev/null 2>&1; then
    pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
fi

echo "Songsmith Audio Analyzer is uninstalled."
