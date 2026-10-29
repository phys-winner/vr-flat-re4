"""Verify original native descriptor handle implementation and actual new bounded asset hooks."""
import os
from pathlib import Path
import hashlib
import json
import mmap
import re
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
LLVM=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
VC=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK=Path(r'C:\Program Files (x86)\Windows Kits\10')
with (ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as file,mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as data:
 phoff=struct.unpack_from('<Q',data,32)[0]; phsize,count=struct.unpack_from('<HH',data,54)
 segments=[struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize) for i in range(count)]
 def read(address,size):
  s=next(s for s in segments if s[0]==1 and s[3]<=address and address+size<=s[3]+s[5]);offset=s[2]+address-s[3];return data[offset:offset+size]
 pairs={0x6572d40:(0xd10643ff,0xf900a3fc,0xa9155ff8,0xa91657f6),0x6575508:(0xd10483ff,0xf9006bfc,0xa90e5ff8,0xa90f57f6),
        0x54b4110:(0xa9bd57f6,0xa9014ff4,0xa9027bfd,0x910083fd),0x657598c:(0xd10183ff,0xa9025ff8,0xa90357f6,0xa9044ff4),0x63892b4:(0x2a1f03e1,0x140a330e),
        0x657201c:(0xf81c0ff7,0xa90157f6,0xa9024ff4,0xa9037bfd)}
 for address,words in pairs.items():assert read(address,len(words)*4)==struct.pack('<'+'I'*len(words),*words),hex(address)
 # The virtual OpenRead wrapper moves caller AllowWrite to w3 and inserts AllowLocal=false in w2.
 assert read(0x6571b1c,12)==struct.pack('<III',0x12000043,0x2a1f03e2,0x14000e79)
 # The real MountOBB consumer sets AllowLocal=true/AllowWrite=false and calls this exact hook.
 assert read(0x6572f04,8)==struct.pack('<II',0x52800022,0x2a1f03e3)
 instruction=struct.unpack('<I',read(0x6572f18,4))[0]
 assert instruction==0x9400097c and 0x6572f18+((instruction&0x3ffffff)<<2)==0x6575508
 # The local native branch opens O_RDONLY; AllowLocal is not a write request.
 assert read(0x65756cc,4)==struct.pack('<I',0x2a1f03e1)
 # Raw-FD native constructor stores a null AAsset and owns the descriptor; all offsets remain 64-bit.
 assert read(0x6572098,8)==struct.pack('<II',0xf9000a9f,0xb9001a95)
 assert read(0x6573118,12)==struct.pack('<III',0xf9401408,0xf9400c09,0xcb090100)
 assert read(0x6573128,24)==struct.pack('<IIIIII',0xf9400c09,0xaa0003e8,0x52800020,0x8b010129,0xf9001509,0xd65f03c0)
 proofs={f'{address:x}':{'bytes':size,'sha256':hashlib.sha256(read(address,size)).hexdigest()} for address,size in
         ((0x6575508,0x354),(0x657598c,0x118),(0x657201c,0xe8),(0x657315c,0x98),(0x6573128,0x18),(0x6572ff0,0x90),(0x54b4110,0xdc),(0x6572ee8,0x108),(0x6571b1c,12))}
 (ROOT/'analysis/research/packaged_obb_native.json').write_text(json.dumps(proofs,indent=2)+'\n')
with tempfile.TemporaryDirectory(prefix='obb-check-') as temporary:
 work=Path(temporary);source=work/'native.c';obj=work/'check.obj';exe=work/'check.exe'
 source.write_text('#include <stdio.h>\n#include <string.h>\n#include <dlfcn.h>\n#include <unistd.h>\nstatic int patch_code(void*a,void*b){return 0;}\n#define LOGF(...) ((void)0)\n#define LOGI(...) ((void)0)\n#include "'+(ROOT/'analysis/re4fix_packaged_obb.h').as_posix()+'"\nvoid install(unsigned long b){install_packaged_obb(b);}\n')
 subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-c',str(source),'-o',str(work/'native.o')],check=True)
 assembly=subprocess.check_output([str(LLVM/'llvm-objdump.exe'),'-d',str(work/'native.o')],text=True)
 for name,address in (('gate_mobile_obb_exists',0x6572d40),('gate_mobile_obb_read',0x6575508)):
  gate=assembly.split('<'+name+'>:',1)[1];words=tuple(int(word,16) for word in re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]{8})',gate,re.M)[:4]);assert words==pairs[address]
 subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-unused-function','-Wno-int-to-void-pointer-cast','-Wno-int-to-pointer-cast','-isystem',str(VC/'include'),'-isystem',str(SDK/'Include/10.0.26100.0/ucrt'),'-c',str(ROOT/'analysis/research/test_packaged_obb.c'),'-o',str(obj)],check=True)
 subprocess.run([str(VC/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{VC/"lib/x64"}',f'/LIBPATH:{SDK/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{SDK/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: original native five-function preimages/eight-body evidence, genuine MountOBB AllowLocal/AllowWrite ABI and descriptor ownership, both naked gates and Android23 Werror')
