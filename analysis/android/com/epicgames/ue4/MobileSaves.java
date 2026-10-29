package com.epicgames.ue4;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

public final class MobileSaves {
    private static final String SYSTEM = "UE4Game/VR4/VR4/Saved/SaveGames/System.sav";
    private static final String INDEX = "RE4-MOBILE-SAVES-v1.sha256";
    private static final int FILE_LIMIT = 16 * 1024 * 1024;
    private static final int TOTAL_LIMIT = 128 * 1024 * 1024;
    private MobileSaves() {}

    private static boolean allowed(String name) {
        return SYSTEM.equals(name) || name.matches("savegame[0-9]{2}\\.sav");
    }

    private static File child(File root, String name) throws IOException {
        File file = new File(root, name);
        String expected = root.getCanonicalPath() + File.separator + name.replace('/', File.separatorChar);
        if (!file.getCanonicalPath().equals(expected)) throw new IOException("Unsafe save path");
        return file;
    }

    private static byte[] read(InputStream input, int maximum) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[16384];
        for (int count; (count = input.read(buffer)) != -1;) {
            if (output.size() > maximum - count) throw new IOException("Save archive is too large");
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    private static String hash(byte[] data) throws IOException {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(data);
            StringBuilder result = new StringBuilder(64);
            for (byte value : digest) result.append(String.format(Locale.ROOT, "%02x", value & 255));
            return result.toString();
        } catch (java.security.NoSuchAlgorithmException failure) { throw new IOException(failure); }
    }

    private static void validate(String name, byte[] data) throws IOException {
        if (data.length < 16 || data.length > FILE_LIMIT) throw new IOException("Invalid save size: " + name);
        if (SYSTEM.equals(name) && !(data[0] == 'G' && data[1] == 'V' && data[2] == 'A' && data[3] == 'S'))
            throw new IOException("Invalid System.sav header");
    }

