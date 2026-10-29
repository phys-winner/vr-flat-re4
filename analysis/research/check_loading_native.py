"""Check original shader ABI and execute production loading helpers with a mocked ARM gate."""
import os
from pathlib import Path
import mmap
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NDK = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64'))
LLVM = NDK / 'bin'
VC = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK = Path(r'C:\Program Files (x86)\Windows Kits\10')
header = (ROOT / 'analysis/re4fix_loading.h').read_text()
symbols = subprocess.check_output([str(LLVM/'llvm-nm.exe'), '-D', '-C', str(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so')], text=True)
for name, address in (
    ('UVR4Utils::SetShaderCacheModeFast()', 0x6381ef0),
    ('UVR4Utils::SetShaderCacheModeBackground()', 0x6381f08),
    ('UVR4Utils::GetNumShaderCompilesRemaining()', 0x6381f3c),
    ('UVR4Utils::IsShaderCacheReady()', 0x6381f20),
    ('FShaderPipelineCache::NumPrecompilesRemaining()', 0x6fb2d64),
    ('FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode)', 0x6fb1e64),
    ('FShaderPipelineCache::ResumeBatching()', 0x6fb1f84),
):
    assert re.search(r'^0*' + f'{address:x}' + r' T ' + re.escape(name) + r'$', symbols, re.M), name

with (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
    phoff = struct.unpack_from('<Q', data, 32)[0]
    step, count = struct.unpack_from('<HH', data, 54)
    segments = [struct.unpack_from('<IIQQQQQQ', data, phoff+i*step) for i in range(count)]
    def words(address, count):
        segment = next(s for s in segments if s[0] == 1 and s[3] <= address and address+count*4 <= s[3]+s[5])
        return struct.unpack_from('<'+'I'*count, data, segment[2]+address-segment[3])
    remaining = words(0x6fb2d64, 4)
    fast = words(0x6381ef0, 6)
    assert remaining == (0xfc1b0fe8, 0xa9015ff8, 0xa90257f6, 0xa9034ff4)
    assert fast == (0xa9bf7bfd, 0x910003fd, 0x52800020, 0x9430bfda, 0xa8c17bfd, 0x1430c020)
    for label, expected in (('remaining', remaining), ('fast', fast)):
        match = re.search(r'const unsigned '+label+r'\[\d+\]=\{([^}]+)\}', header)
        assert match and tuple(int(w, 16) for w in match[1].split(',')) == expected
    def branch(address, opcode):
        assert opcode & 0x7c000000 == 0x14000000
        offset = opcode & 0x3ffffff
        return address+4*(offset-0x4000000 if offset & 0x2000000 else offset)
    assert branch(0x6381efc, fast[3]) == 0x6fb1e64
    assert branch(0x6381f04, fast[5]) == 0x6fb1f84
    background = words(0x6381f08, 6)
    assert background[2] == 0x2a1f03e0
    assert branch(0x6381f14, background[3]) == 0x6fb1e64
    assert branch(0x6381f1c, background[5]) == 0x6fb1f84
    assert branch(0x6381f3c, words(0x6381f3c, 1)[0]) == 0x6fb2d64
    assert words(0x6fb1e80, 4) == (0x7100041f, 0x540001e0, 0x7100081f, 0x540003a1)
    # Both original fast and precompile defaults compile 50 PSOs; only fast has a 16 ms adaptive budget.
    for address, opcode in ((0x6fb8f84, 0x52800642), (0x6fb8ff0, 0x52800642),
                            (0x6fb90cc, 0x1e261000), (0x6fb9134, 0x1e2703e0)):
        assert words(address, 1)[0] == opcode, hex(address)

gate = '''__attribute__((naked)) static int gate_loading_remaining(void) {
    asm volatile(".inst 0xfc1b0fe8\\n" ".inst 0xa9015ff8\\n" ".inst 0xa90257f6\\n" ".inst 0xa9034ff4\\n"
        "adrp x16, :got:g_tgt_loading_remaining\\n" "ldr x16, [x16, :got_lo12:g_tgt_loading_remaining]\\n" "ldr x16, [x16]\\n" "br x16\\n");
}'''
assert header.count(gate) == 1
mocked = header.replace(gate, 'static int gate_loading_remaining(void) { return mock_remaining; }')
prefix = '''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>
static int mock_remaining,thunk_calls,last_stage,mode_calls,last_mode,resume_calls;
static int mobile_android_call_thunk(const char* name,int first,int stage,int last) {
    assert(!strcmp(name,"AndroidThunkJava_MobileLoading") && first==-1 && last==0);
    ++thunk_calls;last_stage=stage;return 1;
}
static int patch_code(void* a,void* b){(void)a;(void)b;return 0;}
#define LOGF(...) ((void)0)
'''
tests = '''
static void mode(int value){++mode_calls;last_mode=value;}
static void resume(void){++resume_calls;}
static void state(int stage,int remaining) {
    unsigned long long value=(unsigned long long)Java_com_epicgames_ue4_MobileLoading_nativeState(0,0);
    assert((unsigned)(value>>32)==(unsigned)stage && (int)(unsigned)value==remaining);
}
int main(void) {
    state(2,-1);loading_batch_mode=mode;loading_resume=resume;
    loading_map_begin("/Game/Levels/SplashScreenMap");state(2,-1);
    assert(thunk_calls==1 && last_stage==2);
    loading_map_end("/Game/Levels/SplashScreenMap",1);assert(g_loading_startup && !g_loading_present_wait);
    hook_loading_fast();assert(mode_calls==1 && last_mode==2 && resume_calls==1);
    mock_remaining=187;assert(hook_loading_remaining()==187);state(4,187);
    mock_remaining=0;assert(hook_loading_remaining()==0);state(4,0);
    loading_present(0);state(4,0);
    loading_map_begin("/Game/Levels/Lobby");state(3,-1);
    assert(thunk_calls==2 && last_stage==3);
    loading_map_end("/Game/Levels/Lobby",0);assert(g_loading_startup && !g_loading_present_wait);
    for(int i=0;i<10;++i)loading_present(0);state(3,-1);
    loading_map_end("/Game/Levels/Lobby",1);assert(!g_loading_startup && g_loading_present_wait==6);
    for(int i=0;i<10;++i)loading_present(-1);assert(g_loading_present_wait==6);state(3,-1);
    mock_remaining=99;assert(hook_loading_remaining()==99);state(3,-1);
    for(int i=0;i<5;++i){loading_present(0);state(3,-1);}
    assert(g_loading_present_wait==1);loading_present(1);assert(g_loading_present_wait==1);
    loading_present(0);state(0,0);assert(!g_loading_present_wait);
    loading_present(0);state(0,0);
    hook_loading_fast();assert(mode_calls==2 && last_mode==1 && resume_calls==2);
    loading_map_begin("/Game/Levels/r100");state(3,-1);
    loading_map_end("/Game/Levels/r100",1);loading_present(0);assert(g_loading_present_wait==5);
    loading_map_begin("/Game/Levels/r101");assert(!g_loading_present_wait);state(3,-1);
    loading_present(0);state(3,-1);
    loading_map_begin("/Game/Levels/Lobby");loading_map_end("/Game/Levels/Lobby",1);assert(g_loading_present_wait==6);
    loading_map_begin("/Game/Levels/r102");assert(!g_loading_present_wait);state(3,-1);
    puts("PASS: production packed JNI state, real remaining counter, startup-only precompile mode, map failures, six successful presents and map-begin cancellation");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='loading-native-check-') as temporary:
    work = Path(temporary)
    source = work / 'original.c'
    source.write_text(prefix+header+'\nvoid install(unsigned long base){install_loading(base);}\n')
    subprocess.run([str(LLVM/'clang.exe'), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(source)], check=True)
    source = work / 'host.c'
    source.write_text(prefix+mocked+tests)
    obj, exe = work/'check.obj', work/'check.exe'
    subprocess.run([str(LLVM/'clang.exe'), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', '-Wno-int-to-pointer-cast', '-Wno-int-to-void-pointer-cast',
        '-isystem', str(VC/'include'), '-isystem', str(SDK/'Include/10.0.26100.0/ucrt'), '-idirafter', str(NDK/'sysroot/usr/include'), '-c', str(source), '-o', str(obj)], check=True)
    subprocess.run([str(VC/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj), 'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
        f'/LIBPATH:{VC/"lib/x64"}', f'/LIBPATH:{SDK/"Lib/10.0.26100.0/ucrt/x64"}', f'/LIBPATH:{SDK/"Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: original shader symbols, hook preimages, native branch targets and all three original batching modes; Android23 Werror')
print('LIMIT: host gate returns a controlled counter; GPU latency and Android UI need hardware verification')
