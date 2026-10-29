package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.view.View;
import java.util.Locale;

public final class MobileAndroid {
    private static Locale processLocale;
    private static native boolean nativeExternalRoot(String path);
    private static native boolean nativeBack();
    private static native void nativeBind();
    private static native void nativeConfirm(int token, boolean yes);
    private static AlertDialog confirmation;
    private static int confirmationToken;
    private static Context appContext;
    private static android.os.Vibrator vibrator;
    private static final android.media.AudioAttributes GAME_USAGE = new android.media.AudioAttributes.Builder()
        .setUsage(android.media.AudioAttributes.USAGE_GAME).build();
    private MobileAndroid() {}

    private static int pendingFlags(int flags) {
        if (Build.VERSION.SDK_INT >= 23 && (flags & 0x06000000) == 0)
            flags |= PendingIntent.FLAG_IMMUTABLE;
        return flags;
    }

    public static PendingIntent getActivity(Context context, int code, Intent intent, int flags) {
        return PendingIntent.getActivity(context, code, intent, pendingFlags(flags));
    }

    public static PendingIntent getBroadcast(Context context, int code, Intent intent, int flags) {
        return PendingIntent.getBroadcast(context, code, intent, pendingFlags(flags));
    }

    public static Intent registerReceiver(Context context, BroadcastReceiver receiver, IntentFilter filter) {
        // Keep system broadcasts and the original explicit external Oculus/GMS routes working.
        if (Build.VERSION.SDK_INT >= 33) {
            int flags = filter.countActions() == 1 && filter.hasAction("android.intent.action.RUN")
                ? Context.RECEIVER_NOT_EXPORTED : Context.RECEIVER_EXPORTED;
            return context.registerReceiver(receiver, filter, flags);
        }
        return context.registerReceiver(receiver, filter);
    }

    public static synchronized Locale startupLocale(Context context) {
        if (processLocale == null) {
            processLocale = Build.VERSION.SDK_INT >= 24
                ? context.getResources().getConfiguration().getLocales().get(0)
                : context.getResources().getConfiguration().locale;
        }
        return processLocale;
    }

    public static void recoverSaves(Context context) {
        java.io.File root = context.getExternalFilesDir(null);
        if (root == null) throw new IllegalStateException("External game storage unavailable");
        try { MobileSaves.recover(root); }
        catch (java.io.IOException failure) { throw new IllegalStateException("Save import recovery failed", failure); }
        if (!nativeExternalRoot(root.getAbsolutePath()))
            throw new IllegalStateException("Phone localization could not be prepared in external game storage");
        appContext = context.getApplicationContext();
        try { nativeBind(); } catch (UnsatisfiedLinkError unavailable) {}
    }

