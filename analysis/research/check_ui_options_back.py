"""Original Options Back cleanup and actual projected header/GT Back C regression."""
import os
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import tempfile
from inspect_flat_bp import Script
from inspect_ui_package import package

ROOT = Path(__file__).resolve().parents[2]
directory = ROOT / 'analysis/research/extras_ui_originals'
asset, body = [(directory / ('OptionsScreen_Menu' + ext)).read_bytes() for ext in ('.uasset', '.uexp')]
assert hashlib.sha256(asset).hexdigest() == 'f815c0ef879316e03effc474ea0e4e65499f1e3ae7ef6c245c34247a7081de76'
assert hashlib.sha256(body).hexdigest() == '065af738d202ea409b388d3352bdc523444042d7e36a5a297b88dd74ae726138'
metadata = package(asset, body)
assert metadata == json.loads((directory / 'OptionsScreen_Menu.package.json').read_text())

def script(name, meta=metadata, raw=body):
    item = next(e for e in meta['exports'] if e['name'] == name)
    data = raw[item['uexp_offset']:item['uexp_offset'] + item['serial_size']]
    candidates = [(at + 4, struct.unpack_from('<I', data, at)[0]) for at in range(4, len(data) - 16)
                  if at + 4 + struct.unpack_from('<I', data, at)[0] + 12 == len(data) and data[-13] == 0x53]
    assert len(candidates) == 1
    at, length = candidates[0]
    parser = Script(data[at:at + length], meta['names'], meta['imports'], meta['exports'])
    nodes = {}
    while parser.at < len(parser.data):
        runtime = parser.at + parser.runtime_delta
        nodes[runtime] = parser.expression()
    assert parser.at + parser.runtime_delta == struct.unpack_from('<I', data, at - 8)[0]
    return nodes

wrapper = script('BndEvt__BackButton_K2Node_ComponentBoundEvent_0_OnUIComponentClicked__DelegateSignature')
assert wrapper[36]['call'] == 'ExecuteUbergraph_OptionsScreen_Menu' and wrapper[36]['arguments'] == [{'int': 416}]
graph = script('ExecuteUbergraph_OptionsScreen_Menu')
assert graph[416]['let_object'][1]['call'] == '/Script/Engine.Actor.GetOwner'
assert graph[436]['let_object'][1]['call']['call'] == '/Script/Engine.Actor.GetOwner'
assert graph[478]['let'][2]['cast'].endswith('.UI_PauseMenu_Content_BP_C')
assert graph[553]['call'] == {'call': 'SubAction_B', 'arguments': []}
trifold = ROOT / 'analysis/research/flat_ui_graph/UI_PauseMenu_Content_Trifold_Base_BP'
trifold_asset,trifold_body = trifold.with_suffix('.uasset').read_bytes(),trifold.with_suffix('.uexp').read_bytes()
assert hashlib.sha256(trifold_asset).hexdigest() == '354ead27426c8cdb99474568855cce146f22d063c68cfcd64ffe04cf7d742ca2'
assert hashlib.sha256(trifold_body).hexdigest() == '781e4aac1d8d5f708c57a20999237a4a3814747e81f2d7d9a57db1284cb1ae68'
nodes = script('ExecuteUbergraph_UI_PauseMenu_Content_Trifold_Base_BP',package(trifold_asset,trifold_body),trifold_body)
assert nodes[4531]['let'][0]['field'] == ['LastPauseMenuSubScreen'] and nodes[4531]['let'][2] == {'name': 'None'}
assert nodes[4786]['let_bool'][1]['call'] == '/Script/VR4.VR4UIManager.PopUIScreen'
assert nodes[4786]['let_bool'][1]['arguments'][0]['field'] == ['UIScreen']
assert nodes[4815]['let_bool'][1]['arguments'][0]['field'] == ['DoorRightScreen']
cover_dir = ROOT / 'analysis/research/options_cover_originals'
originals = {}
for stem,asset_hash,body_hash in (
    ('OptionsCoverScreen_BP','a549dafb6113cd17d0fee00e389f8185de9540089d1da7797c3501dc8cf2bc2b','28916f928f09139c62f3244f55c941b327eeef4099c9fb4178bab013ff366f22'),
    ('UI_PauseMenu_BP','c907507fbebe9398aa4f1f5027ca41dd65f0d5d8e21cadc5b1d6a0e3273a18d5','e72b5687b36ad5afddd9204dc7f28c29b0c1b1a62ba6f8ad1b8c7c23f27b1271'),
    ('UI_PauseMenu_Header_BP','aa25542f8ef75651878545faf2d98a14876417ed86d046789f69b76a9ad55f8f','f77adceceba216e8ecdaa8c11277b1818f0d3b37f9060da805bc75959ca5166c'),
    ('UI_PauseMenu_Header_Button','67198cdc1a6c2350d7a0b0224fa0187c670082d3c3fa2ad37f3e411ab4306daa','062769fd16abcb54aa3f2568d3ce1019c7af55658bc1d5449e57ad86f057ce7c'),
    ('UI_PauseMenu_Header_Base_BP','648f41749b5a17e5164f6668ca9bfaa76f61fcd0bc59bdbd6f2e29902347d884','36e0c9c5d5950b6d4e24c35cd4311d511f2c3fd9488d2d9ea4d082b090b2f122')):
    raw_asset,raw_body = [(cover_dir/(stem+ext)).read_bytes() for ext in ('.uasset','.uexp')]
    assert hashlib.sha256(raw_asset).hexdigest()==asset_hash and hashlib.sha256(raw_body).hexdigest()==body_hash
    meta=package(raw_asset,raw_body)
    assert meta==json.loads((cover_dir/(stem+'.package.json')).read_text())
    originals[stem]=(meta,raw_body)
