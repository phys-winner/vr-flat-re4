"""Execute the production document-launch guard with available/missing/denied Android handlers."""
from pathlib import Path
import subprocess,tempfile,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
source=(ROOT/'analysis/android/com/epicgames/ue4/MobileFilesActivity.java').read_text(encoding='utf-8')
body=source.split('    private void openDocument(',1)[1].split('    private void inspectCache(',1)[0]
assert source.count('startActivityForResult(')==1
assert all('openDocument(picker, '+n+');' in source for n in ('CACHE','IMPORT','EXPORT'))
manifest=ET.parse(ROOT/'apk_decoded/AndroidManifest.xml').getroot();ns='{http://schemas.android.com/apk/res/android}'
queries=manifest.find('queries').findall('intent')
assert {e.find('action').get(ns+'name') for e in queries}=={'android.intent.action.OPEN_DOCUMENT','android.intent.action.CREATE_DOCUMENT'}
assert not any(e.get(ns+'name')=='android.permission.ACCESS_NETWORK_STATE' for e in manifest)
app=(ROOT/'apk_decoded/smali/com/epicgames/ue4/GameApplication.smali').read_text()
assert 'Lcom/epicgames/ue4/g/' not in app
assert not list((ROOT/'apk_decoded/smali/com/epicgames/ue4/g').rglob('*.smali'))
files={
'android/content/ActivityNotFoundException.java':'package android.content; public class ActivityNotFoundException extends RuntimeException {}',
'android/content/Intent.java':'package android.content; public class Intent { public Object resolveActivity(Object pm){return pm;} }',
'android/R.java':'package android; public class R { public static class string {public static final int ok=1;} }',
'android/app/AlertDialog.java':'''package android.app; public class AlertDialog {public static String message;public static int shown;
 public static class Builder {public Builder(Object c){} public Builder setTitle(String s){return this;}public Builder setMessage(String s){message=s;return this;}public Builder setPositiveButton(int id,Object listener){return this;} public void show(){shown++;}}}''',
'PickerTest.java':'''import android.content.*;import android.app.AlertDialog;
public class PickerTest {
 Object handler=new Object(); int attempts,code,failures; boolean russian;String status;Intent selected;
 Object getPackageManager(){return handler;}
 void startActivityForResult(Intent i,int c){attempts++;if(failures==1)throw new ActivityNotFoundException();if(failures==2)throw new SecurityException();selected=i;code=c;}
 String text(String ru,String en){return russian?ru:en;}void showMessage(String message,boolean error){if(!error)throw new AssertionError();status=message;}
 private void openDocument('''+body+'''
 public static void main(String[] args){
 for(boolean ru:new boolean[]{false,true})for(int code:new int[]{451,452,453}){
  PickerTest t=new PickerTest();t.russian=ru;Intent i=new Intent();int shown=AlertDialog.shown;t.openDocument(i,code);
  if(t.selected!=i||t.code!=code||t.attempts!=1||AlertDialog.shown!=shown)throw new AssertionError("Available handler");
  for(int failure:new int[]{0,1,2}){
   t=new PickerTest();t.russian=ru;t.failures=failure;if(failure==0)t.handler=null;shown=AlertDialog.shown;t.openDocument(i,code);
   if(t.selected!=null||AlertDialog.shown!=shown+1||t.status==null||!t.status.equals(AlertDialog.message)||!t.status.contains("DocumentsUI")||t.attempts!=(failure==0?0:1))throw new AssertionError("Missing/denied handler");
   if(ru!=t.status.contains("Системный"))throw new AssertionError("Locale");
  }
 }
 System.out.println("PASS: exact production guard; available, missing/disabled, removed and denied handlers; all 3 operations; Russian/English recovery dialog");
 }
}'''}
with tempfile.TemporaryDirectory(prefix='picker-check-',dir=ROOT/'analysis/build') as tmp:
 work=Path(tmp);paths=[]
 for n,s in files.items():
  p=work/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf-8');paths.append(p)
 subprocess.run(['javac','-encoding','UTF-8','-d',str(work),*map(str,paths)],check=True)
 subprocess.run(['java','-ea','-cp',str(work),'PickerTest'],check=True)
