package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetFileDescriptor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.WeakHashMap;
import java.util.zip.CRC32;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;

/** Check the same whole OBB inputs the native reader mounts, before resuming main initialization. */
public final class MobileCache {
    static final String[] NAMES = {
        "main.201.com.Armature.VR4.obb", "patch.201.com.Armature.VR4.obb"
    };
    static final long[] SIZES = {4278831240L, 4237413580L};
    private static final WeakHashMap<Activity, AlertDialog> dialogs = new WeakHashMap<Activity, AlertDialog>();
    private static volatile boolean extracting;
    static final String ZIP_PREFERENCE = "cache_zip_uri";
    private static native boolean nativeZip(int fd, long mainOffset, long mainLength, long patchOffset, long patchLength);

    private MobileCache() {}

    private static boolean readableRange(FileInputStream input, long start, long size, boolean wholeFile)
            throws IOException {
        FileChannel channel = input.getChannel();
        long available = channel.size();
        if (start < 0 || start > available || size > available - start || (wholeFile && available != size))
            return false;
        ByteBuffer probe = ByteBuffer.allocate(1);
        if (channel.read(probe, start) != 1) return false;
        ((java.nio.Buffer) probe).clear();
        return channel.read(probe, start + size - 1) == 1;
    }

    private static boolean packaged(Context context, int index) {
        // openFd accepts only the stored assets supported by the native descriptor-backed reader.
        try (AssetFileDescriptor asset = context.getAssets().openFd(NAMES[index])) {
            if (asset.getLength() != SIZES[index]) return false;
            try (FileInputStream input = asset.createInputStream()) {
                return readableRange(input, asset.getStartOffset(), SIZES[index], false);
            }
        } catch (IOException | SecurityException unavailable) { return false; }
    }

    private static boolean external(File directory, int index) {
        if (directory == null) return false;
        try {
            File file = new File(directory, NAMES[index]);
            if (!file.isFile()) return false;
            try (FileInputStream input = new FileInputStream(file)) {
                return readableRange(input, 0, SIZES[index], true);
            }
        } catch (IOException | SecurityException unavailable) { return false; }
    }

    static synchronized String missing(Context context) {
        if (extracting) return NAMES[0] + "\n" + NAMES[1];
        File directory = null;
        try { directory = context.getObbDir(); }
        catch (SecurityException unavailable) {}
        try { if (directory != null) recover(directory); }
        catch (IOException unavailable) { return NAMES[0] + "\n" + NAMES[1]; }
        if (selectedZip(context)) return "";
        StringBuilder missing = new StringBuilder();
        for (int index = 0; index < NAMES.length; ++index) {
            if (!packaged(context, index) && !external(directory, index)) {
                if (missing.length() != 0) missing.append('\n');
                missing.append(NAMES[index]);
            }
        }
        return missing.toString();
    }

    static String directoryPath(Context context) {
        File directory = null;
        try { directory = context.getObbDir(); }
        catch (SecurityException unavailable) {}
        return directory != null ? directory.getAbsolutePath()
            : new File(android.os.Environment.getExternalStorageDirectory(), "Android/obb/" + context.getPackageName()).getAbsolutePath();
    }

