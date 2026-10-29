"""Build the canonical Android layer with local tools; never install or change game data."""
import os
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ['ANDROID_SDK_ROOT'])
TOOLS = Path(os.environ['RE4_TOOLS_DIR'])
OUTPUT = ROOT / 'analysis/build/mobile-android-base.apk'
RECEIPT = ROOT / 'analysis/research/mobile_android_build.json'


def run(arguments):
    subprocess.run([str(argument) for argument in arguments], cwd=ROOT, check=True)


def main():
    android = '{http://schemas.android.com/apk/res/android}'
    manifest = ET.parse(ROOT / 'apk_decoded/AndroidManifest.xml').getroot()
    game = next(activity for activity in manifest.find('application').findall('activity')
                if activity.get(android + 'name') == 'com.epicgames.ue4.GameActivity')
    categories = {item.get(android + 'name') for item in game.find('intent-filter').findall('category')}
    assert {'android.intent.category.LAUNCHER', 'com.oculus.intent.category.VR'} <= categories, 'Original GameActivity self-query requires its internal VR category alongside the normal launcher'
    assert game.get(android+'enableOnBackInvokedCallback')=='true', 'GameActivity requires its API33 Back callback opt-in'
    sources = [Path(__file__).resolve(), ROOT / 'apk_decoded/AndroidManifest.xml', ROOT / 'apk_decoded/apktool.yml']
    sources += sorted((ROOT / 'apk_decoded/smali').rglob('*.smali'))
    sources += sorted((ROOT / 'apk_decoded/res').rglob('*'))
    sources += sorted((ROOT / 'analysis/android').rglob('*.java'))
    sources += [ROOT / 'analysis/research/android_cleanup_audit.json']
    inputs = {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources if path.is_file()}
    if RECEIPT.exists() and OUTPUT.exists():
        old = json.loads(RECEIPT.read_text())
        if old['inputs'] == inputs and old['apk_sha256'] == hashlib.sha256(OUTPUT.read_bytes()).hexdigest():
            print('PASS: unchanged Android inputs, verified cached base', OUTPUT)
            return
    output_dir = OUTPUT.parent
    output_dir.mkdir(parents=True, exist_ok=True)
    candidate=output_dir/'mobile-android-base.tmp.apk'
    with tempfile.TemporaryDirectory(prefix='android-source-',dir=output_dir) as temporary:
        work=Path(temporary).resolve()
        assert work.is_relative_to(output_dir.resolve())
        classes,dex_dir,stage=work/'classes',work/'dex',work/'app'
        classes.mkdir();dex_dir.mkdir();stage.mkdir()
        for name in ('AndroidManifest.xml','apktool.yml'):shutil.copyfile(ROOT/'apk_decoded'/name,stage/name)
        for name in ('smali','res'):shutil.copytree(ROOT/'apk_decoded'/name,stage/name)
        java_sources=sorted((ROOT/'analysis/android').rglob('*.java'))
        run(['javac','-source','8','-target','8','-encoding','UTF-8','-classpath',SDK/'platforms/android-36/android.jar','-d',classes,*java_sources])
        compiled_classes=work/'classes.jar'
        with zipfile.ZipFile(compiled_classes,'w') as archive:
            for path in sorted(classes.rglob('*.class')):archive.write(path,path.relative_to(classes).as_posix())
        run([SDK/'build-tools/36.0.0/d8.bat','--min-api','23','--lib',SDK/'platforms/android-36/android.jar','--output',dex_dir,compiled_classes])
        compiled=work/'compiled.apk'
        run(['java','-jar',TOOLS/'apktool.jar','b','-f','-o',compiled,stage])
        with zipfile.ZipFile(compiled,'a') as archive:
            assert 'classes2.dex' not in archive.namelist()
            archive.write(dex_dir/'classes.dex','classes2.dex',compress_type=zipfile.ZIP_DEFLATED)
        audit=json.loads((ROOT/'analysis/research/android_cleanup_audit.json').read_text())
        removed={item['path'].split('apk_decoded/unknown/',1)[1]:item for item in audit['unknown_metadata']}
        with zipfile.ZipFile(ROOT/'re4_p171i.apk') as original,zipfile.ZipFile(compiled) as built,zipfile.ZipFile(candidate,'w') as output:
            replacements={name for name in built.namelist() if not name.startswith('META-INF/')}
            assert {'AndroidManifest.xml','classes.dex','classes2.dex','resources.arsc','res/xml/locales_config.xml'}<=replacements
            assert not any(name.startswith(('lib/','assets/')) for name in replacements)
            for info in original.infolist():
                if info.filename.startswith('META-INF/'):continue
                if info.filename.startswith('res/') and info.filename not in replacements:continue
                if info.filename.startswith('lib/') and not (ROOT/'apk_decoded'/info.filename).is_file():continue
                if info.filename in removed:
                    assert hashlib.sha256(original.read(info.filename)).hexdigest()==removed[info.filename]['sha256']
                    continue
                output.writestr(info,built.read(info.filename) if info.filename in replacements else original.read(info.filename))
            for name in sorted(replacements-set(original.namelist())):
                info=zipfile.ZipInfo(name);info.compress_type=zipfile.ZIP_DEFLATED
                output.writestr(info,built.read(name))
    with zipfile.ZipFile(ROOT/'re4_p171i.apk') as original,zipfile.ZipFile(candidate) as built:
        assert len(set(built.namelist()))==len(built.namelist())
        assert set(built.namelist())-set(original.namelist())=={'classes2.dex','res/xml/locales_config.xml','res/drawable-anydpi-v26/icon.xml','res/drawable-nodpi/launcher_foreground.png'}
        for name in original.namelist():
            if name.startswith(('lib/','assets/')) and name in built.namelist():assert original.read(name)==built.read(name),name
    def resource_ids(apk):
        text = subprocess.check_output([str(SDK / 'build-tools/36.0.0/aapt2.exe'), 'dump', 'resources', str(apk)], text=True, encoding='utf-8')
        return {name: number for number, name in re.findall(r'^\s*resource (0x[0-9a-f]+) (\S+)', text, re.M)}
    original_ids, final_ids = resource_ids(ROOT / 're4_p171i.apk'), resource_ids(candidate)
    assert all(original_ids.get(name) == number for name, number in final_ids.items() if name in original_ids), 'Retained Android resource IDs shifted'
    assert set(final_ids) - set(original_ids) == {'xml/locales_config','color/launcher_background','drawable/launcher_foreground'}
    badging = subprocess.check_output([str(SDK / 'build-tools/36.0.0/aapt2.exe'), 'dump', 'badging', str(candidate)], text=True, encoding='utf-8')
    assert "minSdkVersion:'23'" in badging and "targetSdkVersion:'37'" in badging
    binary_manifest = subprocess.check_output([str(SDK / 'build-tools/36.0.0/aapt2.exe'), 'dump', 'xmltree', str(candidate), '--file', 'AndroidManifest.xml'], text=True, encoding='utf-8')
    start = binary_manifest.index('"com.epicgames.ue4.GameActivity"')
    following_activity = binary_manifest.find('E: activity ', start)
    game_binary = binary_manifest[start:following_activity if following_activity >= 0 else len(binary_manifest)]
    assert '"android.intent.category.LAUNCHER"' in game_binary and '"com.oculus.intent.category.VR"' in game_binary, 'Compiled GameActivity lost launcher/internal engine category'
    assert 'enableOnBackInvokedCallback' in game_binary, 'Compiled GameActivity lost API33 Back callback opt-in'
    assert all(hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest for name,digest in inputs.items()),'Android sources changed during build; candidate not published'
    candidate.replace(OUTPUT)
    RECEIPT.write_text(json.dumps({'inputs': inputs, 'apk_sha256': hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
                                  'base': 're4_p171i.apk', 'resource_ids_retained': len(set(original_ids) & set(final_ids)),
                                  'tools': {'apktool': str(TOOLS / 'apktool.jar'), 'platform': 'android-36', 'build_tools': '36.0.0'}}, indent=2) + '\n')
    print('PASS: min23/target37, normal launcher+original engine category, original native dependencies byte-identical,', len(set(original_ids) & set(final_ids)), 'existing resource IDs retained; output', OUTPUT)


if __name__ == '__main__': main()
