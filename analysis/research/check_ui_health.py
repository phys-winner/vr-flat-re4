"""Verify original health ownership/render-only ABI and execute the actual flat-health helper."""
import os
from pathlib import Path
import hashlib,json,struct,subprocess,tempfile
import inspect_flat_bp as bp
from inspect_ui_package import package
root=Path(__file__).resolve().parents[2]
directory=root/'analysis/research/health_ui_originals'
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert struct.unpack_from('<I',native,0xa0b75c0+0x24)[0]==0x118
digests={
 'VR4Bio4PlayerPawn_BP':('36d2ba51878ceb68aef3f9c7a34585aa7521744a75679ec629f4993852014068','223318ac080fdbbb32b85960e1147b1b7195036e4528d5ade6b16ca29fd61777'),
 'UI_HealthVignette_BP':('eac51a5cacd347e34d227d4ee5ce3d1d5923e867fdd3fa53b72503691488e562','ac114b46d67a05cd9fa064ce2ff4a35580e0da897a6b94f1ba2ff46c27e833db'),
 'UI_HealthVignette_Widget':('27019cbbd476fa5d7a9be22c42b8468c14c561f2189c839c74518aa282ea7f65','7ac9dec52dc6a70ec87c6062d22c5b8c70e0569bf8b3d6f8d1330fb297a7115f'),
 'HUD_HeartbeatBG':('5dc5a5cfb670949ced486e88d8b67e17995ed4daf8b5695b971a4ebc207d0d86','54636760b68a9250d87cd314b97da7ceae4ad47c2714a8e507c133a411a846e6'),
 'HUD_HeartbeatFine':('4013d0a4887362ff7c68227cf290e83bda0b7af14d5b94992375acd34fd2fd74','1218382a981a4f30df6bec42d52d9e18a1fa1d043d7d0514733ed63beb5c3ff9'),
 'HUD_HeartbeatCaution':('8c3adf46479290980064b4bc6b6a6f0675e1dc8909e7428dfa3e9b66b5dacbc8','25a6eff225f6e3ad3362569878f8d314808be4bcb4c5c12d618630305ae1cac0'),
 'HUD_HeartbeatDanger':('b4215e9fe55698db477ad2c9df2a9130da52120a8da220afa9f4441139972169','64165f26baf636cb30110a5e79a72435eac8e6e39bbdb0013ccedb703281e6f2')}
for stem,pair in digests.items():
 raw=[(directory/(stem+ext)).read_bytes() for ext in ('.uasset','.uexp')]
 assert tuple(hashlib.sha256(b).hexdigest() for b in raw)==pair
 metadata=package(*raw)
 assert metadata==json.loads((directory/(stem+'.package.json')).read_text())
 if stem.startswith('HUD_Heartbeat'):
  item=next(e for e in metadata['exports'] if e['class_path']=='/Script/Engine.StaticMesh')
  payload=raw[1][item['uexp_offset']:item['uexp_offset']+item['serial_size']]
  at=payload.index(struct.pack('<II',metadata['names'].index('ExtendedBounds'),0))+49
  values={}
  for _ in range(3):
   name,_,kind,_,size,index=struct.unpack_from('<6I',payload,at)
   header=49 if metadata['names'][kind]=='StructProperty' else 25
   assert not index and metadata['names'][kind] in ('StructProperty','FloatProperty')
   values[metadata['names'][name]]=struct.unpack_from('<'+'f'*(size//4),payload,at+header)
   at+=header+size
  x,y,z=values['BoxExtent'];assert 10<x<12 and 61<y<64 and 5<z<10
bp.DIRECTORY=directory
spawn=bp.read_script('VR4Bio4PlayerPawn_BP','SpawnUi')
encoded=json.dumps(spawn['tree'])
for text in ('HealthUI','HealthHud','BeginDeferredActorSpawnFromClass','SetObjectPropertyByName','Instigator','FinishSpawningActor'):assert text in encoded
visible=bp.read_script('UI_HealthVignette_BP','SetVisible')
invisible=bp.read_script('UI_HealthVignette_BP','SetInvisible')
init=bp.read_script('UI_HealthVignette_BP','Init',1066)
graph=bp.read_script('UI_HealthVignette_BP','ExecuteUbergraph_UI_HealthVignette_BP')
for text in ('MessageTextWidget','MessageWidget','GetUserWidgetObject','Instigator','Camera'):assert text in json.dumps(init['tree'])
for tree in (visible['tree'],invisible['tree']):
 for text in ('HeartbeatBG','HeartbeatWaveform','MessageTextWidget','SceneComponent.SetVisibility'):assert text in json.dumps(tree)
assert 'SetAlpha' in json.dumps(graph['tree']) and 'VignetteQte' in json.dumps(graph['tree'])
props=json.loads((directory/'UI_HealthVignette_BP.component_properties.json').read_text())
message=next(p for p in props if p['export']['name']=='MessageTextWidget_GEN_VARIABLE')
message={p['name']:p for p in message['properties']}
assert message['GeometryMode']['value']=='EWidgetGeometryMode::Cylinder' and message['CylinderArcAngle']['value']==20
print('PASS: original14 health/pawn/widget/mesh hashes and bounded packages; exact HealthHud spawn/Instigator owner, original visibility/text alpha,20degree cylinder and original curved VR meshes; replacement uses existing rendered viewport dial',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=root/'analysis/research/test_ui_health.c'
with tempfile.TemporaryDirectory(prefix='re4-ui-health-') as td:
 folder=Path(td);obj=folder/'test.obj';exe=folder/'test.exe'
 subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
  '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib',
  'libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',
  f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