    public static void resumeMainInit(final Activity activity, final Runnable startup) {
        if (activity.isFinishing() || activity.isDestroyed()) return;
        AlertDialog current = dialogs.get(activity);
        if (current != null && current.isShowing()) return;
        MobileLoading.update(activity, 1);
        String missing = missing(activity);
        if (missing.length() == 0) {
            MobileLoading.update(activity, 2);
            startup.run();
            return;
        }
        MobileLoading.update(activity, 0);
        boolean russian = MobileAndroid.startupLocale(activity).getLanguage().equals("ru");
        String path = directoryPath(activity);
        String message = russian
            ? "Для запуска нужны оба файла данных игры. Выберите ZIP-архив или поместите оригинальные OBB-файлы в папку ниже и нажмите «Повторить»."
            : "Both game data files are required. Choose a ZIP archive, or place the original OBB files in the folder below and tap Retry.";
        AlertDialog dialog = MobileCacheUi.create(activity, russian ? "Кеш не найден" : "Cache not found", message,
            russian ? "ПАПКА КЕША И ИМЕНА ФАЙЛОВ" : "CACHE FOLDER AND FILE NAMES", path + "\n\n" + NAMES[0] + "\n" + NAMES[1], false,
            MobileCacheUi.quiet(russian ? "Выход" : "Exit", new Runnable() {
                public void run() {
                    dialogs.remove(activity);
                    // NativeActivity destruction waits for the engine thread still blocked on main init.
                    try { activity.finishAndRemoveTask(); }
                    finally { android.os.Process.killProcess(android.os.Process.myPid()); }
                }
            }),
            MobileCacheUi.secondary(russian ? "Повторить" : "Retry", new Runnable() {
                public void run() {
                    dialogs.remove(activity);
                    resumeMainInit(activity, startup);
                }
            }),
            MobileCacheUi.primary(russian ? "Выбрать ZIP" : "Choose ZIP", new Runnable() {
                public void run() {
                    dialogs.remove(activity);
                    activity.startActivity(new Intent(activity, MobileFilesActivity.class).putExtra("cache", true));
                }
            }));
        dialogs.put(activity, dialog);
        dialog.show();
    }

    public static void detach(Activity activity, boolean initialized) {
        MobileLoading.detach(activity);
        AlertDialog dialog = dialogs.remove(activity);
        if (dialog != null) dialog.dismiss();
        if (!initialized) android.os.Process.killProcess(android.os.Process.myPid());
    }

    private static boolean selectedZip(Context context) {
        String value = context.getSharedPreferences("mobile_cache", Context.MODE_PRIVATE).getString(ZIP_PREFERENCE, null);
        if (value == null) return false;
        try { mount(context, Uri.parse(value)); return true; }
        catch (IOException | SecurityException | UnsatisfiedLinkError unavailable) { return false; }
    }

    static void mount(Context context, Uri uri) throws IOException {
        try (ParcelFileDescriptor descriptor = context.getContentResolver().openFileDescriptor(uri, "r")) {
            if (descriptor == null) throw new IOException("Cannot open cache ZIP");
            try (FileInputStream input = new ParcelFileDescriptor.AutoCloseInputStream(descriptor)) {
                Archive archive = inspect(input.getChannel(), SIZES);
                if (archive.compressed()) throw new IOException("Compressed OBB files must be extracted");
                if (!nativeZip(descriptor.getFd(), archive.entries[0].offset, SIZES[0], archive.entries[1].offset, SIZES[1]))
                    throw new IOException("Cannot mount cache ZIP");
            }
        }
    }

    static final class Entry {
        long offset, length, compressed, crc;
        int method;
    }
    static final class Archive {
        final Entry[] entries = new Entry[2];
        boolean compressed() { return entries[0].method != 0 || entries[1].method != 0; }
    }

    private static ByteBuffer read(FileChannel channel, long offset, int length) throws IOException {
        if (offset < 0 || length < 0 || offset > channel.size() - length) throw new IOException("Truncated cache ZIP");
        ByteBuffer bytes = ByteBuffer.allocate(length).order(ByteOrder.LITTLE_ENDIAN);
        while (bytes.hasRemaining()) {
            int count = channel.read(bytes, offset + bytes.position());
            if (count <= 0) throw new IOException("Cannot read cache ZIP");
        }
        ((java.nio.Buffer) bytes).flip();
        return bytes;
    }
    private static int u16(ByteBuffer bytes, int at) { return bytes.getShort(at) & 65535; }
    private static long u32(ByteBuffer bytes, int at) { return bytes.getInt(at) & 0xffffffffL; }
    private static long positive(long value) throws IOException {
        if (value < 0) throw new IOException("ZIP64 value exceeds supported file size");
        return value;
    }

