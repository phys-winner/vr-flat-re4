"""Validate native Check/message bindings and execute the production C input guards."""
from pathlib import Path
import hashlib
import os
import re
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
header=(ROOT/'analysis/re4fix_key_touch.h').read_text()
for address,a,b in re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header):
    assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16)),address
for descriptor,offset in ((0x9dcdb88,0x238),(0x9dcd3a8,0x360),(0x9dcd380,0x364)):
    assert struct.unpack_from('<I',native,descriptor+0x24)[0]==offset
assert struct.unpack_from('<I',native,0x8f82588)[0]==0x52803e04 # SceneComponent original size0x1f0.
assert struct.unpack_from('<I',native,0x8ce9440)[0]==0x52808604 # BoxComponent original size0x430.
callsite=0x62dc4fc
branch=struct.unpack_from('<I',native,callsite)[0]
assert branch>>26==0x25
immediate=branch&0x3ffffff
if immediate&0x2000000:immediate-=0x4000000
assert callsite+immediate*4==0x62dc2a4 and callsite+4==0x62dc500
assert all(word in header for word in ('"mov x0,x19\\n"','"mov x1,x30\\n"','"b key_touch_confirm_hook\\n"'))
assert '0x104' not in header and 'HexagonalEmblem' not in header
assert not any(word in header for word in ('g_cmd_until','PutInHand','UseItem','0x62f4470','0x62f8e88'))
assert native[0x423f561:0x423f561+17]==b'OnFinishMessages\0'
print('PASS: exact canonical native profile, nullable MessageVolume branch, Tick callsite/LR, generic parent-class scope')

loader=ROOT/'analysis/research/host_oodle_decode.py'
env={'__file__':str(loader),'__name__':'key_check_native_geometry'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),env)
u,base,buffers,call=(env[name] for name in ('u','base','buffers','call'))
from unicorn.arm64_const import UC_ARM64_REG_S0,UC_ARM64_REG_X8
actor,root,point,box,pawn,head,out=(buffers+i*0x1000 for i in range(7))
u.mem_write(actor,bytes(0x420));u.mem_write(root,bytes(0x1f0))
u.mem_write(actor+0x130,struct.pack('<Q',root))
u.mem_write(root+0x1d0,struct.pack('<3f',100,20,30))
u.mem_write(point,struct.pack('<3f',103,24,30))
call(0x62f50d4,[actor,0,point])
assert struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0)))[0]==25
u.mem_write(box,bytes(0x42c))
tf=struct.pack('<4f3f3f2f',0,0,0,1,100,20,30,1,1,1,0,0)
u.mem_write(box+0x1c0,tf);u.mem_write(box+0x420,struct.pack('<3f',2,2,2))
call(0x62f50d4,[actor,box,point])
assert struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0)))[0]==5
for scale,expected in ((-1,5),(0,4)):
    u.mem_write(box+0x1dc,struct.pack('<f',scale));call(0x62f50d4,[actor,box,point])
    assert struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0)))[0]==expected
u.mem_write(pawn,bytes(0x2e0));u.mem_write(pawn+0x2d8,struct.pack('<Q',head))
u.mem_write(head+0x1c0,tf);u.mem_write(out,b'\xa5'*48)
u.reg_write(UC_ARM64_REG_X8,out);call(0x6341aec,[pawn])
assert bytes(u.mem_read(out,48))==tf
print('PASS: complete original ARM64 GetDistSquared: NULL/root25, box5, mirrored5/zero-scale4; exact48-byte packed FTransform x8 return')

clang=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=ROOT/'analysis/research/test_key_touch.c'
with tempfile.TemporaryDirectory(prefix='re4-key-check-') as td:
    out=Path(td);obj=out/'guards.obj';exe=out/'guards.exe'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',
        '-DRE4_KEY_TOUCH_ASM_CHECK','-c',str(source),'-o',str(out/'arm64.o')],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c',str(source),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: complete Android ARM64 source/assembly compilation and executed production C guard fixtures')
print('LIMIT: host checks alone do not prove native gate passage; final normal-APK acceptance belongs to device receipts')
