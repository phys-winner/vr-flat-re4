package com.epicgames.ue4;

import android.app.Activity;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.os.Build;
import android.view.Display;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import java.lang.ref.WeakReference;
import java.util.TreeSet;

/**
 * The native Options menu owns phone settings. Android supplies display capabilities,
 * startup render geometry, frame-rate requests and sensor/controller lifecycle.
 */
public final class MobileSettings {
    private static WeakReference<Activity> game = new WeakReference<Activity>(null);
    private static boolean displayListener, nativeWarning;
    private static final Handler gyroHandler = new Handler(Looper.getMainLooper());
    private static SensorManager sensors;
    private static boolean resumed, gyroListening;
    private static long gyroTimestamp;
    private static int appliedFrameLimit, reportedWidth, reportedHeight;
    private static final SensorEventListener gyroListener = new SensorEventListener() {
        public void onAccuracyChanged(Sensor sensor, int accuracy) {}
        public void onSensorChanged(SensorEvent event) {
            if (!resumed || !gyroListening) return;
            Activity activity = game.get();
            long previous = gyroTimestamp;
            gyroTimestamp = event.timestamp;
            if (activity == null || !activity.hasWindowFocus() || previous == 0) return;
            float dt = (event.timestamp - previous) * 0.000000001f;
            if (!(dt > 0 && dt <= .1f) || event.values.length < 3) return;
            int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
            float x = event.values[0], y = event.values[1];
            float yaw = rotation == 1 ? -x : rotation == 3 ? x : rotation == 2 ? -y : y;
            float pitch = rotation == 1 ? y : rotation == 3 ? -y : rotation == 2 ? -x : x;
            if (Math.abs(yaw) < .012218f) yaw = 0;
            if (Math.abs(pitch) < .012218f) pitch = 0;
            nativeGyro(yaw * dt, pitch * dt);
        }
    };
    private static final Runnable gyroWatch = new Runnable() {
        public void run() {
            Activity activity = game.get();
            boolean wanted = false;
            try {
                wanted = resumed && activity != null && activity.hasWindowFocus() && nativeGyroActive();
                if (resumed && activity != null) applyFrameRate(activity);
            }
            catch (UnsatisfiedLinkError pending) {}
            if (wanted && !gyroListening && sensors != null) {
                Sensor gyro = sensors.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
                gyroTimestamp = 0;
                gyroListening = gyro != null && sensors.registerListener(gyroListener, gyro, SensorManager.SENSOR_DELAY_GAME);
            } else if (!wanted && gyroListening) {
                sensors.unregisterListener(gyroListener);
                gyroListening = false;
                gyroTimestamp = 0;
            }
            if (resumed) gyroHandler.postDelayed(this, 100);
        }
    };

    private MobileSettings() {}

    public static native void nativeDisplay(int milliHz);
    public static native void nativeDensity(int milliDensity);
    public static native boolean nativeGyroActive();
    public static native void nativeGyro(float yaw, float pitch);
    public static native void nativeDisplayModes(long modes);
    public static native int nativeFrameLimit();
    public static native int nativeRenderPercent();
    public static native void nativeViewportSizes(int viewWidth, int viewHeight, int renderWidth, int renderHeight);

    public static void pause(Activity activity) {
        MobileGamepad.pause(activity);
        if (game.get() != activity) return;
        resumed = false;
        gyroHandler.removeCallbacks(gyroWatch);
        if (gyroListening && sensors != null) sensors.unregisterListener(gyroListener);
        gyroListening = false;
        gyroTimestamp = 0;
        appliedFrameLimit = 0;
    }

    /** Keep the native render extent fixed for this process; the compositor is created at that extent. */
    public static int[] surfaceSize(Activity activity, SurfaceView surface, int width, int height) {
        int percent = nativeRenderPercent();
        if (surface != null && surface.getWidth() > 0 && surface.getHeight() > 0) {
            width = surface.getWidth(); height = surface.getHeight();
        }
        if (percent != 50 && percent != 67 && percent != 75 && percent != 85) percent = 100;
        if (width < 32 || height < 32) return new int[] {width, height};
        // Native scene buffers round up to four and Android can round down to eight; share an extent both keep.
        int w = Math.max(32, (int) ((long) width * percent / 100) & ~7);
        int h = Math.max(32, (int) ((long) height * percent / 100) & ~7);
        nativeViewportSizes(width, height, w, h);
        if (w != reportedWidth || h != reportedHeight) {
            reportedWidth = w; reportedHeight = h;
            Log.i("RE4FIX-N", "MOBILE RENDER scale=" + percent + "% surface=" + w + "x" + h + " view=" + width + "x" + height);
        }
        return new int[] {w, h};
    }

