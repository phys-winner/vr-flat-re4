"""Verify exact original decoder ABI and execute the actual bounded request parser."""
import os
from pathlib import Path
import hashlib
import json
import mmap
import re
import struct
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
LLVM = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
VC = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK = Path(r'C:\Program Files (x86)\Windows Kits\10')
header = ROOT / 'analysis/research/localization_decode.h'
with (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream, mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as image:
    phoff = struct.unpack_from('<Q',image,32)[0]
    phsize, phcount = struct.unpack_from('<HH',image,54)
    segments = [struct.unpack_from('<IIQQQQQQ',image,phoff+i*phsize) for i in range(phcount)]
    def read(address,size):
        segment = next(s for s in segments if s[0]==1 and s[3]<=address and address+size<=s[3]+s[5])
        offset = segment[2]+address-segment[3]
        return image[offset:offset+size]
    code={0x96b64e0:(0xa9ba6ffc,0xa90167fa,0xa9025ff8,0xa90357f6,0xa9044ff4,0xa9057bfd,0x910143fd,0xd10183ff),
          0x96b4b88:(0xa9bf7bfd,0x910003fd,0xaa1f03e2,0x94000004,0x11004000,0xa8c17bfd,0xd65f03c0),
          0x96b6f6c:(0xd10103ff,0xa9024ff4,0xa9037bfd,0x9100c3fd)}
    for address,words in code.items(): assert read(address,len(words)*4)==struct.pack('<'+'I'*len(words),*words)
    proofs={f'{address:x}':{'bytes':size,'sha256':hashlib.sha256(read(address,size)).hexdigest()} for address,size in
            ((0x96b64e0,0x6e0),(0x96b4b88,0x1c),(0x96b6f6c,0xdc),(0x5c02b0c,0x164))}
    # Original wrapper uses x0..x7 plus six stack arguments; fuzz/CRC=1 and thread phase=3.
    wrapper=subprocess.check_output([str(LLVM/'llvm-objdump.exe'),'-d','--no-show-raw-insn','--start-address=0x5c02b80','--stop-address=0x5c02bc0',str(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so')],text=True)
    for instruction in ('stp\tx0, x24, [sp, #0x18]','mov\tw4, #0x1','mov\tw5, #0x1','stp\txzr, xzr, [sp]','str\tw8, [sp, #0x28]','str\txzr, [sp, #0x10]','OodleLZ_Decompress'):
        assert instruction in wrapper,instruction
(ROOT/'analysis/research/localization_originals/native_decoder_proof.json').write_text(json.dumps(proofs,indent=2)+'\n')
with tempfile.TemporaryDirectory(prefix='locdecode-check-') as temporary:
    work=Path(temporary)
    source=work/'native.c'
    source.write_text('#include <stdio.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include <fcntl.h>\n#include <string.h>\n#define LOGF(...) ((void)0)\n#include "'+header.as_posix()+'"\nint call(unsigned long base){return localization_decode(base);}\n')
    subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
    source.write_text('''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOCDECODE_TEST
#include "'''+header.as_posix()+'''"
int main(int argc,char**argv){
 assert(argc==2); FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
 unsigned char* data=malloc(size);assert(data);assert(fread(data,1,size,f)==(size_t)size);fclose(f);
 LocDecodeJob jobs[5]; assert(loc_decode_parse(data,size,jobs));
 for(unsigned i=0;i<5;i++){assert(jobs[i].locale==i&&jobs[i].raw<=1048576);unsigned sum=0;for(unsigned b=0;b<jobs[i].count;b++)sum+=jobs[i].blocks[b].raw;assert(sum==jobs[i].raw);}
 for(unsigned length=0;length<(unsigned)size;length+=17)assert(!loc_decode_parse(data,length,jobs));
 unsigned offsets[]={8,12,16,20,24,28};for(unsigned i=0;i<6;i++){unsigned old;memcpy(&old,data+offsets[i],4);unsigned bad=0xffffffffu;memcpy(data+offsets[i],&bad,4);assert(!loc_decode_parse(data,size,jobs));memcpy(data+offsets[i],&old,4);}
 data[0]^=1;assert(!loc_decode_parse(data,size,jobs));data[0]^=1;assert(loc_decode_parse(data,size,jobs));free(data);
 puts("PASS: actual five-locale request parser, all bounded blocks/raw lengths, every17-byte truncation and malformed count/locale/size fields");return 0;
}
''')
    obj,exe=work/'check.obj',work/'check.exe'
    subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-D_CRT_SECURE_NO_WARNINGS','-isystem',str(VC/'include'),'-isystem',str(SDK/'Include/10.0.26100.0/ucrt'),'-c',str(source),'-o',str(obj)],check=True)
    subprocess.run([str(VC/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{VC / "lib/x64"}',f'/LIBPATH:{SDK / "Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{SDK / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe),str(ROOT/'analysis/research/localization_originals/mobile_locres_request.bin')],check=True)
print('PASS: original Oodle native instruction/body evidence, original14-argument ABI, Android23 Werror; hardware decode is separate')
