"""Execute production MobileAndroid against tiny host SDK doubles; no APK inputs change."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
activity = (ROOT/'apk_decoded/smali/com/epicgames/ue4/GameActivity.smali').read_text(encoding='utf-8')
create = re.search(r'\.method public onCreate\(Landroid/os/Bundle;\)V\n(.*?)\.end method',activity,re.S).group(1)
window = '''    invoke-virtual {v1, v2}, Landroid/view/Window;->takeSurface(Landroid/view/SurfaceHolder$Callback2;)V

    # NativeActivity leaves RGB_565; this Window now belongs to the hardware Canvas decor.
    const/4 v2, 0x1

    invoke-virtual {v1, v2}, Landroid/view/Window;->setFormat(I)V

    new-instance v1, Landroid/view/SurfaceView;'''
assert window in create and activity.count('Landroid/view/Window;->setFormat(I)V')==1
assert create.index('Landroid/app/NativeActivity;->onCreate')<create.index(window)<create.index('Landroid/view/SurfaceView;->setZOrderOnTop(Z)V')
assert 'Landroid/view/SurfaceHolder;->setFormat(I)V' not in activity
assert 'Landroid/view/SurfaceHolder;->addCallback(Landroid/view/SurfaceHolder$Callback;)V' in create
manifest=ET.parse(ROOT/'apk_decoded/AndroidManifest.xml').getroot()
android='{http://schemas.android.com/apk/res/android}'
application=manifest.find('application')
game=next(a for a in application.findall('activity') if a.get(android+'name')=='com.epicgames.ue4.GameActivity')
assert application.get(android+'hardwareAccelerated')=='true' and game.get(android+'hardwareAccelerated')!='false'
assert any(m.get(android+'name')=='com.epicgames.ue4.GameActivity.bSupportsVulkan' and m.get(android+'value')=='true' for m in application.findall('meta-data'))
for thunk, target in (('ShowMobileFiles', 'showFiles'), ('ShowMobileLanguage', 'showLanguage')):
    expected = '.method public AndroidThunkJava_'+thunk+'()V\n    .locals 0\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->'+target+'(Landroid/app/Activity;)V\n    return-void\n.end method'
    assert activity.count(expected)==1, thunk
print('PASS: main Window RGBA8888 exactly once after takeSurface(NULL), before SurfaceView creation; hardware Canvas and separate native SurfaceView callbacks/format/Vulkan remain enabled',flush=True)
SOURCES = {
    'android/R.java': 'package android; public final class R {public static final class string {public static final int ok=1;}}',
    'android/app/LocaleManager.java': '''package android.app;
public final class LocaleManager {
 public android.os.LocaleList locales=android.os.LocaleList.forLanguageTags(""); public int changes;
 public android.os.LocaleList getApplicationLocales(){return locales;}
 public void setApplicationLocales(android.os.LocaleList value){locales=value;changes++;}
}''',
    'android/os/Build.java': 'package android.os; public final class Build { public static final class VERSION { public static int SDK_INT; } }',
    'android/media/AudioAttributes.java': 'package android.media; public final class AudioAttributes { public static final int USAGE_GAME=14; public static final class Builder { public Builder setUsage(int usage){return this;} public AudioAttributes build(){return new AudioAttributes();} } }',
    'android/os/Vibrator.java': 'package android.os; public class Vibrator { public boolean hasVibrator(){return true;} public boolean hasAmplitudeControl(){return true;} public void cancel(){} public void vibrate(long ms){} public void vibrate(VibrationEffect effect,android.media.AudioAttributes usage){} }',
    'android/os/VibratorManager.java': 'package android.os; public final class VibratorManager { public Vibrator getDefaultVibrator(){return new Vibrator();} }',
    'android/os/VibrationEffect.java': 'package android.os; public final class VibrationEffect { public static final int DEFAULT_AMPLITUDE=-1; public static VibrationEffect createOneShot(long ms,int strength){return new VibrationEffect();} }',
    'android/view/View.java': 'package android.view; public final class View { public boolean focused=true; public int visibility=5894,focusRequests; public int getSystemUiVisibility(){return visibility;} public void setSystemUiVisibility(int flags){visibility=flags;} public boolean hasWindowFocus(){return focused;} public boolean requestFocus(){focusRequests++;return true;} }',
    'android/view/Window.java': 'package android.view; public final class Window { public final View decor=new View(); public View getDecorView(){return decor;} }',
    'android/content/DialogInterface.java': 'package android.content; public interface DialogInterface { int BUTTON_POSITIVE=-1,BUTTON_NEGATIVE=-2; void dismiss(); interface OnClickListener { void onClick(DialogInterface dialog,int which); } interface OnDismissListener { void onDismiss(DialogInterface dialog); } }',
    'android/app/AlertDialog.java': 'package android.app; import android.content.DialogInterface; public final class AlertDialog implements DialogInterface { public static AlertDialog last; public static int showingCount,createdCount; public static boolean failShow; public CharSequence title,message,positive,negative; public boolean showing; private Activity owner; private DialogInterface.OnClickListener yes,no; private DialogInterface.OnDismissListener dismissed; private final android.view.Window window=new android.view.Window(); public void setOwnerActivity(Activity a){owner=a;} public Activity getOwnerActivity(){return owner;} public android.view.Window getWindow(){return window;} public void setOnDismissListener(DialogInterface.OnDismissListener listener){dismissed=listener;} public void show(){if(failShow)throw new IllegalStateException();if(showing||showingCount!=0)throw new AssertionError();showing=true;showingCount++;last=this;} public void dismiss(){if(!showing)return;showing=false;showingCount--;if(dismissed!=null)dismissed.onDismiss(this);} public void click(int which){(which==BUTTON_POSITIVE?yes:no).onClick(this,which);dismiss();} public static final class Builder { private final AlertDialog dialog=new AlertDialog(); public Builder(Activity activity){} public Builder setTitle(CharSequence text){dialog.title=text;return this;} public Builder setMessage(CharSequence text){dialog.message=text;return this;} public Builder setPositiveButton(CharSequence text,DialogInterface.OnClickListener listener){dialog.positive=text;dialog.yes=listener;return this;} public Builder setNegativeButton(CharSequence text,DialogInterface.OnClickListener listener){dialog.negative=text;dialog.no=listener;return this;} public AlertDialog create(){createdCount++;return dialog;} } }',
    'android/os/LocaleList.java': 'package android.os; public final class LocaleList { private final java.util.Locale selected; public LocaleList(java.util.Locale x){selected=x;} public java.util.Locale get(int index){if(index!=0)throw new AssertionError();return selected;} }',
    'android/content/res/Configuration.java': 'package android.content.res; public final class Configuration { public java.util.Locale locale, selected; public android.os.LocaleList getLocales(){return new android.os.LocaleList(selected);} }',
    'android/content/res/Resources.java': 'package android.content.res; public final class Resources { public final Configuration configuration=new Configuration(); public Configuration getConfiguration(){return configuration;} }',
    'android/content/Intent.java': 'package android.content; public final class Intent { public Class<?> target; public Intent(){} public Intent(Context c,Class<?> t){target=t;} }',
    'android/content/BroadcastReceiver.java': 'package android.content; public class BroadcastReceiver {}',
    'android/content/IntentFilter.java': 'package android.content; public final class IntentFilter { private final java.util.Set<String> actions=new java.util.HashSet<String>(); public IntentFilter(String x){addAction(x);} public void addAction(String x){actions.add(x);} public int countActions(){return actions.size();} public boolean hasAction(String x){return actions.contains(x);} }',
    'android/content/Context.java': 'package android.content; public class Context { public static final int RECEIVER_NOT_EXPORTED=4,RECEIVER_EXPORTED=2; public static final String VIBRATOR_SERVICE="vibrator",VIBRATOR_MANAGER_SERVICE="vibrator_manager"; public int receiverFlags; public BroadcastReceiver receiver; public IntentFilter filter; public final Intent returnIntent=new Intent(); public final android.content.res.Resources resources=new android.content.res.Resources(); public Intent registerReceiver(BroadcastReceiver r,IntentFilter f){return registerReceiver(r,f,-1);} public Intent registerReceiver(BroadcastReceiver r,IntentFilter f,int flags){receiver=r;filter=f;receiverFlags=flags;return returnIntent;} public android.content.res.Resources getResources(){return resources;} public java.io.File getExternalFilesDir(String type){return null;} public Context getApplicationContext(){return this;} public Object getSystemService(String name){return null;} }',
    'android/app/PendingIntent.java': 'package android.app; public final class PendingIntent { public static final int FLAG_IMMUTABLE=0x04000000; public static android.content.Context context; public static int code,flags; public static android.content.Intent intent; public static boolean broadcast; public static PendingIntent getActivity(android.content.Context c,int x,android.content.Intent i,int f){context=c;code=x;intent=i;flags=f;broadcast=false;return new PendingIntent();} public static PendingIntent getBroadcast(android.content.Context c,int x,android.content.Intent i,int f){PendingIntent p=getActivity(c,x,i,f);broadcast=true;return p;} }',
    'android/app/Activity.java': 'package android.app; public class Activity extends android.content.Context { public boolean finishing,destroyed,focus=true; public int defaultBacks; public Runnable queued; public android.content.Intent started; public final android.window.OnBackInvokedDispatcher dispatcher=new android.window.OnBackInvokedDispatcher(); private final android.view.Window window=new android.view.Window(); public android.view.Window getWindow(){return window;} public void runOnUiThread(Runnable r){queued=r;} public boolean isFinishing(){return finishing;} public boolean isDestroyed(){return destroyed;} public boolean hasWindowFocus(){return focus;} public void startActivity(android.content.Intent i){started=i;} public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher(){return dispatcher;} public void onBackPressed(){defaultBacks++;} }',
    'android/window/OnBackInvokedCallback.java': 'package android.window; public interface OnBackInvokedCallback { void onBackInvoked(); }',
    'android/window/OnBackInvokedDispatcher.java': 'package android.window; public final class OnBackInvokedDispatcher { public static final int PRIORITY_DEFAULT=0; public int registers,unregisters; public OnBackInvokedCallback callback; public void registerOnBackInvokedCallback(int priority,OnBackInvokedCallback c){if(priority!=0||callback!=null)throw new AssertionError();callback=c;registers++;} public void unregisterOnBackInvokedCallback(OnBackInvokedCallback c){if(callback!=c)throw new AssertionError();callback=null;unregisters++;} }',
    'com/epicgames/ue4/GameActivity.java': 'package com.epicgames.ue4; public final class GameActivity extends android.app.Activity {}',
    'com/epicgames/ue4/MobileSettings.java': 'package com.epicgames.ue4; public final class MobileSettings { public static void pause(android.app.Activity activity) {} }',
    'com/epicgames/ue4/MobileFilesActivity.java': 'package com.epicgames.ue4; public final class MobileFilesActivity {}',
}

SOURCES['android/os/LocaleList.java'] = '''package android.os;
public final class LocaleList {
 private final java.util.Locale[] selected;
 public LocaleList(java.util.Locale... values){selected=values;}
 public java.util.Locale get(int index){return selected[index];}
 public boolean isEmpty(){return selected.length==0;}
 public static LocaleList forLanguageTags(String tags){return tags.isEmpty()?new LocaleList():new LocaleList(java.util.Locale.forLanguageTag(tags));}
}'''
SOURCES['android/content/Context.java'] = SOURCES['android/content/Context.java'].replace(
    'public Object getSystemService(String name){return null;}',
    'public final android.app.LocaleManager localeManager=new android.app.LocaleManager(); public boolean localeAvailable=true; public Object getSystemService(String name){return null;} public <T>T getSystemService(Class<T> type){return localeAvailable?type.cast(localeManager):null;}')
SOURCES['android/app/AlertDialog.java'] = SOURCES['android/app/AlertDialog.java'].replace(
    'public CharSequence title,message,positive,negative;',
    'public CharSequence title,message,positive,negative; public String[] choices; public int selected; private DialogInterface.OnClickListener choice; public void choose(int index){choice.onClick(this,index);}')
SOURCES['android/app/AlertDialog.java'] = SOURCES['android/app/AlertDialog.java'].replace(
    'public AlertDialog create(){createdCount++;return dialog;}',
    'public Builder setSingleChoiceItems(String[] names,int index,DialogInterface.OnClickListener listener){dialog.choices=names;dialog.selected=index;dialog.choice=listener;return this;} public Builder setPositiveButton(int text,DialogInterface.OnClickListener listener){dialog.positive=Integer.toString(text);dialog.yes=listener;return this;} public AlertDialog show(){AlertDialog value=create();value.show();return value;} public AlertDialog create(){createdCount++;return dialog;}')

with tempfile.TemporaryDirectory(prefix='android-java-check-', dir=ROOT / 'analysis/research') as directory:
    work = Path(directory).resolve()
    assert work.is_relative_to((ROOT / 'analysis/research').resolve())
    paths = []
    for name, body in SOURCES.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body, encoding='utf-8')
        paths.append(str(path))
    production = ROOT / 'analysis/android/com/epicgames/ue4'
    paths += [str(production / name) for name in ('MobileAndroid.java', 'MobileSaves.java')]
    paths.append(str(ROOT / 'analysis/research/MobileAndroidTest.java'))
    jdk=Path(os.environ['JAVA_HOME'])
    android=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'platforms/android-36/android.jar'))
    subprocess.run([str(jdk/'bin/javac.exe'), '--release', '8', '-encoding', 'UTF-8', '-cp', str(android),
        '-d', str(work/'sdk'), *map(str,production.glob('*.java'))], check=True)
    subprocess.run([str(jdk/'bin/javac.exe'), '--release', '8', '-encoding', 'UTF-8', '-d', str(work), *paths], check=True)
    # Execute the real production JNI export too, with a fake root input queue handler.
    clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
    vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
    sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
    source=work/'back.c';obj=work/'back.obj';dll=work/'back.dll'
    source.write_text('#include <jni.h>\n#include <string.h>\n#define PROT_READ 1\n#define PROT_WRITE 2\n#define PROT_EXEC 4\nstatic int protect_mem(void* p,size_t n,int flags){return 0;}\nstatic int g_touch_gt_tid,g_touch_context,back_result,back_calls;\nstatic int gettid(void){return 1;}\nstatic long ptr_now_ms(void){return 1000;}\nstatic int touch_mobile_back(void){++back_calls;return back_result;}\n#define LOGF(...) ((void)0)\n#define LOGI(...) ((void)0)\n#include "'+(ROOT/'analysis/re4fix_mobile_android.h').as_posix()+'"\nJNIEXPORT void JNICALL Java_MobileAndroidTest_backConfigure(JNIEnv* env,jclass type,jboolean result){(void)env;(void)type;back_result=result;back_calls=0;}\nJNIEXPORT jint JNICALL Java_MobileAndroidTest_backCalls(JNIEnv* env,jclass type){(void)env;(void)type;return back_calls;}\nJNIEXPORT jint JNICALL Java_MobileAndroidTest_confirmPrepare(JNIEnv* env,jclass type,jint kind){(void)env;(void)type;mobile_android_confirm_cancel();if(!mobile_android_confirm_request(kind,0,1000))return 0;g_mobile_confirm=(g_mobile_confirm&~(uint64_t)7)|MOBILE_CONFIRM_SHOWN;return (unsigned)g_mobile_confirm>>8;}\nJNIEXPORT jint JNICALL Java_MobileAndroidTest_confirmTake(JNIEnv* env,jclass type,jint kind){(void)env;(void)type;return mobile_android_confirm_take(kind,1001);}\n')
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast','-I',str(jdk/'include'),'-I',str(jdk/'include/win32'),'-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c',str(source),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO','/DLL',f'/OUT:{dll}','/EXPORT:Java_com_epicgames_ue4_MobileAndroid_nativeBack','/EXPORT:Java_com_epicgames_ue4_MobileAndroid_nativeConfirm',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(jdk/'bin/java.exe'), '--enable-native-access=ALL-UNNAMED', '-ea', '-cp', str(work), 'MobileAndroidTest',str(dll)], check=True)
