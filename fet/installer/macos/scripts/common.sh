#!/bin/sh
# Shared helpers for the Songsmith Analog Compressor installer scripts (same as Songsmith EQ's).
# macOS Installer runs the scripts as root with $1 = package, $2 = install location,
# $3 = target volume. Every path removed is spelled out in full; nothing of Songsmith EQ's and
# nothing in any presets folder is ever touched.

target_root() {
    v="${1:-/}"
    printf '%s' "${v%/}"
}

console_user_home() {
    u=$(stat -f%Su /dev/console 2>/dev/null)
    case "$u" in
        ""|root|loginwindow|_mbsetupuser) return 0 ;;
    esac
    h=$(dscl . -read "/Users/$u" NFSHomeDirectory 2>/dev/null | awk '{print $2}')
    [ -n "$h" ] && [ -d "$h" ] && printf '%s' "$h"
}

# Removes one of THIS product's bundles. Refuses anything that isn't an absolute path to a
# "Songsmith Analog Compressor" bundle, so a mistake can't delete another plug-in or a folder.
remove_bundle() {
    case "$1" in
        /*"/Songsmith Analog Compressor.vst3"|/*"/Songsmith Analog Compressor.component"|/*"/Songsmith Analog Compressor.app") ;;
        *) echo "Songsmith Analog Compressor installer: refusing to remove '$1'" >&2; return 0 ;;
    esac
    if [ -e "$1" ] || [ -L "$1" ]; then
        rm -rf "$1" && echo "Songsmith Analog Compressor installer: removed $1"
    fi
    return 0
}
