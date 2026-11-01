"""Run actual Android gamepad lifecycle Java with counted InputManager/JNI seams."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JDK = Path(os.environ["JAVA_HOME"]) / "bin"
source = (ROOT / "analysis/android/com/epicgames/ue4/MobileGamepad.java").read_text(encoding="utf8")
seam = "private static native void nativeReset(int deviceId);"
assert source.count(seam) == 1
source = source.replace(seam, "private static void nativeReset(int deviceId) { MobileGamepadTest.reset(deviceId); }")
sources = {
    "com/epicgames/ue4/MobileGamepad.java": source,
    "android/content/Context.java": '''package android.content; public class Context {
        public static final String INPUT_SERVICE="input";
        public android.hardware.input.InputManager inputs;
        public Context getApplicationContext(){return this;}
        public Object getSystemService(String name){assert INPUT_SERVICE.equals(name);return inputs;}
    }''',
    "android/app/Activity.java": "package android.app; public class Activity extends android.content.Context {}",
    "android/os/Looper.java": "package android.os; public class Looper { public static Looper getMainLooper(){return new Looper();} }",
    "android/os/Handler.java": "package android.os; public class Handler { public Handler(Looper looper){assert looper!=null;} }",
    "android/hardware/input/InputManager.java": '''package android.hardware.input; public class InputManager {
        public interface InputDeviceListener {void onInputDeviceAdded(int id);void onInputDeviceChanged(int id);void onInputDeviceRemoved(int id);}
        public int registers,unregisters; public InputDeviceListener listener;
        public void registerInputDeviceListener(InputDeviceListener value,android.os.Handler handler){assert listener==null;listener=value;++registers;}
        public void unregisterInputDeviceListener(InputDeviceListener value){assert listener==value;listener=null;++unregisters;}
    }''',
    "com/epicgames/ue4/GameActivity.java": "package com.epicgames.ue4; public class GameActivity extends android.app.Activity {}",
    "com/epicgames/ue4/MobileGamepadTest.java": '''package com.epicgames.ue4;
    public class MobileGamepadTest {
        static int calls,last; static boolean pending;
        static void reset(int id){++calls;last=id;if(pending)throw new UnsatisfiedLinkError();}
        public static void main(String[] args){
            GameActivity game=new GameActivity(); android.app.Activity other=new android.app.Activity();
            MobileGamepad.apply(null);MobileGamepad.apply(other);MobileGamepad.pause(other);assert calls==0;
            MobileGamepad.apply(game);assert calls==0;
            game.inputs=new android.hardware.input.InputManager(); MobileGamepad.apply(game);
            assert game.inputs.registers==1&&game.inputs.listener!=null;
            MobileGamepad.apply(game);assert game.inputs.registers==1;
            game.inputs.listener.onInputDeviceAdded(3);assert calls==0;
            game.inputs.listener.onInputDeviceChanged(3);assert calls==1&&last==3;
            game.inputs.listener.onInputDeviceRemoved(3);assert calls==2&&last==3;
            MobileGamepad.pause(other);assert calls==2&&game.inputs.unregisters==0;
            MobileGamepad.pause(game);assert calls==3&&last==-1&&game.inputs.unregisters==1&&game.inputs.listener==null;
            pending=true;MobileGamepad.pause(game);assert calls==4&&game.inputs.unregisters==1;
            pending=false;MobileGamepad.apply(game);assert game.inputs.registers==2;
            MobileGamepad.pause(game);assert calls==5&&game.inputs.unregisters==2;
            System.out.println("PASS: production pad lifecycle, duplicate resume, removal/change callbacks, foreign Activity isolation, pause release, unregister/resume and pre-native initialization");
        }
    }''',
}
with tempfile.TemporaryDirectory(prefix="re4-java-gamepad-") as temporary:
    work = Path(temporary)
    paths = []
    for name, body in sources.items():
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(body, encoding="utf8")
        paths.append(str(target))
    subprocess.run([str(JDK / "javac.exe"), "--release", "8", "-encoding", "UTF-8", "-d", str(work), *paths], check=True)
    subprocess.run([str(JDK / "java.exe"), "-ea", "-cp", str(work), "com.epicgames.ue4.MobileGamepadTest"], check=True)
