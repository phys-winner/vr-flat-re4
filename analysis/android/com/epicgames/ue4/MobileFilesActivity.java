package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.*;
import java.util.SortedMap;

public final class MobileFilesActivity extends Activity {
    private static final int EXPORT = 451, IMPORT = 452, CACHE = 453;
    private TextView status;
    private boolean busy;
    private boolean russian;
    private MobileCacheUi.Panel cacheUi;

    public void onCreate(Bundle state) {
        super.onCreate(state);
        russian = MobileAndroid.startupLocale(this).getLanguage().equals("ru");
        if (getIntent().getBooleanExtra("cache", false)) {
            cacheUi = MobileCacheUi.activity(this, text("Подключить кеш игры", "Set up game data"),
                text("Выберите ZIP с обоими оригинальными OBB-файлами. Несжатый архив можно использовать без распаковки.",
                    "Choose a ZIP containing both original OBB files. A stored archive can be used without extracting it."),
                text("ПАПКА КЕША И ИМЕНА ФАЙЛОВ", "CACHE FOLDER AND FILE NAMES"),
                MobileCache.directoryPath(this) + "\n\n" + MobileCache.NAMES[0] + "\n" + MobileCache.NAMES[1]);
            status = cacheUi.status;
            cacheUi.actions(null,
                MobileCacheUi.quiet(text("Назад", "Back"), new Runnable() { public void run() { if (!busy) finish(); }}),
                MobileCacheUi.primary(text("Выбрать ZIP", "Choose ZIP"), new Runnable() { public void run() { if (!busy) chooseCache(); }}));
            if (state == null) chooseCache();
            return;
        }
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(36, 24, 36, 24);
        panel.setBackgroundColor(0xff14171a);
        status = new TextView(this);
        status.setTextColor(0xffeeeeee);
        status.setTextSize(18);
        status.setText(text("Сохранения\n\nПеред экспортом сохраните игру на машинке. Архив содержит игровые сохранения и штатные настройки. Импорт сохраняет резервную копию и завершает игру: затем запустите её снова. Язык меняется при следующем запуске игры.", "Saves\n\nSave at a typewriter before exporting. The archive contains game saves and the original game settings. Import preserves a backup and closes the game; launch it again afterward. Language changes apply at the next game launch."));
        panel.addView(status);
        addButton(panel, text("Экспорт сохранений", "Export saves"), new Runnable() { public void run() {
            Intent picker = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            picker.addCategory(Intent.CATEGORY_OPENABLE);
            picker.setType("application/zip");
            picker.putExtra(Intent.EXTRA_TITLE, "RE4-saves.zip");
            openDocument(picker, EXPORT);
        }});
        addButton(panel, text("Импорт сохранений", "Import saves"), new Runnable() { public void run() {
            Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            picker.addCategory(Intent.CATEGORY_OPENABLE);
            picker.setType("application/zip");
            openDocument(picker, IMPORT);
        }});
        addButton(panel, text("Язык игры", "Game language"), new Runnable() { public void run() {
            MobileAndroid.showLanguagePicker(MobileFilesActivity.this);
        }});
        addButton(panel, text("Назад", "Back"), new Runnable() { public void run() { finish(); }});
        setContentView(panel);
    }

    private String text(String ru, String en) { return russian ? ru : en; }

    private void addButton(LinearLayout panel, String title, final Runnable action) {
        Button button = new Button(this);
        button.setText(title);
        button.setOnClickListener(new View.OnClickListener() { public void onClick(View view) {
            if (!busy) action.run();
        }});
        panel.addView(button);
    }

