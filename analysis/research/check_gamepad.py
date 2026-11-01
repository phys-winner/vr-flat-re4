"""Execute production pad input/frame/reset C; only AArch64 tail gates are host seams."""
import mmap
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "analysis/re4fix_gamepad.h").read_text(encoding="utf8")
for name in ("axis", "button"):
    start = source.index("__attribute__((naked)) static void gate_pad_" + name)
    end = source.index("\n}", start) + 2
    source = source[:start] + source[end:]
with (ROOT / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as native:
    for addr, body in re.findall(r"\{(0x[0-9a-f]+), \{([^}]+)\}\}", source):
        words = tuple(int(word.strip(), 16) for word in body.split(","))
        assert struct.unpack_from("<" + "I" * len(words), native, int(addr, 16)) == words, addr

clang = Path(os.environ["ANDROID_SDK_ROOT"]) / "ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe"
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-gamepad-") as temporary:
    work = Path(temporary)
    (work / "gamepad_under_test.h").write_text(source, encoding="utf8")
    actions = (ROOT / "analysis/re4fix_ui_actions.h").read_text(encoding="utf8")
    start = actions.index("static unsigned touch_menu_title_mask(")
    (work / "title_snapshot_under_test.h").write_text(actions[start:actions.index("\n}", start) + 2], encoding="utf8")
    obj, exe = work / "gamepad.obj", work / "gamepad.exe"
    subprocess.run([str(clang), "--target=x86_64-pc-windows-msvc", "-O2", "-Werror", "-Wno-int-to-void-pointer-cast", "-isystem", str(vc / "include"),
                    "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"), "-I", str(work), "-c", "-o", str(obj),
                    str(ROOT / "analysis/research/test_gamepad.c")], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj), "libcmt.lib", "libucrt.lib",
                    "libvcruntime.lib", "oldnames.lib", "kernel32.lib", f"/LIBPATH:{vc / 'lib/x64'}",
                    f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: native gamepad preimages and actual production frame/reset paths")
