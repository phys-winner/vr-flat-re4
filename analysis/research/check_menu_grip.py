"""Check original native dependencies and execute the actual input/menuGrip/Back C."""
import os
from pathlib import Path
import mmap
import re
import struct
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
header=root/"analysis/re4fix_menu_grip.h"
code=header.read_text(encoding="utf8")
checks=[tuple(int(v,16) for v in row) for row in re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}",code)]
field_code=code.split("fields[] = {",1)[1].split("};",1)[0]
fields=[tuple(int(v,16) for v in row) for row in re.findall(r"\{(0x[0-9a-f]+),(0x[0-9a-f]+|0|8)\}",field_code)]
assert len(checks)==22 and len(fields)==8
with (root/"apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        phoff=struct.unpack_from("<Q",native,0x20)[0]
        stride,count=struct.unpack_from("<HH",native,0x36)
        segments=[struct.unpack_from("<IIQQQQQQ",native,phoff+i*stride) for i in range(count)]
        def off(address,size=1):
            s=next(s for s in segments if s[0]==1 and s[3]<=address and address+size<=s[3]+s[5])
            return s[2]+address-s[3]
        for address,a,b in checks:
            assert struct.unpack_from("<II",native,off(address,8))==(a,b),hex(address)
        for address,value in fields:
            assert struct.unpack_from("<I",native,off(address+0x24,4))[0]==value,hex(address)
        assert struct.unpack_from("<q",native,off(0x9e0fee8,8))[0]==2
print("PASS: 22 native preimages, eight UI output/ownership fields and authored Grip enum",flush=True)
for name in ["check_inventory_drag_native.py","check_inventory_ui_action_native.py"]:
    subprocess.run([sys.executable,"-I",str(root/"analysis/research"/name)],check=True)
assert "pthread_mutex_" not in code and "g_grip_latched" not in code and "g_cmd_until" not in code
assert code.index("gettid()!=gt") < code.index("unsigned type=phone_type()")
assert "touch_menu_pointer_choose(request.serial,MP_CANCEL)" in code
assert "mg_owned.hand_name==mg_name(hand)" in code and "mg_owned.pawn_name==mg_name(pawn)" in code
assert "(base+0x633c54c)" in code and "(base+0x633c558)" in code
unpause=(root/"analysis/re4fix_unpause.h").read_text()
assert unpause.index("__atomic_exchange_n(&g_touch_back_request") < unpause.index("touch_menu_grip_cancel(hand)") < unpause.index("int accepted = try_unpause(0)")
source=(root/"analysis/re4fix.c").read_text()
assert source.index('#include "re4fix_input.h"') < source.index('#include "re4fix_menu_grip.h"')
hook=source.split("void hook_mhtick(",1)[1].split("// ---- holster hover",1)[0]
assert hook.index("gate_mhtick(self, dt)") < hook.index("touch_menu_grip_tick(self)") < hook.index("touch_menu_visual_tick(self)")
print("PASS: actual integration ordering, normal InputGrip entries, Back-before-TryUnpause and no gameplay RG mutation/native call under helper mutex",flush=True)

clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk=Path(r"C:\Program Files (x86)\Windows Kits\10")
csource=root/"analysis/research/test_menu_grip.c"
subprocess.run([str(clang),"--target=aarch64-linux-android23","-std=c11","-D_DEFAULT_SOURCE","-O2","-Werror","-fsyntax-only",str(csource)],check=True)
print("PASS: actual Android aarch64 input/menuGrip/Back syntax",flush=True)
with tempfile.TemporaryDirectory(prefix="re4-menu-grip-") as temp:
    d=Path(temp)
    (d/"host_clock.h").write_text('''#include <time.h>
static long re4_clock_ms=10000;
#define CLOCK_MONOTONIC 1
static int re4_clock_gettime(int clock,struct timespec* t){(void)clock;t->tv_sec=re4_clock_ms/1000;t->tv_nsec=(re4_clock_ms%1000)*1000000;return 0;}
#define clock_gettime re4_clock_gettime
''')
    (d/"unistd.h").write_text('''static int usleep(unsigned long n){re4_clock_ms+=(long)(n/1000);return 0;}
''')
    (d/"pthread.h").write_text('''typedef struct { int locked; } pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
static int pthread_mutex_lock(pthread_mutex_t* m){assert(!m->locked);m->locked=1;return 0;}
static int pthread_mutex_unlock(pthread_mutex_t* m){assert(m->locked);m->locked=0;return 0;}
static int pthread_mutex_trylock(pthread_mutex_t* m){if(m->locked)return 1;m->locked=1;return 0;}
''')
    fixture=d/"native_fixture.h"
    fixture.write_text('''__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_native_installer(void){
    unsigned char* memory=VirtualAlloc((void*)0x20000000,0xb000000,0x3000,4);assert(memory);
    static const struct{unsigned address,word[2];} original[]={
'''+"\n".join("{0x%x,{0x%x,0x%x}},"%row for row in checks)+'''
    };
    for(unsigned i=0;i<sizeof original/sizeof *original;i++)memcpy(memory+original[i].address,original[i].word,8);
'''+"\n".join("*(unsigned*)(memory+0x%x+0x24)=0x%x;"%row for row in fields)+'''
    *(const char**)(memory+0x9e0fee0)="EVR4UIAction::Grip";*(long long*)(memory+0x9e0fee8)=2;
    unsigned long base=(unsigned long)(unsigned long long)memory;
    phone_player=(void*(*)(const void*))(memory+0x633d030);phone_active=(void*(*)(void))(memory+0x63380dc);phone_type=(unsigned(*)(void))(memory+0x6338aec);
    g_menu_grip_ready=0;install_touch_menu_grip(base);assert(g_menu_grip_ready);
    for(unsigned i=0;i<sizeof original/sizeof *original;i++){
        *(unsigned*)(memory+original[i].address)^=1;g_menu_grip_ready=0;install_touch_menu_grip(base);assert(!g_menu_grip_ready);*(unsigned*)(memory+original[i].address)^=1;
    }
    static const struct{unsigned address,value;} fields[]={
'''+"\n".join("{0x%x,0x%x},"%row for row in fields)+'''
    };
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
        *(unsigned*)(memory+fields[i].address+0x24)^=1;install_touch_menu_grip(base);assert(!g_menu_grip_ready);*(unsigned*)(memory+fields[i].address+0x24)^=1;
    }
    *(long long*)(memory+0x9e0fee8)=4;install_touch_menu_grip(base);assert(!g_menu_grip_ready);*(long long*)(memory+0x9e0fee8)=2;
    *(const char**)(memory+0x9e0fee0)="EVR4UIAction::Trigger";install_touch_menu_grip(base);assert(!g_menu_grip_ready);
    *(const char**)(memory+0x9e0fee0)=0;install_touch_menu_grip(base);assert(!g_menu_grip_ready);
    *(const char**)(memory+0x9e0fee0)="EVR4UIAction::Grip";
    phone_type=0;install_touch_menu_grip(base);assert(!g_menu_grip_ready);phone_type=(unsigned(*)(void))(memory+0x6338aec);
    install_touch_menu_grip(0);assert(!g_menu_grip_ready);install_touch_menu_grip(base);assert(g_menu_grip_ready);
    g_menu_grip_ready=0;assert(VirtualFree(memory,0,0x8000));
    puts("PASS: actual installer original-byte success, all22 opcode/eight field corruptions, enum/name/null/dependency/base refusal");
}
''',encoding="utf8")
    obj,exe=d/"menu.obj",d/"menu.exe"
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-std=c11","-O2","-Werror",
        "-Wno-int-to-pointer-cast","-Wno-int-to-void-pointer-cast","-I",str(d),"-include",str(d/"host_clock.h"),
        '-DMENU_GRIP_NATIVE_FIXTURE="'+fixture.as_posix()+'"',
        "-isystem",str(vc/"include"),"-isystem",str(sdk/"Include/10.0.26100.0/ucrt"),
        "-c","-o",str(obj),str(csource)],check=True)
    subprocess.run([str(vc/"bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc/'lib/x64'}",f"/LIBPATH:{sdk/'Lib/10.0.26100.0/ucrt/x64'}",f"/LIBPATH:{sdk/'Lib/10.0.26100.0/um/x64'}"],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS: actual C executed; serial clock/mutex fixture, hardware direct-helper drag remains root acceptance",flush=True)
