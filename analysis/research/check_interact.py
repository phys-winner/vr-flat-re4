"""Verify native dependencies and execute the actual mobile interaction C helper."""
import os
from pathlib import Path
import mmap
import hashlib
import re
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / "analysis/re4fix_interact.h"
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}",header.read_text(encoding="utf-8"))
assert len(checks) == 58
install_bytes = []
with (root / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        offset = struct.unpack_from("<Q",native,0x20)[0]
        stride,count = struct.unpack_from("<HH",native,0x36)
        segments = [struct.unpack_from("<IIQQQQQQ",native,offset+i*stride) for i in range(count)]
        def off(address):
            segment = next(s for s in segments if s[0] == 1 and s[3] <= address < s[3]+s[5])
            return segment[2]+address-segment[3]
        for address,word0,word1 in checks:
            address = int(address,16)
            actual = struct.unpack_from("<II",native,off(address))
            assert actual == (int(word0,16),int(word1,16)),hex(address)
            install_bytes.append((address,*actual))
        for first,last,digest in (
            (0x62a8fc0,0x62a90dc,'0127fac8466672fcda1f07ed4dc6b0b75a5f167b90da755a3b623fb79cd00383'),
            (0x627feb8,0x6280170,'eec31049e4e96a996162e8d9711692a7c8414985635c07f869e78183abe6a3a3'),
            (0x629a930,0x629ad20,'55d2e315791c3c6072c282f15b364e61fcf65a7092c61c742fabbd6824b561e4'),
        ):
            assert hashlib.sha256(native[off(first):off(last)]).hexdigest()==digest,hex(first)
        # Original getter side0/1 reads owned left/right slots490/498, validates UObject and GamePlayerHand class.
        assert struct.unpack_from('<I',native,off(0x62a8fdc))[0]==0xf9424813
        assert struct.unpack_from('<I',native,off(0x62a904c))[0]==0xf9424c13
        # IsFreeToGrab indexes handedness1f1+side; the helper must preserve selected side.
        assert struct.unpack_from('<II',native,off(0x627ff18))==(0x8b210008,0x3947c508)
        for field,expected in [(0x9d9e298,0x300),(0x9dbf850,0x3d9),(0x9dbf7f0,0x3da)]:
            assert struct.unpack_from("<I",native,off(field+0x24))[0] == expected,hex(field)
        # Native enum table is FEnumeratorParam(name pointer, int64 value); pointers relocate.
        for entry,value in [(0x9d92278,1),(0x9d92288,2),(0x9d92298,3),(0x9d922a8,4)]:
            assert struct.unpack_from("<q",native,off(entry+8))[0] == value
print("PASS: 50 interaction and eight optional pickup native preimages, native hand/pawn/ray-origin/class/handedness and parent/command/action metadata",flush=True)
# Independent read-only proof uses complete native bodies, relocated names and
# real EKeys symbols; it also verifies display/input mapping and IgnoreKeys parity.
subprocess.run([sys.executable,"-I",str(root / "analysis/research/check_action_command_native.py")],check=True)
subprocess.run([sys.executable,"-B",str(root / "analysis/research/check_interact_los.py")],check=True)
source = header.read_text(encoding="utf-8")
assert 'memcmp(name,"EVR4CommandButton::",19)' in source
assert "name[19] != faces[i].letter || name[20]" in source
for command,bit,letter in [(1,0,"A"),(2,1,"B"),(3,8,"X"),(4,9,"Y")]:
    assert "{%d,%d,'%s'}" % (command,bit,letter) in source
    assert "#define OB_%s %su" % (letter,hex(1<<bit)) in (root / "analysis/re4fix_input.h").read_text()
assert "for (unsigned command = 5; command <= 16; command++)" in source
assert "*(const unsigned*)(base+0x9dbf850+0x24) != 0x3d9" in source
assert "*(const unsigned*)(base+0x9d9e298+0x24) != 0x300" in source
print("PASS: installer relocated enum-name/field guard and current face-mask integration",flush=True)

with tempfile.TemporaryDirectory(prefix="re4-interact-syntax-") as temporary:
    stub = Path(temporary) / "interact.c"
    stub.write_text("typedef unsigned long size_t;\n"
        "extern int memcmp(const void*,const void*,size_t);\n"
        "extern int strcmp(const char*,const char*);\n"
        "static volatile int g_touch_interact_request,g_touch_cancel,g_touch_reload_request,g_reload_phase;\n"
        "static volatile long g_cmd_until[32];\nstatic volatile unsigned g_grip_latched;\n"
        "static void* (*phone_player)(const void*);\nstatic void* (*phone_active)(void);\n"
        "static unsigned (*phone_type)(void);\nstatic int (*rl_paused)(void),(*rl_player_valid)(void);\n"
        "static void* (*f_GetCurrentGrip)(const void*);\n"
        "static void (*f_SetGripForcedHover)(void*,void*,int);\n"
        "static int g_gripmx,g_ngrips;static struct{void* g;}g_grips[256];\n"
        "static void pthread_mutex_lock(int* m){(void)m;}static void pthread_mutex_unlock(int* m){(void)m;}\n"
        "static int reload_grip_registered(void* p){return p!=0;}\n"
        "static int cinema_on_game_thread(void){return 1;}\n"
        "static void obj_name(const void* p,char* out,int size){(void)p;(void)size;out[0]=0;}\n"
        "#define LOGF(...) ((void)0)\n"
        f'#include "{header.as_posix()}"\n'
        "void check(void* h,int s,long n,unsigned long b){install_touch_interact(b);touch_interact_tick(h,s,n);}\n",
        encoding="utf-8")
    subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",str(stub)],check=True)
print("PASS: Android aarch64 interaction helper syntax",flush=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-interact-check-") as temporary:
    obj,exe = Path(temporary) / "interact.obj",Path(temporary) / "interact.exe"
    # Execute the actual installer on an isolated low-address host image. Native
    # RVA/preimages above come from the original library, not invented stubs.
    # A low Windows allocation preserves the helper's Android unsigned-long base
    # without rewriting any production C or its checked control flow.
    fixture = Path(temporary) / "native_fixture.h"
    fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_install_guard(void) {
    unsigned char* memory = VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);
    assert(memory && (uintptr_t)memory < 0x100000000ULL);
    static const struct { unsigned addr, words[2]; } original[] = {
''' + '\n'.join('        {0x%x,{0x%x,0x%x}},' % row for row in install_bytes) + r'''
    };
    for(unsigned i=0;i<sizeof original/sizeof *original;i++)
        memcpy(memory+original[i].addr,original[i].words,8);
    const char* names[]={"EVR4CommandButton::A","EVR4CommandButton::B","EVR4CommandButton::X","EVR4CommandButton::Y"};
    for(unsigned i=0;i<4;i++) {
        unsigned char* entry=memory+0x9d92278+16*i;
        *(const char**)entry=names[i]; *(long long*)(entry+8)=i+1;
    }
    static const struct { unsigned value; const char* name; } kinds[]={
        {0,"EActionButtonType::Talk"},{1,"EActionButtonType::Check"},{5,"EActionButtonType::JumpOver"},
        {7,"EActionButtonType::Kick"},{8,"EActionButtonType::GoUp"},{9,"EActionButtonType::GetDown"},
        {16,"EActionButtonType::Open"},{25,"EActionButtonType::Climb"},{40,"EActionButtonType::PickUp"},
        {44,"EActionButtonType::Suplex"},{47,"EActionButtonType::Save"},{48,"EActionButtonType::NoDisp"},
        {54,"EActionButtonType::Answer"},{57,"EActionButtonType::BackKick"},{71,"EActionButtonType::Any"},
    };
    *(unsigned*)(memory+0x9d4f170+40)=72;
    for(unsigned i=0;i<sizeof kinds/sizeof *kinds;i++) {
        unsigned char* entry=memory+0x9d4ecf0+16*kinds[i].value;
        *(const char**)entry=kinds[i].name; *(long long*)(entry+8)=kinds[i].value;
    }
    *(unsigned*)(memory+0x9dbf850+0x24)=0x3d9;
    *(unsigned*)(memory+0x9dbf7f0+0x24)=0x3da;
    *(unsigned*)(memory+0x9d9e298+0x24)=0x300;
    *(unsigned*)(memory+0x9dcb308+0x24)=0x260;
    unsigned long base=(unsigned long)(uintptr_t)memory;
    install_touch_interact(base);assert(ti_ready==1&&ti_pickup_class);
    for(unsigned i=0;i<sizeof original/sizeof *original;i++) {
        *(unsigned*)(memory+original[i].addr)^=1;
        install_touch_interact(base);if(i<50)assert(ti_ready==0);else assert(ti_ready==1&&!ti_pickup_class);
        *(unsigned*)(memory+original[i].addr)^=1;
    }
    for(unsigned i=0;i<4;i++) {
        unsigned char* entry=memory+0x9d92278+16*i;
        *(long long*)(entry+8)=16; install_touch_interact(base);assert(ti_ready==0);
        *(long long*)(entry+8)=i+1;
        *(const char**)entry=0; install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry="EVR4CommandButton::?"; install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry="EVR4CommandButton::AX"; install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry="EVR4WrongCommand::A"; install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry=names[i];
    }
    *(unsigned*)(memory+0x9d4f170+40)=71;install_touch_interact(base);assert(ti_ready==0);
    *(unsigned*)(memory+0x9d4f170+40)=72;
    for(unsigned i=0;i<sizeof kinds/sizeof *kinds;i++) {
        unsigned char* entry=memory+0x9d4ecf0+16*kinds[i].value;
        *(long long*)(entry+8)=99;install_touch_interact(base);assert(ti_ready==0);
        *(long long*)(entry+8)=kinds[i].value;
        *(const char**)entry=0;install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry="EActionButtonType::?";install_touch_interact(base);assert(ti_ready==0);
        *(const char**)entry=kinds[i].name;
    }
    *(unsigned*)(memory+0x9dbf850+0x24)=0x3da;install_touch_interact(base);assert(ti_ready==0);
    *(unsigned*)(memory+0x9dbf850+0x24)=0x3d9;
    *(unsigned*)(memory+0x9dbf7f0+0x24)=0x3db;install_touch_interact(base);assert(ti_ready==0);
    *(unsigned*)(memory+0x9dbf7f0+0x24)=0x3da;
    *(unsigned*)(memory+0x9d9e298+0x24)=0x301;install_touch_interact(base);assert(ti_ready==0);
    *(unsigned*)(memory+0x9d9e298+0x24)=0x300;install_touch_interact(base);assert(ti_ready==1);
    assert(VirtualFree(memory,0,0x8000));
    puts("PASS: actual installer on original preimages; all50 code mismatches, face/semantic enum corruptions and three metadata-offset corruptions decline");
}
''',encoding="utf-8")
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-O2","-Werror",
        "-Wno-int-to-pointer-cast","-Wno-int-to-void-pointer-cast",
        "-isystem",str(vc / "include"),"-isystem",str(sdk / "Include/10.0.26100.0/ucrt"),
        '-DINTERACT_NATIVE_FIXTURE="'+fixture.as_posix()+'"',
        "-c","-o",str(obj),str(root / "analysis/research/test_interact.c")],check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}",f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"],check=True)
    subprocess.run([str(exe)],check=True)
    # These mutations restore the actual p345 native-hover hold and a possible
    # regression which removes a foreign forced hover on the regular path.
    for label,old,new in (
        ('los_gripcenter_replaces_body','struct ti_vec origin=ti_ray_origin(pawn);','struct ti_vec origin=ti_grab_center(g_grips[0].g);'),
        ('native_hover_left_held','if(!pickup)interact_pickup_owned(grip,&pickup);','(void)pickup;'),
        ('foreign_hover_removed','if(forced_hover && reload_grip_registered(grip))f_SetGripForcedHover(selected,grip,0);',
         'if(pickup && reload_grip_registered(grip))f_SetGripForcedHover(selected,grip,0);'),
    ):
        assert source.count(old)==1
        candidate=Path(temporary)/(label+'.h');candidate.write_text(source.replace(old,new),encoding='utf-8')
        badtest=Path(temporary)/(label+'.c')
        badtest.write_text((root/'analysis/research/test_interact.c').read_text(encoding='utf-8').replace(
            '#include "../re4fix_interact.h"','#include "'+candidate.as_posix()+'"'),encoding='utf-8')
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-c','-o',str(obj),str(badtest)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        result=subprocess.run([str(exe)],capture_output=True,text=True)
        assert result.returncode!=0 and 'Assertion failed:' in result.stdout+result.stderr,(label,result.stdout,result.stderr)
        print('PASS: unchanged actual C pickup fixture rejects '+label,flush=True)
