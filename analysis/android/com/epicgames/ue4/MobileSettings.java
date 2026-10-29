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
import java.lang.ref.WeakReference;

/**
 * Phone settings now live in the game's own Options menu ("Touch controls"); the native layer owns them.
 * This helper only reports the panel refresh rate used by the "Display" frame-limit choice.
 */
public final class MobileSettings {
    private static WeakReference<Activity> game = new WeakReference<Activity>(null);
    private static boolean displayListener, nativeWarning;
    private static final Handler gyroHandler = new Handler(Looper.getMainLooper());
    private static SensorManager sensors;
    private static boolean resumed, gyroListening;
    private static long gyroTimestamp;
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
            try { wanted = resumed && activity != null && activity.hasWindowFocus() && nativeGyroActive(); }
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

    public static void pause(Activity activity) {
        if (game.get() != activity) return;
        resumed = false;
        gyroHandler.removeCallbacks(gyroWatch);
        if (gyroListening && sensors != null) sensors.unregisterListener(gyroListener);
        gyroListening = false;
        gyroTimestamp = 0;
    }

    /** Called by the original GameActivity onResume, and on display mode changes. */
    public static void apply(final Activity activity) {
        if (activity == null) return;
        if (activity.getClass().getName().equals("com.epicgames.ue4.GameActivity")) {
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
            nativeDensity(Math.round(activity.getResources().getDisplayMetrics().density * 1000.0f));
        } catch (UnsatisfiedLinkError pending) {
            // GameActivity resume retries after the original native library initialization.
            if (!nativeWarning) { nativeWarning = true; Log.w("RE4FIX-N", "Mobile settings await native library"); }
        }
    }
}
