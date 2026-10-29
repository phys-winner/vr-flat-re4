"""Apply exact canonical Android-source changes; original APK/OBB remain untouched."""
from pathlib import Path
import hashlib
import json
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ANDROID = '{http://schemas.android.com/apk/res/android}'
ET.register_namespace('android', ANDROID[1:-1])
PREIMAGES = ROOT / 'analysis/research/mobile_android_smali_preimages.json'
RECEIPT = ROOT / 'analysis/research/mobile_android_source_patch.json'


def main():
    sources = json.loads(PREIMAGES.read_text())
    if RECEIPT.exists():
        receipt = json.loads(RECEIPT.read_text())
        for name, digest in receipt['outputs'].items():
            assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
        print('PASS: canonical Android source patch already applied, hashes match')
        return
    updates = {}
    calls = 0
    for name, digest in sources.items():
        path = ROOT / name
        data = path.read_bytes()
        assert hashlib.sha256(data).hexdigest() == digest, name
        text = data.decode('utf-8')
        def pending(match):
            nonlocal calls
            calls += 1
            return match.group(1) + 'Lcom/epicgames/ue4/MobileAndroid;->' + match.group(2)
        text = re.sub(r'(invoke-static(?:/range)? \{[^}]+\}, )Landroid/app/PendingIntent;->(get(?:Activity|Broadcast)\(Landroid/content/Context;ILandroid/content/Intent;I\)Landroid/app/PendingIntent;)', pending, text)
        def receiver(match):
            nonlocal calls
            calls += 1
            return 'invoke-static' + match.group(1) + 'Lcom/epicgames/ue4/MobileAndroid;->registerReceiver(Landroid/content/Context;Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;'
        text = re.sub(r'invoke-virtual( \{[^}]+\}, )Landroid/(?:app/(?:NativeActivity|Activity|Service)|content/Context);->registerReceiver\(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;\)Landroid/content/Intent;', receiver, text)
        if name.endswith('/GameActivity.smali'):
            locale = 'invoke-static {}, Ljava/util/Locale;->getDefault()Ljava/util/Locale;'
            assert text.count(locale) == 1
            text = text.replace(locale, 'invoke-static {v9}, Lcom/epicgames/ue4/MobileAndroid;->startupLocale(Landroid/content/Context;)Ljava/util/Locale;')
            native = 'invoke-virtual/range {v1 .. v8}, Lcom/epicgames/ue4/GameActivity;->nativeSetGlobalActivity(ZZLjava/lang/String;Ljava/lang/String;ZLjava/lang/String;Z)V'
            assert text.count(native) == 1
            text = text.replace(native, 'invoke-static {v9}, Lcom/epicgames/ue4/MobileAndroid;->recoverSaves(Landroid/content/Context;)V\n\n    ' + native)
            text += '\n.method public AndroidThunkJava_ShowMobileFiles()V\n    .locals 0\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->showFiles(Landroid/app/Activity;)V\n    return-void\n.end method\n'
            text += '\n.method public AndroidThunkJava_ShowMobileLanguage()V\n    .locals 0\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->showLanguage(Landroid/app/Activity;)V\n    return-void\n.end method\n'
            text += '\n.method public AndroidThunkJava_ShowMobileSettings()V\n    .locals 0\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->showSettings(Landroid/app/Activity;)V\n    return-void\n.end method\n'
            resume = 'invoke-super {p0}, Landroid/app/NativeActivity;->onResume()V'
            assert text.count(resume) == 1
            text = text.replace(resume, resume + '\n\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileSettings;->apply(Landroid/app/Activity;)V')
            text = text.replace(resume, resume + '\n\n    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->attachBack(Landroid/app/Activity;)V')
            pause = 'invoke-super {p0}, Landroid/app/NativeActivity;->onPause()V'
            assert text.count(pause) == 1
            text = text.replace(pause, 'invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->detachBack(Landroid/app/Activity;)V\n\n    ' + pause)
            text += '''
.method public onBackPressed()V
    .locals 1
    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->onBackPressed(Landroid/app/Activity;)Z
    move-result v0
    if-nez v0, :mobile_back_done
    invoke-super {p0}, Landroid/app/NativeActivity;->onBackPressed()V
    :mobile_back_done
    return-void
.end method

.method public onKeyUp(ILandroid/view/KeyEvent;)Z
    .locals 1
    const/4 v0, 0x4
    if-ne p1, v0, :mobile_key_super
    invoke-static {p0}, Lcom/epicgames/ue4/MobileAndroid;->onBackPressed(Landroid/app/Activity;)Z
    move-result v0
    if-eqz v0, :mobile_key_super
    const/4 v0, 0x1
    return v0
    :mobile_key_super
    invoke-super {p0, p1, p2}, Landroid/app/NativeActivity;->onKeyUp(ILandroid/view/KeyEvent;)Z
    move-result v0
    return v0
.end method
'''
        updates[name] = text.encode('utf-8')
    assert calls == 20, calls
    manifest_name = 'apk_decoded/AndroidManifest.xml'
    raw = (ROOT / manifest_name).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == 'bc1ede88eb107db215bfe290c2aefc04800b108aaf9035cc2a5cc8380da6e7e4'
    # apktool preserved a malformed metadata export as text; it was never a binary manifest element.
    text = raw.decode('utf-8')
    begin = text.index('" android:value="@string/app_id"')
    end = text.index('        <meta-data android:name="com.oculus.ossplash.colorspace"', begin)
    manifest = ET.fromstring(text[:begin] + text[end:])
    sdk = ET.Element('uses-sdk', {ANDROID + 'minSdkVersion': '23', ANDROID + 'targetSdkVersion': '37'})
    manifest.insert(0, sdk)
    for feature in list(manifest.findall('uses-feature')):
        name = feature.get(ANDROID + 'name')
        if name == 'android.hardware.vr.headtracking': manifest.remove(feature)
        elif name == 'android.hardware.usb.host': feature.set(ANDROID + 'required', 'false')
    for permission in manifest.findall('uses-permission'):
        if permission.get(ANDROID + 'name') in ('android.permission.READ_EXTERNAL_STORAGE', 'android.permission.WRITE_EXTERNAL_STORAGE'):
            permission.set(ANDROID + 'maxSdkVersion', '28')
    app = manifest.find('application')
    app.set(ANDROID + 'appCategory', 'game')
    app.set(ANDROID + 'localeConfig', '@xml/locales_config')
    for component in app:
        if component.tag in ('activity', 'receiver', 'service') and ANDROID + 'exported' not in component.attrib:
            component.set(ANDROID + 'exported', 'true' if component.find('intent-filter') is not None else 'false')
    game = next(node for node in app.findall('activity') if node.get(ANDROID + 'name') == 'com.epicgames.ue4.GameActivity')
    game.set(ANDROID + 'excludeFromRecents', 'false')
    game.set(ANDROID + 'enableOnBackInvokedCallback', 'true')
    game.set(ANDROID + 'configChanges', game.get(ANDROID + 'configChanges') + '|locale|layoutDirection')
    intent = game.find('intent-filter')
    for category in list(intent.findall('category')): intent.remove(category)
    ET.SubElement(intent, 'category', {ANDROID + 'name': 'android.intent.category.LAUNCHER'})
    # Original GameActivity queries its own VR category to initialize required engine input.
    ET.SubElement(intent, 'category', {ANDROID + 'name': 'com.oculus.intent.category.VR'})
    ET.SubElement(app, 'activity', {ANDROID + 'name': 'com.epicgames.ue4.MobileFilesActivity', ANDROID + 'exported': 'false', ANDROID + 'screenOrientation': 'landscape', ANDROID + 'theme': '@android:style/Theme.Material.NoActionBar'})
    ET.SubElement(app, 'activity', {ANDROID + 'name': 'com.epicgames.ue4.MobileSettings', ANDROID + 'exported': 'false', ANDROID + 'screenOrientation': 'landscape', ANDROID + 'theme': '@android:style/Theme.Material.NoActionBar'})
    ET.indent(manifest)
    updates[manifest_name] = ET.tostring(manifest, encoding='utf-8', xml_declaration=True) + b'\n'
    metadata_name = 'apk_decoded/apktool.yml'
    metadata = (ROOT / metadata_name).read_text()
    assert metadata.count('  targetSdkVersion: 28') == 1 and metadata.count('  minSdkVersion: 23') == 1
    updates[metadata_name] = metadata.replace('  targetSdkVersion: 28', '  targetSdkVersion: 37').encode()
    locales = ('ru', 'en', 'de-DE', 'es-419', 'fr-FR', 'ja-JP')
    config = ET.Element('locale-config')
    for locale in locales: ET.SubElement(config, 'locale', {ANDROID + 'name': locale})
    ET.indent(config)
    updates['apk_decoded/res/xml/locales_config.xml'] = ET.tostring(config, encoding='utf-8', xml_declaration=True) + b'\n'
    for name, data in updates.items():
        path = ROOT / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    receipt = {'legacy_calls_replaced': calls, 'sdk': {'min': 23, 'target': 37}, 'locales': locales, 'inputs': sources,
               'outputs': {name: hashlib.sha256(data).hexdigest() for name, data in updates.items()}}
    RECEIPT.write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS: exact preimages, 20 safe-register/immutable-PendingIntent calls, cold-start locale/recovery, canonical manifest/resources')


if __name__ == '__main__': main()
