"""Check the original treasure sale route and execute actual QA fixture guards."""
from pathlib import Path
import hashlib
import json
import os
import re
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
directory=ROOT/'analysis/research/merchant_originals'
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
receipt=json.loads((directory/'receipt.json').read_text())
for item in receipt['files']:
    body=(directory/item['output']).read_bytes()
    assert len(body)==item['bytes'] and hashlib.sha256(body).hexdigest()==item['sha256'] and item['repeats']==2
for stem in ('VR4PauseMerchantPlayerPawn_BP','UI_KeysTreasures_Grab_Button_BP','UI_MerchantScreen_InventoryFrame_BP','UI_Treasure_Item_BP'):
    package=json.loads((directory/(stem+'.package.json')).read_text())
    assert hashlib.sha256((directory/(stem+'.uasset')).read_bytes()).hexdigest()==package['asset_sha256']
    assert hashlib.sha256((directory/(stem+'.uexp')).read_bytes()).hexdigest()==package['body_sha256']
package=json.loads((directory/'VR4PauseMerchantPlayerPawn_BP.package.json').read_text())
cls=next(e for e in package['exports'] if e['name']=='VR4PauseMerchantPlayerPawn_BP_C')
assert next(i for i in package['imports'] if i['index']==cls['super'])['path']=='/Game/Blueprints/Player/Pause/VR4PausePlayerPawn_BP.VR4PausePlayerPawn_BP_C'
package=json.loads((directory/'UI_KeysTreasures_Grab_Button_BP.package.json').read_text())
entry=next(e for e in package['exports'] if e['name']=='VRUIButton_GEN_VARIABLE')
body=(directory/'UI_KeysTreasures_Grab_Button_BP.uexp').read_bytes()[entry['uexp_offset']:entry['uexp_offset']+entry['serial_size']]
names=package['names'];fname=lambda at:names[struct.unpack_from('<I',body,at)[0]]
assert len(body)==111 and fname(58)=='AllowedAction' and fname(66)=='EnumProperty'
assert struct.unpack_from('<II',body,74)==(8,0) and fname(82)=='EVR4UIAction' and body[90]==0
assert fname(91)=='EVR4UIAction::Grip' and struct.unpack_from('<I',body,95)[0]==0
def script(stem,name):
    result=json.loads((directory/(stem+'.'+name+'.script.json')).read_text())
    package=json.loads((directory/(stem+'.package.json')).read_text())
    entry=next(e for e in package['exports'] if e['name']==name)
    assert result['export']==entry
    payload=(directory/(stem+'.uexp')).read_bytes()[entry['uexp_offset']:entry['uexp_offset']+entry['serial_size']]
    assert hashlib.sha256(payload).hexdigest()==entry['sha256']
    return result
initialize=script('UI_MerchantScreen_InventoryFrame_BP','Initialize')
assert any(c['call']=='InitializeForMerchant' for c in initialize['calls'])
screen=script('UI_MerchantScreen_InventoryFrame_BP','SetScreen')
assert [c['call'] for c in screen['calls'] if c['call'] in ('RefreshItems','QuickIntro')]==['RefreshItems','QuickIntro']
grab=script('UI_Treasure_Item_BP','GrabItem');assert any(c['call']=='GrabWithHand' for c in grab['calls'])
grab=script('UI_Treasure_Item_BP','TryCreateAttacheItem');assert any(c['call']=='TryEnterMoveItemState' for c in grab['calls'])
keys=json.loads((directory/'UI_PauseMenu_Content_KeysTreasures_BP.package.json').read_text())
assert '/Game/Props/BIO4/items/Treasures/Geometry/idm057_spinel' in keys['names']
print('PASS: exact original merchant inherits pause pawn; shared treasure tab, Grip action, real item/attache move route and item0057 Spinel mesh',flush=True)

