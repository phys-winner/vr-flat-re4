"""Execute actual MobileSettings gyro/lifecycle Java with Android doubles and counted JNI seams."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JDK = Path(os.path.join(os.environ['JAVA_HOME'], 'bin'))
SOURCES = {
    'android/content/Context.java': '''package android.content; public class Context {
        public static final String SENSOR_SERVICE="sensor", DISPLAY_SERVICE="display";
        public Context getApplicationContext(){return this;} public Object getSystemService(String name){return null;}
    }''',
    'android/app/Activity.java': '''package android.app; public class Activity extends android.content.Context {
        public boolean focus=true; public final android.hardware.SensorManager sensors=new android.hardware.SensorManager();
        public final android.view.WindowManager window=new android.view.WindowManager();
        public boolean hasWindowFocus(){return focus;} public android.view.WindowManager getWindowManager(){return window;}
        public Object getSystemService(String name){return SENSOR_SERVICE.equals(name)?sensors:new android.hardware.display.DisplayManager();}
        public android.content.res.Resources getResources(){return new android.content.res.Resources();}
    }''',
    'android/content/res/Resources.java': 'package android.content.res; public class Resources { public android.util.DisplayMetrics getDisplayMetrics(){return new android.util.DisplayMetrics();} }',
    'android/util/DisplayMetrics.java': 'package android.util; public class DisplayMetrics { public float density=2; }',
    'android/util/Log.java': 'package android.util; public class Log { public static int w(String tag,String message){return 0;} }',
    'android/view/WindowManager.java': 'package android.view; public class WindowManager { public final Display display=new Display(); public Display getDefaultDisplay(){return display;} }',
    'android/view/Display.java': 'package android.view; public class Display { public int rotation; public int getRotation(){return rotation;} public int getDisplayId(){return 0;} public float getRefreshRate(){return 60;} }',
    'android/hardware/Sensor.java': 'package android.hardware; public class Sensor { public static final int TYPE_GYROSCOPE=4; }',
    'android/hardware/SensorEvent.java': 'package android.hardware; public class SensorEvent { public long timestamp; public float[] values; public SensorEvent(long stamp,float... data){timestamp=stamp;values=data;} }',
    'android/hardware/SensorEventListener.java': 'package android.hardware; public interface SensorEventListener { void onAccuracyChanged(Sensor sensor,int accuracy); void onSensorChanged(SensorEvent event); }',
    'android/hardware/SensorManager.java': '''package android.hardware; public class SensorManager {
        public static final int SENSOR_DELAY_GAME=1; public boolean present=true,registerOk=true;
        public int registers,unregisters; public SensorEventListener listener;
        public Sensor getDefaultSensor(int type){if(type!=Sensor.TYPE_GYROSCOPE)throw new AssertionError();return present?new Sensor():null;}
        public boolean registerListener(SensorEventListener value,Sensor sensor,int delay){if(sensor==null||delay!=SENSOR_DELAY_GAME)throw new AssertionError();registers++;if(registerOk)listener=value;return registerOk;}
        public void unregisterListener(SensorEventListener value){if(listener!=value)throw new AssertionError();unregisters++;listener=null;}
    }''',
    'android/hardware/display/DisplayManager.java': 'package android.hardware.display; public class DisplayManager { public interface DisplayListener {void onDisplayAdded(int id);void onDisplayRemoved(int id);void onDisplayChanged(int id);} public void registerDisplayListener(DisplayListener listener,android.os.Handler handler){} }',
    'android/os/Looper.java': 'package android.os; public class Looper { public static Looper getMainLooper(){return new Looper();} }',
    'android/os/Handler.java': '''package android.os; public class Handler {
        public Runnable pending; public long delay; public Handler(Looper looper){}
        public boolean post(Runnable action){action.run();return true;}
        public boolean postDelayed(Runnable action,long ms){pending=action;delay=ms;return true;}
        public void removeCallbacks(Runnable action){if(pending==action)pending=null;}
    }''',
    'com/epicgames/ue4/GameActivity.java': 'package com.epicgames.ue4; public class GameActivity extends android.app.Activity {}',
}
production = (ROOT / 'analysis/android/com/epicgames/ue4/MobileSettings.java').read_text(encoding='utf8')
for name, arguments, result, body in (
    ('nativeDisplay', 'int milliHz', 'void', ''),
    ('nativeDensity', 'int milliDensity', 'void', ''),
    ('nativeGyroActive', '', 'boolean', 'return MobileGyroTest.active();'),
    ('nativeGyro', 'float yaw, float pitch', 'void', 'MobileGyroTest.gyro(yaw,pitch);'),
):
    seam = f'public static native {result} {name}({arguments});'
    assert production.count(seam) == 1, name
    production = production.replace(seam, f'public static {result} {name}({arguments}) {{{body}}}')
SOURCES['com/epicgames/ue4/MobileSettings.java'] = production
with tempfile.TemporaryDirectory(prefix='re4-java-gyro-') as temporary:
    work = Path(temporary)
    paths = []
    for name, source in SOURCES.items():
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(source, encoding='utf8')
        paths.append(str(target))
    paths.append(str(ROOT / 'analysis/research/MobileGyroTest.java'))
    subprocess.run([str(JDK/'javac.exe'), '--release', '8', '-encoding', 'UTF-8', '-d', str(work), *paths], check=True)
    subprocess.run([str(JDK/'java.exe'), '-ea', '-cp', str(work), 'com.epicgames.ue4.MobileGyroTest'], check=True)
print('PASS: actual production Java gyro listener/watch/apply/pause; only four JNI declarations replaced by host counters')
