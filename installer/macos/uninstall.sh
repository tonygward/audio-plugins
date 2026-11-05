#!/bin/bash
# Uninstalls every Songsmith plugin and app the suite installer (or a plugin's own
# installer) can put on this Mac.
#
#   installer/macos/uninstall.sh          # lists what it will remove and asks first
#   installer/macos/uninstall.sh --yes    # no question (for scripts and CI)
#
# Removes the VST3 plugins, Audio Units and standalone apps of Songsmith EQ, Songsmith Analog
# Compressor and Songsmith Maximiser, and makes macOS forget their package receipts. Your
# presets in ~/Library/Songsmith Studio/<plugin>/Presets are NOT touched, and nothing else in the
# plugin folders is. Quit your recording software first. Asks for an administrator password.
set -u

yes=0
for arg in "$@"; do
    case "$arg" in
        --yes|-y) yes=1 ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

names=("Songsmith EQ" "Songsmith Analog Compressor" "Songsmith Maximiser")
# The package receipts the installers record (never change these ids).
prefixes=("com.songsmithstudio.songsmitheq" "com.songsmithstudio.songsmithanalogcompressor" "com.songsmithstudio.songsmithmaximiser")

bundles=()
for name in "${names[@]}"; do
    for b in "/Library/Audio/Plug-Ins/VST3/$name.vst3" \
             "/Library/Audio/Plug-Ins/Components/$name.component" \
             "/Applications/$name.app"; do
        [ -e "$b" ] && bundles+=("$b")
    done
done
receipts=()
for prefix in "${prefixes[@]}"; do
    for part in vst3 au app; do
        pkgutil --pkg-info "$prefix.$part" >/dev/null 2>&1 && receipts+=("$prefix.$part")
    done
done

if [ ${#bundles[@]} -eq 0 ] && [ ${#receipts[@]} -eq 0 ]; then
    echo "No Songsmith plugins are installed. Nothing to remove."
    exit 0
fi

echo "This removes:"
for b in "${bundles[@]+"${bundles[@]}"}"; do echo "  $b"; done
[ ${#receipts[@]} -gt 0 ] && echo "and forgets ${#receipts[@]} package receipt(s)."
echo "Your presets in ~/Library/Songsmith Studio are kept."

if [ "$yes" -ne 1 ]; then
    printf "Remove them? [y/N] "
    read -r answer
    case "$answer" in
        y|Y|yes|Yes) ;;
        *) echo "Nothing removed."; exit 0 ;;
    esac
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "Removing files from /Library and /Applications needs an administrator password."
    exec sudo /bin/bash "$0" --yes
fi

for b in "${bundles[@]+"${bundles[@]}"}"; do
    case "$b" in
        /Library/Audio/Plug-Ins/VST3/Songsmith*.vst3|/Library/Audio/Plug-Ins/Components/Songsmith*.component|/Applications/Songsmith*.app)
            rm -rf "$b" && echo "Removed $b" ;;
        *) echo "Refusing to remove '$b'" >&2 ;;
    esac
done
for id in "${receipts[@]+"${receipts[@]}"}"; do
    pkgutil --forget "$id" >/dev/null && echo "Forgot package receipt $id"
done

# Make Audio Unit hosts notice the components have gone.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo "Songsmith Plugins is uninstalled. Your presets were kept."
