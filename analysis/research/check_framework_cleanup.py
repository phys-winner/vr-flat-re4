"""Check framework-free DEX definitions, platform bridges and stable retained resources."""
import os
from pathlib import Path
import json,re,struct,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[2]
app=ROOT/'apk_decoded'
removed=set(json.loads((ROOT/'analysis/research/framework_cleanup.json').read_text())['removed_classes'])
for p in (app/'smali').rglob('*.smali'):
    t=p.read_text()
    assert not removed.intersection(re.findall(r'L([\w/$]+);',t)),p
    owner=next(l.split()[-1] for l in t.splitlines() if l.startswith('.class '))
    assert not owner.startswith(('La/','Landroid/')),p
    assert not re.search(r'L(?:a/|android/(?:arch|support)/)',t),p
helper=(app/'smali/com/google/vr/sdk/samples/permission/PermissionHelper.smali').read_text()
fragment=(app/'smali/com/google/vr/sdk/samples/permission/PermissionFragment.smali').read_text()
assert 'Landroid/content/Context;->checkSelfPermission(Ljava/lang/String;)I' in helper
assert 'Landroid/app/Fragment;->requestPermissions([Ljava/lang/String;I)V' in fragment
assert '->onAcquirePermissions([Ljava/lang/String;[I)V' in fragment
notification=(app/'smali/com/epicgames/ue4/LocalNotificationReceiver.smali').read_text()
assert 'const/16 v9, 0x1a\n    if-lt v8, v9, :platform_notification_legacy' in notification
assert 'Notification$Builder;-><init>(Landroid/content/Context;Ljava/lang/String;)V' in notification
assert 'Notification$Builder;-><init>(Landroid/content/Context;)V' in notification
sdk=Path(os.environ['ANDROID_SDK_ROOT'])
for cls,signatures in {
 'android.app.Notification$Builder':['Notification$Builder(android.content.Context);','Notification$Builder(android.content.Context, java.lang.String);','setSmallIcon(int);','setContentTitle(java.lang.CharSequence);','setContentText(java.lang.CharSequence);','setTicker(java.lang.CharSequence);','setStyle(android.app.Notification$Style);','build();'],
 'android.app.Notification$BigTextStyle':['bigText(java.lang.CharSequence);'],
 'android.content.Context':['checkSelfPermission(java.lang.String);'],
 'android.app.Fragment':['requestPermissions(java.lang.String[], int);']}.items():
    api=subprocess.check_output(['javap','-classpath',str(sdk/'platforms/android-36/android.jar'),cls],text=True)
    assert all(s in api for s in signatures),(cls,signatures)
application=(app/'smali/com/epicgames/ue4/GameApplication.smali').read_text()
assert 'Landroid/app/Application;->onCreate()V' in application and 'Lcom/epicgames/ue4/g/' not in application
if len(sys.argv)>1:
    apk=Path(sys.argv[1]);count=0
    with zipfile.ZipFile(apk) as z:
        for n in z.namelist():
            if not re.fullmatch(r'classes\d*\.dex',n):continue
            b=z.read(n);so=struct.unpack_from('<I',b,60)[0];to=struct.unpack_from('<I',b,68)[0]
            nc,co=struct.unpack_from('<II',b,96)
            for i in range(nc):
                typ=struct.unpack_from('<I',b,co+32*i)[0];sid=struct.unpack_from('<I',b,to+4*typ)[0]
                off=struct.unpack_from('<I',b,so+4*sid)[0]
                while b[off]&128:off+=1
                off+=1;descriptor=b[off:b.index(0,off)].decode()
                assert not descriptor.startswith(('La/','Landroid/')),descriptor
                count+=1
            assert not re.search(rb'L(?:a/|android/(?:arch|support)/)',b),n
    def ids(path):
        text=subprocess.check_output([str(sdk/'build-tools/36.0.0/aapt2.exe'),'dump','resources',str(path)],encoding='utf-8')
        return {n:i for i,n in re.findall(r'^\s*resource (0x[0-9a-f]+) (\S+)',text,re.M)}
    old,new=ids(ROOT/'re4_p398a.apk'),ids(apk)
    expected=set(json.loads((ROOT/'analysis/research/resource_cleanup.json').read_text())['retained'])
    assert set(new)==expected and all(old[n]==v for n,v in new.items()),(new,expected)
    print('PASS:',count,'DEX class definitions; no bundled a/android classes or support refs; all 7 resource IDs unchanged')
print('PASS: deleted-class closure, native permission APIs/callback, both notification constructor branches and Android SDK signatures')
