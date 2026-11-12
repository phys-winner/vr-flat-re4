"""Check exact R104 pickup provenance and execute the QA-only collection guards."""
from pathlib import Path
import hashlib
import json
import os
import re
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
header=(ROOT/'analysis/research/qa_key_item_use.h').read_text()
prototype=(ROOT/'analysis/research/key_native_select_prototype.h').read_text()
for source_text in (header,prototype):
    for address,a,b in re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source_text):
        assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16)),address
rows=json.loads((ROOT/'analysis/research/merchant_originals/room_records.json').read_text())
ita=next(section for row in rows if row['room']=='104' for section in row['sections'] if section['tag']=='ITA')
assert ita['count']==27 and ita['sha256']=='433f0622d1b67bf80f51c910c822759dc26471b3959871a13ff3643a1b4a1072'
for index,item,id,save in ((15,0xa4,0xf,16),(16,0xa5,0x10,17)):
    entry=bytes.fromhex(ita['records'][index]['converted'])
    assert len(entry)==0xd0 and entry[0x38:0x3b]==bytes((3,3,id))
    assert struct.unpack_from('<H',entry,0xa4)[0]==item
    assert struct.unpack_from('<HH',entry,0xa8)==(0,save)
    # SceAtInit's original complete ITA loop toggles bit 0x80 before actor spawn.
    assert (id^0x80)==(0x8f if item==0xa4 else 0x90)
    assert f'id=item==0xa4?0x8f:0x90' in header
assert struct.unpack_from('<III',native,0x6150fc0)==(0x3940e90a,0x5219014a,0x3900e90a)
assert struct.unpack_from('<III',native,0x6151008)==(0x39400109,0x52190129,0x39000109)
transitions=(ROOT/'analysis/research/qa_user_transitions.h').read_text()
assert all(f'!strcmp(action,"{name}")' in transitions for name in ('emblemright','emblemleft','keystate','keyfixture','savefixture'))
assert '!strcmp(action,"keyfixture"))value=20;' in transitions
assert '!strcmp(action,"savefixture"))value=22;' in transitions
assert '"HexagonalEmblem",0)' in header  # FNAME_Find only: no name-pool insertion.
loader=ROOT/'analysis/research/host_oodle_decode.py'
environment={'__file__':str(loader),'__name__':'original_key_combine_check'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),environment)
u,base,buffers,call=(environment[name] for name in ('u','base','buffers','call'))
before=bytes(u.mem_read(base+0xa500780,0x100)),bytes(u.mem_read(base+0xa501bd8,0x9000))
for first,second in ((0xa4,0xa5),(0xa5,0xa4)):
    u.mem_write(buffers,b'\xa5'*16)
    assert call(0x5f34198,[first,second,buffers])==1
    assert bytes(u.mem_read(buffers,16))==bytes.fromhex('a600')+b'\xa5'*14
assert before==(bytes(u.mem_read(base+0xa500780,0x100)),bytes(u.mem_read(base+0xa501bd8,0x9000)))
print('PASS: complete original ARM64 itemCombine recognizes both half orders as0xa6; inventory/player bytes unchanged')
# Execute the complete original RoomSave getter with relocated runtime tables.
# These writes configure emulator fixtures only; canonical bytes/device stay intact.
room_manager=struct.unpack('<Q',bytes(u.mem_read(base+0xa361d50,8)))[0]
saved_table=bytes(u.mem_read(base+0xa364288,8));saved_pool=bytes(u.mem_read(room_manager+0x30,8))
table,room_zero,room_one,pool=buffers+0x10000,buffers+0x11000,buffers+0x12000,buffers+0x13000
u.mem_write(table,bytes(160));u.mem_write(room_zero,bytes(24));u.mem_write(room_one,bytes(5*24))
u.mem_write(base+0xa364288,struct.pack('<Q',table));u.mem_write(room_manager+0x30,struct.pack('<Q',pool))
u.mem_write(table+16,struct.pack('<QH',room_one,5));u.mem_write(room_one+4*24,b'\x01')
assert call(0x614cdb0,[room_manager,0x104])==pool
u.mem_write(table,struct.pack('<QH',room_zero,1));u.mem_write(room_zero,b'\x01');u.mem_write(room_one,b'\x01')
assert call(0x614cdb0,[room_manager,0x104])==pool+2*216
u.mem_write(room_one+4*24,b'\0');assert call(0x614cdb0,[room_manager,0x104])==0
u.mem_write(base+0xa364288,saved_table);u.mem_write(room_manager+0x30,saved_pool)
print('PASS: complete original ARM64 RoomSave getter uses dynamic active-room index, including relocation0=>2 and disabled-room refusal')
# Original EBio4FlagType metadata and getter distinguish global Key9 from RoomScenario9.
enum_table=struct.unpack('<Q',bytes(u.mem_read(base+0x9d556f0+0x20,8)))[0]
assert enum_table==base+0x9d556b0
assert struct.unpack('<I',bytes(u.mem_read(base+0x9d556f0+0x28,4)))[0]==4
for index,name in enumerate(('SceAt','Key','RoomScenario','Scenario')):
    pointer,value=struct.unpack('<Qq',bytes(u.mem_read(enum_table+16*index,16)))
    assert value==index and bytes(u.mem_read(pointer,64)).split(b'\0',1)[0]==('EBio4FlagType::'+name).encode()
game_storage=struct.unpack('<Q',bytes(u.mem_read(base+0xa361d30,8)))[0]
assert game_storage==base+0xa3f5e48
game=struct.unpack('<Q',bytes(u.mem_read(game_storage,8)))[0]
assert game==base+0xa501bd8
saved_key_word=bytes(u.mem_read(game+0x5378,4));room_before=bytes(u.mem_read(pool+4,4))
flag_array,flag_data=buffers+0x18000,buffers+0x18100
u.mem_write(flag_array,struct.pack('<Qii',flag_data,1,1))
flag=struct.pack('<B3xiBB2x',0xff,9,1,1);u.mem_write(flag_data,flag)
for word in (0,2,0x00400000,0x00400002):
    u.mem_write(game+0x5378,struct.pack('<I',word))
    call(0x62f2678,[0,flag_array,0xff]) # Complete original SetBio4Flags, host-only fixture.
    assert bytes(u.mem_read(game+0x5378,4))==struct.pack('<I',word|0x00400000)
    assert bytes(u.mem_read(pool+4,4))==room_before and bytes(u.mem_read(flag_data,12))==flag
u.mem_write(game+0x5378,saved_key_word)
print('PASS: original enum Key=1; complete original ARM64 SetBio4Flags(Key9) writes Game+5378 and preserves RoomScenario bytes')
clang=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-qa-key-') as td:
    out=Path(td)
    for filename in ('test_qa_key_item_use.c','test_key_native_select_prototype.c'):
        source=ROOT/'analysis/research'/filename;obj=out/(source.stem+'.obj');exe=out/(source.stem+'.exe')
        subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-c',str(source),'-o',str(obj)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],cwd=out,check=True)
print('PASS: original native preimages and two original R104 ITA pickup identities; ARM64 syntax and executed QA guard checks')
print('LIMIT: QA fixtures stay excluded from normal APKs; gameplay acceptance is documented by selected-device receipts')