    /** Called from the native rumble thread: amplitude 1..255 for ms, or 0 to stop. */
    public static void vibrate(int amplitude, int durationMs) {
        try {
            android.os.Vibrator device = vibrator;
            if (device == null) {
                Context context = appContext;
                if (context == null) return;
                device = Build.VERSION.SDK_INT >= 31 ? Vibrator31.get(context)
                    : (android.os.Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
                if (device == null || !device.hasVibrator()) return;
                vibrator = device;
            }
            if (amplitude <= 0 || durationMs <= 0) { device.cancel(); return; }
            if (Build.VERSION.SDK_INT >= 26) {
                int strength = device.hasAmplitudeControl() ? Math.max(1, Math.min(255, amplitude))
                    : android.os.VibrationEffect.DEFAULT_AMPLITUDE;
                // Game usage: not muted by the system "touch feedback" switch, still follows vibration settings.
                device.vibrate(android.os.VibrationEffect.createOneShot(durationMs, strength), GAME_USAGE);
            } else device.vibrate(durationMs);
        } catch (RuntimeException ignored) {
            // A vendor vibrator service failure must never reach the native game thread.
        }
    }

    private static final class Vibrator31 {
        static android.os.Vibrator get(Context context) {
            android.os.VibratorManager manager =
                (android.os.VibratorManager) context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            return manager != null ? manager.getDefaultVibrator() : null;
        }
    }

    public static void showFiles(final Activity activity) {
        activity.runOnUiThread(new Runnable() {
            public void run() {
                if (activity.isFinishing() || activity.isDestroyed() || !activity.hasWindowFocus()) return;
                activity.startActivity(new Intent(activity, MobileFilesActivity.class));
            }
        });
    }

    public static void showLanguage(final Activity activity) {
        if (!gameActivity(activity)) return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                if (activity.isFinishing() || activity.isDestroyed() || !activity.hasWindowFocus()) return;
                showLanguagePicker(activity);
            }
        });
    }

    public static void showLanguagePicker(final Activity activity) {
        final boolean russian = startupLocale(activity).getLanguage().equals("ru");
        if (Build.VERSION.SDK_INT < 33) {
            new AlertDialog.Builder(activity)
                .setMessage(russian ? "На этой версии Android используется язык системы. Изменение применяется при следующем запуске игры."
                    : "This Android version uses the system language. Changes apply at the next game launch.")
                .setPositiveButton(android.R.string.ok, null).show();
            return;
        }
        final String[] tags = {"", "ru", "en", "de-DE", "es-419", "fr-FR", "ja-JP"};
        final String[] names = {russian ? "Язык системы" : "System default", "Русский", "English", "Deutsch", "Español (Latinoamérica)", "Français", "日本語"};
        final android.app.LocaleManager manager = activity.getSystemService(android.app.LocaleManager.class);
        if (manager == null) return;
        android.os.LocaleList locales = manager.getApplicationLocales();
        String current = locales.isEmpty() ? "" : locales.get(0).toLanguageTag();
        int selected = 0;
        for (int i = 1; i < tags.length; i++)
            if (current.equalsIgnoreCase(tags[i]) || (tags[i].equals("ru") && current.startsWith("ru-"))) selected = i;
        new AlertDialog.Builder(activity)
            .setTitle(russian ? "Язык игры · со следующего запуска" : "Game language · applies next launch")
            .setSingleChoiceItems(names, selected, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    if (which >= 0 && which < tags.length)
                        manager.setApplicationLocales(android.os.LocaleList.forLanguageTags(tags[which]));
                    dialog.dismiss();
                }
            }).setNegativeButton(russian ? "Отмена" : "Cancel", null).show();
    }

    public static void showConfirmation(final Activity activity, final int token, final int kind, final boolean russian) {
        if (token <= 0 || kind < 1 || kind > 7 || !gameActivity(activity)) {
            nativeConfirm(token, false);
            return;
        }
        activity.runOnUiThread(new Runnable() {
            public void run() {
                boolean modalFocus = confirmation != null && confirmation.getOwnerActivity() == activity
                    && confirmation.getWindow() != null && confirmation.getWindow().getDecorView().hasWindowFocus();
                if (activity.isFinishing() || activity.isDestroyed() || (!activity.hasWindowFocus() && !modalFocus)) {
                    nativeConfirm(token, false);
                    return;
                }
                if (confirmation != null && confirmationToken == token) return;
                if (confirmation != null) confirmation.dismiss();
                final String[] messages = russian ? new String[] {
                    "Открыть режим «Наёмники»?\n\nПри первом открытии игра сбрасывает отдельный прогресс этого режима. Прогресс основной кампании не меняется.",
                    "Открыть профессиональную сложность для новой игры?",
                    "Открыть набор костюмов Mafia для новой игры?",
                    "Открыть Chicago Typewriter и Hand Cannon через штатную систему бонусного оружия?",
                    "Открыть Horror, Big Head, High Speed и Golden Guns? Эти режимы могут менять сложность и баланс.",
                    "Продолжить после нажатия Start?"
                } : new String[] {
                    "Unlock Mercenaries?\n\nThe first unlock resets that mode's separate progress. Main campaign progress is unchanged.",
                    "Unlock Professional difficulty for a new game?",
                    "Unlock the Mafia costume set for a new game?",
                    "Unlock Chicago Typewriter and Hand Cannon through the game's original bonus weapon system?",
                    "Unlock Horror, Big Head, High Speed and Golden Guns? These modes can change difficulty and balance.",
                    "Continue after pressing Start?"
                };
                try {
                    final View decor = activity.getWindow().getDecorView();
                    final int visibility = decor.getSystemUiVisibility();
                    DialogInterface.OnClickListener answer = new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface clicked, int which) {
                            if (confirmation != clicked || confirmationToken != token) return;
                            nativeConfirm(token, which == DialogInterface.BUTTON_POSITIVE
                                && !activity.isFinishing() && !activity.isDestroyed());
                        }
                    };
                    final AlertDialog dialog = new AlertDialog.Builder(activity)
                        .setTitle(russian ? "Подтверждение" : "Confirmation")
                        .setMessage(messages[Math.min(kind, 6) - 1])
                        .setPositiveButton(russian ? "Да" : "Yes", answer)
                        .setNegativeButton(russian ? "Нет" : "No", answer)
                        .create();
                    dialog.setOwnerActivity(activity);
                    dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                        public void onDismiss(DialogInterface dismissed) {
                            nativeConfirm(token, false);
                            if (confirmation != dialog) return;
                            confirmation = null;
                            confirmationToken = 0;
                            if (!activity.isFinishing() && !activity.isDestroyed()) {
                                decor.setSystemUiVisibility(visibility);
                                decor.requestFocus();
                            }
                        }
                    });
                    confirmation = dialog;
                    confirmationToken = token;
                    dialog.show();
                } catch (RuntimeException unavailable) {
                    if (confirmationToken == token) {
                        if (confirmation != null) confirmation.dismiss();
                        confirmation = null;
                        confirmationToken = 0;
                    }
                    nativeConfirm(token, false);
                }
            }
        });
    }

    private static boolean gameActivity(Activity activity) {
        return activity != null && activity.getClass().getName().equals("com.epicgames.ue4.GameActivity");
    }

    public static boolean onBackPressed(Activity activity) {
        if (!gameActivity(activity) || activity.isFinishing() || activity.isDestroyed() || !activity.hasWindowFocus()) return false;
        return nativeBack();
    }

    public static void attachBack(Activity activity) {
        if (Build.VERSION.SDK_INT >= 33 && gameActivity(activity) && !activity.isFinishing() && !activity.isDestroyed())
            Back33.attach(activity);
    }

    public static void detachBack(Activity activity) {
        MobileSettings.pause(activity);
        if (confirmation != null && confirmation.getOwnerActivity() == activity) confirmation.dismiss();
        if (Build.VERSION.SDK_INT >= 33 && gameActivity(activity)) Back33.detach(activity);
    }

    // Kept in a separate class so Android23-32 never load the API33 dispatcher types.
    private static final class Back33 {
        private static final java.util.WeakHashMap<Activity,android.window.OnBackInvokedCallback> callbacks =
            new java.util.WeakHashMap<Activity,android.window.OnBackInvokedCallback>();
        static void attach(final Activity activity) {
            if (callbacks.containsKey(activity)) return;
            final java.lang.ref.WeakReference<Activity> reference = new java.lang.ref.WeakReference<Activity>(activity);
            android.window.OnBackInvokedCallback callback = new android.window.OnBackInvokedCallback() {
                public void onBackInvoked() {
                    Activity active = reference.get();
                    if (active == null || callbacks.get(active) != this || active.isFinishing()
                        || active.isDestroyed() || !active.hasWindowFocus()) return;
                    if (!MobileAndroid.onBackPressed(active)) active.onBackPressed();
                }
            };
            activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,callback);
            callbacks.put(activity,callback);
        }
        static void detach(Activity activity) {
            android.window.OnBackInvokedCallback callback = callbacks.remove(activity);
            if (callback != null) activity.getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(callback);
        }
    }
}
