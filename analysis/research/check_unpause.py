"""Verify native Back ABI/UI permission route and execute the actual unpause helper C."""
import os
from pathlib import Path
import mmap
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / "analysis/re4fix_unpause.h"
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}",header.read_text(encoding="utf-8"))
assert len(checks) == 15
body=header.read_text(encoding="utf-8").split("static void touch_unpause_tick",1)[1].split("static void install_touch_unpause",1)[0]
assert body.index("__atomic_exchange_n(&g_touch_back_request") < body.index("touch_menu_grip_cancel(hand)") < body.index("int accepted = try_unpause(0)")
assert body.index("unpause_layout(screen") < body.index("touch_menu_grip_cancel(hand)")
with (root / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
        offset = struct.unpack_from("<Q",native,0x20)[0]
        stride,count = struct.unpack_from("<HH",native,0x36)
        segments = [struct.unpack_from("<IIQQQQQQ",native,offset+i*stride) for i in range(count)]
        def off(address):
            segment = next(s for s in segments if s[0] == 1 and s[3] <= address < s[3]+s[5])
            return segment[2]+address-segment[3]
        for address,first,second in checks:
            address = int(address,16)
            assert struct.unpack_from("<II",native,off(address)) == (int(first,16),int(second,16)),hex(address)
        assert struct.unpack_from("<q",native,off(0x9df5528))[0] == 0
        assert struct.unpack_from("<I",native,off(0x9df629c))[0] == 0x11b0
        assert struct.unpack_from("<II",native,off(0x6332594)) == (0xf81d0ff5,0xa9014ff4)
        # FEnumeratorParam's relocated name pointer proves reason 0 is Game, not Load/MainMenu.
        section_offset = struct.unpack_from("<Q",native,0x28)[0]
        section_stride,section_count = struct.unpack_from("<HH",native,0x3a)
        found = False
        for i in range(section_count):
            section = struct.unpack_from("<IIQQQQIIQQ",native,section_offset+i*section_stride)
            if section[1] != 4:
                continue
            for cursor in range(section[4],section[4]+section[5],24):
                address,kind,addend = struct.unpack_from("<QQq",native,cursor)
                if address != 0x9df5520:
                    continue
                assert kind & 0xffffffff == 1027
                position = off(addend)
                assert native[position:native.find(b"\0",position)] == b"EVR4TryUnpauseReason::Game"
                found = True
        assert found
print("PASS: 15 native preimages, static byte-reason ABI, Game=0 enum/name, stock toggle caller and dynamic UI ProcessEvent gate",flush=True)

clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
source = root / "analysis/research/test_unpause.c"
subprocess.run([str(clang),"--target=aarch64-linux-android23","-O2","-Werror","-fsyntax-only",str(source)],check=True)
print("PASS: Android aarch64 unpause helper syntax",flush=True)
with tempfile.TemporaryDirectory(prefix="re4-unpause-check-",dir=root / "analysis/research") as temporary:
    directory = Path(temporary).resolve()
    assert directory.is_relative_to((root / "analysis/research").resolve())
    obj,exe = directory / "unpause.obj",directory / "unpause.exe"
    subprocess.run([str(clang),"--target=x86_64-pc-windows-msvc","-O2","-Werror",
        "-Wno-int-to-pointer-cast","-Wno-int-to-void-pointer-cast",
        "-isystem",str(vc / "include"),"-isystem",str(sdk / "Include/10.0.26100.0/ucrt"),
        "-c","-o",str(obj),str(source)],check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"),"/NOLOGO",f"/OUT:{exe}",str(obj),
        "libcmt.lib","libucrt.lib","libvcruntime.lib","oldnames.lib","kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}",f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"],check=True)
    subprocess.run([str(exe)],check=True)