    private static SurfaceView findSurface(View view) {
        if (view instanceof SurfaceView) return (SurfaceView) view;
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                SurfaceView found = findSurface(group.getChildAt(i));
                if (found != null) return found;
            }
        }
        return null;
    }

    private static void applyFrameRate(Activity activity) {
        int target = nativeFrameLimit();
        if (target < 24000 || target > 240000 || target == appliedFrameLimit) return;
        Display display = activity.getWindowManager().getDefaultDisplay();
        Display.Mode current = display.getMode(), chosen = null;
        float wanted = target / 1000.0f;
        for (Display.Mode mode : display.getSupportedModes()) {
            if (mode.getPhysicalWidth() != current.getPhysicalWidth() || mode.getPhysicalHeight() != current.getPhysicalHeight()) continue;
            if (mode.getRefreshRate() >= wanted - .5f && (chosen == null || mode.getRefreshRate() < chosen.getRefreshRate())) chosen = mode;
        }
        if (chosen == null) chosen = current;
        WindowManager.LayoutParams params = activity.getWindow().getAttributes();
        if (params.preferredDisplayModeId != chosen.getModeId()) {
            params.preferredDisplayModeId = chosen.getModeId();
            activity.getWindow().setAttributes(params);
        }
        SurfaceView surface = findSurface(activity.getWindow().getDecorView());
        if (surface == null || !surface.getHolder().getSurface().isValid()) return;
        if (Build.VERSION.SDK_INT >= 30) surface.getHolder().getSurface().setFrameRate(wanted, android.view.Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
        appliedFrameLimit = target;
        Log.i("RE4FIX-N", "MOBILE FPS display request=" + wanted + " mode=" + chosen.getRefreshRate());
    }

    /** Called by the original GameActivity onResume, and on display mode changes. */
    public static void apply(final Activity activity) {
        if (activity == null) return;
        if (activity.getClass().getName().equals("com.epicgames.ue4.GameActivity")) {
            MobileGamepad.apply(activity);
            game = new WeakReference<Activity>(activity);
            sensors = (SensorManager) activity.getApplicationContext().getSystemService(Context.SENSOR_SERVICE);
            resumed = true;
            gyroHandler.removeCallbacks(gyroWatch);
            gyroHandler.post(gyroWatch);
            if (!displayListener) {
                DisplayManager displays = (DisplayManager) activity.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
                if (displays != null) {
                    displays.registerDisplayListener(new DisplayManager.DisplayListener() {
                        public void onDisplayAdded(int id) {}
                        public void onDisplayRemoved(int id) {}
                        public void onDisplayChanged(int id) {
                            Activity current = game.get();
                            if (current != null && id == current.getWindowManager().getDefaultDisplay().getDisplayId())
                                apply(current);
                        }
                    }, new Handler(Looper.getMainLooper()));
                    displayListener = true;
                }
            }
        }
        float hz = activity.getWindowManager().getDefaultDisplay().getRefreshRate();
        if (Float.isNaN(hz) || Float.isInfinite(hz) || hz < 24.0f || hz > 240.0f) return;
        try {
            nativeDisplay(Math.round(hz * 1000.0f));
            Display display = activity.getWindowManager().getDefaultDisplay();
            Display.Mode current = display.getMode();
            TreeSet<Integer> rates = new TreeSet<Integer>();
            for (Display.Mode mode : display.getSupportedModes()) {
                if (mode.getPhysicalWidth() == current.getPhysicalWidth() && mode.getPhysicalHeight() == current.getPhysicalHeight()) {
                    int rate = Math.round(mode.getRefreshRate());
                    if (rate >= 24 && rate <= 240) rates.add(rate);
                }
            }
            long packed = 0; int count = 0;
            for (int rate : rates) { if (count == 8) break; packed |= (long) rate << (count++ * 8); }
            if (packed != 0) nativeDisplayModes(packed);
            nativeDensity(Math.round(activity.getResources().getDisplayMetrics().density * 1000.0f));
            applyFrameRate(activity);
        } catch (UnsatisfiedLinkError pending) {
            // GameActivity resume retries after the original native library initialization.
            if (!nativeWarning) { nativeWarning = true; Log.w("RE4FIX-N", "Mobile settings await native library"); }
        }
    }
}
