"""Native discovery proof and actual C filesystem deployment/failure checks."""
import os
from pathlib import Path
import hashlib
import mmap
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / "analysis/re4fix_mobile_text.h"
native_checks = {
    0x7ff5cc8: (0xd10183ff,0xa90357f6),
    0x7ff5cf0: (0x979da0d0,0xb94013e8),
    0x7ff5d84: (0x979da14d,0xb9400808),
    0x7ff5da0: (0x910063e8,0x97971c69),
    0x7ff8618: (0x9797072b,0x52800036),
    0x7ff8644: (0x9797ac2d,0x7100001f),
    0x7ff8648: (0x7100001f,0x1a80d6d6),
    0x7ff8660: (0x52800c88,0x1b086edb),
}
with (root / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        offset = struct.unpack_from("<Q",native,0x20)[0]
        stride,count = struct.unpack_from("<HH",native,0x36)
        segments = [struct.unpack_from("<IIQQQQQQ",native,offset+i*stride) for i in range(count)]
        def off(address):
            segment = next(s for s in segments if s[0] == 1 and s[3] <= address < s[3]+s[5])
            return segment[2]+address-segment[3]
        for address,expected in native_checks.items():
            assert struct.unpack_from("<II",native,off(address)) == expected,hex(address)
        assert native[off(0x3fed008):off(0x3fed008)+16].decode("utf-16le") == "%sPaks/\0"
print("PASS: 8 native Saved/Paks autodiscovery and numeric _999_P priority preimages, %sPaks/ format",flush=True)

generated = (root / "analysis/mobile_text_pak.h").read_text(encoding="ascii")
body = generated.split("{",1)[1].rsplit("}",1)[0]
embedded = bytes(int(number) for number in re.findall(r"\d+",body))
pak = (root / "analysis/paks/pakchunk99-Android_ETC2_999_P.pak").read_bytes()
assert embedded == pak and len(embedded) > 500000
assert "/sdcard/Android/data/com.Armature.VR4/files" in header.read_text(encoding="utf-8")
print(f"PASS: embedded PAK exact {len(pak)} bytes, SHA256 {hashlib.sha256(pak).hexdigest()}",flush=True)

clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
ndk_headers=clang.parent.parent/'sysroot/usr/include'
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
test_root = root / "analysis/research"
with tempfile.TemporaryDirectory(prefix="re4-text-deploy-",dir=test_root) as temporary:
    temp = Path(temporary).resolve()
    assert temp.is_relative_to(test_root.resolve())
    stub = temp / "mobile_text.c"
    stub.write_text("#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include <unistd.h>\n"
        "#define LOGF(...) ((void)0)\n"
        f'#include "{header.as_posix()}"\n'
        "void check(void){deploy_mobile_text();}\n",encoding="utf-8")
    subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",str(stub)],check=True)
    print("PASS: Android aarch64 deployed helper syntax",flush=True)
    obj,exe = temp / "mobile_text.obj",temp / "mobile_text.exe"
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-O2","-Werror",
        "-isystem",str(vc / "include"),"-isystem",str(sdk / "Include/10.0.26100.0/ucrt"),
        "-isystem",str(sdk / "Include/10.0.26100.0/um"),"-isystem",str(sdk / "Include/10.0.26100.0/shared"),
        "-idirafter",str(ndk_headers),
        "-c","-o",str(obj),str(root / "analysis/research/test_mobile_text.c")],check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}",f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"],check=True)
    package = temp / "package"
    package.mkdir()
    subprocess.run([str(exe),(package / "files").as_posix()],check=True)
