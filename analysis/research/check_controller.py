"""Verify the controller-only visibility native path and execute the actual helper C."""
import os
from pathlib import Path
import mmap
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / "analysis/re4fix_controller.h"
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}",header.read_text(encoding="utf-8"))
assert len(checks) == 10
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
            assert struct.unpack_from("<II",native,off(address)) == (int(word0,16),int(word1,16)),hex(address)
        assert struct.unpack_from("<I",native,off(0xa269120+0x24))[0] == 0x440
log = (root / "analysis/research/p237_controller_reflection.log").read_text(encoding="utf-8")
assert "name='Controller' type='ObjectProperty' offset=548 element=8 dim=1" in log
assert "name='Gun' type='ObjectProperty' offset=538" in log
assert "name='HandProxy' type='ObjectProperty' offset=530" in log
print("PASS: 10 native preimages, StaticMesh+440, p237 reflected Controller+548 distinct Gun+538/HandProxy+530",flush=True)

with tempfile.TemporaryDirectory(prefix="re4-controller-syntax-") as temporary:
    stub = Path(temporary) / "controller.c"
    stub.write_text("typedef unsigned long size_t;\n"
        "extern int memcmp(const void*,const void*,size_t);\n"
        "extern void* memcpy(void*,const void*,size_t);\n"
        "extern int strcmp(const char*,const char*);\nextern int strncmp(const char*,const char*,size_t);\n"
        "static void* (*phone_player)(const void*);\nstatic void* (*phone_active)(void);\n"
        "static unsigned (*phone_type)(void);\nstatic int phone_inspect_ready;\n"
        "static void phone_inspect_fname(const void* p,char* b,int s){(void)p;(void)s;b[0]=0;}\n"
        "static void obj_name(const void* p,char* b,int s){(void)p;(void)s;b[0]=0;}\n"
        "#define LOGF(...) ((void)0)\n"
        "static int patch_code(void* t,void* h){(void)t;(void)h;return 1;}\n"
        f'#include "{header.as_posix()}"\n'
        "void check(void* h,unsigned long b){install_touch_controller(b);touch_controller_tick(h);}\n",
        encoding="utf-8")
    subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",str(stub)],check=True)
print("PASS: Android aarch64 controller helper syntax",flush=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-controller-check-") as temporary:
    obj,exe = Path(temporary) / "controller.obj",Path(temporary) / "controller.exe"
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-O2","-Werror",
        "-Wno-int-to-pointer-cast","-Wno-int-to-void-pointer-cast",
        "-isystem",str(vc / "include"),"-isystem",str(sdk / "Include/10.0.26100.0/ucrt"),
        "-c","-o",str(obj),str(root / "analysis/research/test_controller.c")],check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}",f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"],check=True)
    subprocess.run([str(exe)],check=True)
