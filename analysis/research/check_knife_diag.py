"""Check native preconditions and execute the actual read-only diagnostic helper."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
header = root / "analysis/re4fix_knife_diag.h"
text = header.read_text(encoding="utf-8")
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", text)
assert len(checks) == 20
for address, word0, word1 in checks:
    assert struct.unpack_from("<II", native, int(address, 16)) == (int(word0, 16), int(word1, 16)), address
print("PASS: 20 getter/layout opcode preconditions", flush=True)

with tempfile.TemporaryDirectory(prefix="re4-knife-syntax-") as temp:
    stub = Path(temp) / "knife.c"
    stub.write_text("typedef unsigned long size_t;\n"
        "extern int memcmp(const void*,const void*,size_t),strcmp(const char*,const char*);\n"
        "extern float sqrtf(float);\n"
        "static volatile long g_knife_diag_until,g_anim_t0;\n"
        "static volatile int g_anim_kind,g_touch_weapon;\n"
        "static void* (*phone_player)(const void*);static void* (*phone_active)(void);\n"
        "static unsigned (*phone_type)(void);static void* (*f_GetCurrentGrip)(const void*);\n"
        "static long ptr_now_ms(void){return 1;}\n"
        "static void obj_name(const void* p,char* n,int s){(void)p;(void)s;n[0]=0;}\n"
        "#define LOGF(...) ((void)0)\n"
        f'#include "{header.as_posix()}"\n'
        "void check(void* p,unsigned long b){install_touch_knife_diag(b);touch_knife_diag(p);}\n",
        encoding="utf-8")
    subprocess.run([str(clang), "--target=aarch64-linux-android23", "-O2", "-Werror",
                    "-fsyntax-only", str(stub)], check=True)
print("PASS: Android aarch64 diagnostic header syntax", flush=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-knife-check-") as temp:
    obj, exe = Path(temp) / "knife.obj", Path(temp) / "knife.exe"
    subprocess.run([str(clang), "--target=x86_64-pc-windows-msvc", "-O2", "-Werror",
        "-Wno-int-to-pointer-cast", "-Wno-int-to-void-pointer-cast",
        "-isystem", str(vc / "include"), "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"),
        "-c", "-o", str(obj), str(root / "analysis/research/test_knife_diag.c")], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj),
        "libcmt.lib", "libucrt.lib", "libvcruntime.lib", "oldnames.lib", "kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
