"""Run the actual loading panel against small UI doubles; no device or APK changes."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JDK = Path(os.path.join(os.environ['JAVA_HOME'], 'bin'))
SOURCES = {
    'android/R.java': 'package android; public class R {public static class attr {public static int progressBarStyleHorizontal=1;}}',
    'android/os/Build.java': 'package android.os; public class Build {public static class VERSION {public static int SDK_INT=36;}}',
    'android/content/Context.java': 'package android.content; public class Context {public static final String WINDOW_SERVICE="window";public Object getSystemService(String name){return null;}}',
    'android/content/res/ColorStateList.java': 'package android.content.res; public class ColorStateList {public static ColorStateList valueOf(int color){return new ColorStateList();}}',
    'android/graphics/PixelFormat.java': 'package android.graphics; public class PixelFormat {public static final int OPAQUE=-1;}',
    'android/graphics/Typeface.java': 'package android.graphics; public class Typeface {public static final Typeface SERIF=new Typeface();}',
    'android/content/res/Resources.java': 'package android.content.res; public class Resources {public Metrics getDisplayMetrics(){return new Metrics();} public static class Metrics {public float density=1;}}',
    'android/graphics/Color.java': 'package android.graphics; public class Color {public static final int WHITE=-1; public static int rgb(int r,int g,int b){return 0;}}',
    'android/os/SystemClock.java': 'package android.os; public class SystemClock {public static long now=10000; public static long elapsedRealtime(){return now;}}',
    'android/view/Gravity.java': 'package android.view; public class Gravity {public static final int CENTER=17;}',
    'android/view/View.java': '''package android.view; public class View {
 public static final int IMPORTANT_FOR_ACCESSIBILITY_YES=1,VISIBLE=0,INVISIBLE=4; final android.content.Context context;
 public Object token=new Object();public int visibility,background,systemUi;
 public ViewGroup parent; public Runnable pending; public View(android.content.Context c){context=c;}
 public android.content.Context getContext(){return context;} public Object getParent(){return parent;}
 public android.content.res.Resources getResources(){return new android.content.res.Resources();}
 public void removeCallbacks(Runnable r){if(pending==r)pending=null;}
 public boolean postDelayed(Runnable r,long ms){if(ms!=1000)throw new AssertionError();pending=r;return true;}
 public void setPadding(int a,int b,int c,int d){} public void setBackgroundColor(int c){background=c;}
 public void setVisibility(int v){visibility=v;} public Object getWindowToken(){return token;}
 public int getSystemUiVisibility(){return systemUi;}public void setSystemUiVisibility(int v){systemUi=v;}
 public void setClickable(boolean b){} public void setImportantForAccessibility(int a){}
 protected void onDetachedFromWindow(){}
}''',
    'android/view/Window.java': 'package android.view; public class Window {public final View decor=new View(new android.content.Context());public View getDecorView(){return decor;}}',
    'android/view/WindowManager.java': '''package android.view; public class WindowManager {
 public final ViewGroup root=new ViewGroup(new android.content.Context());public boolean fail;public LayoutParams latest;
 public void addView(View v,LayoutParams p){latest=p;if(fail)throw new IllegalStateException();root.addView(v);}
 public void removeViewImmediate(View v){if(!root.children.contains(v))throw new IllegalArgumentException();root.removeView(v);}
 public static class LayoutParams {public static final int TYPE_APPLICATION_PANEL=1000,FLAG_NOT_FOCUSABLE=8,FLAG_LAYOUT_IN_SCREEN=256,FLAG_FULLSCREEN=1024;
 public Object token;public int type,flags,layoutInDisplayCutoutMode;public LayoutParams(int w,int h,int t,int f,int format){type=t;flags=f;}}
}''',
    'android/view/ViewGroup.java': '''package android.view; public class ViewGroup extends View {
 public final java.util.List<View> children=new java.util.ArrayList<View>(); public ViewGroup(android.content.Context c){super(c);}
 public void addView(View v){children.add(v);v.parent=this;} public void addView(View v,LayoutParams p){addView(v);}
 public void removeView(View v){children.remove(v);v.parent=null;v.onDetachedFromWindow();}
 public static class LayoutParams {public LayoutParams(int a,int b){}}
}''',
    'android/widget/FrameLayout.java': '''package android.widget; public class FrameLayout extends android.view.ViewGroup {
 public FrameLayout(android.content.Context c){super(c);} public static class LayoutParams extends android.view.ViewGroup.LayoutParams {public LayoutParams(int a,int b,int g){super(a,b);}}
}''',
    'android/widget/LinearLayout.java': '''package android.widget; public class LinearLayout extends android.view.ViewGroup {
 public static final int VERTICAL=1; public LinearLayout(android.content.Context c){super(c);} public void setOrientation(int v){} public void setGravity(int g){} public static class LayoutParams extends android.view.ViewGroup.LayoutParams {public int topMargin;public LayoutParams(int a,int b){super(a,b);}}
}''',
    'android/widget/ProgressBar.java': 'package android.widget; public class ProgressBar extends android.view.View {public boolean indeterminate;public int max,progress;public ProgressBar(android.content.Context c){super(c);} public ProgressBar(android.content.Context c,Object a,int s){super(c);} public void setIndeterminate(boolean b){indeterminate=b;} public void setMax(int n){max=n;} public void setProgress(int n){progress=n;} public void setProgressTintList(android.content.res.ColorStateList c){}public void setIndeterminateTintList(android.content.res.ColorStateList c){}public void setProgressBackgroundTintList(android.content.res.ColorStateList c){}}',
    'android/widget/TextView.java': '''package android.widget; public class TextView extends android.view.View {
 public String text; public TextView(android.content.Context c){super(c);} public void setTextColor(int c){} public void setTextSize(int s){} public void setGravity(int g){} public void setText(String s){text=s;}public void setTypeface(android.graphics.Typeface t){}
}''',
    'android/app/Activity.java': '''package android.app; public class Activity extends android.content.Context {
 public boolean finishing,destroyed; public Runnable queued; public final android.view.ViewGroup content=new android.view.ViewGroup(this);
 public final android.view.WindowManager windows=new android.view.WindowManager();public final android.view.Window window=new android.view.Window();
 public android.view.Window getWindow(){return window;} public Object getSystemService(String n){return windows;}
 public boolean isFinishing(){return finishing;} public boolean isDestroyed(){return destroyed;}
 public void runOnUiThread(Runnable r){queued=r;} public void addContentView(android.view.View v,android.view.ViewGroup.LayoutParams p){content.addView(v,p);}
}''',
    'com/epicgames/ue4/MobileAndroid.java': 'package com.epicgames.ue4; public class MobileAndroid {public static java.util.Locale locale=java.util.Locale.US;public static java.util.Locale startupLocale(android.content.Context c){return locale;}}',
    'com/epicgames/ue4/MobileLoadingTest.java': '''package com.epicgames.ue4;
import android.app.Activity; import android.view.View; import android.view.ViewGroup; import android.widget.TextView;
public class MobileLoadingTest {
 static void require(boolean b){if(!b)throw new AssertionError();}
 static TextView label(View v){return (TextView)((ViewGroup)((ViewGroup)v).children.get(0)).children.get(1);}
 static TextView percent(View v){return (TextView)((ViewGroup)((ViewGroup)v).children.get(0)).children.get(3);}
 static void update(Activity a,int s){MobileLoading.update(a,s);a.queued.run();}
 public static void main(String[] args) throws Exception {
  Activity a=new Activity();MobileLoading.update(a,2);require(a.windows.root.children.isEmpty());a.queued.run();
  View panel=a.windows.root.children.get(0);require(a.content.children.isEmpty()&&label(panel).text.equals("Starting game")&&panel.pending!=null);
  require(a.windows.latest.type==1000&&a.windows.latest.flags==(8|256|1024)&&a.windows.latest.token==a.window.decor.token);
  require(a.windows.latest.layoutInDisplayCutoutMode==1);
  require(panel.background==0xFF100F0E&&((TextView)((ViewGroup)((ViewGroup)panel).children.get(0)).children.get(0)).text.equals("RESIDENT EVIL 4"));
  panel.pending.run();require(label(panel).text.equals("Starting game"));
  update(a,3);require(a.windows.root.children.size()==1&&a.windows.root.children.get(0)==panel&&label(panel).text.equals("Loading level"));
  a.windows.root.removeView(panel);require(panel.pending==null);update(a,3);require(a.windows.root.children.size()==1);panel=a.windows.root.children.get(0);
  java.lang.reflect.Field stage=panel.getClass().getDeclaredField("stage");stage.setAccessible(true);stage.setInt(panel,4);
  java.lang.reflect.Method render=panel.getClass().getDeclaredMethod("render",int.class);render.setAccessible(true);
  android.widget.ProgressBar progress=(android.widget.ProgressBar)((ViewGroup)((ViewGroup)panel).children.get(0)).children.get(2);
  render.invoke(panel,100);require(!progress.indeterminate&&progress.max==100&&progress.progress==0&&percent(panel).text.equals("0%")&&label(panel).text.equals("Preparing graphics"));
  render.invoke(panel,40);require(progress.progress==60&&percent(panel).text.equals("60%")&&percent(panel).visibility==View.VISIBLE);
  render.invoke(panel,150);require(progress.max==150&&progress.progress==0);
  render.invoke(panel,0);require(progress.progress==150&&percent(panel).text.equals("100%"));
  render.invoke(panel,-1);require(progress.indeterminate&&percent(panel).visibility==View.INVISIBLE&&percent(panel).text.equals(""));
  Activity other=new Activity();MobileLoading.detach(other);require(a.windows.root.children.size()==1);
  update(a,0);require(a.windows.root.children.isEmpty()&&panel.pending==null);
  MobileAndroid.locale=new java.util.Locale("ru");update(other,1);panel=other.windows.root.children.get(0);require(label(panel).text.equals("Проверка кеша"));
  update(a,2);require(other.windows.root.children.isEmpty()&&panel.pending==null);panel=a.windows.root.children.get(0);
  a.destroyed=true;update(a,3);require(a.windows.root.children.isEmpty()&&panel.pending==null);
  update(a,2);require(a.windows.root.children.isEmpty());MobileLoading.detach(a);
  Activity failed=new Activity();failed.windows.fail=true;update(failed,2);require(failed.windows.root.children.isEmpty());
  failed.windows.fail=false;update(failed,2);require(failed.windows.root.children.size()==1);MobileLoading.detach(failed);
  failed.window.decor.token=null;update(failed,2);require(failed.windows.root.children.isEmpty());
  String[] langs={"en","fr","de","es","it","ru","ja","ko","zh-CN","zh-TW"};
  for(String lang:langs){MobileAndroid.locale=java.util.Locale.forLanguageTag(lang);Activity localized=new Activity();update(localized,2);require(!label(localized.windows.root.children.get(0)).text.isEmpty());MobileLoading.detach(localized);}
  System.out.println("PASS: separate nonfocusable application window/token, Room-style title/colors, real shader percentages, ten locales, missing token/add failure recovery, reuse, hide and Activity cleanup");
 }
}''',
}

with tempfile.TemporaryDirectory(prefix='loading-check-', dir=ROOT / 'analysis/research') as temporary:
    work = Path(temporary)
    paths = []
    for name, source in SOURCES.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding='utf-8')
        paths.append(path)
    paths.append(ROOT / 'analysis/android/com/epicgames/ue4/MobileLoading.java')
    subprocess.run([str(JDK / 'javac.exe'), '--release', '8', '-encoding', 'UTF-8', '-d', str(work), *map(str, paths)], check=True)
    subprocess.run([str(JDK / 'java.exe'), '-ea', '-cp', str(work), 'com.epicgames.ue4.MobileLoadingTest'], check=True)
