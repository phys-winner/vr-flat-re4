package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Bundle;
import android.util.AtomicFile;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;

/** Edits the same normalized control geometry used by native HUD drawing and touch hit tests. */
public final class MobileControls {
    private static final int COUNT = 17, FLOATS = COUNT * 4, MAGIC = 0x52345431;
    private static WeakReference<Dialog> current = new WeakReference<Dialog>(null);
    private static native float[] nativeLayout(boolean defaults);
    private static native boolean nativeApply(float[] values);
    private MobileControls() {}

    private static AtomicFile storage(Context context) throws IOException {
        File root = context.getExternalFilesDir(null);
        if (root == null) throw new IOException("External game storage unavailable");
        return new AtomicFile(new File(root, "re4_touch_layout_v1"));
    }

    static boolean valid(float[] values) {
        if (values == null || values.length != FLOATS) return false;
        for (int i = 0; i < COUNT; i++) {
            int at = i * 4;
            for (int k = 0; k < 4; k++)
                if (Float.isNaN(values[at + k]) || Float.isInfinite(values[at + k])) return false;
            if (values[at] < 0 || values[at] > 1 || values[at + 1] < 0 || values[at + 1] > 1
                    || values[at + 2] < .025f || values[at + 2] > .18f
                    || (values[at + 3] != 0 && values[at + 3] != 1)) return false;
        }
        return true;
    }

    public static void load(Context context) {
        try (DataInputStream input = new DataInputStream(storage(context).openRead())) {
            if (input.readInt() != MAGIC) throw new IOException("Unknown touch layout version");
            float[] values = new float[FLOATS];
            for (int i = 0; i < FLOATS; i++) values[i] = input.readFloat();
            if (input.read() != -1 || !valid(values) || !nativeApply(values))
                throw new IOException("Invalid touch layout");
        } catch (FileNotFoundException absent) {
            // A fresh install keeps native defaults, including the second fire button.
        } catch (IOException | SecurityException | UnsatisfiedLinkError failure) {
            Log.w("RE4FIX-N", "Touch layout unavailable; keeping current controls", failure);
        }
    }

    private static void save(Context context, float[] values) throws IOException {
        if (!valid(values)) throw new IOException("Invalid touch layout");
        AtomicFile file = storage(context);
        FileOutputStream stream = null;
        try {
            stream = file.startWrite();
            DataOutputStream output = new DataOutputStream(stream);
            output.writeInt(MAGIC);
            for (float value : values) output.writeFloat(value);
            output.flush();
            file.finishWrite(stream);
            stream = null;
        } finally {
            if (stream != null) file.failWrite(stream);
        }
        if (!nativeApply(values)) throw new IOException("Native touch layout rejected");
    }

    public static void open(final Activity activity) {
        if (activity == null) return;
        activity.runOnUiThread(new Runnable() {
            public void run() {
                Dialog shown = current.get();
                if (activity.isFinishing() || activity.isDestroyed() || (shown != null && shown.isShowing())) return;
                try {
                    float[] values = nativeLayout(false);
                    if (!valid(values)) throw new IllegalStateException("Native touch layout unavailable");
                    Editor editor = new Editor(activity, values);
                    current = new WeakReference<Dialog>(editor);
                    editor.show();
                    Window window = editor.getWindow();
                    if (window != null) window.setLayout(-1, -1);
                } catch (RuntimeException | UnsatisfiedLinkError failure) {
                    Log.w("RE4FIX-N", "Cannot open touch layout editor", failure);
                }
            }
        });
    }

    static float labelSize(float radius, float width, float height, float nominal) {
        float side=Math.max(0,radius)*1.35f;
        return nominal*Math.min(1,Math.min(side/Math.max(1,width),side/Math.max(1,height)));
    }

