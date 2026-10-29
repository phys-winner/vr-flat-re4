"""Original movement/damage/scope contract and actual guarded comfort-hook execution."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_vr_comfort.h').read_text(encoding='utf8')
for lo,hi,digest in (
    (0x6345d1c,0x6345e10,'4810b816f64f5d17f1cde6043c56e2e71d2854bfa5e27035e66695cb3665bb77'),
    (0x6219aac,0x6219ae0,'3777236b6aebfd2bb1bdac18559877cafd8adf851b07ca4102e2f3b8b9f904f7'),
    (0x622ecb0,0x622ee04,'23799afb2a254576b0ca077362d518c0fe717a955eaa3a925f87ff156d9af019'),
    (0x64b7218,0x64b72d8,'34f93265224fba0f2ce57281cf3b60249950ec886f74e4fc5edb8c9ad88c357c'),
    (0x6340c0c,0x6340c24,'ae8db80e789706518f0208b1d75860bd9bde2ba43713f4ac1f03ea04f60cd1b6'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==11
for address,a,b in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16)),address
fields=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',header.split('} fields[] = {')[1].split('};')[0])
assert len(fields)==5
rela={a:c for a,t,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
names=['DamageWipeValue','CurrentMovementWipeValue','DamageVignetteScale',
       'MovementScreenVignetteMesh','MovementVignetteStrength']
for (address,offset),name in zip(fields,names):
    address=int(address,16)
    assert struct.unpack_from('<I',native,address+36)[0]==int(offset,16)
    p=rela[address];assert native[p:native.index(b'\0',p)].decode()==name
assert rela[0x9d3cfd8+16+0x808]==0x6345d1c
assert rela[0x9cffa48+16+0x808]==0x6219aac
def call_target(address):
    op=struct.unpack_from('<I',native,address)[0]
    assert op&0xfc000000==0x94000000
    imm=op&0x3ffffff
    if imm&0x2000000:imm-=0x4000000
    return address+4*imm
assert call_target(0x6219ac0)==0x6345d1c
assert call_target(0x6219ac8)==0x62196e8
# Original output is float/float/bool, disabled values0/1; scope override follows base.
assert struct.unpack_from('<III',native,0x6345df0)==(0x52a7f008,0xb90002df,0xb9000288)
assert struct.unpack_from('<I',native,0x6345dfc)[0]==0x3900027f
# CameraCover independently adds the reflected damage value to movement alpha.
assert struct.unpack_from('<I',native,0x622ed30)[0]==0x1e212802
assert struct.unpack_from('<I',native,0x622ed6c)[0]==0xbd424a64
assert header.index('gate_comfort_vignette(pawn,alpha,scale,scope);')<header.index('*alpha=0.0f;')
assert '*scope=' not in header
assert not any(x in header for x in ('SetVisibility','SetTimeScales','StopFade','SetParametersAlpha'))
inst=[int(x,16) for x in re.findall(r'\.inst (0x[0-9a-f]+)',header)]
assert inst==list(struct.unpack_from('<4I',native,0x6345d1c))
print('PASS: 5 native ranges, 11 opcode pairs, 5 named reflected fields, real virtual/base/scope calls and damage producer separation',flush=True)

core=(root/'analysis/re4fix_cinema.h').read_text(encoding='utf8')
guard=core[core.index('static int cinema_object('):core.index('static int cinema_on_game_thread(')]
fixture=(root/'analysis/research/test_vr_comfort.c').read_text(encoding='utf8')
prefix='#include <string.h>\n'+guard
prefix+='unsigned long fixture_code_addresses[]={'+','.join(a for a,_,_ in checks)+'};\n'
prefix+='unsigned fixture_code_count='+str(len(checks))+';\n'
prefix+='unsigned long fixture_field_addresses[]={'+','.join(a for a,_ in fields)+'};\n'
prefix+='unsigned fixture_field_count='+str(len(fields))+';\n'
prefix+='void fixture_image(unsigned char* p) {\n'
for a,x,y in checks:
    prefix+=f'*(unsigned*)(p+{a})={x};*(unsigned*)(p+{a}+4)={y};\n'
for a,x in fields:prefix+=f'*(unsigned*)(p+{a}+36)={x};\n'
prefix+='}\n'
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-comfort-') as td:
    td=Path(td);source=td/'comfort.c';source.write_text(prefix+fixture,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',
                    '-I',str(root/'analysis'),'-DTEST_REAL_GATE','-fsyntax-only',str(source)],check=True)
    obj,exe=td/'comfort.obj',td/'comfort.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-I',str(root/'analysis'),'-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),
        '-isystem',str(sdk/'Include/10.0.26100.0/shared'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: production ARM64 gate syntax, actual shared type guard and host hook/installer; 22 code +5 metadata corruption refusals',flush=True)