    /** ZIP64 is required: the two original OBB files together exceed eight gigabytes. */
    static Archive inspect(FileChannel channel, long[] sizes) throws IOException {
        long length = channel.size();
        ByteBuffer tail = read(channel, Math.max(0, length - 65557), (int)Math.min(length, 65557));
        int eocd = -1;
        for (int at = tail.limit() - 22; at >= 0; --at)
            if (tail.getInt(at) == 0x06054b50 && at + 22 + u16(tail, at + 20) == tail.limit()) { eocd = at; break; }
        if (eocd < 0) throw new IOException("Invalid cache ZIP directory");
        long end = length - tail.limit() + eocd;
        if (u16(tail, eocd + 4) != 0 || u16(tail, eocd + 6) != 0)
            throw new IOException("Split ZIP archives are not supported");
        long count = u16(tail, eocd + 10), bytes = u32(tail, eocd + 12), start = u32(tail, eocd + 16);
        if (count == 65535 || bytes == 0xffffffffL || start == 0xffffffffL) {
            ByteBuffer locator = read(channel, end - 20, 20);
            if (locator.getInt(0) != 0x07064b50 || locator.getInt(4) != 0 || locator.getInt(16) != 1)
                throw new IOException("Invalid ZIP64 locator");
            long location = positive(locator.getLong(8));
            ByteBuffer zip64 = read(channel, location, 56);
            long record = positive(zip64.getLong(4));
            if (zip64.getInt(0) != 0x06064b50 || record < 44 || record > end - 20 - location - 12
                || zip64.getInt(16) != 0 || zip64.getInt(20) != 0 || zip64.getLong(24) != zip64.getLong(32))
                throw new IOException("Invalid ZIP64 directory");
            count = positive(zip64.getLong(32)); bytes = positive(zip64.getLong(40)); start = positive(zip64.getLong(48));
            end = location;
        } else if (u16(tail, eocd + 8) != count) throw new IOException("Split ZIP directory");
        if (count < 2 || count > 100000 || start > end || bytes > end - start)
            throw new IOException("Invalid cache ZIP directory bounds");
        long directoryEnd = start + bytes, at = start;
        Archive archive = new Archive();
        for (long item = 0; item < count; ++item) {
            if (at > directoryEnd - 46) throw new IOException("Truncated ZIP directory entry");
            ByteBuffer header = read(channel, at, 46);
            if (header.getInt(0) != 0x02014b50) throw new IOException("Invalid ZIP entry");
            int nameLength = u16(header, 28), extraLength = u16(header, 30), commentLength = u16(header, 32);
            if (46L + nameLength + extraLength + commentLength > directoryEnd - at) throw new IOException("Invalid ZIP entry length");
            byte[] name = read(channel, at + 46, nameLength).array();
            String path = new String(name, StandardCharsets.UTF_8);
            String leaf = path.substring(path.lastIndexOf('/') + 1);
            for (int index = 0; index < 2; ++index) if (NAMES[index].equals(leaf.replace(".201.VRLOC.com.Armature.VR4.obb", ".201.com.Armature.VR4.obb"))) {
                if (archive.entries[index] != null || path.startsWith("/") || path.contains("\\") || path.contains("../") || path.indexOf('\0') >= 0)
                    throw new IOException("Duplicate or unsafe cache OBB path");
                long unpacked = u32(header, 24), packed = u32(header, 20), local = u32(header, 42), disk = u16(header, 34);
                ByteBuffer extra = read(channel, at + 46 + nameLength, extraLength);
                for (int field = 0; field + 4 <= extraLength;) {
                    int kind = u16(extra, field), size = u16(extra, field + 2); field += 4;
                    if (size > extraLength - field) throw new IOException("Invalid ZIP extra field");
                    if (kind == 1) {
                        int cursor = field;
                        if (unpacked == 0xffffffffL) { if (cursor + 8 > field + size) throw new IOException("Missing ZIP64 length"); unpacked = positive(extra.getLong(cursor)); cursor += 8; }
                        if (packed == 0xffffffffL) { if (cursor + 8 > field + size) throw new IOException("Missing ZIP64 packed length"); packed = positive(extra.getLong(cursor)); cursor += 8; }
                        if (local == 0xffffffffL) { if (cursor + 8 > field + size) throw new IOException("Missing ZIP64 offset"); local = positive(extra.getLong(cursor)); cursor += 8; }
                        if (disk == 65535) { if (cursor + 4 > field + size) throw new IOException("Missing ZIP64 disk"); disk = u32(extra, cursor); }
                    }
                    field += size;
                }
                int flags = u16(header, 8), method = u16(header, 10);
                if (disk != 0 || (flags & 65) != 0 || (method != 0 && method != 8) || unpacked != sizes[index]
                    || packed <= 0 || (method == 0 && packed != unpacked) || local >= start)
                    throw new IOException("Invalid, encrypted, or wrong-size OBB: " + leaf);
                ByteBuffer localHeader = read(channel, local, 30);
                int localName = u16(localHeader, 26), localExtra = u16(localHeader, 28);
                if (localHeader.getInt(0) != 0x04034b50 || u16(localHeader, 6) != flags || u16(localHeader, 8) != method
                    || localName != nameLength || !Arrays.equals(name, read(channel, local + 30, localName).array()))
                    throw new IOException("Cache ZIP local header disagrees with directory");
                long offset = local + 30L + localName + localExtra;
                if (offset > start || packed > start - offset) throw new IOException("Cache OBB exceeds ZIP data bounds");
                Entry entry = new Entry(); entry.offset = offset; entry.length = unpacked; entry.compressed = packed;
                entry.method = method; entry.crc = u32(header, 16); archive.entries[index] = entry;
            }
            at += 46L + nameLength + extraLength + commentLength;
        }
        if (at != directoryEnd || archive.entries[0] == null || archive.entries[1] == null)
            throw new IOException("Cache ZIP must contain both original OBB files");
        Entry a = archive.entries[0], b = archive.entries[1];
        if (a.offset < b.offset + b.compressed && b.offset < a.offset + a.compressed)
            throw new IOException("Overlapping cache OBB files");
        for (Entry entry : archive.entries) if (entry.method == 0 && read(channel, entry.offset, 4).getInt(0) != 0x04034b50)
            throw new IOException("Invalid OBB container header");
        return archive;
    }