def original(stem,name):
    meta,raw=originals[stem]
    return script(name,meta,raw)
root_graph=original('UI_PauseMenu_BP','ExecuteUbergraph_UI_PauseMenu_BP')
assert root_graph[4221]['let'][2]['call']['field']==['LastPauseMenuScreen']
assert root_graph[4325]['let_bool'][1]['arguments'][0]['value']['field']==['PauseMenuStartInfo']
assert root_graph[4325]['let_bool'][1]['arguments'][1]=={'byte_int':2}
assert root_graph[4384]['let'][2]['cases'][0]['result']['cases'][1]['result']['switch_value']['field']==['Temp_byte_Variable']
assert root_graph[3440]['let'][0]['field']==['LastPauseMenuScreen'] and root_graph[3440]['let'][2]['field']==['CurrentMenu_Enum']
root_refs=original('UI_PauseMenu_BP','CreateReferences')
assert root_refs[379]['let_object'][0]['field']==['Ref_Header_InGame']
header_meta,header_body=originals['UI_PauseMenu_Header_BP']
inventory=next(e for e in header_meta['exports'] if e['name']=='Button_Inventory_GEN_VARIABLE')
payload=header_body[inventory['uexp_offset']:inventory['uexp_offset']+inventory['serial_size']]
assert inventory['class_path']=='/Script/Engine.ChildActorComponent'
def object_tag(data,name):
    at=data.index(struct.pack('<II',header_meta['names'].index(name),0))
    kind,number,size,array=struct.unpack_from('<4I',data,at+8)
    assert header_meta['names'][kind]=='ObjectProperty' and (number,size,array,data[at+24])==(0,4,0,0)
    return struct.unpack_from('<i',data,at+25)[0]
actor_class=object_tag(payload,'ChildActorClass')
assert next(i['path'] for i in header_meta['imports'] if i['index']==actor_class).endswith('.UI_PauseMenu_Header_Button_C')
actor=header_meta['exports'][object_tag(payload,'ChildActorTemplate')-1]
assert actor['outer']==inventory['index'] and actor['class_path'].endswith('.UI_PauseMenu_Header_Button_C')
actor_data=header_body[actor['uexp_offset']:actor['uexp_offset']+actor['serial_size']]
at=actor_data.index(struct.pack('<II',header_meta['names'].index('MenuSelection'),0))
kind,number,size,array=struct.unpack_from('<4I',actor_data,at+8)
assert header_meta['names'][kind]=='ByteProperty' and (number,size,array,actor_data[at+32])==(0,8,0,0)
value,value_number=struct.unpack_from('<II',actor_data,at+33)
assert not value_number and header_meta['names'][value]=='UI_PauseMenu_Enum::NewEnumerator2'
tab=original('UI_PauseMenu_Header_Button','ExecuteUbergraph_UI_PauseMenu_Header_Button')
assert tab[1005]['target']['context']['field']==['Button'] and tab[1005]['target']['call']['field']==['OnClicked']
assert tab[548]['jump_if_not']==274 and tab[313]['context']['field']==['Ref_Parent_Header']
assert tab[313]['call']=={'call':'ChangeMenuState','arguments':[{'field':['MenuSelection'],'owner':32}]}
change=original('UI_PauseMenu_Header_Base_BP','ChangeMenuState')
assert change[104]['context']['field']==['Ref_Parent_PauseMenu'] and change[104]['call']['call']=='SwapMenuFn'
swap=original('UI_PauseMenu_BP','SwapMenuFn')
assert swap[38]['call']=='CanSwapMenu' and swap[99]['jump_if_not']==300
assert swap[113]['let'][0]['field']==['NextMenu_Enum'] and swap[211]['call']=='Content_Exit'
assert swap[377]['let'][0]['field']==['LastPauseMenuSubScreen'] and swap[377]['let'][2]=={'name':'None'}
print('PASS:10 exact original cover/root/header members; Inventory2 requests intentionally remember Options; exact Inventory ChildActor/template/MenuSelection2 -> original Button.Click -> ChangeMenuState -> guarded SwapMenuFn, with original content animation and remembered-tab publication',flush=True)
capture_path=cover_dir/'p348_live_bindings.json'
assert hashlib.sha256(capture_path.read_bytes()).hexdigest()=='4c403655c412832b970de4a39a58290946fc56fdb77f077f4c691cbcf1f3bb92'
capture=json.loads(capture_path.read_text())
live,bindings=capture['snapshot'],capture['child_bindings']
assert live['pid']==14962
assert live['content']['native']['owner']=='0x0'
assert bindings['address']==live['root']['address']
child=bindings['Content_Actor']
assert child['cls']=='ChildActorComponent' and child['outer']==child['native']['owner']==live['root']['address']
assert child['native']['child_actor']==live['content']['address']
assert bindings['properties']['Content_Actor']['offset']==0x358
for node,field,offset in (('root','Ref_ContentActor',0x398),('root','Ref_Header_InGame',0x3b8),
                        ('content','CentralCoverScreen',0x3a8),('header','Ref_Parent_PauseMenu',0x240),
                        ('header','Button_Inventory',0x290),('actor','Ref_Parent_Header',0x2f8),('actor','Button',0x238)):
    prop=live[node]['properties'][field]
    assert (prop['kind'],prop['offset'],prop['size'],prop['dim'])==('ObjectProperty',offset,8,1)
