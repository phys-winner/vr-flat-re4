"""Execute actual MobileSettings gyro/video/lifecycle Java with Android doubles and counted JNI seams."""
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
        public final android.view.Window appWindow=new android.view.Window();
        public final android.hardware.display.DisplayManager displays=new android.hardware.display.DisplayManager();
        public boolean hasWindowFocus(){return focus;} public android.view.WindowManager getWindowManager(){return window;}
        public android.view.Window getWindow(){return appWindow;}
        public Object getSystemService(String name){return SENSOR_SERVICE.equals(name)?sensors:displays;}
        public android.content.res.Resources getResources(){return new android.content.res.Resources();}
    }''',
    'android/content/res/Resources.java': 'package android.content.res; public class Resources { public android.util.DisplayMetrics getDisplayMetrics(){return new android.util.DisplayMetrics();} }',
    'android/util/DisplayMetrics.java': 'package android.util; public class DisplayMetrics { public float density=2; }',
    'android/util/Log.java': 'package android.util; public class Log { public static int w(String tag,String message){return 0;} public static int i(String tag,String message){return 0;} }',
    'android/view/WindowManager.java': 'package android.view; public class WindowManager { public static class LayoutParams { public int preferredDisplayModeId; } public final Display display=new Display(); public Display getDefaultDisplay(){return display;} }',
    'android/view/Display.java': '''package android.view; public class Display {
        public int rotation; public int getRotation(){return rotation;} public int getDisplayId(){return 0;} public float getRefreshRate(){return mode.hz;}
        public Mode mode=new Mode(1,2400,1080,60); public Mode[] modes={mode};
        public Mode getMode(){return mode;} public Mode[] getSupportedModes(){return modes;}
        public static class Mode {
            public int id,width,height;public float hz;
            public Mode(int id,int width,int height,float hz){this.id=id;this.width=width;this.height=height;this.hz=hz;}
            public int getPhysicalWidth(){return width;} public int getPhysicalHeight(){return height;}
            public float getRefreshRate(){return hz;} public int getModeId(){return id;}
        }
    }''',
    'android/view/View.java': 'package android.view; public class View { public int width=2400,height=1080;public int getWidth(){return width;} public int getHeight(){return height;} }',
    'android/view/ViewGroup.java': 'package android.view; public class ViewGroup extends View { public SurfaceView surface=new SurfaceView();public View[] children={surface};public int getChildCount(){return children.length;} public View getChildAt(int index){return children[index];} }',
    'android/view/Window.java': '''package android.view; public class Window {
        private WindowManager.LayoutParams params=new WindowManager.LayoutParams();public final ViewGroup decor=new ViewGroup();public int attributes;
        public WindowManager.LayoutParams getAttributes(){return params;} public void setAttributes(WindowManager.LayoutParams p){params=p;attributes++;}
        public View getDecorView(){return decor;}
    }''',
    'android/view/Surface.java': 'package android.view; public class Surface { public static final int FRAME_RATE_COMPATIBILITY_DEFAULT=0;public boolean valid=true;public int requests;public float rate;public boolean isValid(){return valid;}public void setFrameRate(float rate,int compatibility){if(!valid||compatibility!=0)throw new AssertionError();requests++;this.rate=rate;} }',
    'android/view/SurfaceHolder.java': 'package android.view; public class SurfaceHolder { public final Surface surface=new Surface();public Surface getSurface(){return surface;} }',
    'android/view/SurfaceView.java': 'package android.view; public class SurfaceView extends View { public final SurfaceHolder holder=new SurfaceHolder();public SurfaceHolder getHolder(){return holder;} }',
    'android/os/Build.java': 'package android.os; public class Build { public static class VERSION { public static int SDK_INT=30; } }',
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
    'android/hardware/display/DisplayManager.java': 'package android.hardware.display; public class DisplayManager { public DisplayListener listener;public int registers;public interface DisplayListener {void onDisplayAdded(int id);void onDisplayRemoved(int id);void onDisplayChanged(int id);} public void registerDisplayListener(DisplayListener listener,android.os.Handler handler){registers++;this.listener=listener;} public void changed(int id){if(listener!=null)listener.onDisplayChanged(id);} }',
    'android/os/Looper.java': 'package android.os; public class Looper { public static Looper getMainLooper(){return new Looper();} }',
    'android/os/Handler.java': '''package android.os; public class Handler {
        public Runnable pending; public long delay; public Handler(Looper looper){}
        public boolean post(Runnable action){action.run();return true;}
        public boolean postDelayed(Runnable action,long ms){pending=action;delay=ms;return true;}
        public void removeCallbacks(Runnable action){if(pending==action)pending=null;}
    }''',
    'com/epicgames/ue4/GameActivity.java': 'package com.epicgames.ue4; public class GameActivity extends android.app.Activity {}',
    'com/epicgames/ue4/MobileGamepad.java': 'package com.epicgames.ue4; public class MobileGamepad { public static void apply(android.app.Activity activity){} public static void pause(android.app.Activity activity){} }',
}
production = (ROOT / 'analysis/android/com/epicgames/ue4/MobileSettings.java').read_text(encoding='utf8')
for name, arguments, result, body in (
    ('nativeDisplay', 'int milliHz', 'void', 'MobileVideoTest.displayHz=milliHz;'),
    ('nativeDensity', 'int milliDensity', 'void', 'MobileVideoTest.density=milliDensity;'),
    ('nativeDisplayModes', 'long modes', 'void', 'MobileVideoTest.displayModes=modes;MobileVideoTest.modeReports++;'),
    ('nativeFrameLimit', '', 'int', 'return MobileVideoTest.frameLimit;'),
    ('nativeRenderPercent', '', 'int', 'return MobileVideoTest.renderPercent;'),
    ('nativeViewportSizes', 'int viewWidth, int viewHeight, int renderWidth, int renderHeight', 'void', 'MobileVideoTest.viewport(viewWidth,viewHeight,renderWidth,renderHeight);'),
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
    paths.append(str(ROOT / 'analysis/research/MobileVideoTest.java'))
    subprocess.run([str(JDK/'javac.exe'), '--release', '8', '-encoding', 'UTF-8', '-d', str(work), *paths], check=True)
    subprocess.run([str(JDK/'java.exe'), '-ea', '-cp', str(work), 'com.epicgames.ue4.MobileGyroTest'], check=True)
    subprocess.run([str(JDK/'java.exe'), '-ea', '-cp', str(work), 'com.epicgames.ue4.MobileVideoTest'], check=True)
print('PASS: actual production Java gyro/video/watch/apply/pause; eight JNI declarations replaced, original sensor assertions retained with counted display/surface lifecycle doubles')