for lo,hi,digest in (
    (0x6316cd0,0x6316cfc,'38340ec4bc48b88010f78e389fc72d6232c354a90e9191d8d1dcb1f5d331a61b'),
    (0x6316e24,0x631700c,'4ee5429c50e2e866d6313f6819d41ff74e68fcaf017400c57168cc3b8b0c5505'),
    (0x5f4dcc0,0x5f4ddb4,'76106015d7973d67dc7e43b640d5f23dd677c07d5e0ba8758959c1b5a42d7f64'),
    (0x617d214,0x617d2b0,'dd114a138164fb87befb06804f4367231c455284e43e98fc5d3d6deffe6aa468'),
    (0x62ec678,0x62ec894,'e78c8cce86dfb7231b3d96b75b2a897b1ce9a38d21427ab0b48ffe03d5142d37'),
    (0x631493c,0x63149bc,'cf108cfd190e64e19ffc97d270b63aaf341c285e5d457e5ed9d223d1f91f1ed7'),
    (0x6485860,0x648588c,'3cc97f3ece08f71d525014c95bfbf7759bb0048f007d812f01763106ff180c3b'),
    (0x69df1cc,0x69df24c,'2144371488f7df4eaa292697317803db88ca4ffa31e731786a20f9ecd6f85344'),
    (0x870e7a0,0x870e9a0,'f0b1857322da769736e95ba5b701bb842d6f985663614399a6b4e20d6e7bcdef'),
    (0x614cdb0,0x614ceb8,'56f83208882d4fc734e50fd7b4828df58a3db7d41958c28167aebbd690bb7464'),
    (0x60093c0,0x60095d8,'b72d26c0def6609ca5886ac08ab805f716f862aef53fdfd3432e3a097dafec96'),
    (0x6003ed8,0x6004430,'4e14cd8aa200ccccf0adca1658ed5d27329c16e2ce522f30a63bcf75c06abd98'),
    (0x620dc24,0x620dd38,'8fbee49f6f5a3e8a113b20a32b89f6c4c3a71e1c285dceaac25516aaaf76877a'),
    (0x63191f4,0x63191fc,'7474f66d7d13581f6234bc7f18f259fd8c450e4845a6f130411d83119f36fbaf'),
    (0x633e52c,0x633e6a8,'d0047df82412670f23170e1d15d583fd9a3d27a9e0208749b058fda31bbbd79c'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
def target(at):
    op=struct.unpack_from('<I',native,at)[0];assert op&0x7c000000==0x14000000
    immediate=op&0x3ffffff
    if immediate&0x2000000:immediate-=0x4000000
    return at+4*immediate
for at,to in ((0x6316cf4,0x6316e24),(0x6316f40,0x5f4dcc0),(0x6316f48,0x62e17a4),
              (0x6316fd8,0x63155a8),(0x617d244,0x6333dd0),(0x617d274,0x633223c),
              (0x62ec78c,0x6153664),(0x62ec7a0,0x54e83b0),(0x62e9b7c,0x6153ec0),
              (0x6314950,0x69df1cc),(0x631495c,0x6485860),(0x6314968,0x870e9a0),
              (0x6314974,0x6485860),(0x870e7b8,0x8c70eb4),
              (0x6007c1c,0x61553e8),(0x6003254,0x61553e8),(0x60094c4,0x5d5c6c8),
              (0x60094f0,0x5f54144),(0x600950c,0x5f54268),(0x6009538,0x617d214),
              (0x6004348,0x5f54144),(0x6004364,0x5f54268),(0x60043fc,0x615ca24)):
    assert target(at)==to
assert struct.unpack_from('<III',native,0x5f4dcf8)==(0xb94002a8,0x2a1603e2,0x0b000108)
assert struct.unpack_from('<I',native,0x5f4dd04)[0]==0xb90002a8
assert struct.unpack_from('<I',native,0x6316f04)[0]==0x528a069a
assert struct.unpack_from('<I',native,0x6316fd0)[0]==0x52800141
assert struct.unpack_from('<I',native,0x617d23c)[0]==0x528000c1
assert struct.unpack_from('<I',native,0x6150fc4)[0]==0x5219014a
assert struct.unpack_from('<I',native,0x62ec7a4)[0]==0xf901fe7f
print('PASS: original ConfirmClicked Sell9 -> ConfirmSellItems -> native price/money addition and handle removal -> state10; original SubScreenOpen16 -> Merchant start6 -> TryPause',flush=True)

header=(ROOT/'analysis/research/qa_treasure_touch.h').read_text()
for at,a,b in re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header):
    assert struct.unpack_from('<II',native,int(at,16))==(int(a,16),int(b,16)),at
assert header.startswith('// QA only:') and '#ifdef RE4_QA_USER_TRANSITIONS' in header
for metadata,offset in ((0x9dc4c20,0x270),(0x9dc4b38,0x2c0),(0x9dc4a48,0x2f0),
                        (0x9dc4ce0,0x258),(0x9dc4cb0,0x260),(0x9dc3ba8,0x40),
                        (0x9dc3c68,8),(0x9dc3c98,0),(0x9d6d628,0x240),
                        (0x9d6d598,0x248),(0x9e0fb28,0x320),(0xa0b7530,0x130)):
    assert struct.unpack_from('<I',native,metadata+0x24)[0]==offset,hex(metadata)
clang=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
test=ROOT/'analysis/research/test_qa_treasure_touch.c'
with tempfile.TemporaryDirectory(prefix='re4-treasure-') as directory_name:
    folder=Path(directory_name);obj=folder/'test.obj';exe=folder/'test.exe'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror','-fsyntax-only',str(test)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c',str(test),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    release=folder/'release.c';release.write_text('#include "'+(ROOT/'analysis/research/qa_treasure_touch.h').as_posix()+'"\nint main(void){return 0;}\n')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror','-fsyntax-only',str(release)],check=True)
print('PASS: AArch64 QA/normal exclusion and actual-C fixtures; hardware sale still requires root touch acceptance.',flush=True)

# Run the original ARM64 decoder's loader and native dispatch/count/price code.
loader=ROOT/'analysis/research/host_oodle_decode.py'
prefix=loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0]
mark='u.hook_add(UC_HOOK_CODE, external, begin=stubbase, end=stubbase+0xfffff)'
assert prefix.count(mark)==1
environment={'__file__':str(loader),'__name__':'treasure_original_native'}
exec(compile(prefix.replace(mark,'original_external_hook='+mark),str(loader),'exec'),environment)
u,base,buffers,call=(environment[key] for key in ('u','base','buffers','call'))
u.hook_del(environment['original_external_hook'])
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_X8,UC_ARM64_REG_S0,UC_ARM64_REG_SP,UC_ARM64_REG_LR,UC_ARM64_REG_PC
def external(uc,address,size,unused):
    name=environment['names'][address];a,b,c=(uc.reg_read(UC_ARM64_REG_X0+i) for i in range(3));result=a
    if name in ('memcpy','memmove'):assert c<0x1000000;uc.mem_write(a,bytes(uc.mem_read(b,c)))
    elif name=='memset':assert c<0x1000000;uc.mem_write(a,bytes([b&255])*c)
    elif name=='strcmp':
        def string(at):
            raw=bytes(uc.mem_read(at,64));assert b'\0' in raw;return raw.split(b'\0',1)[0]
        first,second=string(a),string(b);result=0 if first==second else 1 if first>second else 0xffffffffffffffff
    else:raise AssertionError(('unexpected external',name,hex(uc.reg_read(UC_ARM64_REG_LR))))
    uc.reg_write(UC_ARM64_REG_X0,result);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_LR))