    interface Progress { void update(int file, long completed, long total); }

    static void extract(FileChannel channel, Archive archive, File directory) throws IOException {
        extract(channel, archive, directory, null);
    }

    static void extract(FileChannel channel, Archive archive, File directory, Progress progress) throws IOException {
        if (directory == null || (!directory.isDirectory() && !directory.mkdirs())) throw new IOException("OBB storage unavailable");
        long total = archive.entries[0].length + archive.entries[1].length;
        if (directory.getUsableSpace() < total + 1048576) throw new IOException("Not enough free space to extract both OBB files");
        File transaction = new File(directory, ".mobile-cache-import");
        synchronized (MobileCache.class) {
            if (extracting) throw new IOException("Cache extraction is already running");
            recover(directory);
            if (!transaction.mkdir()) throw new IOException("Cannot create cache import transaction");
            extracting = true;
        }
        try {
            long lastProgress = System.nanoTime(), completed = 0;
            if (progress != null) progress.update(0, 0, total);
            try (FileOutputStream output = new FileOutputStream(new File(transaction, "owner"))) {
                output.write("RE4-CACHE-1".getBytes(StandardCharsets.UTF_8)); output.getFD().sync();
            }
            for (int index = 0; index < 2; ++index) {
                final Entry entry = archive.entries[index];
                InputStream slice = new InputStream() {
                    long position;
                    public int read() throws IOException { byte[] value = new byte[1]; return read(value, 0, 1) == -1 ? -1 : value[0] & 255; }
                    public int read(byte[] value, int offset, int length) throws IOException {
                        if (length == 0) return 0;
                        if (position == entry.compressed) return -1;
                        int count = channel.read(ByteBuffer.wrap(value, offset, (int)Math.min(length, entry.compressed - position)), entry.offset + position);
                        if (count <= 0) throw new IOException("Truncated cache OBB data");
                        position += count; return count;
                    }
                };
                Inflater inflater = entry.method == 8 ? new Inflater(true) : null;
                try (InputStream input = inflater == null ? slice : new InflaterInputStream(slice, inflater, 65536);
                     FileOutputStream output = new FileOutputStream(new File(transaction, "new" + index))) {
                    byte[] buffer = new byte[1048576]; CRC32 crc = new CRC32(); long written = 0;
                    for (int count; (count = input.read(buffer)) != -1;) {
                        if (count > entry.length - written) throw new IOException("Cache OBB exceeds expected size");
                        output.write(buffer, 0, count); crc.update(buffer, 0, count); written += count;
                        long now = System.nanoTime();
                        if (progress != null && (now - lastProgress >= 100000000L || written == entry.length)) {
                            progress.update(index, completed + written, total); lastProgress = now;
                        }
                    }
                    if (written != entry.length || crc.getValue() != entry.crc
                        || (inflater != null && (!inflater.finished() || inflater.getBytesRead() != entry.compressed)))
                        throw new IOException("Cache OBB checksum mismatch");
                    output.getFD().sync();
                } finally { if (inflater != null) inflater.end(); }
                try (FileInputStream input = new FileInputStream(new File(transaction, "new" + index))) {
                    if (read(input.getChannel(), 0, 4).getInt(0) != 0x04034b50) throw new IOException("Invalid extracted OBB container");
                }
                completed += entry.length;
            }
            byte[] journal = {(byte)(new File(directory, NAMES[0]).exists() ? 1 : 0), (byte)(new File(directory, NAMES[1]).exists() ? 1 : 0)};
            File stagedJournal = new File(transaction, "journal-new");
            try (FileOutputStream output = new FileOutputStream(stagedJournal)) { output.write(journal); output.getFD().sync(); }
            if (!stagedJournal.renameTo(new File(transaction, "journal"))) throw new IOException("Cannot prepare cache recovery journal");
            for (int index = 0; index < 2; ++index) {
                File target = new File(directory, NAMES[index]);
                if (journal[index] == 1 && !target.renameTo(new File(transaction, "old" + index))) throw new IOException("Cannot back up existing OBB");
                if (!new File(transaction, "new" + index).renameTo(target)) throw new IOException("Cannot install cache OBB");
            }
            try (FileOutputStream output = new FileOutputStream(new File(transaction, "committed"))) { output.write(1); output.getFD().sync(); }
        } finally {
            synchronized (MobileCache.class) { extracting = false; recover(directory); }
        }
    }

