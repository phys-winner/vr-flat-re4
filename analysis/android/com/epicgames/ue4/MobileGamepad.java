package com.epicgames.ue4;

import android.app.Activity;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import java.lang.ref.WeakReference;

/** Android device removal must release held native controls even without a final input event. */
public final class MobileGamepad {
    private static WeakReference<Activity> game = new WeakReference<Activity>(null);
    private static InputManager inputs;
    private static boolean listening;
    private static final InputManager.InputDeviceListener listener = new InputManager.InputDeviceListener() {
        public void onInputDeviceAdded(int id) {}
        public void onInputDeviceChanged(int id) { reset(id); }
        public void onInputDeviceRemoved(int id) { reset(id); }
    };

    private MobileGamepad() {}
    private static native void nativeReset(int deviceId);

    private static void reset(int id) {
        try { nativeReset(id); }
        catch (UnsatisfiedLinkError pending) { /* The first resume may precede native initialization. */ }
    }

    public static void apply(Activity activity) {
        if (activity == null || !activity.getClass().getName().equals("com.epicgames.ue4.GameActivity")) return;
        game = new WeakReference<Activity>(activity);
        if (inputs == null) inputs = (InputManager) activity.getApplicationContext().getSystemService(Context.INPUT_SERVICE);
        if (inputs != null && !listening) {
            inputs.registerInputDeviceListener(listener, new Handler(Looper.getMainLooper()));
            listening = true;
        }
    }

    public static void pause(Activity activity) {
        if (game.get() != activity) return;
        if (inputs != null && listening) inputs.unregisterInputDeviceListener(listener);
        listening = false;
        reset(-1);
    }
}