u.hook_add(UC_HOOK_CODE,external,begin=environment['stubbase'],end=environment['stubbase']+0xfffff)
def put(at,fmt,*values):u.mem_write(at,struct.pack(fmt,*values))
room=next(row for row in json.loads((directory/'room_records.json').read_text()) if row['room']=='101')
ita=next(section for section in room['sections'] if section['tag']=='ITA')
raw=bytes.fromhex(ita['header'])+b''.join(bytes.fromhex(row['bytes']) for row in ita['records'])+bytes.fromhex(ita['tail'])
assert hashlib.sha256(raw).hexdigest()==ita['sha256']
source,converted,empty_aev=buffers+0x1000000,buffers+0x1100000,buffers+0x1200000
u.mem_write(source,raw);call(0x5d3b024,[converted,16+41*0xd0,source,len(raw)])
before=bytes(u.mem_read(converted,16+41*0xd0))
assert before==bytes.fromhex(ita['header'])+b''.join(bytes.fromhex(row['converted']) for row in ita['records'])
game=base+0xa501bd8;put(base+0xa3f5e48,'<Q',game);u.mem_write(game,bytes(0x9000));put(game+0x5038,'<H',0x101)
u.mem_write(empty_aev,struct.pack('<4sHHQ',b'AEV\0',0x104,0,0))
call(0x6150dd0,[empty_aev,converted])
for index in range(41):
    assert u.mem_read(converted+16+index*0xd0+0x3a,1)[0]==(before[16+index*0xd0+0x3a]^0x80)
