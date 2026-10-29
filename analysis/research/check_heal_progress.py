"""Verify the original consumable use ABI and execute its production snapshot guards."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_heal_progress.h'
text=header.read_text(encoding='utf8')
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo,hi,digest in (
    (0x6277ffc,0x6278340,'e9d6fa63f0ee857ec403305327c637494cce8be2ee8768b8fe41e7201587c157'),
    (0x6278e78,0x6278e84,'bc603558e4b814bf33ec55d79fce6d7ab47352e27d42c2d688b23fc1a84bb900'),
    (0x62e9178,0x62e9190,'33963f04a33dae6f2d66301b02a6013cadbe0f4832449adfc414f97076243fd2'),
    (0x641231c,0x64123dc,'97920bc612720889f4f871208c25770d3493b78fc1943be509a8b08c24dfa078'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',text)
assert len(checks)==10
for addr,a,b in checks:assert struct.unpack_from('<II',native,int(addr,16))==(int(a,16),int(b,16)),addr
for addr,value in ((0x9d99168,0x4f8),(0x9d99110,0x5a0)):
    assert struct.unpack_from('<I',native,addr+36)[0]==value
assert struct.unpack_from('<I',native,0x6278e80)[0]==0x1401c0be # Tail GetFraction returns float in s0.
assert struct.unpack_from('<III',native,0x62e9178)==(0xbd401000,0xbd400001,0x1e211800)
for caller,target in ((0x64a5c48,0x6477808),(0x6477818,0x6424cec),(0x6331e2c,0x62a2f74)):
    word=struct.unpack_from('<I',native,caller)[0];assert word&0xfc000000==0x94000000
    immediate=word&0x3ffffff
    if immediate&0x2000000:immediate-=0x4000000
    assert caller+4*immediate==target
print('PASS: four original consumable ranges, ten opcode guards, timer/inventory-hand fields, native float return ABI and PauseHand -> MenuHand -> GameHand / PausePawn -> GamePawn inheritance',flush=True)

weapon=(root/'analysis/re4fix_weapon.h').read_text(encoding='utf8')
guard=weapon[weapon.index('static int tw_typed('):weapon.index('static int tw_binding(')]
source=(root/'analysis/research/test_heal_progress.c').read_text(encoding='utf8').replace('/* TYPED_GUARD */',guard).replace('HEAL_HEADER',header.as_posix())
fixture=''
for addr,a,b in checks:fixture+=f'*(unsigned*)(image+{addr})={a};*(unsigned*)(image+{addr}+4)={b};\n'
for addr,value in ((0x9d99168,0x4f8),(0x9d99110,0x5a0)):fixture+=f'*(unsigned*)(image+{addr}+36)={value};\n'
fixture+='tw_consumable=(void* (*)(const void*))(uintptr_t)(base+0x62ac400);phone_player=(void* (*)(const void*))(uintptr_t)(base+0x633d030);phone_active=(void* (*)(void))(uintptr_t)(base+0x63380dc);phone_type=(unsigned (*)(void))(uintptr_t)(base+0x6338aec);\n'
fixture+='qs_native_hand=(void* (*)(const void*,unsigned char))(uintptr_t)(base+0x62a8fc0);\n'
fixture+='heal_fraction=0;install_touch_heal_progress(base);assert((uintptr_t)heal_fraction==base+0x6278e78);\n'
for addr,a,b in checks:
    fixture+=f'*(unsigned*)(image+{addr})^=1;heal_fraction=0;install_touch_heal_progress(base);assert(!heal_fraction);*(unsigned*)(image+{addr})^=1;\n'
fixture+='*(unsigned*)(image+0x9d99168+36)^=1;heal_fraction=0;install_touch_heal_progress(base);assert(!heal_fraction);*(unsigned*)(image+0x9d99168+36)^=1;tw_consumable=consumable;install_touch_heal_progress(base);assert(!heal_fraction);\n'
source=source.replace('/* NATIVE_FIXTURE */',fixture)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-heal-progress-') as temporary:
    directory=Path(temporary);test=directory/'heal.c';test.write_text(source,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
    obj,exe=directory/'heal.obj',directory/'heal.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),'-c','-o',str(obj),str(test)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