print('PASS: immutable p348 live topology; content Actor.Owner is null, exact owned Content_Actor binds the current content; C fixture uses the captured property offsets and null Actor.Owner',flush=True)
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for address, offset in ((0x9e10ed8, 0x308), (0x9e10ea8, 0x310), (0x9e0fb28, 0x320), (0x9e0fbb0, 0x318), (0x9e0fb88, 0x31c), (0xa1087b8, 0x1f8)):
    assert struct.unpack_from('<I', native, address + 36)[0] == offset
assert struct.unpack_from('<4I', native, 0x6371bcc) == (0xd101c3ff, 0xa90457f6, 0xa9054ff4, 0xa9067bfd)
print('PASS: immutable original Options Back -> two native owners -> SubAction_B clears LastPauseMenuSubScreen and pops both actual panels; Click24/48 ABI and registered/parent/action/size fields', flush=True)

header = (ROOT / 'analysis/re4fix_flat_view.h').read_text()
world = (ROOT / 'analysis/re4fix_world_touch.h').read_text()
source = (ROOT / 'analysis/re4fix.c').read_text()
assert 'unpause_ui_back = fv_options_back_action;' in header
unpause = (ROOT / 'analysis/re4fix_unpause.h').read_text()
assert unpause.index('touch_menu_grip_cancel(hand)') < unpause.index('unpause_ui_back ? unpause_ui_back(screen)') < unpause.index('int accepted = try_unpause(0)')
fragments = {
    'FRAME_OWNED_FRAGMENT': header[header.index('static int fv_frame_owned('):header.index('static void fv_frame_restore(')],
    'FRAME_DESCENDS_FRAGMENT': header[header.index('static int fv_descends_from('):header.index('static int fv_component_plane(')],
    'UI_CLICK_FRAGMENT': 'static void (*fv_ui_click)(void*,const void*,const void*);\n' + header[header.index('static int fv_click_component('):header.index('// Native widget rectangles')],
    'UI_TARGET_FRAGMENT': header[header.index('// GameThread (menu hand tick): publish'):header.index('static long fv_hint_since;')],
    'Q_ROT_FRAGMENT': source[source.index('static void q_rot('):source.index('static float g_eye_height')],
    'UI_PROJECT_FRAGMENT': world[world.index('static int wt_project('):world.index('// Input thread: a short still tap')],
    'UNPAUSE_BACK_FRAGMENT': unpause[:unpause.index('static void install_touch_unpause(')],
}
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
test = ROOT / 'analysis/research/test_ui_header.c'
with tempfile.TemporaryDirectory(prefix='re4-options-back-') as temp:
    out = Path(temp)
    defines = []
    for name, code in fragments.items():
        path = out / (name.lower() + '.h')
        path.write_text(code)
        defines.append('-D' + name + '="' + path.as_posix() + '"')
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', *defines, '-fsyntax-only', str(test)], check=True)
    obj, exe = out / 'options.obj', out / 'options.exe'
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', '-isystem', str(vc/'include'),
                    '-isystem', str(sdk/'Include/10.0.26100.0/ucrt'), *defines, '-c', '-o', str(obj), str(test)], check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj), 'libcmt.lib', 'libucrt.lib',
                    'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib', f'/LIBPATH:{vc/"lib/x64"}',
                    f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}', f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: actual Android23 ARM64 -Werror syntax and projected native header/Options Back C host execution', flush=True)
