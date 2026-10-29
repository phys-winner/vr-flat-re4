import com.epicgames.ue4.MobileSaves;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public class MobileSavesTest {
    private static final String SYSTEM = "UE4Game/VR4/VR4/Saved/SaveGames/System.sav";
    private interface Checked { void run() throws Exception; }
    private static void rejected(Checked operation) throws Exception {
        try { operation.run(); throw new AssertionError("Invalid input accepted"); }
        catch (IOException expected) {}
    }
    private static byte[] archive(String[] names, byte[][] contents) throws Exception {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(output)) {
            for (int index = 0; index < names.length; ++index) {
                zip.putNextEntry(new ZipEntry(names[index])); zip.write(contents[index]); zip.closeEntry();
            }
        }
        return output.toByteArray();
    }
    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        Files.createDirectories(root.resolve(SYSTEM).getParent());
        byte[] system = Files.readAllBytes(Paths.get(args[1]));
        byte[] slot = Files.readAllBytes(Paths.get(args[2]));
        Files.write(root.resolve(SYSTEM), system); Files.write(root.resolve("savegame00.sav"), slot);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        MobileSaves.export(root.toFile(), output);
        SortedMap<String, byte[]> saves = MobileSaves.inspect(new ByteArrayInputStream(output.toByteArray()));
        assert saves.size() == 2;
        assert Arrays.equals(saves.get(SYSTEM), system) && Arrays.equals(saves.get("savegame00.sav"), slot);
        Files.write(root.resolve("savegame00.sav"), new byte[32]);
        MobileSaves.restore(root.toFile(), saves);
        assert Arrays.equals(Files.readAllBytes(root.resolve("savegame00.sav")), slot);
        assert Arrays.equals(Files.readAllBytes(root.resolve(SYSTEM)), system);
        assert !Files.exists(root.resolve(".mobile-import"));
        rejected(() -> MobileSaves.inspect(new ByteArrayInputStream(archive(new String[]{"../evil.sav"}, new byte[][]{slot}))));
        rejected(() -> MobileSaves.inspect(new ByteArrayInputStream(archive(new String[]{"savegame00.sav"}, new byte[][]{slot}))));
        rejected(() -> MobileSaves.inspect(new ByteArrayInputStream(archive(new String[]{SYSTEM}, new byte[][]{new byte[16]}))));
        Path transaction = root.resolve(".mobile-import");
        Files.createDirectories(transaction.resolve("old"));
        Files.write(transaction.resolve("owner"), "RE4-MOBILE-IMPORT-1\n".getBytes(StandardCharsets.UTF_8));
        Files.move(root.resolve("savegame00.sav"), transaction.resolve("old/savegame00.sav"));
        Files.write(root.resolve("savegame00.sav"), new byte[64]);
        Files.write(root.resolve("savegame01.sav"), new byte[64]);
        Files.write(transaction.resolve("journal"), "1 savegame00.sav\n0 savegame01.sav\n".getBytes(StandardCharsets.UTF_8));
        MobileSaves.recover(root.toFile());
        assert Arrays.equals(Files.readAllBytes(root.resolve("savegame00.sav")), slot);
        assert !Files.exists(root.resolve("savegame01.sav")) && !Files.exists(transaction);
        MobileSaves.recover(root.toFile());
        Files.createDirectory(transaction);
        Files.write(transaction.resolve("foreign.txt"), new byte[]{7});
        rejected(() -> MobileSaves.recover(root.toFile()));
        assert Files.exists(transaction.resolve("foreign.txt"));
        System.out.println("PASS: actual save export/inspect/restore, exact bytes, unsafe paths/missing index/bad header refusal, interrupted transaction rollback and idempotence");
    }
}
