"""Original native proof and actual C phone avatar leaf/installer self-check."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_phone_view.h').read_text(encoding='utf8')
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==12
for address,a,b in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16)),address
for lo,hi,digest in (
    (0x642c718,0x642c7d8,'3a4c955e3886ff8f6eea64c9c226525c765f542228f1518be77b65e77f111353'),
    (0x62a65e0,0x62a677c,'324ca13539726afa350dd83d4b3338e3e2f8e62b4950fd03b7ef83e1394536db'),
    (0x62af678,0x62af6ec,'a03afe797f321c3972993d5ac12ab80c684428de42b32d65dfe9bd6632e2af08'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
for metadata,offset in ((0x9dac9e0,0xba8),(0x9da8c10,0x220),(0x9da8be0,0x228),
                        (0x9da8bb0,0x230),(0xa0b7530,0x130),(0xa235ba8,0xc0)):
    assert struct.unpack_from('<I',native,metadata+0x24)[0]==offset,hex(metadata)
for metadata,offset in ((0xa269120,0x440),(0xa2668d8,0xf0)):
    assert struct.unpack_from('<I',native,metadata+0x24)[0]==offset,hex(metadata)
name_address=0x9df91a8+5*16
shoff=struct.unpack_from('<Q',native,40)[0];stride,count=struct.unpack_from('<HH',native,58)
name=None
for i in range(count):
    section=struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*stride)
    if section[1]!=4:continue
    for pos in range(section[4],section[4]+section[5],section[9]):
        address,kind,target=struct.unpack_from('<QQq',native,pos)
        if address==name_address:
            assert kind&0xffffffff==1027;name=native[target:native.index(b'\0',target)]
assert name==b'EPlayerPawn::Communicator'
assert struct.unpack_from('<q',native,name_address+8)[0]==5
assert 'patch_code(' not in header and 'SetHidden' not in header
assert 'visible(meshes[i],0,0)' in header and 'phone_type()!=5 || phone_active()!=pawn' in header
assert header.index('cutscene_avatar_component(primary,meshset)')<header.index('cw_get_mesh(pawn,0)')
print('PASS: four avatar/eight read-only probe native preimages, scalar TF48/bounds28 native getters, eight fields and Communicator5 enum; shared installed writer validated by dependencies',flush=True)

clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=root/'analysis/research/test_phone_view.c'
with tempfile.TemporaryDirectory(prefix='re4-phone-view-') as td:
    folder=Path(td);android=folder/'android.o';obj=folder/'host.obj';exe=folder/'host.exe'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-c','-o',str(android),str(source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)

cutscene=(root/'analysis/re4fix_cutscene_weapon.h').read_text(encoding='utf8')
main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
hook=cutscene.split('static void hook_cutscene_avatar(void* pawn)',1)[1].split('\n}',1)[0]
if 'phone_view_avatar_post(pawn)' in hook and 'install_phone_view(base);' in main:
    assert hook.index('gate_cutscene_avatar(pawn)')<hook.index('phone_view_avatar_post(pawn)')
    assert main.index('install_touch_cutscene_weapon(base);')<main.index('install_phone_view(base);')
    print('PASS: shared native writer first, phone post-call and installer integration ordering',flush=True)
else:
    print('READY: helper/native/C checks pass; root post-writer/installer integration pending',flush=True)