print('PASS: original ConvertITA exact41 records and SceAtInit itself assigns ITA9 actionID0x89; no inventory issuance or substituted game calls',flush=True)

entry=converted+16+9*0xd0;manager=base+0xa541b38
dynamic=buffers+0x1c00000;u.mem_write(dynamic,bytes(0x100));put(dynamic,'<Q',entry);put(dynamic+0x3a,'<B',0x42)
put(entry,'<Q',0xffffffff);put(manager+0xd8,'<Q',dynamic)
old_dynamic=bytes(u.mem_read(dynamic,0x100));old_entry=bytes(u.mem_read(entry,0xd0))
assert call(0x6153ec0,[0x89])==entry and call(0x6153ec0,[0x42])==dynamic
assert call(0x6153ec0,[0x7f])==0
assert bytes(u.mem_read(dynamic,0x100))==old_dynamic and bytes(u.mem_read(entry,0xd0))==old_entry
print('PASS: original GetItemBySceAtId calls SceAtPtr; original ARM64 lookup passes a dynamic node outside AEV/ITA arrays, returns exact Spinel descriptor/read-only',flush=True)
# An active-list data fixture selects the exact initialized original record; the dispatcher is unmodified.
put(manager+0xd8,'<Q',entry);put(entry,'<Q',0xffffffff)
seen=[]
def at_collection(uc,address,size,unused):
    seen.append(uc.reg_read(UC_ARM64_REG_X0));uc.emu_stop()
hook=u.hook_add(UC_HOOK_CODE,at_collection,begin=base+0x61538d4,end=base+0x61538d4)
u.reg_write(UC_ARM64_REG_SP,environment['stack']+0x1f0000);u.reg_write(UC_ARM64_REG_LR,environment['ret']);u.reg_write(UC_ARM64_REG_X0,0x89)
u.emu_start(base+0x6155664,environment['ret'],timeout=10000000,count=100000)
assert seen==[entry] and u.reg_read(UC_ARM64_REG_PC)==base+0x61538d4
u.hook_del(hook)
print('PASS: original SceAtExecute(0x89) -> authored type3 callback -> arm_sceAtGetItem with the exact original Spinel descriptor; stopped at collection boundary',flush=True)

