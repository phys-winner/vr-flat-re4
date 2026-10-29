"""Execute the production memory patcher with 4/16/64 KiB page contracts."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
core = (root / 'analysis/re4fix.c').read_text(encoding='utf-8')
patcher = core[core.index('static int protect_mem('):core.index('\nunsigned long g_ue4_base_cached;')]
assert core.count('mprotect(') == 1, 'All production protections must use the runtime page size'
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define _SC_PAGESIZE 30
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
static long pages;
static unsigned calls;
static int result, seen_protection;
static uintptr_t seen_start;
static size_t seen_size;
static long sysconf(int name) { assert(name == _SC_PAGESIZE); return pages; }
static int mprotect(void* start, size_t size, int protection) {
    ++calls; seen_start=(uintptr_t)start; seen_size=size; seen_protection=protection;
    assert(!(seen_start % pages) && !(size % pages)); return result;
}
'''
suffix = r'''
int main(void) {
    for (pages=4096; pages<=65536; pages*=4) {
        for (uintptr_t offset=0; offset<(uintptr_t)pages; offset++) {
            size_t sizes[]={4,8,16,(size_t)pages+1};
            for (unsigned i=0;i<4;i++) {
                uintptr_t start=0x100000+offset, end=start+sizes[i];
                calls=0;
                assert(!protect_mem((void*)start,sizes[i],PROT_READ|PROT_WRITE|PROT_EXEC));
                assert(calls==1 && seen_start==0x100000 && seen_protection==7);
                assert(seen_start+seen_size>=end && seen_start+seen_size-end<(uintptr_t)pages);
            }
        }
    }
    long invalid[]={-1,0,6000};
    for(unsigned i=0;i<3;i++) { pages=invalid[i];calls=0;assert(protect_mem((void*)0x100000,16,7)==-1 && !calls); }
    pages=16384;calls=0;
    assert(protect_mem((void*)0x100001,0,7)==-1 && !calls);
    assert(protect_mem((void*)0x100001,SIZE_MAX,7)==-1 && !calls);
    unsigned char target[32], original[32];
    memset(target,0xa5,sizeof target);memcpy(original,target,sizeof original);
    void* hook=(void*)(uintptr_t)0x123456789abcdef0ULL;
    result=-1;
    assert(patch_code(target,hook)==-1 && !memcmp(target,original,sizeof target));
    result=0;
    assert(!patch_code(target,hook));
    unsigned instructions[2]={0x58000051,0xd61f0220};
    assert(!memcmp(target,instructions,8) && !memcmp(target+8,&hook,8));
    assert(!memcmp(target+16,original+16,16));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='re4-pages-') as temporary:
    work = Path(temporary)
    source, obj, exe = work/'pages.c', work/'pages.obj', work/'pages.exe'
    source.write_text(prefix + patcher + suffix, encoding='utf-8')
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
        '-isystem', str(vc/'include'), '-isystem', str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c', str(source), '-o', str(obj)], check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
        'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}', f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: production patcher, every 4/16/64 KiB page offset, crossing writes, exact trampoline, protection failure/invalid-size rejection; all protection callers use the shared runtime page contract')
