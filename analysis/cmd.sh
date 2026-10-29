#!/bin/bash
# cmd.sh "<command>[; <command> ...]" [wait_s] [log_lines] - send commands to the in-game dev console.
# Commands: btn <A|B|X|Y|MENU|L3|R3|RT|RG|LT|LG> <ms> | stick <l|r> <x> <y> <ms> | hand <l|r> <x> <y> <z> | hand <l|r> off
#           swing <ms> | force <holster-substring> <ms> | fhover <grip-owner-substring> <ms> [side] | look <yaw> <pitch>
#           holsters | grips | state | inspectphone | knifediag <0..5000 ms>
# Transport: local file -> adb push /data/local/tmp -> run-as cp into the app dir (stdin forwarding is unreliable here).
DEV="${DEV:-${ANDROID_SERIAL:?Set ANDROID_SERIAL or DEV to the target device}}"
PKG=VRLOC.com.Armature.VR4
TMP="${TMPDIR:-/tmp}/re4cmd.$$.txt"
printf '%s\n' "$1" | tr ';' '\n' > "$TMP"
MSYS_NO_PATHCONV=1 adb -s "$DEV" push "$(cygpath -w "$TMP")" /data/local/tmp/re4cmd.txt >/dev/null 2>&1
MSYS_NO_PATHCONV=1 adb -s "$DEV" shell run-as $PKG cp /data/local/tmp/re4cmd.txt ./re4cmd.txt
rm -f "$TMP"
sleep "${2:-1}"
if [ "${3:-8}" != "0" ]; then
    adb -s "$DEV" shell run-as $PKG cat re4fix.log | grep -aE "CMD|HOLSTER|HOVER|GRIP|PHONE|PAWN|BIO4|INPUTSTATE|RELOAD|BODY|KNIFE|AIM|[Pp][Aa][Uu][Ss][Ee]" | tail -"${3:-8}"
fi
