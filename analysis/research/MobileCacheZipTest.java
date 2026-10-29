package com.epicgames.ue4;

import java.io.*;
import java.nio.file.Files;
import java.util.Arrays;

public final class MobileCacheZipTest {
    private static void require(boolean value) { if (!value) throw new AssertionError(); }
    private static MobileCache.Archive inspect(File file, long[] sizes) throws IOException {
        try (FileInputStream input = new FileInputStream(file)) { return MobileCache.inspect(input.getChannel(), sizes); }
    }
    private static void invalid(File file, long[] sizes) throws IOException {
        try { inspect(file, sizes); throw new AssertionError("Accepted invalid ZIP: " + file); }
        catch (IOException expected) {}
    }
    private static void extract(File zip, File target, long[] sizes) throws IOException {
        try (FileInputStream input = new FileInputStream(zip)) {
            MobileCache.Archive archive = MobileCache.inspect(input.getChannel(), sizes);
            final java.util.List<long[]> progress = new java.util.ArrayList<long[]>();
            final File destination = target;
            MobileCache.extract(input.getChannel(), archive, target, new MobileCache.Progress() {
                public void update(int file, long completed, long total) {
                    require(file >= 0 && file <= 1 && completed >= 0 && completed <= total);
                    if (!progress.isEmpty()) require(completed >= progress.get(progress.size() - 1)[0]);
                    progress.add(new long[] {completed, total});
                    try { MobileCache.recover(destination); } // Simulate a resumed Activity during the real extraction.
                    catch (IOException failure) { throw new AssertionError(failure); }
                }
            });
            require(progress.get(0)[0] == 0 && progress.get(progress.size() - 1)[0] == sizes[0] + sizes[1]);
        }
    }
    public static void main(String[] args) throws Exception {
        File root = new File(args[0]); long[] sizes = {Long.parseLong(args[1]), Long.parseLong(args[2])};
        require(!inspect(new File(root, "stored.zip"), sizes).compressed());
        require(!inspect(new File(root, "legacy.zip"), sizes).compressed());
        require(inspect(new File(root, "compressed.zip"), sizes).compressed());
        for (String name : new String[] {"duplicate", "missing", "wrongsize", "encrypted", "mismatched", "badbounds", "truncated"})
            invalid(new File(root, name + ".zip"), sizes);
        MobileCache.Archive large = inspect(new File(root, "zip64.zip"), MobileCache.SIZES);
        require(!large.compressed() && large.entries[1].offset > 5000000000L
            && large.entries[0].length == 4278831240L && large.entries[1].length == 4237413580L);
        File stored = new File(root, "out-stored"), compressed = new File(root, "out-compressed");
        extract(new File(root, "stored.zip"), stored, sizes);
        extract(new File(root, "compressed.zip"), compressed, sizes);
        for (int index = 0; index < 2; ++index) {
            File a = new File(stored, MobileCache.NAMES[index]), b = new File(compressed, MobileCache.NAMES[index]);
            require(a.length() == sizes[index] && Arrays.equals(Files.readAllBytes(a.toPath()), Files.readAllBytes(b.toPath())));
        }
        byte[] old = Files.readAllBytes(new File(stored, MobileCache.NAMES[0]).toPath());
        try { extract(new File(root, "badcrc.zip"), stored, sizes); throw new AssertionError("Accepted bad CRC"); }
        catch (IOException expected) {}
        require(Arrays.equals(old, Files.readAllBytes(new File(stored, MobileCache.NAMES[0]).toPath())));
        require(!new File(stored, ".mobile-cache-import").exists());
        File transaction = new File(stored, ".mobile-cache-import"); require(transaction.mkdir());
        Files.write(new File(transaction, "owner").toPath(), "RE4-CACHE-1".getBytes("UTF-8"));
        Files.write(new File(transaction, "journal").toPath(), new byte[] {1, 1});
        require(new File(stored, MobileCache.NAMES[0]).renameTo(new File(transaction, "old0")));
        Files.write(new File(stored, MobileCache.NAMES[0]).toPath(), new byte[] {1, 2, 3});
        java.lang.reflect.Field extracting = MobileCache.class.getDeclaredField("extracting"); extracting.setAccessible(true);
        extracting.setBoolean(null, true);
        try {
            MobileCache.recover(stored);
            require(new File(transaction, "old0").isFile());
            require(Arrays.equals(new byte[] {1, 2, 3}, Files.readAllBytes(new File(stored, MobileCache.NAMES[0]).toPath())));
            android.content.Context active = new android.content.Context(); active.obbDir = stored;
            require(MobileCache.missing(active).equals(MobileCache.NAMES[0] + "\n" + MobileCache.NAMES[1]));
        } finally { extracting.setBoolean(null, false); }
        MobileCache.recover(stored);
        require(Arrays.equals(old, Files.readAllBytes(new File(stored, MobileCache.NAMES[0]).toPath())));
        require(!transaction.exists());
        File reserved = new File(root, "unrecognized"); require(reserved.mkdir());
        transaction = new File(reserved, ".mobile-cache-import"); require(transaction.mkdir());
        File userFile = new File(transaction, "user-file"); Files.write(userFile.toPath(), new byte[] {9});
        try { MobileCache.recover(reserved); throw new AssertionError("Removed unowned transaction"); }
        catch (IOException expected) {}
        require(userFile.isFile());
        System.out.println("PASS: stored/compressed/extraction, CRC, both original sizes, 64-bit ZIP offsets, reject malformed/missing/duplicate/encrypted OBBs; interrupted replacement rollback and unowned-data preservation");
    }
}