    private static final class Editor extends Dialog {
        private static final int[] SLOTS = {0,16,4,1,8,6,7,12,13,14,15,3,5,2,9,10};
        private static final String[] EN = {"Fire","Left fire","Aim","Interact","Reload","Knife","Primary weapon",
            "Secondary weapon","Grenade","Heal","Ashley","Inventory","Pause","Back","Communicator","Confirm"};
        private static final String[] RU = {"Огонь","Огонь слева","Прицел","Действие","Перезарядка","Нож","Основное оружие",
            "Второе оружие","Граната","Лечение","Эшли","Инвентарь","Пауза","Назад","Рация","Подтвердить"};
        private static final String[] PREVIEW_EN = {"Fire","Fire\nL","Aim","Use","Reload","Knife","1","2","Gr.","+","Ash.","Bag","II","Back","Radio","OK"};
        private static final String[] PREVIEW_RU = {"Огонь","Огонь\nЛ","Приц.","Действ.","Заряд","Нож","1","2","Гр.","+","Эш.","Вещи","II","Назад","Рация","Да"};
        private final Activity activity;
        private final boolean russian;
        private float[] values;
        private int selection;
        private boolean updating;
        private LayoutView preview;
        private Spinner controls;
        private CheckBox visible;
        private SeekBar size;
        private TextView sizeText;

