"""Verify owned weapon marker suppression and preservation of the original wrist display."""
import os
from pathlib import Path
import json,hashlib,subprocess,tempfile,struct
root=Path(__file__).resolve().parents[2]
evidence=root/'analysis/research/weapon_ui_originals'
receipt=json.loads((evidence/'receipt.json').read_text())
for item in receipt['files']:
 path=evidence/Path(item['path']).name
 assert path.stat().st_size==item['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256'] and item['repeats']==2
package=json.loads((evidence/'UI_AmmoCounter_Pistol.package.json').read_text())
exports={row['name']:row for row in package['exports']}
assert exports['Widget_GEN_VARIABLE']['class_path']=='/Script/UMG.WidgetComponent'
assert exports['Cube_GEN_VARIABLE']['class_path']=='/Script/Engine.StaticMeshComponent'
assert exports['UI_AmmoCounter_Pistol_C']['class_path']=='/Script/Engine.BlueprintGeneratedClass'
shotgun=root/'analysis/research/shotgun_ui_originals'
receipt=json.loads((shotgun/'receipt.json').read_text())
for item in receipt['files']:
 path=shotgun/Path(item['source']).name
 assert path.stat().st_size==item['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256'] and item['repeats']==2
for stem in ('VR4GamePlayer_Wep07_BP','VR4GamePlayer_Wep08_BP','VR4GamePlayer_Wep33_BP'):
 package=json.loads((shotgun/(stem+'.package.json')).read_text())
 for name in ('ForestockGrip_Indicator','PrimaryGrip_Indicator','TossGrip_Indicator'):
  assert any(item['name']==name and item['class_path']=='/Script/VR4.VR4GamePlayerGripIndicator' for item in package['exports'])
source=(root/'analysis/re4fix_weapon_ui.h').read_text()
assert 'VR4GamePlayerGripIndicator' in source and 'touch_wrist_ammo_tick' not in source and 'watchActor' not in source
wrist=root/'analysis/research/wrist_ui_originals'
receipt=json.loads((wrist/'receipt.json').read_text())
assert receipt['native_sha256']=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for item in receipt['files']:
 path=wrist/Path(item['source']).name
 assert path.stat().st_size==item['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256'] and item['repeats']==2
package=json.loads((wrist/'UI_WristWatch_Face_Widget.package.json').read_text())
assert any(row['name']=='AmmoCountParent' and row['class_path']=='/Script/UMG.Overlay' for row in package['exports'])
assert any(row['name']=='Icon_Ammo' and row['class_path']=='/Script/UMG.Image' for row in package['exports'])
graph=json.loads((wrist/'UI_WristWatch_Face_Widget.ExecuteUbergraph_UI_WristWatch_Face_Widget.script.json').read_text())
for name in ('AmmoCountParent','Icon_Ammo'):
 assert any(row['expression'].get('context',{}).get('field')==[name] and
  row['expression'].get('call',{}).get('call')=='/Script/UMG.Widget.SetRenderOpacity' and
  row['expression']['call']['arguments']==[{'float':0.0}] for row in graph['tree'])
attach=json.loads((wrist/'UI_WristWatch_BP.AttachToArm.script.json').read_text())
assert attach['calls'][0]['call']=='/Script/Engine.Actor.K2_AttachToComponent'
assert attach['calls'][0]['arguments'][0]['field']==['Mesh']
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
assert struct.unpack_from('<4I',native,0x7da4280)==(0xd10183ff,0xf90013f7,0xa90357f6,0xa9044ff4)
assert struct.unpack_from('<I',native,0x9f6ff58+0x24)[0]==0xc3
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231');sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
for filename in ('test_weapon_ui.c','test_weapon_ui_wrist.c'):
 test=root/'analysis/research'/filename
 subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
 with tempfile.TemporaryDirectory(prefix='re4-weapon-ui-') as directory:
  obj,exe=Path(directory)/'test.obj',Path(directory)/'test.exe'
  subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(test)],check=True)
  subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
  subprocess.run([str(exe)],check=True)
