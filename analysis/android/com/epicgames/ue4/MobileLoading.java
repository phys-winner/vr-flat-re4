package com.epicgames.ue4;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import java.lang.ref.WeakReference;
import java.util.Locale;

/** UI-thread animation continues while the engine thread loads a map. */
public final class MobileLoading {
    private static WeakReference<Panel> current = new WeakReference<Panel>(null);
    private static boolean nativeAvailable = true;
    private static native long nativeState();
    private MobileLoading() {}

    /** 0 = finished, 1 = checking cache, 2 = starting engine, 3 = loading level. */
    public static void update(final Activity activity, final int stage) {
        if (activity == null || stage < 0 || stage > 3) return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                Panel panel = current.get();
                if (stage == 0 || activity.isFinishing() || activity.isDestroyed()) {
                    detach(activity);
                    return;
                }
                if (panel == null || panel.getContext() != activity || panel.getParent() == null) {
                    if (panel != null) panel.remove();
                    panel = new Panel(activity);
                    if (!panel.show(activity)) return;
                    current = new WeakReference<Panel>(panel);
                }
                panel.stage = stage;
                panel.refresh();
            }
        });
    }

    /** Called on the UI thread during Activity destruction. */
    public static void detach(Activity activity) {
        Panel panel = current.get();
        if (panel != null && panel.getContext() == activity) {
            panel.remove();
            current.clear();
        }
    }

    private static final class Panel extends FrameLayout implements Runnable {
        private static final String[][] TEXT = {
            {"Checking game cache", "Starting game", "Loading level", "Preparing graphics"},
            {"Vérification du cache", "Démarrage du jeu", "Chargement du niveau", "Préparation des graphismes"},
            {"Spieldaten werden geprüft", "Spiel wird gestartet", "Level wird geladen", "Grafik wird vorbereitet"},
            {"Comprobando los datos", "Iniciando el juego", "Cargando el nivel", "Preparando los gráficos"},
            {"Verifica dei dati", "Avvio del gioco", "Caricamento del livello", "Preparazione della grafica"},
            {"Проверка кеша", "Запуск игры", "Загрузка уровня", "Подготовка графики"},
            {"ゲームデータを確認中", "ゲームを起動中", "レベルを読み込み中", "グラフィックを準備中"},
            {"게임 데이터 확인 중", "게임 시작 중", "레벨 로딩 중", "그래픽 준비 중"},
            {"正在检查游戏数据", "正在启动游戏", "正在加载关卡", "正在准备图形"},
            {"正在檢查遊戲資料", "正在啟動遊戲", "正在載入關卡", "正在準備圖形"}
        };
        final TextView label;
        final TextView percent;
        final ProgressBar progress;
        final int language;
        final WindowManager windows;
        boolean added;
        int stage;
        int shaderTotal;

        Panel(Activity activity) {
            super(activity);
            language = language(MobileAndroid.startupLocale(activity));
            windows = (WindowManager) activity.getSystemService(Context.WINDOW_SERVICE);
            setBackgroundColor(0xFF100F0E);
            setClickable(true);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
            LinearLayout column = new LinearLayout(activity);
            column.setOrientation(LinearLayout.VERTICAL);
            column.setGravity(Gravity.CENTER);
            float dp = getResources().getDisplayMetrics().density;
            column.setPadding((int)(48 * dp), (int)(24 * dp), (int)(48 * dp), (int)(24 * dp));
            TextView title = text(activity, 30, 0xFFF0E9DB);
            title.setText("RESIDENT EVIL 4");
            title.setTypeface(Typeface.SERIF);
            column.addView(title);
            label = text(activity, 16, 0xFFDDD2BF);
            LinearLayout.LayoutParams statusLayout = new LinearLayout.LayoutParams(-1, -2);
            statusLayout.topMargin = (int)(24 * dp);
            column.addView(label, statusLayout);
            progress = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
            progress.setIndeterminate(true);
            ColorStateList tint = ColorStateList.valueOf(0xFFDAC29A);
            progress.setProgressTintList(tint);
            progress.setIndeterminateTintList(tint);
            progress.setProgressBackgroundTintList(ColorStateList.valueOf(0xFF3B342A));
            LinearLayout.LayoutParams barLayout = new LinearLayout.LayoutParams(-1, (int)(6 * dp));
            barLayout.topMargin = (int)(20 * dp);
            column.addView(progress, barLayout);
            percent = text(activity, 14, 0xFFDDD2BF);
            LinearLayout.LayoutParams percentLayout = new LinearLayout.LayoutParams(-1, -2);
            percentLayout.topMargin = (int)(12 * dp);
            column.addView(percent, percentLayout);
            addView(column, new FrameLayout.LayoutParams(-1, -2, Gravity.CENTER));
        }

        boolean show(Activity activity) {
            WindowManager.LayoutParams layout = new WindowManager.LayoutParams(-1, -1,
                WindowManager.LayoutParams.TYPE_APPLICATION_PANEL,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                    | WindowManager.LayoutParams.FLAG_FULLSCREEN, PixelFormat.OPAQUE);
            layout.token = activity.getWindow().getDecorView().getWindowToken();
            if (windows == null || layout.token == null) return false;
            setSystemUiVisibility(activity.getWindow().getDecorView().getSystemUiVisibility());
            if (android.os.Build.VERSION.SDK_INT >= 28) layout.layoutInDisplayCutoutMode = 1;
            try {
                windows.addView(this, layout);
                added = true;
                return true;
            } catch (RuntimeException unavailable) {
                remove();
                return false;
            }
        }

        static TextView text(Context context, int sp, int color) {
            TextView view = new TextView(context);
            view.setGravity(Gravity.CENTER);
            view.setTextSize(sp);
            view.setTextColor(color);
            return view;
        }

        static int language(Locale locale) {
            String lang = locale.getLanguage();
            String[] languages = {"en", "fr", "de", "es", "it", "ru", "ja", "ko"};
            for (int i = 0; i < languages.length; i++) if (lang.equals(languages[i])) return i;
            if (lang.equals("zh")) {
                String country = locale.getCountry();
                return locale.getScript().equals("Hant") || country.equals("TW") || country.equals("HK") || country.equals("MO") ? 9 : 8;
            }
            return 0;
        }

        void refresh() {
            removeCallbacks(this);
            run();
        }

        public void run() {
            Activity activity = (Activity) getContext();
            if (activity.isFinishing() || activity.isDestroyed()) { detach(activity); return; }
            int remaining = -1;
            if (stage != 1 && nativeAvailable) {
                try {
                    long state = nativeState();
                    int nativeStage = (int) (state >>> 32);
                    if (nativeStage == 0) {
                        detach((Activity) getContext());
                        return;
                    }
                    if (nativeStage >= 2 && nativeStage <= 4) {
                        stage = nativeStage;
                        remaining = (int) state;
                    }
                } catch (UnsatisfiedLinkError unavailable) { nativeAvailable = false; }
            }
            render(remaining);
            postDelayed(this, 1000);
        }

        void render(int remaining) {
            if (stage == 4 && remaining >= 0) {
                shaderTotal = Math.max(shaderTotal, remaining);
                progress.setIndeterminate(false);
                progress.setMax(Math.max(1, shaderTotal));
                progress.setProgress(remaining == 0 ? Math.max(1, shaderTotal) : shaderTotal - remaining);
            } else progress.setIndeterminate(true);
            // shortcut: shader progress uses the largest observed queue; new jobs can move it backwards.
            boolean measured = stage == 4 && remaining >= 0;
            label.setText(TEXT[language][Math.max(0, Math.min(3, stage - 1))]);
            percent.setText(measured ? (remaining == 0 ? 100
                : (shaderTotal - (long) remaining) * 100 / Math.max(1, shaderTotal)) + "%" : "");
            percent.setVisibility(measured ? View.VISIBLE : View.INVISIBLE);
        }

        void remove() {
            removeCallbacks(this);
            if (added || getParent() != null) {
                try { windows.removeViewImmediate(this); }
                catch (IllegalArgumentException alreadyRemoved) {}
                added = false;
            }
        }

        protected void onDetachedFromWindow() {
            removeCallbacks(this);
            super.onDetachedFromWindow();
        }
    }
}
