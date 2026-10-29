"""Guard the original engine's hidden self-query alongside the normal phone launcher."""
from pathlib import Path
import hashlib
import json
import re
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
baseline = ROOT / 'analysis/research/android_baseline/smali/com/epicgames/ue4/GameActivity.smali'
source = ROOT / 'apk_decoded/smali/com/epicgames/ue4/GameActivity.smali'
with zipfile.ZipFile(ROOT / 're4_p171i.apk') as archive:
    assert hashlib.sha256(archive.read('classes.dex')).hexdigest() == 'ec367412d3b8153e8f6d402c8e781e9370e1565aa1aa2463470182b024a744a7'


def method(text, declaration):
    start = text.index(declaration)
    return text[start:text.index('.end method', start) + len('.end method')]


original, current = baseline.read_text(), source.read_text()
delivery_original = baseline.with_name('b.smali').read_text()
delivery_current = source.with_name('b.smali').read_text()
assert delivery_original == delivery_current, 'Accepted mobile delivery helper changed'
delivery = method(delivery_original, '.method public static a(Landroid/app/Activity;I)Z')
assert 'const/4 p0, 0x1' in delivery and 'return p0' in delivery and delivery.count('invoke-') == 0
declaration = '.method static constructor <clinit>()V'
assert method(original, declaration) == method(current, declaration), 'Original library initialization order changed'
creation = method(original, '.method public onCreate(Landroid/os/Bundle;)V')
category = creation.index('const-string v5, "com.oculus.intent.category.VR"')
start = creation.rfind('new-instance v3, Landroid/content/Intent;', 0, category)
end = creation.index(':goto_24', category)
query = creation[start:end]
assert 'queryIntentActivities(Landroid/content/Intent;I)Ljava/util/List;' in query
assert 'PackagedForOculusMobile:Z' in query and 'if-nez v3, :cond_22' in query
assert 'const/4 v11, 0x1' in creation and 'const/4 v14, 0x0' in creation
(ROOT / 'analysis/research/android_baseline/game_activity_oculus_query.smali').write_text(query)

android = '{http://schemas.android.com/apk/res/android}'
manifest = ET.parse(ROOT / 'apk_decoded/AndroidManifest.xml').getroot()
game = next(item for item in manifest.find('application').findall('activity')
            if item.get(android + 'name') == 'com.epicgames.ue4.GameActivity')
intent = game.find('intent-filter')
categories = {item.get(android + 'name') for item in intent.findall('category')}
assert {'android.intent.category.LAUNCHER', 'com.oculus.intent.category.VR'} <= categories
assert intent.find('action').get(android + 'name') == 'android.intent.action.MAIN'
assert manifest.find('uses-sdk').get(android + 'targetSdkVersion') == '37'


def process_lines(path):
    text = path.read_text()
    pid = re.search(r'RE4FIX-N\(\s*(\d+)\): re4fix loaded', text).group(1)
    return pid, [line for line in text.splitlines() if re.search(r'RE4FIX-N\(\s*' + pid + r'\)', line)]


baseline_pid, baseline_lines = process_lines(ROOT / 'analysis/logs/p270_core.log')
failed_pid, failed_lines = process_lines(ROOT / 'analysis/logs/p271_modern.log')
baseline_ovr = [line for line in baseline_lines if 'ovrp_PreInitialize3' in line]
failed_ovr = [line for line in failed_lines if 'ovrp_PreInitialize3' in line]
assert baseline_ovr and not failed_ovr
assert any('MOBILELOCALE actual native current language=' in line for line in failed_lines)
(ROOT / 'analysis/research/android_startup_proof.json').write_text(json.dumps({
    'original_dex_sha256': 'ec367412d3b8153e8f6d402c8e781e9370e1565aa1aa2463470182b024a744a7',
    'original_clinit_unchanged': True, 'original_query_excerpt_sha256': hashlib.sha256(query.encode()).hexdigest(),
    'accepted_delivery_helper_unchanged_constant_true': True,
    'retained_categories': sorted(categories), 'baseline_pid': baseline_pid, 'failed_pid': failed_pid,
    'baseline_ovr_init': baseline_ovr, 'failed_ovr_init': failed_ovr,
    'corrected_startup_hardware': 'pending coordinator candidate'}, indent=2) + '\n')
print('PASS: exact accepted DEX/clinit/delivery helper, original hidden self-query excerpt, normal launcher+engine category, target37; process-specific p270 OVR init vs missing p271 init')
