"""Verify exact native nonblocking route and run the actual persistence adapter."""
import os
from pathlib import Path
import hashlib,json,re,struct,subprocess,tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
rela={a:c for a,t,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
for got,target in ((0xa361d30,0xa3f5e48),(0xa362e30,0xa50ae25),(0xa362e48,0xa4ff378)):
    assert rela[got]==target
def word(address):return struct.unpack_from('<I',native,address)[0]
def branch(address):
    op=word(address);assert op&0x7c000000==0x14000000
    value=op&0x3ffffff
    if value&0x2000000:value-=0x4000000
    return address+4*value
graph=json.loads((ROOT/'analysis/research/extras_save_graph/native_graph.json').read_text())
for item in graph.values():
    if 'sha256' in item:
        first,last=int(item['start'],16),int(item['end'],16)
        assert hashlib.sha256(native[first:last]).hexdigest()==item['sha256']
# Constructor owns no file buffer: only two FixedFrames and a cDataSwap. The matching
# destructor tailcalls cDataSwap's ret, so the adapter must free its own cloned buffer.
assert branch(0x5f1b0f0)==branch(0x5f1b0f8)==0x5f13784
assert branch(0x5f1b108)==0x5d5b868 and branch(0x5f1b110)==0x5d5b878
assert word(0x5d5b878)==0xd65f03c0
assert struct.unpack_from('<I',native,0x42f9530)[0]==2079352
# Exact existing Load stages only: selecting a slot, or the authored no-save-slots
# message. Other negative errors never qualify for private system publication.
assert rela[0x9ce5a18+2*48]==0x5f1c640 and rela[0x9ce5a18+5*48]==0x5f20590
assert word(0x5f1c768)==0x12804308 and word(0x5f1c76c)==0x528000a9
assert branch(0x5f1c784)==0x63586a0
assert 0x5f20610+native[0x4304578+1]*4==0x5f20654
error_index=-537+0x21c
assert 0x5f20648+struct.unpack_from('<H',native,0x430457c+2*error_index)[0]*4==0x5f20b40
assert word(0x5f20b44)==0x52800929 and word(0x5f20b4c)==0xb906d669
assert branch(0x5f20654)==0x6358210
# GetInstance validates its UObject; the read-only menu bytes match original queries.
assert rela[0xa3667f0]==0xa582478
assert word(0x6358180)==0x39490d08 and word(0x63581f8)==0x39491108 and word(0x6358270)==0x39491508
assert word(0x6358cc0)==0x39490000 and word(0x6358cc8)==0x39491800
# Original sysfileRead publishes precisely the complete buffer/size and native ready bit.
assert word(0x5f1dc20)==0xf942f016 and word(0x5f1dc30)==0xb9000d2a
assert word(0x5f1dc38)==0x3900013f and word(0x5f1dc3c)==0xf9000d36
assert word(0x5f1dcc0)==0x5280002b and word(0x5f1dccc)==0x390e014b
ready=json.loads((ROOT/'analysis/research/lobby_save_originals/card_readiness_device.json').read_text())
assert (ready['mode'],ready['stage'],ready['error'],ready['message'],ready['ready'],ready['length'])==(0,[5,1,0],-537,73,1,2079352)
assert ready['snapshot']==ready['card_buffer']
assert (ready['access_mode'],ready['confirmed'],ready['cancelled'],ready['showing_message'],ready['menu_state'])==(0,0,0,1,2)
# CardSave flags0x20 applies system-only0x98 and tails to mode1 stock prelude.
assert word(0x5f1fe50)==0x362800c1 and word(0x5f1fe58)==0x5280130c
assert word(0x5f1fe70)==0x52800020 and branch(0x5f1fe78)==0x5f1fcf0
# Only mode2 reaches the prelude loop; mode1 branches to the return epilogue.
assert word(0x5f1fdfc)==0x71000a7f and word(0x5f1fe00)==0x54000121
assert branch(0x5f1fdf8)==0x5f214e4
# System-file-check declares existing system structure bit9; in-slot state8 then selects overall8.
assert word(0x5f1c02c)==0x32170108
assert word(0x5f1b65c)==0x37480068 and word(0x5f1b668)==0x52800108
assert rela[0x9ce5a18+8*48+16]==0x5f1f480
# Authored system-only saveMain starts at substate2, bypassing ordinary makeSaveData at substate1.
assert word(0x5f1f8b0)==0x37380508 and word(0x5f1f8b4)==0x52800054
assert word(0x5f1f980)==0x52800054
switch=struct.unpack_from('<13H',native,0x430455e)
assert 0x5f1f4b4+switch[1]*4==0x5f1f580
assert 0x5f1f4b4+switch[2]*4==0x5f1f7b8
assert 0x5f1f4b4+switch[3]*4==0x5f1f7e0
assert branch(0x5f1f7e4)==0x5f1e630 and branch(0x5f1e644)==0x5f1f268
assert word(0x5f1f3bc)==0x37380168 # original system-only slot-copy bypass
assert branch(0x5f1e694)==0x5f210e4
# Original writer stores final MoveFile result at nativeglobal8660; getter tests exactly this byte.
assert word(0x5f21238)==0x5290cc0a and word(0x5f21240)==0x382a6928
assert word(0x61d47e0)==0x5290cc09 and word(0x61d47ec)==0x7100011f
assert branch(0x61d06c0)==0x5f215b4 and branch(0x61d06dc)==0x5f215b4
assert branch(0x5f215c8)==0x5f1ff68 and branch(0x5f215dc)==0x5f20068
assert word(0x5f20098)==0xf901cabf and word(0x5f20150)==0x12010d29
subprocess.run(['python','-X','utf8',str(ROOT/'analysis/research/check_extras_save_bp.py')],check=True)

header=(ROOT/'analysis/re4fix_extras_save.h').read_text()
assert not any(x in header for x in ('CardSysSave','ArmMainLoopUpdate','AutoSave(','while('))
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==18
extras_header=(ROOT/'analysis/re4fix_mobile_extras.h').read_text()
state=extras_header[extras_header.index('static unsigned extras_dirty,'):extras_header.index('static int extras_frontend(')]
prefix='#include <stdint.h>\n'+state+'unsigned long fixture_extras_save_code[]={'+','.join(a for a,*_ in checks)+'};\n'
prefix+=f'unsigned fixture_extras_save_count={len(checks)};\nvoid fixture_extras_save_image(unsigned char* p){{\n'
for address,*words in checks:
    values=tuple(int(x,16) for x in words)
    assert struct.unpack_from('<4I',native,int(address,16))==values,address
    for index,value in enumerate(words):prefix+=f'*(unsigned*)(p+{address}+{index*4})={value};\n'
for got,target in ((0xa361d30,0xa3f5e48),(0xa362e30,0xa50ae25),(0xa362e48,0xa4ff378)):
    prefix+=f'*(uintptr_t*)(p+{got})=(uintptr_t)p+{target};\n'
prefix+='}\n'
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
fixture=(ROOT/'analysis/research/test_extras_save.c').read_text()
with tempfile.TemporaryDirectory(prefix='re4-extras-save-') as temporary:
    directory=Path(temporary);source=directory/'save.c';source.write_text(prefix+fixture)
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(ROOT/'analysis'),'-c','-o',str(directory/'save-arm64.o'),str(source)],check=True)
    obj,exe=directory/'save.obj',directory/'save.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-DEXTRAS_SAVE_TEST','-O2','-Werror','-I',str(ROOT/'analysis'),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: 26 complete native body hashes; exact original Lobby/no-slots guards and snapshot ownership; nonblocking system-only native route, honest MoveFile result, ARM64 gates and actual host C fixture')
