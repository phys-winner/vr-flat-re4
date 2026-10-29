"""Exercise the production cache guard using real OBB sizes/files and small Android UI doubles."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JDK = Path(os.path.join(os.environ['JAVA_HOME'], 'bin'))
SDK = Path(os.environ['ANDROID_SDK_ROOT'])
PRODUCTION = ROOT / 'analysis/android/com/epicgames/ue4/MobileCache.java'
SOURCES = {
    'android/content/Context.java': '''package android.content;
public class Context {
 public final android.content.res.AssetManager assets=new android.content.res.AssetManager();
 public java.io.File obbDir; public boolean obbDenied; public java.util.Locale locale=java.util.Locale.US;
 public android.content.res.AssetManager getAssets(){return assets;}
 public java.io.File getObbDir(){if(obbDenied)throw new SecurityException();return obbDir;}
 public String getPackageName(){return "com.Armature.VR4";}
 public static final int MODE_PRIVATE=0;
 public android.content.SharedPreferences getSharedPreferences(String name,int mode){return new android.content.SharedPreferences();}
 public android.content.ContentResolver getContentResolver(){return new android.content.ContentResolver();}
}''',
    'android/content/SharedPreferences.java': '''package android.content;
public class SharedPreferences {public String getString(String key,String fallback){return fallback;}}''',
    'android/content/ContentResolver.java': '''package android.content;
public class ContentResolver {public android.os.ParcelFileDescriptor openFileDescriptor(android.net.Uri uri,String mode)throws java.io.IOException{throw new java.io.IOException();}}''',
    'android/content/Intent.java': '''package android.content;
public class Intent {public Intent(Context context,Class<?> type){} public Intent putExtra(String key,boolean value){return this;}}''',
    'android/net/Uri.java': '''package android.net;
public class Uri {public static Uri parse(String value){return new Uri();}}''',
    'android/os/Environment.java': '''package android.os;
public class Environment {public static java.io.File getExternalStorageDirectory(){return new java.io.File("/storage/emulated/0");}}''',
    'android/os/ParcelFileDescriptor.java': '''package android.os;
public class ParcelFileDescriptor implements java.io.Closeable {
 public java.io.FileDescriptor getFileDescriptor(){return new java.io.FileDescriptor();} public int getFd(){return -1;} public void close(){}
 public static class AutoCloseInputStream extends java.io.FileInputStream {public AutoCloseInputStream(ParcelFileDescriptor fd){super(fd.getFileDescriptor());}}
}''',
    'android/content/DialogInterface.java': '''package android.content;
public interface DialogInterface { interface OnClickListener { void onClick(DialogInterface dialog,int which); } }''',
    'android/content/res/AssetManager.java': '''package android.content.res;
public final class AssetManager {
 public static final class Entry { public java.io.File file; public long start,length;
  public Entry(java.io.File f,long s,long n){file=f;start=s;length=n;} }
 public final java.util.Map<String,Entry> entries=new java.util.HashMap<String,Entry>();
 public boolean denied; public int opened,closed;
 public AssetFileDescriptor openFd(String name) throws java.io.IOException {
  if(denied)throw new SecurityException(); Entry e=entries.get(name);
  if(e==null)throw new java.io.FileNotFoundException(name);opened++;return new AssetFileDescriptor(this,e);
 }
}''',
    'android/content/res/AssetFileDescriptor.java': '''package android.content.res;
public final class AssetFileDescriptor implements java.io.Closeable {
 private final AssetManager owner; private final AssetManager.Entry entry; private boolean closed;
 AssetFileDescriptor(AssetManager a,AssetManager.Entry e){owner=a;entry=e;}
 public long getLength(){return entry.length;} public long getStartOffset(){return entry.start;}
 public java.io.FileInputStream createInputStream() throws java.io.IOException {return new java.io.FileInputStream(entry.file);}
 public void close(){if(!closed){closed=true;owner.closed++;}}
}''',
    'android/app/Activity.java': '''package android.app;
public class Activity extends android.content.Context {
 public boolean finishing,destroyed,throwOnRemove; public int removedTasks;
 public boolean isFinishing(){return finishing;} public boolean isDestroyed(){return destroyed;}
 public void finish(){finishing=true;}
 public android.content.Intent started; public void startActivity(android.content.Intent intent){started=intent;}
 public void finishAndRemoveTask(){removedTasks++;if(throwOnRemove)throw new IllegalStateException("task service unavailable");finishing=true;}
}''',
    'android/os/Process.java': '''package android.os;
public final class Process {
 public static int kills,lastKilled; public static int myPid(){return 1234;}
 public static void killProcess(int pid){if(pid!=myPid())throw new AssertionError();kills++;lastKilled=pid;}
}''',
    'android/app/AlertDialog.java': '''package android.app;
import android.content.DialogInterface;
public final class AlertDialog implements DialogInterface {
 public static AlertDialog latest; public static Runnable dismissed; public String title,message,positiveText,negativeText,neutralText; public boolean showing,cancelable=true;
 private DialogInterface.OnClickListener positive,negative,neutral;
 public boolean isShowing(){return showing;} public void show(){showing=true;latest=this;} public void dismiss(){if(!showing)return;showing=false;if(dismissed!=null)dismissed.run();}
 public void positive(){positive.onClick(this,-1);dismiss();} public void negative(){negative.onClick(this,-2);dismiss();}
 public void neutral(){neutral.onClick(this,-3);dismiss();}
 public static final class Builder {
  private final AlertDialog dialog=new AlertDialog(); public Builder(Activity a){} public Builder(Activity a,int theme){}
  public Builder setTitle(CharSequence s){dialog.title=s.toString();return this;}
  public Builder setMessage(CharSequence s){dialog.message=s.toString();return this;}
  public Builder setCancelable(boolean b){dialog.cancelable=b;return this;}
  public Builder setPositiveButton(CharSequence s,DialogInterface.OnClickListener l){dialog.positiveText=s.toString();dialog.positive=l;return this;}
  public Builder setNegativeButton(CharSequence s,DialogInterface.OnClickListener l){dialog.negativeText=s.toString();dialog.negative=l;return this;}
  public Builder setNeutralButton(CharSequence s,DialogInterface.OnClickListener l){dialog.neutralText=s.toString();dialog.neutral=l;return this;}
  public AlertDialog create(){return dialog;}
 }
}''',
    'com/epicgames/ue4/MobileAndroid.java': '''package com.epicgames.ue4;
public final class MobileAndroid {public static java.util.Locale startupLocale(android.content.Context context){return context.locale;}}''',
    'com/epicgames/ue4/MobileLoading.java': '''package com.epicgames.ue4;
public final class MobileLoading {
 public static final java.util.List<Integer> stages=new java.util.ArrayList<Integer>();
 public static void update(android.app.Activity activity,int stage){stages.add(stage);}
 public static void detach(android.app.Activity activity){}
}''',
    'com/epicgames/ue4/MobileFilesActivity.java': '''package com.epicgames.ue4; public class MobileFilesActivity extends android.app.Activity {}''',
    'com/epicgames/ue4/MobileCacheUi.java': '''package com.epicgames.ue4;
public final class MobileCacheUi {
 public static final class Action {final String title;final int kind;final Runnable run; Action(String title,int kind,Runnable run){this.title=title;this.kind=kind;this.run=run;}}
 public static Action primary(String title,Runnable run){return new Action(title,1,run);}
 public static Action secondary(String title,Runnable run){return new Action(title,2,run);}
 public static Action quiet(String title,Runnable run){return new Action(title,3,run);}
 private static void invoke(Action action,android.app.AlertDialog dialog){ACTION_BODY}
 public static android.app.AlertDialog create(android.app.Activity activity,String title,String description,String detailLabel,String detail,boolean cancelable,Action... actions){
  android.app.AlertDialog.Builder builder=new android.app.AlertDialog.Builder(activity).setTitle(title).setMessage(description+"\\n"+detailLabel+"\\n"+detail).setCancelable(cancelable);
  for(final Action action:actions){android.content.DialogInterface.OnClickListener click=new android.content.DialogInterface.OnClickListener(){public void onClick(android.content.DialogInterface dialog,int which){invoke(action,(android.app.AlertDialog)dialog);}};
   if(action.kind==1)builder.setPositiveButton(action.title,click);else if(action.kind==2)builder.setNegativeButton(action.title,click);else builder.setNeutralButton(action.title,click);}
  return builder.create();
 }
}''',
}

ui_source = (ROOT / 'analysis/android/com/epicgames/ue4/MobileCacheUi.java').read_text(encoding='utf-8')
action_body = ui_source.split('public void onClick(View tapped) {', 1)[1].split('}});', 1)[0]
SOURCES['com/epicgames/ue4/MobileCacheUi.java'] = SOURCES['com/epicgames/ue4/MobileCacheUi.java'].replace('ACTION_BODY', action_body)

smali = (ROOT / 'apk_decoded/smali/com/epicgames/ue4/GameActivity.smali').read_text()
def method(signature):
    return smali.split(signature, 1)[1].split('.end method', 1)[0]
assert '.implements Ljava/lang/Runnable;' in smali
gate = method('.method private h()V')
assert gate.count('MobileCache;->resumeMainInit(Landroid/app/Activity;Ljava/lang/Runnable;)V') == 2
assert '->nativeResumeMainInit()V' not in gate
assert 'iget-boolean v0, p0, Lcom/epicgames/ue4/GameActivity;->U:Z' in gate
assert 'if-eqz v0, :mobile_cache_first_init' in gate
assert gate.index('->run()V') < gate.index('\n    :mobile_cache_first_init\n') < gate.index('MobileCache;->resumeMainInit')
callback = method('.method public run()V')
assert callback.count('->nativeResumeMainInit()V') == 1 and '->U:Z' in callback
assert smali.count('->nativeResumeMainInit()V') == 1
destruction = method('.method public onDestroy()V')
assert 'iget-boolean v0, p0, Lcom/epicgames/ue4/GameActivity;->U:Z' in destruction
assert 'MobileCache;->detach(Landroid/app/Activity;Z)V' in destruction
native = (ROOT / 'analysis/re4fix_packaged_obb.h').read_text()
source = PRODUCTION.read_text(encoding='utf-8')
files_source = (ROOT / 'analysis/android/com/epicgames/ue4/MobileFilesActivity.java').read_text(encoding='utf-8')
result_body = files_source.split('protected void onActivityResult(', 1)[1].split('private void chooseCache()', 1)[0]
assert 'takePersistableUriPermission' not in result_body, 'Cancelled/invalid/extracted selections must not retain SAF grants'
assert files_source.count('takePersistableUriPermission') == 1 and 'releaseCacheZip(previous, uri.toString())' in files_source
for name in ('main.201.com.Armature.VR4.obb', 'patch.201.com.Armature.VR4.obb'):
    assert f'"{name}"' in source and f'"{name}"' in native
for size in ('4278831240', '4237413580'):
    assert size + 'L' in source and size + 'LL' in native

with tempfile.TemporaryDirectory(prefix='cache-guard-check-', dir=ROOT / 'analysis/research') as temporary:
    work = Path(temporary).resolve()
    paths = []
    for name, body in SOURCES.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body, encoding='utf-8')
        paths.append(path)
    paths += [PRODUCTION, ROOT / 'analysis/research/MobileCacheTest.java', ROOT / 'analysis/research/MobileCacheZipTest.java']
    subprocess.run([str(JDK / 'javac.exe'), '-encoding', 'UTF-8', '-d', str(work), *map(str, paths)], check=True)
    empty = work / 'obb'; empty.mkdir()
    subprocess.run([str(JDK / 'java.exe'), '-ea', '-cp', str(work), 'com.epicgames.ue4.MobileCacheTest',
                    str(ROOT / 'com.Armature.VR4'), str(empty)], check=True)
    import sys
    subprocess.run([sys.executable, '-B', str(ROOT / 'analysis/research/check_mobile_cache_zip.py'), str(work)], check=True)
    sdk_out = work / 'sdk'; sdk_out.mkdir()
    subprocess.run([str(JDK / 'javac.exe'), '-source', '8', '-target', '8', '-encoding', 'UTF-8',
                    '-classpath', str(SDK / 'platforms/android-36/android.jar'), '-d', str(sdk_out),
                    *map(str, sorted((ROOT / 'analysis/android').rglob('*.java')))], check=True)
    bytecode = subprocess.check_output([str(JDK / 'javap.exe'), '-c', '-p',
                                       str(sdk_out / 'com/epicgames/ue4/MobileCache.class')], text=True)
    assert 'java/nio/Buffer.clear:()Ljava/nio/Buffer;' in bytecode
    assert 'java/nio/ByteBuffer.clear:()Ljava/nio/ByteBuffer;' not in bytecode

print('PASS: sole engine main-init call is guarded, native OBB mapping agrees, Android SDK compilation and actual Android-compatible Buffer.clear descriptor pass; no APK built or device touched')