    public static void export(File root, OutputStream destination) throws IOException {
        recover(root);
        TreeMap<String, byte[]> saves = new TreeMap<String, byte[]>();
        for (int index = 0; index < 100; ++index) {
            String name = String.format(Locale.ROOT, "savegame%02d.sav", index);
            snapshot(root, name, saves);
        }
        snapshot(root, SYSTEM, saves);
        if (saves.isEmpty()) throw new IOException("No saves found");
        int total = 0;
        for (byte[] data : saves.values()) {
            total += data.length;
            if (total > TOTAL_LIMIT) throw new IOException("Save collection is too large");
        }
        StringBuilder hashes = new StringBuilder();
        try (ZipOutputStream zip = new ZipOutputStream(destination)) {
            for (Map.Entry<String, byte[]> save : saves.entrySet()) {
                zip.putNextEntry(new ZipEntry(save.getKey()));
                zip.write(save.getValue());
                zip.closeEntry();
                hashes.append(hash(save.getValue())).append("  ").append(save.getKey()).append('\n');
            }
            zip.putNextEntry(new ZipEntry(INDEX));
            zip.write(hashes.toString().getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();
        }
    }

    private static void snapshot(File root, String name, Map<String, byte[]> output) throws IOException {
        File file = child(root, name);
        if (!file.exists()) return;
        long modified = file.lastModified(), length = file.length();
        byte[] data;
        try (InputStream input = new FileInputStream(file)) { data = read(input, FILE_LIMIT); }
        if (length != data.length || length != file.length() || modified != file.lastModified())
            throw new IOException("Save changed while exporting; retry after pausing");
        validate(name, data);
        output.put(name, data);
    }

    public static SortedMap<String, byte[]> inspect(InputStream source) throws IOException {
        TreeMap<String, byte[]> saves = new TreeMap<String, byte[]>();
        byte[] index = null;
        int total = 0;
        try (ZipInputStream zip = new ZipInputStream(source)) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null;) {
                String name = entry.getName();
                if (entry.isDirectory() || (!allowed(name) && !INDEX.equals(name)))
                    throw new IOException("Unexpected archive entry: " + name);
                byte[] data = read(zip, INDEX.equals(name) ? 32768 : FILE_LIMIT);
                total += data.length;
                if (total > TOTAL_LIMIT || saves.size() > 100) throw new IOException("Save archive is too large");
                if (INDEX.equals(name)) {
                    if (index != null) throw new IOException("Duplicate hash index");
                    index = data;
                } else {
                    validate(name, data);
                    if (saves.put(name, data) != null) throw new IOException("Duplicate save: " + name);
                }
                zip.closeEntry();
            }
        }
        if (index == null || saves.isEmpty()) throw new IOException("Use an archive exported by RE4 Mobile");
        StringBuilder expected = new StringBuilder();
        for (Map.Entry<String, byte[]> save : saves.entrySet())
            expected.append(hash(save.getValue())).append("  ").append(save.getKey()).append('\n');
        if (!Arrays.equals(index, expected.toString().getBytes(StandardCharsets.UTF_8)))
            throw new IOException("Save archive checksums do not match");
        return saves;
    }

    private static void synced(File file, byte[] data) throws IOException {
        File parent = file.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create save directory");
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(data);
            output.getFD().sync();
        }
    }

    private static void move(File from, File to) throws IOException {
        File parent = to.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create save directory");
        if (!from.renameTo(to)) throw new IOException("Cannot atomically move save: " + to.getName());
    }

    public static void restore(File root, SortedMap<String, byte[]> saves) throws IOException {
        recover(root);
        File transaction = child(root, ".mobile-import");
        if (!transaction.mkdir()) throw new IOException("Cannot create import transaction");
        synced(child(transaction, "owner"), "RE4-MOBILE-IMPORT-1\n".getBytes(StandardCharsets.UTF_8));
        StringBuilder journal = new StringBuilder();
        for (Map.Entry<String, byte[]> save : saves.entrySet()) {
            if (!allowed(save.getKey())) throw new IOException("Unsafe save name");
            validate(save.getKey(), save.getValue());
            File target = child(root, save.getKey());
            journal.append(target.exists() ? "1 " : "0 ").append(save.getKey()).append('\n');
            synced(child(transaction, "new/" + save.getKey()), save.getValue());
        }
        synced(child(transaction, "journal"), journal.toString().getBytes(StandardCharsets.UTF_8));
        try {
            for (String line : journal.toString().split("\n")) {
                String name = line.substring(2);
                if (line.charAt(0) == '1') move(child(root, name), child(transaction, "old/" + name));
                move(child(transaction, "new/" + name), child(root, name));
            }
            synced(child(transaction, "committed"), new byte[] {1});
        } catch (IOException failure) {
            recover(root);
            throw failure;
        }
        // Retain a normal archive before replacement, separately from the recovery journal.
        recover(root);
    }

    public static void recover(File root) throws IOException {
        File transaction = child(root, ".mobile-import");
        if (!transaction.exists()) return;
        if (!transaction.isDirectory()) throw new IOException("Reserved import path is not a directory");
        File owner = child(transaction, "owner");
        if (!owner.exists()) {
            File[] children = transaction.listFiles();
            if (children != null && children.length == 0 && transaction.delete()) return;
            throw new IOException("Unrecognized import transaction; existing files preserved");
        }
        byte[] ownership;
        try (InputStream input = new FileInputStream(owner)) { ownership = read(input, 64); }
        if (!Arrays.equals(ownership, "RE4-MOBILE-IMPORT-1\n".getBytes(StandardCharsets.UTF_8)))
            throw new IOException("Unrecognized import transaction; existing files preserved");
        File journal = child(transaction, "journal");
        if (journal.exists() && !child(transaction, "committed").exists()) {
            byte[] bytes;
            try (InputStream input = new FileInputStream(journal)) { bytes = read(input, 32768); }
            for (String line : new String(bytes, StandardCharsets.UTF_8).split("\n")) {
                if (line.length() < 3 || !(line.startsWith("0 ") || line.startsWith("1 ")) || !allowed(line.substring(2)))
                    throw new IOException("Invalid save recovery journal");
                String name = line.substring(2);
                File target = child(root, name), old = child(transaction, "old/" + name);
                if (old.exists()) {
                    if (target.exists() && !target.delete()) throw new IOException("Cannot recover replaced save");
                    move(old, target);
                } else if (line.charAt(0) == '0' && target.exists() && !target.delete())
                    throw new IOException("Cannot recover new save");
            }
        }
        remove(transaction);
    }

    private static void remove(File file) throws IOException {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot list import transaction");
            for (File child : children) {
                if (!child.getCanonicalPath().equals(child.getAbsolutePath())) throw new IOException("Unsafe transaction path");
                remove(child);
            }
        }
        if (!file.delete()) throw new IOException("Cannot remove completed import transaction");
    }
}
