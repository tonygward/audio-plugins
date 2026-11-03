#!/bin/sh
# Shared helpers for the Songsmith EQ installer's preinstall/postinstall scripts.
# build-installer.sh copies this file next to each component's scripts.
#
# macOS Installer runs the scripts as root with:
#   $1 = path to the component package
#   $2 = install location (for example /Library/Audio/Plug-Ins/VST3)
#   $3 = target volume ("/" for the startup disk)
#
# Every path removed here is spelled out in full. Nothing else in the plug-in folders,
# and nothing in the user's presets folder, is ever touched.

# The target volume without a trailing slash: "" for the startup disk, "/Volumes/X" otherwise.
target_root() {
    v="${1:-/}"
    printf '%s' "${v%/}"
}

# The home folder of the user logged in at the console (not $USER, which is root here).
# Prints nothing when nobody real is logged in (login window, setup assistant, a headless
# machine), so the per-user clean-up is simply skipped.
console_user_home() {
    u=$(stat -f%Su /dev/console 2>/dev/null)
    case "$u" in
        ""|root|loginwindow|_mbsetupuser) return 0 ;;
    esac
    h=$(dscl . -read "/Users/$u" NFSHomeDirectory 2>/dev/null | awk '{print $2}')
    [ -n "$h" ] && [ -d "$h" ] && printf '%s' "$h"
}

# Remove one bundle if it exists. Refuses anything that isn't an absolute path ending in
# one of the three bundle extensions, so a mistake here can't delete a folder.
remove_bundle() {
    case "$1" in
        /*.vst3|/*.component|/*.app) ;;
        *) echo "Songsmith EQ installer: refusing to remove '$1'" >&2; return 0 ;;
    esac
    if [ -e "$1" ] || [ -L "$1" ]; then
        rm -rf "$1" && echo "Songsmith EQ installer: removed $1"
    fi
    return 0
}
