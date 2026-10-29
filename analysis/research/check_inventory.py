"""Verify native Inventory pause ABI/gates and execute the actual request helper; no device access."""
import os
from pathlib import Path
import mmap
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_inventory.h'
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header.read_text(encoding='utf8'))
assert len(checks)==20
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        assert native[:6]==b'\x7fELF\x02\x01'
        offset=struct.unpack_from('<Q',native,0x20)[0]
        stride,count=struct.unpack_from('<HH',native,0x36)
        segments=[struct.unpack_from('<IIQQQQQQ',native,offset+i*stride) for i in range(count)]
        def native_offset(address,size):
            segment=next(s for s in segments if s[0]==1 and s[3]<=address and address+size<=s[3]+s[5])
            return segment[2]+address-segment[3]
        for address,first,second in checks:
            address=int(address,16)
            assert struct.unpack_from('<II',native,native_offset(address,8))==(int(first,16),int(second,16)),hex(address)
        for start,end,digest in [
            (0x633e52c,0x633e678,'3e1d145b4b6367d357343323a695f319aa6211271252c8e5dfc1df55155d1f1c'),
            (0x620db10,0x620dc24,'321ed313e2b6ed3d5ce2ca1e34af31344a7ab782794c93afa96d5ab4927d5857'),
            (0x620b93c,0x620bf8c,'00170a55bd1915a68dbe2804e2d47d7b1303beeef228b8ac278745af9e40254b'),
        ]:
            assert hashlib.sha256(native[native_offset(start,end-start):native_offset(end-1,1)+1]).hexdigest()==digest
        fields=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}',header.read_text().split('drag_fields[]={',1)[1].split('};',1)[0])
        assert len(fields)==7
        for at,offset in fields:assert struct.unpack_from('<I',native,native_offset(int(at,16)+0x24,4))[0]==int(offset,16)
        assert struct.unpack_from('<q',native,native_offset(0x9df6738,8))[0]==2
        assert struct.unpack_from('<III',native,native_offset(0x6333dd0,12))==(0x39000001,0x39000402,0xd65f03c0)
        assert struct.unpack_from('<II',native,native_offset(0x62b4a50,8))==(0x79400be0,0x9401f5fa)
        call=struct.unpack_from('<I',native,native_offset(0x62e5d58,4))[0]
        immediate=call&0x03ffffff
        if immediate&0x02000000:immediate-=0x04000000
        assert call>>26==0x25 and 0x62e5d58+immediate*4==0x620b93c
source_text=header.read_text()
assert 'touch_inventory_drag_tick' not in (root/'analysis/re4fix_flat_view.h').read_text()
assert source_text.count('gate_inventory_drag(item,chosen,parent,scale);')==1
assert 'inventory_drag_caller=(const void*)(base+0x62e5d5c);' in source_text
assert 'g_tgt_inventory_drag=base+0x620b94c;' in source_text
assert 'inventory_drag_dispatch(item,transform,parent,scale,__builtin_return_address(0));' in source_text
print('PASS: 20 native preimages, seven inventory/GrabState fields, original FRay/FPlane/SetTargetTransform bodies, exact UpdateActiveItem BL/LR and four-instruction gate; no post-hand target writer',flush=True)

clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=root/'analysis/research/test_inventory.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
with tempfile.TemporaryDirectory(prefix='re4-inventory-check-') as temporary:
    obj,exe=Path(temporary)/'inventory.obj',Path(temporary)/'inventory.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