inventory=base+0xa500780;items=buffers+0x1300000;u.mem_write(inventory,bytes(0x100));u.mem_write(items,bytes(14*4))
put(inventory+0x20,'<Q',items);put(inventory+0x30,'<I',4)
assert call(0x5f33f14,[inventory,0x57])==0
put(items,'<HHBB',0x57,1,1,0)
assert call(0x5f33f14,[inventory,0x57])==1
put(items+2,'<H',2);assert call(0x5f33f14,[inventory,0x57])==2
put(items+5,'<B',1);assert call(0x5f33f14,[inventory,0x57])==0
put(items+5,'<B',0);put(items+4,'<B',0);assert call(0x5f33f14,[inventory,0x57])==0
merchant=buffers+0x1400000;u.mem_write(merchant,bytes(0x1000));put(merchant+0x10,'<Q',base+0xa3f69a0)
put(items,'<HHBB',0x57,1,1,0)
price=call(0x5f4da50,[merchant,items,1]);assert price==2000,price
print('PASS: original ItemMgr::num distinguishes0/1/2, owner/inactive items; original Merchant::buyupPrice with immutable original price table values Spinel1 at2000 pesetas',flush=True)
actor=buffers+0x1500000;room_saves=buffers+0x1600000
u.mem_write(actor,bytes(0x600));put(actor+0x3f8,'<Q',entry);put(actor+0x4c0,'<H',0x57);put(actor+0x4c2,'<BB',1,1)
put(items,'<HHBB',0x57,0,1,0);put(entry+0x90,'<Q',0)
u.mem_write(room_saves,bytes(512*0xd8));put(base+0xa540ce8+0x30,'<Q',room_saves)
money=bytes(u.mem_read(game+0x5034,4));old_saves=bytes(u.mem_read(room_saves,512*0xd8))
assert call(0x5f33f14,[inventory,0x57])==0
call(0x62ec678,[actor])
assert call(0x5f33f14,[inventory,0x57])==1
assert bytes(u.mem_read(actor+0x3f8,8))==bytes(8) and u.mem_read(actor+0x4dc,1)==b'\x01'
assert not (u.mem_read(entry+0x38,1)[0]&1) and bytes(u.mem_read(game+0x5034,4))==money
assert bytes(u.mem_read(room_saves,512*0xd8))!=old_saves
collected_actor=bytes(u.mem_read(actor,0x600));collected_item=bytes(u.mem_read(items,14*4))
call(0x62ec678,[actor])
assert call(0x5f33f14,[inventory,0x57])==1 and bytes(u.mem_read(actor,0x600))==collected_actor and bytes(u.mem_read(items,14*4))==collected_item
print('PASS: complete original ARM64 Actor.AddToInventory -> native item execution/collection, Spinel0->1, native persistent pickup flag, descriptor disabled, actor descriptor cleared/collected flag, money unchanged, second call no-op; no substituted native game calls',flush=True)

# A Quest merchant is EEm10Type::Merchant (6), not necessarily a legacy cEm18.
# Execute the authoritative getter, weak-object resolution, original virtual actor
# component search and native class checks with host object/class data only.
model=buffers+0x1700000;merchant_actor=buffers+0x1710000;component=buffers+0x1720000
component_class=buffers+0x1730000;merchant_class=buffers+0x1740000;wrong_class=buffers+0x1750000
chains=buffers+0x1760000;owned_slots=buffers+0x1770000;chunk_table=buffers+0x1780000;chunk_data=buffers+0x1790000
u.mem_write(model,bytes(0x1200));u.mem_write(merchant_actor,bytes(0x300));u.mem_write(component,bytes(0xc0))
for cls in (component_class,merchant_class,wrong_class):u.mem_write(cls,bytes(0x80))
put(component_class+0x30,'<QI',chains,0);put(merchant_class+0x30,'<QI',chains,1);put(wrong_class+0x30,'<QI',chains,0)
put(chains,'<QQ',component_class+0x30,merchant_class+0x30)
put(base+0xa5cd090,'<Q',merchant_class);put(base+0xaa7ff18,'<Q',component_class)
assert struct.unpack('<Q',u.mem_read(base+0xa0b8380+0x618,8))[0]==base+0x870e7a0
put(merchant_actor,'<Q',base+0xa0b8380);put(merchant_actor+0x1a0,'<Q',owned_slots)
put(merchant_actor+0x1b0,'<I',1);put(merchant_actor+0x1c8,'<I',1);put(owned_slots,'<Q',component)
put(component+0x10,'<Q',merchant_class);put(component+0xa0,'<Q',merchant_actor)
gu=struct.unpack('<Q',u.mem_read(base+0xa352308,8))[0]
assert base<=gu<base+environment['size']
put(gu+0x10,'<Q',chunk_table);put(gu+0x24,'<I',8);put(chunk_table,'<Q',chunk_data)
put(chunk_data+3*24,'<Q',merchant_actor);put(chunk_data+3*24+0x10,'<I',7)
put(model+0x3d4,'<II',3,7)
assert struct.unpack('<QQ',u.mem_read(base+0x9d5da60,16))==(base+0x42e7a6a,6)
assert native[0x42e7a6a:0x42e7a6a+len(b'EEm10Type::Merchant\0')]==b'EEm10Type::Merchant\0'
for case in ('Quest10/6','legacy18','serial_mismatch','dead_owner','absent_component','wrong_component_class','null_model'):
    put(model+0x118,'<BB',0x18 if case=='legacy18' else 0x10,6)
    put(model+0x3d8,'<I',8 if case=='serial_mismatch' else 7)
    put(chunk_data+3*24+0xb,'<B',0x10 if case=='dead_owner' else 0)
    put(merchant_actor+0x1c8,'<I',0 if case=='absent_component' else 1)
    put(component+0x10,'<Q',wrong_class if case=='wrong_component_class' else merchant_class)
    regions=((model,0x1200),(merchant_actor,0x300),(component,0xc0),(items,14*4),(game+0x5034,4))
    snapshots=[bytes(u.mem_read(at,length)) for at,length in regions]
    result=call(0x631493c,[0 if case=='null_model' else model])
    assert result==(component if case in ('Quest10/6','legacy18') else 0),(case,hex(result))
    assert snapshots==[bytes(u.mem_read(at,length)) for at,length in regions],case
