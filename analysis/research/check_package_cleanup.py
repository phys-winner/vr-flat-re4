"""Verify the cleaned canonical APK source and optional built APK without device writes."""
import os
from pathlib import Path
import hashlib,io,json,re,subprocess,sys,xml.etree.ElementTree as ET,zipfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
app=ROOT/'apk_decoded'; android='{http://schemas.android.com/apk/res/android}'
report=json.loads((ROOT/'analysis/research/package_cleanup.json').read_text())
m=ET.parse(app/'AndroidManifest.xml').getroot()
assert m.get('package')=='com.Armature.VR4'
assert m.find('application').get(android+'icon')==m.find('application').get(android+'roundIcon')=='@drawable/icon'
assert all(android+'debuggable' not in e.attrib for e in m.iter())
assert not any(e.get(android+'name')=='android.permission.ACCESS_NETWORK_STATE' for e in m)
assert not (app/'unknown').exists()
removed=set(report['removed_classes'])
for p in (app/'smali').rglob('*.smali'):
    t=p.read_text(); assert not removed.intersection(re.findall(r'L([A-Za-z0-9_/$]+);',t)),p
assert not list((app/'smali/com/google/android').rglob('*.smali'))
assert not list((app/'smali/com/oculus').rglob('*.smali'))
for n in report['removed_libraries']:assert not list((app/'lib').rglob(n)),n
adaptive=ET.parse(app/'res/drawable-anydpi-v26/icon.xml').getroot()
assert adaptive.tag=='adaptive-icon'
assert adaptive.find('background').get(android+'drawable')=='@color/launcher_background'
assert adaptive.find('foreground').get(android+'drawable')=='@drawable/launcher_foreground'
assert (app/'res/drawable-nodpi/launcher_foreground.png').is_file()
for n in report['native_jni_roots_retained']:assert (app/'smali'/(n+'.smali')).is_file(),n
for p in (ROOT/'analysis').glob('*'):
    if p.suffix in ('.h','.c'):
        b=p.read_bytes();assert b'/data/data/VRLOC.com.Armature.VR4/' not in b,p
        assert b'/Android/data/VRLOC.com.Armature.VR4/' not in b,p
for n in report['obb_renames']:
    p=ROOT/'com.Armature.VR4'/n['new'];assert p.stat().st_size==n['bytes'] and not p.with_name(n['old']).exists()
if len(sys.argv)>1:
    apk=Path(sys.argv[1]);tools=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'build-tools/36.0.0'))
    manifest=subprocess.check_output([str(tools/'aapt2.exe'),'dump','xmltree',str(apk),'--file','AndroidManifest.xml'],text=True,encoding='utf-8')
    assert '"com.Armature.VR4"' in manifest and 'debuggable' not in manifest and 'ACCESS_NETWORK_STATE' not in manifest
    badging=subprocess.check_output([str(tools/'aapt2.exe'),'dump','badging',str(apk)],text=True,encoding='utf-8')
    icons=re.findall(r"application-icon-\d+:'([^']+)'",badging)
    assert icons and all(p=='res/drawable-anydpi-v26/icon.xml' for p in icons),icons
    with zipfile.ZipFile(apk) as z:
        compiled_icon=subprocess.check_output([str(tools/'aapt2.exe'),'dump','xmltree',str(apk),'--file','res/drawable-anydpi-v26/icon.xml'],text=True,encoding='utf-8')
        assert 'E: adaptive-icon' in compiled_icon and 'E: background' in compiled_icon and 'E: foreground' in compiled_icon
        assert not any(any(n.endswith('/'+lib) for lib in report['removed_libraries']) for n in z.namelist())
        assert not any(n in z.namelist() for n in ['build-data.properties','error_prone/Annotations.gwt.xml'])
        for p in list((app/'res').rglob('icon.png'))+[app/'res/drawable-nodpi/launcher_foreground.png']:
            with Image.open(p) as source, Image.open(io.BytesIO(z.read(p.relative_to(app).as_posix()))) as compiled:
                assert source.size==compiled.size and source.convert('RGBA').tobytes()==compiled.convert('RGBA').tobytes(),p
        assert z.read('lib/arm64-v8a/libre4fix.so')==(app/'lib/arm64-v8a/libre4fix.so').read_bytes()
    print('APK SHA256',hashlib.sha256(apk.read_bytes()).hexdigest())
print('PASS: package, flags, deleted SDK closure, native JNI roots, adaptive icon layers, private paths and OBB names')
