"""Read-only native dependencies and actual C helper checks; no APK/device changes."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
header = (root / "analysis/re4fix_phone.h").read_text(encoding="utf-8")
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", header)
assert len(checks) == 9, f"expected 9 native preconditions, got {len(checks)}"
for address, word0, word1 in checks:
    actual = struct.unpack_from("<II", native, int(address, 16))
    assert actual == (int(word0, 16), int(word1, 16)), f"native opcode mismatch at {address}: {actual}"
print("PASS: 9 native phone opcode preconditions including native Running reads", flush=True)
assert struct.unpack_from('<III',native,0x6234878)==(0x1e2703e0,0x2a1f03e1,0x17fffe78)
# Original SkipToNextLine supplies false/zero to SkipToNextLineInternal; no invented timing/progress writes.
assert 0x6234880+((0x17fffe78&0x3ffffff)-0x4000000)*4==0x6234260
print("PASS: exact native SkipToNextLine(void*) tailcall ABI", flush=True)
inspector = (root / "analysis/re4fix_phone_inspect.h").read_text(encoding="utf-8")
reflection_checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", inspector)
assert len(reflection_checks) == 16
for address, word0, word1 in reflection_checks:
    actual = struct.unpack_from("<II", native, int(address, 16))
    assert actual == (int(word0, 16), int(word1, 16)), f"reflection opcode mismatch at {address}: {actual}"
print("PASS: 16 native reflection layout and function preconditions", flush=True)
source = root / "analysis/re4fix.c"
if '#include "re4fix_phone.h"' in source.read_text(encoding="utf-8"):
    subprocess.run([str(clang), "--target=aarch64-linux-android23", "-O2", "-fsyntax-only",
                    str(source)], cwd=root, check=True)
    print("PASS: integrated phone helper compiled for Android aarch64", flush=True)
else:
    with tempfile.TemporaryDirectory(prefix="re4-phone-syntax-") as temporary:
        stub = Path(temporary) / "phone.c"
        stub.write_text('typedef unsigned long size_t;\n'
            'extern int memcmp(const void*,const void*,size_t);\n'
            'extern void* memcpy(void*,const void*,size_t);\n'
            'extern int strcmp(const char*,const char*);\n'
            'extern char* strstr(const char*,const char*);\n'
            'static volatile int g_touch_context,g_phone_request;\n'
            '#define LOGF(...) ((void)0)\n'
            'static void obj_name(const void* o,char* n,int sz) {(void)o;(void)sz;n[0]=0;}\n'
            'static void touch_interact_inspect_object(void* o) {(void)o;}\n'
            f'#include "{(root / "analysis/re4fix_phone.h").as_posix()}"\n'
            'void syntax(void* p,unsigned long b){install_touch_phone(b);touch_phone_tick(p);}\n',
            encoding="utf-8")
        subprocess.run([str(clang), "--target=aarch64-linux-android23", "-O2", "-Werror",
                        "-fsyntax-only", str(stub)], check=True)
        print("PASS: isolated phone helper compiled for Android aarch64", flush=True)

vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-phone-check-") as temporary:
    obj = Path(temporary) / "phone.obj"
    exe = Path(temporary) / "phone.exe"
    subprocess.run([str(clang), "--target=x86_64-pc-windows-msvc", "-O2",
        "-Wno-int-to-pointer-cast", "-Wno-int-to-void-pointer-cast",
        "-isystem", str(vc / "include"), "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"),
        "-c", "-o", str(obj), str(root / "analysis/research/test_phone.c")], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj),
        "libcmt.lib", "libucrt.lib", "libvcruntime.lib", "oldnames.lib", "kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
