"""Original native QTE proof and execution of the actual input adapter."""
from pathlib import Path
import hashlib
import os
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
header=(root/'analysis/re4fix_qte.h').read_text(encoding='utf8')
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==12
for address,x,y in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(x,16),int(y,16))
assert struct.unpack_from('<I',native,0x9dbf730+36)[0]==0x3dc
assert struct.unpack_from('<I',native,0x9d92378+40)[0]==17
for i in range(17):
    assert struct.unpack_from('<q',native,0x9d92268+16*i+8)[0]==i
motions=['SidesOut','BothDown','BothUp','DominantUp','DominantDown','DominantRight','DominantLeft','AcrossRight','AcrossLeft','UpperV','LowerV','Button','Count']
assert struct.unpack_from('<I',native,0x9ded130+40)[0]==13
motion_relocs={}
shoff=struct.unpack_from('<Q',native,40)[0];shsize,shnum=struct.unpack_from('<HH',native,58)
for i in range(shnum):
    section=struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*shsize)
    if section[1]!=4:continue
    for p in range(section[4],section[4]+section[5],section[9]):
        dst,info,add=struct.unpack_from('<QQq',native,p)
        if 0x9ded060<=dst<0x9ded130:
            assert info&0xffffffff==1027;motion_relocs[dst]=add
for i,name in enumerate(motions):
    address=0x9ded060+16*i;p=motion_relocs[address]
    assert struct.unpack_from('<q',native,address+8)[0]==i
    assert native[p:native.index(b'\0',p)]==('EMotionActionType::'+name).encode()
# The entire authored mapping/input implementation is checked by the existing proof.
subprocess.run([os.sys.executable,'-I',str(root/'analysis/research/check_action_command_native.py')],check=True)
assert not any(name in header for name in ['OnActivateActionButton(','AddToInventory(','SetSlideCocked(','SetMotionType('])

def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

knife=function((root/'analysis/re4fix_input.h').read_text(encoding='utf8'),'static float knife_cut_offset(long elapsed)')
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
kit=sorted((sdk/'Include').iterdir())[-1].name
with tempfile.TemporaryDirectory(prefix='re4-qte-') as temporary:
    folder=Path(temporary)
    curve=folder/'curve.h';curve.write_text(knife,encoding='ascii')
    fixture=folder/'native.h'
    generated=['static void fixture_installer_tests(void) {',
        'unsigned char* b=(unsigned char*)calloc(1,0xa000000);assert(b);']
    for address,x,y in checks:
        generated.append(f'*(unsigned*)(b+{address})={x};*(unsigned*)(b+{int(address,16)+4})={y};')
    names=['Invalid','A','B','X','Y','LB','RB','A_B','LB_RB','Rotate','A_Rapid','B_Rapid','Stick_A','Stick_Up','Waggle','LB_or_RB','HandMotion']
    for i,name in enumerate(names):
        generated.append(f'*(const char**)(b+{0x9d92268+16*i})="EVR4CommandButton::{name}";*(long long*)(b+{0x9d92268+16*i+8})={i};')
    for i,name in enumerate(motions):
        generated.append(f'*(const char**)(b+{0x9ded060+16*i})="EMotionActionType::{name}";*(long long*)(b+{0x9ded060+16*i+8})={i};')
    generated.append('*(unsigned*)(b+0x9ded130+40)=13;')
    generated+=['*(unsigned*)(b+0x9d92378+40)=17;*(unsigned*)(b+0x9dbf730+36)=0x3dc;',
        'qt_ready=0;install_touch_qte((unsigned long)b);assert(qt_ready);']
    for address,_,_ in checks:
        for delta in (0,4):
            a=int(address,16)+delta
            generated.append(f'*(unsigned*)(b+{a})^=1;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);*(unsigned*)(b+{a})^=1;')
    generated+=['*(unsigned*)(b+0x9dbf730+36)^=1;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);*(unsigned*)(b+0x9dbf730+36)^=1;',
        '*(unsigned*)(b+0x9d92378+40)=16;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);*(unsigned*)(b+0x9d92378+40)=17;',
        '*(unsigned*)(b+0x9ded130+40)=12;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);*(unsigned*)(b+0x9ded130+40)=13;',
        '*(long long*)(b+0x9ded060+8)=1;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);*(long long*)(b+0x9ded060+8)=0;',
        '*(long long*)(b+0x9d92268+8)=1;qt_ready=0;install_touch_qte((unsigned long)b);assert(!qt_ready);',
        'free(b);','}']
    fixture.write_text('\n'.join(generated).replace('(unsigned long)b','(uintptr_t)b'),encoding='ascii')
    source=root/'analysis/research/test_qte.c'
    macros=[f'-DQTE_KNIFE_FIXTURE="{curve.as_posix()}"',f'-DQTE_NATIVE_FIXTURE="{fixture.as_posix()}"']
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-fsyntax-only','-Werror',*macros,str(source)],check=True)
    exe=folder/'qte.exe'
    # The production Android ABI uses LP64; long must remain64bit in the host fixture.
    host=folder/'test.c'
    text=source.read_text(encoding='utf8').replace('#include "../re4fix_qte.h"',f'#include "{(root/"analysis/re4fix_qte.h").as_posix()}"')
    host.write_text(text,encoding='utf8')
    adapted=folder/'qte_host.h'
    adapted.write_text(header.replace('unsigned long base','uintptr_t base'),encoding='utf8')
    host.write_text(text.replace((root/'analysis/re4fix_qte.h').as_posix(),adapted.as_posix()),encoding='utf8')
    obj=folder/'qte.obj'
    command=[str(clang),'--target=x86_64-pc-windows-msvc',
        '-O2','-Werror',*macros,str(host),'-c','-o',str(obj),f'-I{vc/"include"}',
        *[f'-I{sdk/"Include"/kit/d}' for d in ['ucrt','shared','um']],
    ]
    subprocess.run(command,check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib"/kit/"ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib"/kit/"um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: 12 native instruction pairs, metadata/enum and actual Android/host QTE adapter checks')