        Editor(Activity activity, float[] values) {
            super(activity, android.R.style.Theme_Material_NoActionBar_Fullscreen);
            this.activity = activity;
            this.values = values.clone();
            russian = "ru".equals(MobileAndroid.startupLocale(activity).getLanguage());
            setOwnerActivity(activity);
        }
        private String text(String en, String ru) { return russian ? ru : en; }
        private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
        private String[] names() { return russian ? RU : EN; }
        private int slot() { return SLOTS[selection]; }
        private boolean menu(int slot) { return slot == 2 || slot == 9 || slot == 10; }
        private Button button(LinearLayout row, String label, View.OnClickListener listener) {
            Button button = new Button(activity);
            button.setText(label);
            button.setAllCaps(false);
            button.setOnClickListener(listener);
            row.addView(button, new LinearLayout.LayoutParams(0, dp(48), 1));
            return button;
        }
        @Override protected void onCreate(Bundle state) {
            super.onCreate(state);
            Window window = getWindow();
            if (window != null) {
                window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                window.getDecorView().setSystemUiVisibility(0x1706);
            }
            LinearLayout root = new LinearLayout(activity);
            root.setOrientation(LinearLayout.VERTICAL);
            root.setBackgroundColor(Color.rgb(22, 24, 26));
            LinearLayout toolbar = new LinearLayout(activity);
            button(toolbar, text("Reset", "Сбросить"), new View.OnClickListener() {
                public void onClick(View view) {
                    new AlertDialog.Builder(activity).setTitle(text("Reset layout?", "Сбросить раскладку?"))
                        .setMessage(text("Factory positions and sizes. Save to apply.", "Заводские позиции и размеры. Нажмите «Сохранить», чтобы применить."))
                        .setNegativeButton(text("Cancel", "Отмена"), null)
                        .setPositiveButton(text("Reset", "Сбросить"), (dialog, which) -> {
                            float[] defaults = nativeLayout(true);
                            if (valid(defaults)) { values = defaults; update(); }
                        }).show();
                }
            });
            button(toolbar, text("Cancel", "Отмена"), view -> dismiss());
            button(toolbar, text("Save", "Сохранить"), view -> {
                try { save(activity, values); dismiss(); }
                catch (IOException | SecurityException failure) {
                    new AlertDialog.Builder(activity).setTitle(text("Layout not saved", "Раскладка не сохранена"))
                        .setMessage(text("Game storage is unavailable. Your layout remains open; try Save again.",
                            "Хранилище игры недоступно. Раскладка остаётся открытой; попробуйте сохранить снова."))
                        .setPositiveButton("OK", null).show();
                }
            });
            root.addView(toolbar);
            TextView hint = new TextView(activity);
            hint.setText(text("Drag a button. Select any hidden/context button below. Custom layout overrides group size/offset.",
                "Перетаскивайте кнопки. Скрытые и контекстные выберите снизу. Раскладка заменяет общий размер и сдвиг."));
            hint.setTextColor(Color.WHITE);
            hint.setTextSize(12);
            hint.setPadding(dp(12), 0, dp(12), 0);
            root.addView(hint);
            preview = new LayoutView(activity);
            root.addView(preview, new LinearLayout.LayoutParams(-1, 0, 1));
            LinearLayout chooser = new LinearLayout(activity);
            controls = new Spinner(activity);
            ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.simple_spinner_item, names());
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            controls.setAdapter(adapter);
            chooser.addView(controls, new LinearLayout.LayoutParams(0, dp(48), 1));
            controls.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                public void onNothingSelected(AdapterView<?> parent) {}
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) { selection = position; update(); }
            });
            visible = new CheckBox(activity);
            visible.setText(text("Show", "Показывать"));
            visible.setTextColor(Color.WHITE);
            visible.setOnCheckedChangeListener((button, checked) -> {
                if (!updating) { values[slot()*4+3] = checked ? 1 : 0; preview.invalidate(); }
            });
            chooser.addView(visible, new LinearLayout.LayoutParams(-2, dp(48)));
            root.addView(chooser);
            LinearLayout sizing = new LinearLayout(activity);
            sizeText = new TextView(activity);
            sizeText.setTextColor(Color.WHITE);
            sizeText.setPadding(dp(12), dp(12), 0, 0);
            sizing.addView(sizeText, new LinearLayout.LayoutParams(dp(116), dp(48)));
            size = new SeekBar(activity);
            size.setMax(155);
            size.setContentDescription(text("Button size", "Размер кнопки"));
            size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                public void onStartTrackingTouch(SeekBar bar) {}
                public void onStopTrackingTouch(SeekBar bar) {}
                public void onProgressChanged(SeekBar bar, int progress, boolean user) {
                    if (user && !updating) { values[slot()*4+2] = (progress + 25) / 1000.0f; update(); }
                }
            });
            sizing.addView(size, new LinearLayout.LayoutParams(0, dp(48), 1));
            root.addView(sizing);
            setContentView(root);
            update();
        }
        private void update() {
            if (preview == null || visible == null || size == null) return;
            updating = true;
            float aspect=(float)activity.getResources().getDisplayMetrics().widthPixels
                / activity.getResources().getDisplayMetrics().heightPixels;
            float ry=values[slot()*4+2]*1.15f,rx=ry/aspect;
            values[slot()*4]=Math.max(rx,Math.min(1-rx,values[slot()*4]));
            values[slot()*4+1]=Math.max(ry,Math.min(1-ry,values[slot()*4+1]));
            visible.setChecked(values[slot()*4+3] != 0);
            size.setProgress(Math.round(values[slot()*4+2]*1000) - 25);
            sizeText.setText(text("Size: ", "Размер: ") + Math.round(values[slot()*4+2]*200) + "% H");
            preview.setContentDescription(names()[selection] + text(". Drag to move.", ". Перетащите для перемещения."));
            updating = false;
            preview.invalidate();
        }

        private final class LayoutView extends View {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final RectF area = new RectF();
            private int pointer = -1;
            private float offsetX, offsetY;
            LayoutView(Context context) { super(context); setFocusable(true); }
            private boolean group(int control) {
                if(slot()==9 || control==9)return control==slot();
                return menu(control)==menu(slot());
            }
            private void bounds() {
                float aspect = (float)activity.getResources().getDisplayMetrics().widthPixels
                    / activity.getResources().getDisplayMetrics().heightPixels;
                float width = Math.min(getWidth()-dp(16), (getHeight()-dp(16))*aspect);
                float height = width / aspect;
                area.set((getWidth()-width)*.5f, (getHeight()-height)*.5f,
                    (getWidth()+width)*.5f, (getHeight()+height)*.5f);
            }
            @Override protected void onDraw(Canvas canvas) {
                bounds();
                if(area.width()<=0 || area.height()<=0)return;
                paint.setStyle(Paint.Style.FILL); paint.setColor(Color.rgb(38,42,45));
                canvas.drawRect(area, paint);
                for (int i=0;i<SLOTS.length;i++) {
                    int at=SLOTS[i]*4;
                    if (!group(SLOTS[i])) continue;
                    float radius=values[at+2]*area.height();
                    float rx=radius*1.15f/area.width(), ry=radius*1.15f/area.height();
                    float x=area.left+Math.max(rx, Math.min(1-rx, values[at]))*area.width();
                    float y=area.top+Math.max(ry, Math.min(1-ry, values[at+1]))*area.height();
                    paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(i==selection?3:1));
                    paint.setColor(values[at+3]==0 ? Color.GRAY : i==selection ? Color.rgb(100,210,255) : Color.WHITE);
                    canvas.drawCircle(x,y,radius,paint);
                    paint.setStyle(Paint.Style.FILL);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(dp(12));
                    String[] lines=(russian ? PREVIEW_RU[i] : PREVIEW_EN[i]).split("\n");
                    float width=0;
                    for(String line:lines)width=Math.max(width,paint.measureText(line));
                    Paint.FontMetrics metrics=paint.getFontMetrics();
                    paint.setTextSize(labelSize(radius,width,(metrics.descent-metrics.ascent)*lines.length,dp(12)));
                    metrics=paint.getFontMetrics();
                    float lineHeight=metrics.descent-metrics.ascent;
                    float baseline=y-lineHeight*lines.length*.5f-metrics.ascent;
                    for(String line:lines) {canvas.drawText(line,x,baseline,paint);baseline+=lineHeight;}
                }
            }
            @Override public boolean performClick() { super.performClick(); return true; }
            @Override public boolean onTouchEvent(MotionEvent event) {
                bounds();
                int action=event.getActionMasked(), index=event.getActionIndex();
                if (action==MotionEvent.ACTION_DOWN) {
                    float x=event.getX(), y=event.getY(), best=Float.MAX_VALUE;
                    int found=-1;
                    for (int i=0;i<SLOTS.length;i++) {
                        int at=SLOTS[i]*4;
                        if (!group(SLOTS[i])) continue;
                        float dx=x-(area.left+values[at]*area.width()), dy=y-(area.top+values[at+1]*area.height());
                        float distance=dx*dx+dy*dy, radius=Math.max(dp(24),values[at+2]*area.height());
                        if (distance<=radius*radius && distance<best) { best=distance;found=i; }
                    }
                    if (found<0) return true;
                    selection=found;controls.setSelection(found);update();
                    pointer=event.getPointerId(0);
                    offsetX=x-(area.left+values[slot()*4]*area.width());
                    offsetY=y-(area.top+values[slot()*4+1]*area.height());
                    return true;
                }
                if (action==MotionEvent.ACTION_CANCEL || action==MotionEvent.ACTION_UP
                        || (action==MotionEvent.ACTION_POINTER_UP && event.getPointerId(index)==pointer)) {
                    pointer=-1;performClick();return true;
                }
                if (action==MotionEvent.ACTION_MOVE && pointer>=0 && area.width()>0 && area.height()>0) {
                    int finger=event.findPointerIndex(pointer);
                    if (finger<0) { pointer=-1;return true; }
                    int at=slot()*4;
                    float ry=values[at+2]*1.15f,rx=ry*area.height()/area.width();
                    values[at]=Math.max(rx,Math.min(1-rx,(event.getX(finger)-offsetX-area.left)/area.width()));
                    values[at+1]=Math.max(ry,Math.min(1-ry,(event.getY(finger)-offsetY-area.top)/area.height()));
                    invalidate();
                }
                return true;
            }
        }
    }
}