print('PASS: original EEm10Type::Merchant=6; complete original GetMerchantComponent -> serial-safe weak owner -> original virtual Actor.FindComponentByClass -> native class check, Quest10/6 and legacy18; missing/dead/mismatched/wrong class/null refusals; actor/model/component/inventory/money read-only, no substituted native game calls',flush=True)

diag_hand=buffers+0x1b00000;diag_root=buffers+0x1b10000;diag_case=buffers+0x1b20000;ray_out=buffers+0x1b30000
diag_merchant=buffers+0x1b40000;diag_inventory=buffers+0x1b50000
u.mem_write(diag_hand,bytes(0x570));u.mem_write(diag_root,bytes(0x200));u.mem_write(diag_case,bytes(0x2a8));u.mem_write(diag_merchant,bytes(0x320))
put(diag_hand,'<Q',base+0xa0b8380);put(diag_hand+0x130,'<Q',diag_root);put(diag_case+0x130,'<Q',diag_root)
put(base+0xa94fb18,'<Q',merchant_class) # Initialized WidgetInteraction class data; empty owned-component list selects native root fallback.
for normal,position in (((-1.,0.,0.),(180.,0.,15167.)),((0.,0.,1.),(110.547,0.,15088.))):
    put(diag_root+0x1c0,'<12f',0,0,0,1,*position,0,1,1,1,0)
    put(diag_case+0x240,'<4f',*normal,0)
    unchanged=bytes(u.mem_read(diag_root,0x200)),bytes(u.mem_read(diag_case,0x2a8)),bytes(u.mem_read(diag_hand,0x570))
    call(0x620dc24,[diag_case])
    plane=tuple(struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0+i)))[0] for i in range(4))
    expected=(*normal,sum(normal[i]*position[i] for i in range(3)))
    assert all(abs(a-b)<.01 for a,b in zip(plane,expected)),(plane,expected)
    u.reg_write(UC_ARM64_REG_X8,ray_out);call(0x633e52c,[diag_hand])
    ray=struct.unpack('<6f',u.mem_read(ray_out,24))
    assert all(abs(a-b)<.01 for a,b in zip(ray,(*position,1,0,0))),ray
    assert unchanged==(bytes(u.mem_read(diag_root,0x200)),bytes(u.mem_read(diag_case,0x2a8)),bytes(u.mem_read(diag_hand,0x570)))
put(diag_merchant+0x318,'<Q',diag_inventory);before=bytes(u.mem_read(diag_merchant,0x320))
assert call(0x63191f4,[diag_merchant])==diag_inventory and bytes(u.mem_read(diag_merchant,0x320))==before
print('PASS: diagnostic field identities; original ARM64 GetUIInteractionRay 24-byte indirect return and GetInteractionPlane four-float return for vertical/horizontal real transforms, Merchant.GetInventoryUIScreen; inputs byte-identical, no substituted native game calls',flush=True)

