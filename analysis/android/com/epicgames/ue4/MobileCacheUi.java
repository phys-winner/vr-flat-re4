package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Locale;

/** Shared cache cards: the same hierarchy, actions and readable paths in every import stage. */
final class MobileCacheUi {
    private static final int BACKGROUND = 0xff101613, CARD = 0xff1c2520, TEXT = 0xffecf3e9;
    private static final int SECONDARY = 0xffadb9ac, ACCENT = 0xffb4d993, LINE = 0xff354333;
    private static final int PRIMARY = 1, SECOND = 2, QUIET = 3;
    private MobileCacheUi() {}

    static final class Action {
        final String title; final int kind; final Runnable run;
        Action(String title, int kind, Runnable run) { this.title = title; this.kind = kind; this.run = run; }
    }
    static Action primary(String title, Runnable run) { return new Action(title, PRIMARY, run); }
    static Action secondary(String title, Runnable run) { return new Action(title, SECOND, run); }
    static Action quiet(String title, Runnable run) { return new Action(title, QUIET, run); }
    private static int dp(Activity activity, float value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
    private static boolean russian(Activity activity) { return MobileAndroid.startupLocale(activity).getLanguage().equals("ru"); }

    private static GradientDrawable shape(Activity activity, int color, int radius, boolean stroke) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color); drawable.setCornerRadius(dp(activity, radius));
        if (stroke) drawable.setStroke(dp(activity, 1), LINE);
        return drawable;
    }
    private static TextView text(Activity activity, String value, int size, int color, boolean bold) {
        TextView view = new TextView(activity);
        view.setText(value); view.setTextSize(size); view.setTextColor(color);
        view.setFontFeatureSettings("kern"); view.setIncludeFontPadding(false);
        view.setTypeface(Typeface.create("sans-serif", bold ? Typeface.BOLD : Typeface.NORMAL));
        view.setLineSpacing(dp(activity, 2), 1.06f);
        return view;
    }
    private static LinearLayout.LayoutParams row(int width, int height) { return new LinearLayout.LayoutParams(width, height); }

    static final class Panel {
        final Activity activity;
        final LinearLayout view, body, actions;
        final TextView status;
        private final boolean narrow;
        private final int maximumBody;
        private ProgressBar progress;
        private TextView progressStage, progressFile, progressAmount;
        private View detailCard;
        private LinearLayout progressBox;
        Panel(final Activity activity, String title, String description, String detailLabel, String detail) {
            this.activity = activity;
            int width = activity.getResources().getDisplayMetrics().widthPixels;
            int height = activity.getResources().getDisplayMetrics().heightPixels;
            narrow = width < dp(activity, 480);
            maximumBody = Math.max(dp(activity, 64), (int)(height * .85f) - dp(activity, narrow ? 270 : 176));
            view = new LinearLayout(activity); view.setOrientation(LinearLayout.VERTICAL);
            view.setPadding(dp(activity, 24), dp(activity, 20), dp(activity, 24), dp(activity, 16));
            view.setBackground(shape(activity, CARD, 24, true)); view.setElevation(dp(activity, 8));
            TextView eyebrow = text(activity, russian(activity) ? "RESIDENT EVIL 4  /  ДАННЫЕ ИГРЫ" : "RESIDENT EVIL 4  /  GAME DATA", 10, ACCENT, true);
            eyebrow.setLetterSpacing(.10f); view.addView(eyebrow, row(-1, -2));
            TextView heading = text(activity, title, 23, TEXT, true);
            LinearLayout.LayoutParams headingLayout = row(-1, -2); headingLayout.topMargin = dp(activity, 9); headingLayout.bottomMargin = dp(activity, 16);
            view.addView(heading, headingLayout);
            ScrollView scroll = new ScrollView(activity) {
                protected void onMeasure(int widthSpec, int heightSpec) {
                    super.onMeasure(widthSpec, View.MeasureSpec.makeMeasureSpec(maximumBody, View.MeasureSpec.AT_MOST));
                }
            };
            scroll.setFillViewport(false); scroll.setClipToPadding(false); scroll.setVerticalScrollBarEnabled(false);
            body = new LinearLayout(activity); body.setOrientation(LinearLayout.VERTICAL);
            status = text(activity, description, 14, SECONDARY, false); body.addView(status, row(-1, -2));
            if (detail != null && !detail.isEmpty()) {
                LinearLayout details = new LinearLayout(activity); details.setOrientation(LinearLayout.VERTICAL);
                details.setPadding(dp(activity, 14), dp(activity, 12), dp(activity, 14), dp(activity, 12));
                details.setBackground(shape(activity, BACKGROUND, 14, true)); detailCard = details;
                if (detailLabel != null) {
                    TextView label = text(activity, detailLabel, 10, ACCENT, true); label.setLetterSpacing(.06f);
                    LinearLayout.LayoutParams labelLayout = row(-1, -2); labelLayout.bottomMargin = dp(activity, 7); details.addView(label, labelLayout);
                }
                TextView content = text(activity, detail, 12, TEXT, false);
                content.setTypeface(Typeface.MONOSPACE); content.setTextIsSelectable(true); content.setHorizontallyScrolling(false);
                details.addView(content, row(-1, -2));
                LinearLayout.LayoutParams detailLayout = row(-1, -2); detailLayout.topMargin = dp(activity, 14); body.addView(details, detailLayout);
            }
            scroll.addView(body); view.addView(scroll, row(-1, -2));
            actions = new LinearLayout(activity); actions.setOrientation(narrow ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
            actions.setGravity(Gravity.END | Gravity.CENTER_VERTICAL);
            LinearLayout.LayoutParams actionLayout = row(-1, -2); actionLayout.topMargin = dp(activity, 18); view.addView(actions, actionLayout);
        }

        void actions(final AlertDialog dialog, Action... choices) {
            actions.removeAllViews();
            for (final Action action : choices) {
                Button button = new Button(activity); button.setText(action.title); button.setTextSize(13); button.setAllCaps(false);
                button.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
                button.setTextColor(action.kind == PRIMARY ? BACKGROUND : action.kind == SECOND ? TEXT : SECONDARY);
                button.setMinHeight(dp(activity, 48)); button.setMinimumHeight(dp(activity, 48)); button.setMinimumWidth(0); button.setMinWidth(0);
                button.setPadding(dp(activity, 16), dp(activity, 8), dp(activity, 16), dp(activity, 8));
                GradientDrawable buttonShape = shape(activity, action.kind == PRIMARY ? ACCENT : action.kind == SECOND ? 0xff29372c : Color.TRANSPARENT, 14, action.kind == SECOND);
                button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x28ffffff), buttonShape, shape(activity, Color.WHITE, 14, false)));
                button.setBackgroundTintList(null);
                if (Build.VERSION.SDK_INT >= 21) button.setStateListAnimator(null);
                LinearLayout.LayoutParams buttonLayout = row(narrow ? -1 : -2, -2);
                if (actions.getChildCount() > 0) {
                    if (narrow) buttonLayout.topMargin = dp(activity, 8); else buttonLayout.leftMargin = dp(activity, 8);
                }
                button.setOnClickListener(new View.OnClickListener() { public void onClick(View tapped) {
                    try { if (action.run != null) action.run.run(); }
                    finally { if (dialog != null) dialog.dismiss(); }
                }});
                actions.addView(button, buttonLayout);
            }
        }

        void status(String message, boolean error) {
            status.setText(message); status.setTextColor(error ? 0xffe7b7a7 : SECONDARY);
            if (progressBox != null && error) progressBox.setVisibility(View.GONE);
            if (detailCard != null && error) detailCard.setVisibility(View.VISIBLE);
        }

        void enabled(boolean enabled) {
            for (int index = 0; index < actions.getChildCount(); ++index) {
                actions.getChildAt(index).setEnabled(enabled); actions.getChildAt(index).setAlpha(enabled ? 1 : .45f);
            }
        }

        void beginProgress() {
            if (detailCard != null) detailCard.setVisibility(View.GONE);
            if (progress != null) { progressBox.setVisibility(View.VISIBLE); return; }
            progressBox = new LinearLayout(activity); progressBox.setOrientation(LinearLayout.VERTICAL); body.addView(progressBox, row(-1, -2));
            progressStage = text(activity, russian(activity) ? "Распаковка и проверка" : "Extracting and verifying", 14, TEXT, true);
            LinearLayout.LayoutParams stageLayout = row(-1, -2); stageLayout.topMargin = dp(activity, 18); progressBox.addView(progressStage, stageLayout);
            progressFile = text(activity, "", 11, SECONDARY, false); progressFile.setTypeface(Typeface.MONOSPACE);
            LinearLayout.LayoutParams fileLayout = row(-1, -2); fileLayout.topMargin = dp(activity, 7); progressBox.addView(progressFile, fileLayout);
            progress = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal); progress.setMax(1000);
            GradientDrawable track = shape(activity, LINE, 5, false), fill = shape(activity, ACCENT, 5, false);
            LayerDrawable layers = new LayerDrawable(new android.graphics.drawable.Drawable[] {track, new ClipDrawable(fill, Gravity.LEFT, ClipDrawable.HORIZONTAL)});
            layers.setId(0, android.R.id.background); layers.setId(1, android.R.id.progress); progress.setProgressDrawable(layers);
            LinearLayout.LayoutParams progressLayout = row(-1, dp(activity, 7)); progressLayout.topMargin = dp(activity, 14); progressBox.addView(progress, progressLayout);
            progressAmount = text(activity, "0%", 12, ACCENT, true);
            LinearLayout.LayoutParams amountLayout = row(-1, -2); amountLayout.topMargin = dp(activity, 8); progressBox.addView(progressAmount, amountLayout);
        }

        void progress(int file, long completed, long total) {
            beginProgress();
            int value = (int)Math.min(999, completed * 1000 / total);
            progress.setProgress(value);
            progressFile.setText(MobileCache.NAMES[file]);
            Locale locale = russian(activity) ? new Locale("ru") : Locale.US;
            progressAmount.setText(String.format(locale, "%d%%  ·  %.2f / %.2f %s  ·  %d/2", value / 10,
                completed / 1e9, total / 1e9, russian(activity) ? "ГБ" : "GB", file + 1));
            progressStage.setText(completed == total ? (russian(activity) ? "Завершение установки…" : "Finishing installation…")
                : (russian(activity) ? "Распаковка и проверка" : "Extracting and verifying"));
        }

        void complete() {
            if (progress != null) {
                progress.setProgress(1000); progressAmount.setText("100%");
                progressStage.setText(russian(activity) ? "Проверка завершена" : "Verification complete");
            }
        }
    }

    static AlertDialog show(Activity activity, String title, String description, String detailLabel, String detail,
                            boolean cancelable, Action... actions) {
        AlertDialog dialog = create(activity, title, description, detailLabel, detail, cancelable, actions);
        dialog.show();
        return dialog;
    }

    static AlertDialog create(final Activity activity, String title, String description, String detailLabel, String detail,
                              boolean cancelable, Action... actions) {
        Panel panel = new Panel(activity, title, description, detailLabel, detail);
        final AlertDialog dialog = new AlertDialog.Builder(activity, android.R.style.Theme_Material_Dialog_NoActionBar)
            .setCancelable(cancelable).create();
        dialog.setView(panel.view, 0, 0, 0, 0);
        panel.actions(dialog, actions);
        dialog.setOnShowListener(new DialogInterface.OnShowListener() { public void onShow(DialogInterface shown) {
            Window window = dialog.getWindow();
            if (window == null) return;
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT)); window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.getDecorView().setPadding(0, 0, 0, 0); window.getDecorView().setForeground(null);
            WindowManager.LayoutParams attributes = window.getAttributes(); attributes.dimAmount = .65f; window.setAttributes(attributes);
            window.setGravity(Gravity.CENTER);
            window.setLayout(Math.min(dp(activity, 620), activity.getResources().getDisplayMetrics().widthPixels - dp(activity, 32)), -2);
        }});
        return dialog;
    }

    static Panel activity(final Activity activity, String title, String description, String detailLabel, String detail) {
        final FrameLayout background = new FrameLayout(activity); background.setBackgroundColor(BACKGROUND);
        Panel panel = new Panel(activity, title, description, detailLabel, detail);
        FrameLayout.LayoutParams layout = new FrameLayout.LayoutParams(Math.min(dp(activity, 620),
            activity.getResources().getDisplayMetrics().widthPixels - dp(activity, 48)), -2, Gravity.CENTER);
        background.addView(panel.view, layout);
        background.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                int[] edges = Build.VERSION.SDK_INT >= 30 ? Insets30.edges(insets)
                    : new int[] {insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(), insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom()};
                view.setPadding(dp(activity, 16) + edges[0], dp(activity, 12) + edges[1], dp(activity, 16) + edges[2], dp(activity, 12) + edges[3]);
                return insets;
            }
        });
        activity.getWindow().setStatusBarColor(BACKGROUND); activity.getWindow().setNavigationBarColor(BACKGROUND);
        activity.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        activity.setContentView(background); background.requestApplyInsets();
        return panel;
    }
    private static final class Insets30 {
        static int[] edges(WindowInsets insets) {
            android.graphics.Insets edges = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            return new int[] {edges.left, edges.top, edges.right, edges.bottom};
        }
    }
}
