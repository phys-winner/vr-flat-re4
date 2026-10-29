"""Original UI leaf binding proof plus actual C visibility/ownership checks."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_menu_visual.h'
text=header.read_text(encoding='utf-8')
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',text)
assert len(checks)==21
for address,first,second in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
field_text=text.split('} fields[] = {',1)[1].split('};',1)[0]
fields=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',field_text)
assert len(fields)==9
for params,offset in fields:
    assert struct.unpack_from('<I',native,int(params,16)+36)[0]==int(offset,16),params
ranges=(
    (0x6310124,0x63102dc,'b82db0cd3bb6b831a58203d188878f468c6cf5fa4b966b9d058a9381d3dfc787'),
    (0x6311258,0x6311330,'b85f6f87feb8003a48ff9b3e7836396697c47f0c4a799d0001a69a03475bddf0'),
    (0x6310630,0x63106b4,'dc3f2bc2133c12e0675678f9ac90cdd1bdbce18dce7cd9b19def725334a00cf2'),
    (0x62af678,0x62af6ec,'a03afe797f321c3972993d5ac12ab80c684428de42b32d65dfe9bd6632e2af08'),
    (0x62a2370,0x62a237c,'11ff97a0fd26a27cf5a63cf39c6e2520c31ada7cac83a4877ecdec30f82f5171'),
    (0x62a3a20,0x62a3a40,'3b21b121839acea40b5c95d0bdb9de69103938565781bd53c4266ccf9e6dd7da'),
    (0x83ad924,0x83ad964,'7b863ac45f6397918de794ac234757bec13177c682fc501e111dfd9b3b5041fd'),
)
for start,end,digest in ranges:
    assert hashlib.sha256(native[start:end]).hexdigest()==digest,hex(start)
# Independent original enum names prove the helper's exact contexts.
shoff=struct.unpack_from('<Q',native,40)[0]
shsize,shnum=struct.unpack_from('<HH',native,58)
names={}
for i in range(shnum):
    section=struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*shsize)
    if section[1]!=4:continue
    for offset in range(section[4],section[4]+section[5],section[9]):
        address,info,addend=struct.unpack_from('<QQq',native,offset)
        if address in (0x9df91a8+2*16,0x9df91a8+4*16,0x9df91a8+7*16):
            assert info&0xffffffff==1027
            names[address]=native[addend:native.index(b'\0',addend)]
assert names=={0x9df91a8+2*16:b'EPlayerPawn::Pause',0x9df91a8+4*16:b'EPlayerPawn::Menu',
              0x9df91a8+7*16:b'EPlayerPawn::InWorldMenu'}
assert struct.unpack_from('<q',native,0x9df9220)[0]==7
print('PASS: 21 native preimages, 9 reflected fields, 7 native binding/input/visibility ranges and Pause2/Menu4/InWorldMenu7 enum proof',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
source=root/'analysis/research/test_menu_visual.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
print('PASS: Android aarch64 actual menu visual helper/test syntax with -Werror',flush=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-menu-visual-') as td:
    obj,exe=Path(td)/'visual.obj',Path(td)/'visual.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