# Empty initialized native interaction array models a room without a physical VR
# override; the original Find implementation executes, rather than a stub.
def qword(at):return struct.unpack('<Q',u.mem_read(at,8))[0]
put(qword(base+0xa3662a8),'<Q',0);put(base+0xa572b90,'<B',1);u.mem_write(base+0xa572f00,bytes(16))
assert call(0x635951c,[18,2])==0
assert qword(base+0xa361d50)==base+0xa540ce8 and qword(base+0x9cee040)==base+0x615bc6c
assert struct.unpack_from('<3f',native,0x43086a8)==(2666.0,0.0,-8495.0)
assert struct.unpack_from('<2f',native,0x42fbc70)==(-11843.0,0.0)
room_rows=json.loads((directory/'room_records.json').read_text())
scene_node=buffers+0x1a00000;source_node=buffers+0x1a10000
def forbid_synchronous_scene(uc,address,size,unused):
    raise AssertionError(('yielding scene/sleep/current-task lookup ran on caller',hex(address-base)))
forbidden=[u.hook_add(UC_HOOK_CODE,forbid_synchronous_scene,begin=base+at,end=base+at)
           for at in (0x6003ed8,0x60093c0,0x6161358,0x61629d4)]
for room_id,id,callback,kind in ((0x101,8,0x6003ed8,2),(0x104,18,0x60093c0,1)):
    row=next(row for row in room_rows if row['room']==f'{room_id:03x}')
    aev=next(section for section in row['sections'] if section['tag']=='AEV')
    raw=bytes.fromhex(next(row for row in aev['records'] if bytes.fromhex(row['bytes'])[0x36]==id)['bytes'])
    u.mem_write(source_node,raw);u.mem_write(scene_node,bytes(0x100));call(0x5d3a7f0,[scene_node,source_node])
    put(scene_node,'<Q',0xffffffff);put(manager+0xd8,'<Q',scene_node)
    call(0x61553e8,[id,20,0,base+callback,0,kind])
    registered=bytes(u.mem_read(scene_node,0x100))
    assert registered[0x38]&1 and registered[0x39]==2 and registered[0x3a]==id
    assert registered[0x3e]==20 and registered[0x50:0x52]==bytes((2,kind))
    assert struct.unpack_from('<QQ',registered,0x40)==(0,base+callback)
    assert call(0x6153ec0,[id])==scene_node
    old_save=bytes(u.mem_read(room_saves,512*0xd8))
    save=call(0x614cdb0,[base+0xa540ce8,room_id])
    assert room_saves<=save<room_saves+512*0xd8 and (save-room_saves)%0xd8==0
    assert bytes(u.mem_read(room_saves,512*0xd8))==old_save
    for current in (0,base+0xa40a0f8,0xffffffffffffffff):
        put(base+0xa542bb8+0x38+2*8,'<Q',0xffffffff)
        put(base+0xa543150,'<Q',current)
        u.mem_write(base+0xa5436e8,bytes(12*0x40))
        unchanged=[bytes(u.mem_read(at,length)) for at,length in ((game,0x9000),(items,14*4),(room_saves,512*0xd8))]
        assert call(0x6155664,[id])==1
        tasks=[bytes(u.mem_read(base+0xa5436e8+i*0x40,0x40)) for i in range(12)]
        active=[(i+8,task) for i,task in enumerate(tasks) if task[0]]
        assert len(active)==1 and active[0][0]==19
        task=active[0][1]
        assert task[0]==1 and task[1]==14 and task[3]==kind
        assert struct.unpack_from('<QQ',task,0x10)==(0,base+callback)
        assert qword(base+0xa543150)==current and bytes(u.mem_read(scene_node,0x100))==registered
        assert unchanged==[bytes(u.mem_read(at,length)) for at,length in ((game,0x9000),(items,14*4),(room_saves,512*0xd8))]
for hook in forbidden:u.hook_del(hook)
print('PASS: original R101 AEV8/kind2 and R104 AEV18/kind1 -> native ConvertSceAtUnit, registration with real empty Find, SceAtPtr, SceAtExecute/SceExec/TaskExec; exactly native task19 queued for null/main/idle(-1), no synchronous scene/SceSleep/current-task lookup; original RoomSave getter, descriptors/player/inventory/money/story flags read-only, no substituted native game calls',flush=True)
print('LIMIT: fixture setup uses original collection; actual device sale, drag/drop, confirmation, removal and money increase remain hardware acceptance.',flush=True)
