#!/bin/bash
# start_game.sh <tag> - fresh launch for interactive sessions; logcat streams to analysis/logs/<tag>.log
# in the background (stop with: kill $(cat analysis/logs/<tag>.pid)).
cd "$(dirname "$0")/.."
TAG="${1:?tag}"
DEV="${DEV:-${ANDROID_SERIAL:?Set ANDROID_SERIAL or DEV to the target device}}"
PKG=VRLOC.com.Armature.VR4
A="adb -s $DEV"
mkdir -p analysis/logs analysis/shots
$A shell am force-stop $PKG
$A shell svc power stayon true
$A shell input keyevent KEYCODE_WAKEUP
$A logcat -G 16M >/dev/null 2>&1; $A logcat -c
$A shell am start -n $PKG/com.epicgames.ue4.GameActivity >/dev/null 2>&1
nohup $A logcat -v time RE4FIX-N:V DEBUG:I AndroidRuntime:E libc:F *:S > "analysis/logs/$TAG.log" 2>&1 &
echo $! > "analysis/logs/$TAG.pid"
echo "started $TAG (logcat pid $(cat analysis/logs/$TAG.pid))"
