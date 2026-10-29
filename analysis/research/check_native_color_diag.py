"""Compile and execute the actual native-pass shader diagnostic and its pipeline restoration."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
llvm=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
symbols=subprocess.check_output([str(llvm/'llvm-nm.exe'),'-D','-S','-C',str(root/'apk_decoded/lib/arm64-v8a/libUE4.so')],text=True)
match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+B\s+VulkanDynamicAPI::vkCmdBindPipeline\s*$',symbols,re.M)
assert match and int(match[1],16)==0xa5e26a8 and int(match[2],16)==8
source=root/'analysis/research/test_native_color_diag.c'
subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-native-color-') as temporary:
    output=Path(temporary)
    (output/'pthread.h').write_text('''#pragma once
typedef struct { int locked; } pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
#define RE4_TEST_MUTEX_LOCKED(m) ((m).locked)
static int pthread_mutex_lock(pthread_mutex_t* m){assert(!m->locked);m->locked=1;return 0;}
static int pthread_mutex_unlock(pthread_mutex_t* m){assert(m->locked);m->locked=0;return 0;}
''')
    obj,exe=output/'test.obj',output/'test.exe'
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-I',str(output),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-idirafter',str(llvm.parent/'sysroot/usr/include'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: original ELF BindPipeline slot, Android ARM64 -Werror and actual native diagnostic C host execution',flush=True)
