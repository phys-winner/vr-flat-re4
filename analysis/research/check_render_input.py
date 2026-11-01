"""Verify real native clamp/caller/gate preimages and execute production denominator override."""
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "analysis/re4fix_render_input.h").read_text(encoding="utf8")
native = (ROOT / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
checks = re.findall(r"\{(0x[0-9a-f]+),(\d+),\{([^}]+)\}\}", header)
assert len(checks) == 4
for address, count, body in checks:
    words = tuple(int(word, 16) for word in body.split(","))
    assert len(words) == int(count)
    assert struct.unpack_from("<" + "I" * len(words), native, int(address, 16)) == words, address
assert [int(word, 16) for word in re.findall(r'\.inst (0x[0-9a-f]+)', header)] == list(struct.unpack_from("<4I", native, 0x6a1dec0))
assert struct.unpack_from("<4I", native, 0x6a1db38) == (0xb001cfa8, 0x9138c108, 0x29000d02, 0xd65f03c0)
print("PASS: actual native JNI render-size store, four original-first gate instructions, exact input caller LR+54beb14 and both native divide/clamp/scale paths", flush=True)
# Execute the original scene quantizer: its fixed add3/AND~3 has no GPU/device branch.
quantizer = native[0x6f7b880:0x6f7b8a4]
assert struct.unpack("<9I", quantizer) == (0xb9400008,0x11000d08,0x121e7508,0xb9000028,
                                        0xb9400408,0x11000d08,0x121e7508,0xb9000428,0xd65f03c0)
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X19, UC_ARM64_REG_X20, UC_ARM64_REG_X30, UC_ARM64_REG_W8
machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
machine.mem_map(0x100000,0x1000); machine.mem_map(0x200000,0x1000)
machine.mem_write(0x100000,quantizer)
def quantize(width,height):
    machine.mem_write(0x200000,struct.pack("<II",width,height))
    machine.reg_write(UC_ARM64_REG_X0,0x200000);machine.reg_write(UC_ARM64_REG_X1,0x200010)
    machine.reg_write(UC_ARM64_REG_X30,0x100100)
    machine.emu_start(0x100000,0x100100,count=16)
    return struct.unpack("<II",machine.mem_read(0x200010,8))
assert quantize(1800,810)==(1800,812) and quantize(2414,1080)==(2416,1080)
# Original Android window tail selects quantum8 for non-VR, quantum1 for VR, then truncates both axes.
window_quantizer = native[0x6a1e10c:0x6a1e138]
assert struct.unpack("<11I",window_quantizer)==(0xb9400289,0x7100011f,0x52800108,0x1a9f0508,
    0x1ac80d29,0x1b087d29,0xb9000289,0xb9400269,0x1ac80d29,0x1b087d28,0xb9000268)
machine.mem_write(0x100200,window_quantizer)
def window_quantize(width,height,vr):
    machine.mem_write(0x200000,struct.pack("<II",width,height))
    machine.reg_write(UC_ARM64_REG_X20,0x200000);machine.reg_write(UC_ARM64_REG_X19,0x200004)
    machine.reg_write(UC_ARM64_REG_W8,vr)
    machine.emu_start(0x100200,0x10022c,count=16)
    return struct.unpack("<II",machine.mem_read(0x200000,8))
assert window_quantize(2412,1080,0)==(2408,1080) and window_quantize(1800,810,0)==(1800,808)
assert window_quantize(2412,1080,1)==(2412,1080) and window_quantize(1800,810,1)==(1800,810)
for width,height in ((2400,1080),(2414,1080),(2415,1081),(1080,2414),(33,35)):
    for percent in (50,67,75,85,100):
        render=(max(32,width*percent//800*8),max(32,height*percent//800*8))
        assert quantize(*render)==render, (width,height,percent,render)
        assert window_quantize(*render,0)==render and window_quantize(*render,1)==render
        assert render[0]<=width and render[1]<=height
print("PASS: actual original ARM64 scene up4 and Android window conditional down8 reproduce1800x810 mismatch and2412->2408; all5 floor8 scales on5 physical/odd/oriented/minimum panels are identities for both original functions/VR branches",flush=True)
llvm = Path(os.environ["ANDROID_SDK_ROOT"]) / "ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin"
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
source = ROOT / "analysis/research/test_render_input.c"
subprocess.run([str(llvm / "clang.exe"), "--target=aarch64-linux-android23", "-O2", "-Werror", "-fsyntax-only", str(source)], check=True)
with tempfile.TemporaryDirectory(prefix="re4-render-input-") as temporary:
    work = Path(temporary); obj, exe = work / "input.obj", work / "input.exe"
    subprocess.run([str(llvm / "clang.exe"), "--target=x86_64-pc-windows-msvc", "-O2", "-Werror", "-isystem", str(vc / "include"),
                    "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"), "-c", "-o", str(obj), str(source)], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj), "libcmt.lib", "libucrt.lib",
                    "libvcruntime.lib", "oldnames.lib", "kernel32.lib", f"/LIBPATH:{vc / 'lib/x64'}",
                    f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
