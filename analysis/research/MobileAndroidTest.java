import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import com.epicgames.ue4.MobileAndroid;
import java.lang.reflect.Field;
import java.util.Locale;

public final class MobileAndroidTest {
    private static native void backConfigure(boolean result);
    private static native int backCalls();
    private static native int confirmPrepare(int kind);
    private static native int confirmTake(int kind);
    private static void require(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        Context context = new Context(); Intent intent = new Intent();
        BroadcastReceiver receiver = new BroadcastReceiver();
        int update = 0x08000000, mutable = 0x02000000, immutable = 0x04000000;
        Build.VERSION.SDK_INT = 22;
        MobileAndroid.getActivity(context, 7, intent, update);
        require(PendingIntent.context == context && PendingIntent.code == 7 && PendingIntent.intent == intent && PendingIntent.flags == update);
        Build.VERSION.SDK_INT = 23;
        MobileAndroid.getActivity(context, 8, intent, update);
        require(PendingIntent.flags == (update | immutable));
        MobileAndroid.getBroadcast(context, 9, intent, update | mutable);
        require(PendingIntent.broadcast && PendingIntent.flags == (update | mutable) && PendingIntent.code == 9);
        MobileAndroid.getBroadcast(context, 10, intent, update | immutable);
        require(PendingIntent.flags == (update | immutable));
        IntentFilter privateFilter = new IntentFilter("android.intent.action.RUN");
        Build.VERSION.SDK_INT = 32;
        require(MobileAndroid.registerReceiver(context, receiver, privateFilter) == context.returnIntent && context.receiverFlags == -1);
        Build.VERSION.SDK_INT = 33;
        MobileAndroid.registerReceiver(context, receiver, privateFilter);
        require(context.receiverFlags == Context.RECEIVER_NOT_EXPORTED && context.receiver == receiver && context.filter == privateFilter);
        privateFilter.addAction("android.intent.action.BATTERY_CHANGED");
        MobileAndroid.registerReceiver(context, receiver, privateFilter);
        require(context.receiverFlags == Context.RECEIVER_EXPORTED);
        MobileAndroid.registerReceiver(context, receiver, new IntentFilter("android.intent.action.BATTERY_CHANGED"));
        require(context.receiverFlags == Context.RECEIVER_EXPORTED);

        Field cached = MobileAndroid.class.getDeclaredField("processLocale"); cached.setAccessible(true);
        Locale defaultBefore = Locale.getDefault();
        try {
            Locale.setDefault(Locale.US); cached.set(null, null);
            context.resources.configuration.selected = Locale.GERMANY;
            context.resources.configuration.locale = Locale.JAPAN;
            require(MobileAndroid.startupLocale(context).equals(Locale.GERMANY));
            context.resources.configuration.selected = Locale.FRANCE;
            require(MobileAndroid.startupLocale(context).equals(Locale.GERMANY));
            cached.set(null, null); Build.VERSION.SDK_INT = 23;
            require(MobileAndroid.startupLocale(context).equals(Locale.JAPAN));
        } finally { Locale.setDefault(defaultBefore); cached.set(null, null); }

        Activity activity = new Activity();
        Build.VERSION.SDK_INT = 33;
        activity.resources.configuration.selected = new Locale("ru", "RU"); cached.set(null, null);
        MobileAndroid.showLanguagePicker(activity);
        AlertDialog languages = AlertDialog.last;
        require(languages.showing && languages.choices.length == 7 && languages.choices[1].equals("Русский")
            && languages.choices[0].equals("Язык системы") && languages.selected == 0);
        languages.choose(1);
        require(activity.localeManager.changes == 1 && activity.localeManager.locales.get(0).toLanguageTag().equals("ru"));
        activity.localeManager.locales = android.os.LocaleList.forLanguageTags("ru-RU");
        MobileAndroid.showLanguagePicker(activity); require(AlertDialog.last.selected == 1);
        AlertDialog.last.choose(0); require(activity.localeManager.locales.isEmpty() && activity.localeManager.changes == 2);
        MobileAndroid.showLanguagePicker(activity); AlertDialog.last.choose(-1); require(activity.localeManager.changes == 2);
        MobileAndroid.showLanguagePicker(activity); AlertDialog.last.choose(7); require(activity.localeManager.changes == 2);
        int languagesCreated = AlertDialog.createdCount; activity.localeAvailable = false;
        MobileAndroid.showLanguagePicker(activity); require(AlertDialog.createdCount == languagesCreated); activity.localeAvailable = true;
        Build.VERSION.SDK_INT = 32; MobileAndroid.showLanguagePicker(activity);
        require(AlertDialog.last.showing && AlertDialog.last.choices == null && AlertDialog.last.message.toString().contains("язык системы"));
        AlertDialog.last.dismiss(); cached.set(null, null);
        com.epicgames.ue4.GameActivity languageGame=new com.epicgames.ue4.GameActivity();
        languageGame.resources.configuration.selected=new Locale("ru","RU");Build.VERSION.SDK_INT=33;
        int pickerCount=AlertDialog.createdCount;
        MobileAndroid.showLanguage(null);MobileAndroid.showLanguage(activity);require(AlertDialog.createdCount==pickerCount);
        for(int failure=0;failure<3;failure++){
            MobileAndroid.showLanguage(languageGame);require(AlertDialog.createdCount==pickerCount);
            if(failure==0)languageGame.focus=false;if(failure==1)languageGame.destroyed=true;if(failure==2)languageGame.finishing=true;
            languageGame.queued.run();require(AlertDialog.createdCount==pickerCount);
            languageGame.focus=true;languageGame.destroyed=languageGame.finishing=false;
        }
        String[] languageTags={"","ru","en","de-DE","es-419","fr-FR","ja-JP"};
        for(int choice=0;choice<languageTags.length;choice++){
            MobileAndroid.showLanguage(languageGame);require(AlertDialog.createdCount==pickerCount);languageGame.queued.run();pickerCount++;
            AlertDialog picker=AlertDialog.last;
            require(picker.choices.length==7&&picker.choices[1].equals("Русский")&&picker.title.toString().contains("следующего запуска"));
            picker.choose(choice);require(languageGame.localeManager.locales.isEmpty()==(choice==0));
            if(choice>0)require(languageGame.localeManager.locales.get(0).toLanguageTag().equals(languageTags[choice]));
            require(MobileAndroid.startupLocale(languageGame).toLanguageTag().equals("ru-RU"));
        }
        languageGame.resources.configuration.selected=languageGame.localeManager.locales.get(0);cached.set(null,null);
        require(MobileAndroid.startupLocale(languageGame).toLanguageTag().equals("ja-JP"));cached.set(null,null);
        MobileAndroid.showFiles(activity); require(activity.started == null);
        activity.focus = false; activity.queued.run(); require(activity.started == null);
        activity.focus = true; MobileAndroid.showFiles(activity); activity.queued.run();
        require(activity.started.target.getName().equals("com.epicgames.ue4.MobileFilesActivity"));
        activity.started = null; MobileAndroid.showFiles(activity); activity.destroyed = true;
        activity.queued.run(); require(activity.started == null);
        activity.destroyed = false; MobileAndroid.showFiles(activity); activity.queued.run();
        require(activity.started.target.getName().equals("com.epicgames.ue4.MobileFilesActivity"));
        boolean unavailable = false;
        try { MobileAndroid.recoverSaves(context); }
        catch (IllegalStateException expected) { unavailable = expected.getMessage().equals("External game storage unavailable"); }
        require(unavailable);
        com.epicgames.ue4.GameActivity game=new com.epicgames.ue4.GameActivity();
        Build.VERSION.SDK_INT=32;backConfigure(true);
        MobileAndroid.attachBack(game);require(game.dispatcher.registers==0);
        require(MobileAndroid.onBackPressed(game) && backCalls()==1);
        backConfigure(false);require(!MobileAndroid.onBackPressed(game) && backCalls()==1);
        backConfigure(true);game.focus=false;require(!MobileAndroid.onBackPressed(game) && backCalls()==0);
        game.focus=true;game.finishing=true;require(!MobileAndroid.onBackPressed(game) && backCalls()==0);
        game.finishing=false;game.destroyed=true;require(!MobileAndroid.onBackPressed(game) && backCalls()==0);game.destroyed=false;
        require(!MobileAndroid.onBackPressed(activity) && backCalls()==0);
        Build.VERSION.SDK_INT=33;MobileAndroid.attachBack(activity);require(activity.dispatcher.registers==0);
        MobileAndroid.attachBack(game);MobileAndroid.attachBack(game);require(game.dispatcher.registers==1);
        android.window.OnBackInvokedCallback callback=game.dispatcher.callback;
        callback.onBackInvoked();require(backCalls()==1 && game.defaultBacks==0);
        backConfigure(false);callback.onBackInvoked();require(backCalls()==1 && game.defaultBacks==1);
        backConfigure(true);game.focus=false;callback.onBackInvoked();require(backCalls()==0);game.focus=true;
        MobileAndroid.detachBack(game);require(game.dispatcher.unregisters==1 && game.dispatcher.callback==null);
        callback.onBackInvoked();require(backCalls()==0); // An old callback cannot queue after pause.
        MobileAndroid.attachBack(game);require(game.dispatcher.registers==2);
        callback.onBackInvoked();require(backCalls()==0);game.dispatcher.callback.onBackInvoked();require(backCalls()==1);
        MobileAndroid.detachBack(game);MobileAndroid.detachBack(game);require(game.dispatcher.unregisters==2);
        for (int language=0;language<2;language++) for (int kind=1;kind<=7;kind++) {
            int token=confirmPrepare(kind),created=AlertDialog.createdCount;
            require(token>0);MobileAndroid.showConfirmation(game,token,kind,language==1);
            require(AlertDialog.createdCount==created && confirmTake(kind)==0);
            game.queued.run();AlertDialog dialog=AlertDialog.last;
            require(dialog.showing && AlertDialog.showingCount==1 && dialog.getOwnerActivity()==game);
            require(dialog.title.equals(language==1?"Подтверждение":"Confirmation"));
            require(dialog.positive.equals(language==1?"Да":"Yes") && dialog.negative.equals(language==1?"Нет":"No"));
            require(dialog.message.length()>20);
            if (kind==1) require(dialog.message.toString().contains(language==1?"отдельный прогресс":"separate progress"));
            if (kind>=6) require(dialog.message.toString().contains("Start"));
            int flags=game.getWindow().getDecorView().getSystemUiVisibility();
            int focused=game.getWindow().getDecorView().focusRequests;
            game.getWindow().getDecorView().setSystemUiVisibility(0);
            dialog.click(DialogInterface.BUTTON_POSITIVE);
            require(confirmTake(kind)==1 && confirmTake(kind)==0 && !dialog.showing && AlertDialog.showingCount==0);
            require(game.getWindow().getDecorView().getSystemUiVisibility()==flags && game.getWindow().getDecorView().focusRequests==focused+1);
        }
        int token=confirmPrepare(1);MobileAndroid.showConfirmation(game,token,1,true);game.queued.run();AlertDialog old=AlertDialog.last;
        game.focus=false;int count=AlertDialog.createdCount;MobileAndroid.showConfirmation(game,token,1,true);game.queued.run();
        require(AlertDialog.createdCount==count && old.showing && confirmTake(1)==0);
        token=confirmPrepare(2);MobileAndroid.showConfirmation(game,token,2,false);game.queued.run();
        require(!old.showing && AlertDialog.last!=old && AlertDialog.showingCount==1);
        old.click(DialogInterface.BUTTON_POSITIVE);require(confirmTake(2)==0);
        AlertDialog.last.click(DialogInterface.BUTTON_NEGATIVE);require(confirmTake(2)==-1 && AlertDialog.showingCount==0);game.focus=true;
        token=confirmPrepare(3);MobileAndroid.showConfirmation(game,token,3,false);game.queued.run();AlertDialog.last.dismiss();require(confirmTake(3)==-1);
        token=confirmPrepare(4);MobileAndroid.showConfirmation(game,token,4,false);game.queued.run();old=AlertDialog.last;
        MobileAndroid.detachBack(game);require(!old.showing && confirmTake(4)==-1);old.click(DialogInterface.BUTTON_POSITIVE);require(confirmTake(4)==0);
        token=confirmPrepare(5);MobileAndroid.showConfirmation(activity,token,5,false);require(confirmTake(5)==-1);
        token=confirmPrepare(5);MobileAndroid.showConfirmation(game,token,5,false);game.destroyed=true;game.queued.run();require(confirmTake(5)==-1);game.destroyed=false;
        token=confirmPrepare(5);game.focus=false;MobileAndroid.showConfirmation(game,token,5,false);game.queued.run();require(confirmTake(5)==-1);game.focus=true;
        token=confirmPrepare(6);AlertDialog.failShow=true;MobileAndroid.showConfirmation(game,token,6,false);game.queued.run();require(confirmTake(6)==-1 && AlertDialog.showingCount==0);AlertDialog.failShow=false;
        token=confirmPrepare(6);MobileAndroid.showConfirmation(game,token,6,false);game.queued.run();game.finishing=true;AlertDialog.last.click(DialogInterface.BUTTON_POSITIVE);require(confirmTake(6)==-1);game.finishing=false;
        System.out.println("PASS: actual MobileAndroid Java and production JNI Back/confirmation bridge; both languages and seven actions, positive/negative/dismiss/lifecycle/error paths, one modal, duplicate/stale/foreign/focus rejection, immersive restoration; real android-36 release8 compilation");
    }
}