    static synchronized void recover(File directory) throws IOException {
        // Returning from the picker must not roll back a worker that still owns its transaction.
        if (extracting) return;
        File transaction = new File(directory, ".mobile-cache-import");
        if (!transaction.exists()) return;
        if (!transaction.isDirectory() || !transaction.getCanonicalPath().equals(transaction.getAbsolutePath()))
            throw new IOException("Unsafe cache import transaction");
        File[] files = transaction.listFiles();
        if (files == null) throw new IOException("Cannot list cache transaction");
        for (File file : files) {
            if (!file.getName().matches("new[01]|old[01]|journal|journal-new|committed|owner") || !file.isFile()
                || !file.getCanonicalPath().equals(file.getAbsolutePath())) throw new IOException("Unrecognized cache transaction file");
        }
        try (FileInputStream input = new FileInputStream(new File(transaction, "owner"))) {
            byte[] expected = "RE4-CACHE-1".getBytes(StandardCharsets.UTF_8), owner = new byte[expected.length];
            if (input.read(owner) != owner.length || input.read() != -1 || !Arrays.equals(owner, expected))
                throw new IOException("Unrecognized cache import transaction");
        }
        File journal = new File(transaction, "journal");
        if (journal.exists() && !new File(transaction, "committed").exists()) {
            byte[] state = new byte[2];
            try (FileInputStream input = new FileInputStream(journal)) {
                if (input.read(state) != 2 || input.read() != -1 || state[0] > 1 || state[1] > 1 || state[0] < 0 || state[1] < 0)
                    throw new IOException("Invalid cache recovery journal");
            }
            for (int index = 0; index < 2; ++index) {
                File target = new File(directory, NAMES[index]), old = new File(transaction, "old" + index);
                if (old.exists()) {
                    if (target.exists() && !target.delete()) throw new IOException("Cannot restore old cache OBB");
                    if (!old.renameTo(target)) throw new IOException("Cannot restore cache backup");
                } else if (state[index] == 0 && target.exists() && !target.delete()) throw new IOException("Cannot roll back cache import");
            }
        }
        files = transaction.listFiles();
        if (files == null) throw new IOException("Cannot list completed cache transaction");
        for (File file : files) if (!file.getName().equals("owner") && !file.delete()) throw new IOException("Cannot remove cache transaction file");
        if (!new File(transaction, "owner").delete()) throw new IOException("Cannot remove cache transaction owner");
        if (!transaction.delete()) throw new IOException("Cannot remove cache transaction");
    }
}