    protected void onActivityResult(int request, int result, Intent intent) {
        super.onActivityResult(request, result, intent);
        if (result != RESULT_OK || intent == null || intent.getData() == null || busy) return;
        final Uri uri = intent.getData();
        if (request == CACHE) {
            inspectCache(uri);
        }
        else if (request == EXPORT) work(new Runnable() { public void run() {
            try (OutputStream output = getContentResolver().openOutputStream(uri, "wt")) {
                if (output == null) throw new IOException(text("Не удалось открыть файл", "Cannot open output file"));
                MobileSaves.export(root(), output);
                message(text("Архив сохранений записан.", "Save archive exported."));
            } catch (Exception failure) { error(failure); }
        }});
        else if (request == IMPORT) work(new Runnable() { public void run() {
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                if (input == null) throw new IOException(text("Не удалось открыть архив", "Cannot open archive"));
                final SortedMap<String, byte[]> saves = MobileSaves.inspect(input);
                runOnUiThread(new Runnable() { public void run() {
                    new AlertDialog.Builder(MobileFilesActivity.this)
                        .setTitle(text("Восстановить сохранения?", "Restore saves?"))
                        .setMessage(text("Будут заменены файлы: ", "Files to replace: ") + saves.size() + "\n" + android.text.TextUtils.join("\n", saves.keySet()) + text("\n\nТекущие сохранения останутся в резервном архиве. После импорта игра завершится.", "\n\nCurrent saves will be backed up. The game will close after import."))
                        .setNegativeButton(text("Отмена", "Cancel"), null)
                        .setPositiveButton(text("Восстановить", "Restore"), new android.content.DialogInterface.OnClickListener() {
                            public void onClick(android.content.DialogInterface dialog, int which) { importSaves(saves); }
                        }).show();
                }});
            } catch (Exception failure) { error(failure); }
        }});
    }

    private void chooseCache() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("*/*");
        picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        openDocument(picker, CACHE);
    }

    private void openDocument(Intent picker, int request) {
        try {
            if (picker.resolveActivity(getPackageManager()) == null) {
                documentsUnavailable();
                return;
            }
            startActivityForResult(picker, request);
        } catch (ActivityNotFoundException | SecurityException unavailable) {
            documentsUnavailable();
        }
    }

    private void documentsUnavailable() {
        String explanation = text(
            "Системный выбор файлов недоступен. Включите или восстановите приложение «Файлы» (DocumentsUI) в настройках Android, затем повторите выбор файла. OBB-файлы кеша также можно поместить вручную в папку кеша.",
            "The system file picker is unavailable. Enable or restore Files (DocumentsUI) in Android settings, then try selecting a file again. You can also place the OBB files in the game cache folder.");
        showMessage(explanation, true);
        new AlertDialog.Builder(this)
            .setTitle(text("Выбор файлов недоступен", "File picker unavailable"))
            .setMessage(explanation)
            .setPositiveButton(android.R.string.ok, null).show();
    }

    private void inspectCache(final Uri uri) {
        message(text("Проверяем состав архива…", "Checking archive contents…"));
        work(new Runnable() { public void run() {
            try (ParcelFileDescriptor descriptor = getContentResolver().openFileDescriptor(uri, "r")) {
                if (descriptor == null) throw new IOException(text("Не удалось открыть архив", "Cannot open archive"));
                try (FileInputStream input = new ParcelFileDescriptor.AutoCloseInputStream(descriptor)) {
                    final boolean compressed = MobileCache.inspect(input.getChannel(), MobileCache.SIZES).compressed();
                    runOnUiThread(new Runnable() { public void run() {
                        if (isFinishing() || isDestroyed()) return;
                        String title = compressed ? text("Архив нужно распаковать", "Extract this archive") : text("Кеш готов к подключению", "Game data found");
                        String description = compressed
                            ? text("Оба OBB-файла найдены, но они сжаты. Для распаковки и проверки нужно 8,52 ГБ свободного места.",
                                "Both OBB files are present and compressed. Extracting and verifying them requires 8.52 GB of free space.")
                            : text("OBB-файлы не сжаты. ZIP можно читать напрямую — сохраните архив на устройстве. Распаковка потребует ещё 8,52 ГБ.",
                                "The OBB files are stored without compression. Use the ZIP directly and keep it on this device, or extract it using another 8.52 GB.");
                        MobileCacheUi.Action cancel = MobileCacheUi.quiet(text("Отмена", "Cancel"), null);
                        MobileCacheUi.Action extract = (compressed ? MobileCacheUi.primary(text("Распаковать", "Extract"), new Runnable() { public void run() { useCache(uri, true); }})
                            : MobileCacheUi.secondary(text("Распаковать", "Extract"), new Runnable() { public void run() { useCache(uri, true); }}));
                        if (compressed) MobileCacheUi.show(MobileFilesActivity.this, title, description, text("СОДЕРЖИМОЕ ZIP", "ZIP CONTENTS"),
                            MobileCache.NAMES[0] + "\n" + MobileCache.NAMES[1], true, cancel, extract);
                        else MobileCacheUi.show(MobileFilesActivity.this, title, description, text("СОДЕРЖИМОЕ ZIP", "ZIP CONTENTS"),
                            MobileCache.NAMES[0] + "\n" + MobileCache.NAMES[1], true, cancel, extract,
                            MobileCacheUi.primary(text("Использовать ZIP", "Use ZIP"), new Runnable() { public void run() { useCache(uri, false); }}));
                    }});
                }
            } catch (Exception failure) {
                android.util.Log.w("RE4Cache", "Cache archive validation failed", failure);
                showMessage(text("Архив кеша бракованный или неполный. Требуются оба оригинальных OBB-файла:\n",
                    "Cache archive is invalid or incomplete. Both original OBB files are required:\n")
                    + MobileCache.NAMES[0] + "\n" + MobileCache.NAMES[1], true);
            }
        }});
    }

    private void useCache(final Uri uri, final boolean extract) {
        work(new Runnable() { public void run() {
            try {
                if (extract) {
                    message(text("Читаем данные из ZIP и проверяем контрольные суммы. Не закрывайте игру.", "Reading the ZIP and verifying checksums. Keep the game open."));
                    try (ParcelFileDescriptor descriptor = getContentResolver().openFileDescriptor(uri, "r")) {
                        if (descriptor == null) throw new IOException("Cannot open cache ZIP");
                        try (FileInputStream input = new ParcelFileDescriptor.AutoCloseInputStream(descriptor)) {
                            MobileCache.Archive archive = MobileCache.inspect(input.getChannel(), MobileCache.SIZES);
                            MobileCache.extract(input.getChannel(), archive, getObbDir(), new MobileCache.Progress() {
                                public void update(final int file, final long completed, final long total) {
                                    runOnUiThread(new Runnable() { public void run() {
                                        if (cacheUi != null && !isFinishing() && !isDestroyed()) cacheUi.progress(file, completed, total);
                                    }});
                                }
                            });
                        }
                    }
                    String previous = getSharedPreferences("mobile_cache", MODE_PRIVATE).getString(MobileCache.ZIP_PREFERENCE, null);
                    if (getSharedPreferences("mobile_cache", MODE_PRIVATE).edit().remove(MobileCache.ZIP_PREFERENCE).commit())
                        releaseCacheZip(previous, null);
                    runOnUiThread(new Runnable() { public void run() {
                        if (isFinishing() || isDestroyed()) return;
                        if (cacheUi != null) cacheUi.complete();
                        MobileCacheUi.show(MobileFilesActivity.this, text("Данные игры готовы", "Game data is ready"),
                            text("Оба OBB-файла проверены и установлены. ZIP больше не нужен — его можно удалить, чтобы освободить место.",
                                "Both OBB files were verified and installed. The ZIP is no longer needed; delete it to free storage or keep a copy."),
                            text("УСТАНОВЛЕНО В", "INSTALLED IN"), MobileCache.directoryPath(MobileFilesActivity.this), false,
                            MobileCacheUi.quiet(text("Удалить ZIP", "Delete ZIP"), new Runnable() { public void run() {
                                message(text("Удаляем ZIP-архив…", "Deleting ZIP archive…"));
                                work(new Runnable() { public void run() {
                                    try {
                                        if (!DocumentsContract.deleteDocument(getContentResolver(), uri)) throw new IOException("Archive deletion refused");
                                        runOnUiThread(new Runnable() { public void run() { finish(); }});
                                    } catch (Exception failure) { showMessage(text("Кеш установлен. Удалить архив не удалось: ", "Cache is installed. The archive could not be deleted: ") + failure.getMessage(), true); }
                                }});
                            }}),
                            MobileCacheUi.primary(text("Оставить ZIP", "Keep ZIP"), new Runnable() { public void run() { finish(); }}));
                    }});
                } else {
                    String previous = getSharedPreferences("mobile_cache", MODE_PRIVATE).getString(MobileCache.ZIP_PREFERENCE, null);
                    MobileCache.mount(MobileFilesActivity.this, uri);
                    try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); }
                    catch (SecurityException unavailable) {
                        throw new IOException(text("Хранилище не позволяет сохранить доступ к ZIP. Архив можно распаковать.", "The provider cannot preserve ZIP access. Extract the archive instead."), unavailable);
                    }
                    boolean persisted = false;
                    for (android.content.UriPermission permission : getContentResolver().getPersistedUriPermissions())
                        if (permission.getUri().equals(uri) && permission.isReadPermission()) persisted = true;
                    if (!persisted) throw new IOException(text("Хранилище не сохранило доступ к ZIP. Выберите локальный архив или распакуйте его.", "Persistent ZIP access was not granted. Choose a local archive or extract it."));
                    if (!getSharedPreferences("mobile_cache", MODE_PRIVATE).edit().putString(MobileCache.ZIP_PREFERENCE, uri.toString()).commit()) {
                        releaseCacheZip(uri.toString(), previous);
                        throw new IOException("Cannot save cache ZIP selection");
                    }
                    releaseCacheZip(previous, uri.toString());
                    runOnUiThread(new Runnable() { public void run() { finish(); }});
                }
            } catch (Exception | UnsatisfiedLinkError failure) {
                showMessage(text("Кеш не установлен: ", "Cache was not installed: ") + failure.getMessage(), true);
            }
        }});
    }

    private void releaseCacheZip(String previous, String selected) {
        if (previous == null || previous.equals(selected)) return;
        try { getContentResolver().releasePersistableUriPermission(Uri.parse(previous), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (SecurityException alreadyReleased) {}
    }

    private File root() throws IOException {
        File root = getExternalFilesDir(null);
        if (root == null || !root.isDirectory()) throw new IOException(text("Хранилище сохранений недоступно", "Save storage unavailable"));
        return root;
    }

    private void importSaves(final SortedMap<String, byte[]> saves) {
        work(new Runnable() { public void run() {
            try {
                File root = root();
                File backup = new File(root, "mobile-save-backup-" + System.currentTimeMillis() + ".zip");
                try (FileOutputStream output = new FileOutputStream(backup)) {
                    MobileSaves.export(root, output);
                }
                try (RandomAccessFile check = new RandomAccessFile(backup, "rw")) { check.getFD().sync(); }
                MobileSaves.restore(root, saves);
                runOnUiThread(new Runnable() { public void run() {
                    new AlertDialog.Builder(MobileFilesActivity.this).setCancelable(false)
                        .setTitle(text("Сохранения восстановлены", "Saves restored"))
                        .setMessage(text("Резервный архив сохранён рядом с игровыми файлами. Закройте игру и запустите её снова.", "A backup archive was saved beside the game files. Close the game and launch it again."))
                        .setPositiveButton(text("Закрыть игру", "Close game"), new android.content.DialogInterface.OnClickListener() {
                            public void onClick(android.content.DialogInterface dialog, int which) {
                                android.os.Process.killProcess(android.os.Process.myPid());
                            }
                        }).show();
                }});
            } catch (Exception failure) { error(failure); }
        }});
    }

    private void work(final Runnable operation) {
        if (busy) return;
        busy = true;
        if (cacheUi != null) cacheUi.enabled(false);
        new Thread(new Runnable() { public void run() {
            try { operation.run(); }
            finally { runOnUiThread(new Runnable() { public void run() { busy = false; if (cacheUi != null) cacheUi.enabled(true); }}); }
        }}, "RE4 saves").start();
    }

    private void message(final String text) {
        showMessage(text, false);
    }

    private void showMessage(final String text, final boolean error) {
        runOnUiThread(new Runnable() { public void run() {
            if (isFinishing() || isDestroyed()) return;
            if (cacheUi != null) cacheUi.status(text, error); else status.setText(text);
        }});
    }

    private void error(Exception failure) { showMessage(text("Операция не завершена: ", "Operation did not complete: ") + failure.getMessage(), true); }
}
