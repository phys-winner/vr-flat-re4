package com.epicgames.ue4;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.res.AssetManager;
import java.io.File;
import java.nio.file.Files;
import java.util.Locale;

public final class MobileCacheTest {
    private static void require(boolean value) { if (!value) throw new AssertionError(); }
    private static File obb(File directory, int index) { return new File(directory, MobileCache.NAMES[index]); }
    private static void packaged(Context context, File original, int index) {
        context.assets.entries.put(MobileCache.NAMES[index], new AssetManager.Entry(obb(original, index), 0, MobileCache.SIZES[index]));
    }
    public static void main(String[] args) throws Exception {
        File original = new File(args[0]), temporary = new File(args[1]);
        for (int i = 0; i < 2; ++i) require(obb(original, i).length() == MobileCache.SIZES[i]);
        Context context = new Context();
        context.obbDir = original;
        require(MobileCache.missing(context).isEmpty()); // Actual untouched whole OBB layout.

        context.obbDir = temporary;
        String both = String.join("\n", MobileCache.NAMES);
        require(MobileCache.missing(context).equals(both));
        Files.createLink(obb(temporary, 1).toPath(), obb(original, 1).toPath());
        require(MobileCache.missing(context).equals(MobileCache.NAMES[0]));
        Files.delete(obb(temporary, 1).toPath());
        Files.createLink(obb(temporary, 0).toPath(), obb(original, 0).toPath());
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        Files.delete(obb(temporary, 0).toPath());
        Files.write(obb(temporary, 0).toPath(), new byte[] {1});
        Files.createLink(obb(temporary, 1).toPath(), obb(original, 1).toPath());
        require(MobileCache.missing(context).equals(MobileCache.NAMES[0]));

        packaged(context, original, 0);
        require(MobileCache.missing(context).isEmpty()); // Packaged main + external patch, same release.
        Files.delete(obb(temporary, 1).toPath());
        packaged(context, original, 1);
        require(MobileCache.missing(context).isEmpty()); // Both packaged, no valid external set.
        AssetManager.Entry patch = context.assets.entries.get(MobileCache.NAMES[1]);
        patch.length--;
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        patch.length++;
        patch.start = -1;
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        patch.start = Long.MAX_VALUE;
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        patch.start = 0;
        patch.file = obb(temporary, 0); // Declared asset length is right, underlying APK is truncated.
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        patch.file = obb(original, 0);
        patch.start = 4096; // A stored asset slice uses the descriptor's nonzero APK offset.
        require(MobileCache.missing(context).isEmpty());
        patch.start = patch.file.length() - patch.length + 1;
        require(MobileCache.missing(context).equals(MobileCache.NAMES[1]));
        context.obbDir = original;
        require(MobileCache.missing(context).isEmpty()); // Invalid packaged asset falls back to external.
        context.assets.denied = true;
        require(MobileCache.missing(context).isEmpty());
        context.obbDenied = true;
        require(MobileCache.missing(context).equals(both));
        context.assets.denied = false;
        context.assets.entries.clear();
        context.obbDenied = false;
        context.obbDir = null;
        require(MobileCache.missing(context).equals(both));
        require(context.assets.opened == context.assets.closed);

        final int[] starts = {0};
        Runnable startup = new Runnable() { public void run() { starts[0]++; }};
        Activity activity = new Activity(); activity.obbDir = temporary;
        MobileCache.resumeMainInit(activity, startup);
        AlertDialog first = AlertDialog.latest;
        require(starts[0] == 0 && first.showing && !first.cancelable);
        require(MobileLoading.stages.toString().equals("[1, 0]"));
        require(first.title.equals("Cache not found") && first.message.contains(both)
            && first.message.contains(temporary.getAbsolutePath()) && first.positiveText.equals("Choose ZIP")
            && first.negativeText.equals("Retry") && first.neutralText.equals("Exit"));
        MobileCache.resumeMainInit(activity, startup);
        require(AlertDialog.latest == first && starts[0] == 0); // Repeated onResume does not duplicate dialogs.
        first.negative();
        AlertDialog second = AlertDialog.latest;
        require(second != first && second.showing && !first.showing && starts[0] == 0);
        activity.obbDir = original;
        second.negative();
        require(starts[0] == 1 && !second.showing && android.os.Process.kills == 0); // Retry only resumes after both inputs pass.
        require(MobileLoading.stages.toString().endsWith("1, 2]"));

        activity.obbDir = null; activity.locale = new Locale("ru");
        MobileCache.resumeMainInit(activity, startup);
        AlertDialog russian = AlertDialog.latest;
        require(russian.title.equals("Кеш не найден") && russian.message.contains(both)
            && russian.message.contains(new File(android.os.Environment.getExternalStorageDirectory(), "Android/obb/com.Armature.VR4").getAbsolutePath())
            && russian.positiveText.equals("Выбрать ZIP") && russian.negativeText.equals("Повторить") && russian.neutralText.equals("Выход"));
        russian.neutral();
        require(activity.finishing && activity.removedTasks == 1 && starts[0] == 1
            && android.os.Process.kills == 1 && android.os.Process.lastKilled == android.os.Process.myPid());
        MobileCache.resumeMainInit(activity, startup);
        require(AlertDialog.latest == russian);

        Activity destroyed = new Activity();
        MobileCache.resumeMainInit(destroyed, startup);
        AlertDialog pending = AlertDialog.latest;
        destroyed.destroyed = true;
        MobileCache.detach(destroyed, false);
        require(!pending.showing && android.os.Process.kills == 2);
        pending.negative();
        require(starts[0] == 1);
        require(destroyed.assets.opened == destroyed.assets.closed);
        Activity failedExit = new Activity(); failedExit.throwOnRemove = true;
        MobileCache.resumeMainInit(failedExit, startup);
        boolean failed = false;
        try { AlertDialog.latest.neutral(); }
        catch (IllegalStateException expected) { failed = true; }
        require(failed && failedExit.removedTasks == 1 && android.os.Process.kills == 3 && starts[0] == 1);
        Activity cached = new Activity(); cached.obbDir = original;
        MobileCache.resumeMainInit(cached, startup);
        require(starts[0] == 2 && cached.removedTasks == 0 && android.os.Process.kills == 3);
        cached.destroyed = true; MobileCache.detach(cached, true); MobileCache.detach(cached, true);
        require(android.os.Process.kills == 3);
        Activity beforeResume = new Activity(); beforeResume.destroyed = true;
        MobileCache.detach(beforeResume, false);
        require(android.os.Process.kills == 4 && starts[0] == 2);
        final int[] chooseStarts = {0}; Runnable chooseStartup = new Runnable() { public void run() { chooseStarts[0]++; }};
        final Activity choose = new Activity(); choose.obbDir = temporary;
        MobileCache.resumeMainInit(choose, chooseStartup);
        AlertDialog initial = AlertDialog.latest;
        final boolean[] focusAfterLaunch = {false};
        AlertDialog.dismissed = new Runnable() { public void run() {
            focusAfterLaunch[0] = choose.started != null;
            MobileCache.resumeMainInit(choose, chooseStartup); // A focus restoration can re-enter the startup guard.
        }};
        try { initial.positive(); } finally { AlertDialog.dismissed = null; }
        require(focusAfterLaunch[0]); // Preserve native AlertDialog ordering: action before dismissal/focus restoration.
        require(choose.started != null && !initial.isShowing() && chooseStarts[0] == 0);
        MobileCache.resumeMainInit(choose, chooseStartup); // Picker cancellation/Back recreates the missing dialog.
        AlertDialog cancelled = AlertDialog.latest; require(cancelled != initial && cancelled.isShowing());
        cancelled.positive(); choose.obbDir = original;
        MobileCache.resumeMainInit(choose, chooseStartup); // A completed import/selection resumes through the same guard.
        require(chooseStarts[0] == 1 && !cancelled.isShowing());
        System.out.println("PASS: real 4GB OBB files, missing main/patch/both, truncated and denied inputs, packaged/external/mixed sources, bounded nonzero/invalid/overflow descriptor ranges, resource cleanup; English/Russian dialog, duplicate resume, retry, own-process Exit including task-service failure, no termination on valid cache/retry and destroyed Activity");
    }
}
