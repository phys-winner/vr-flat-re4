"""Static self-check for the pause/reset native contract; no ADB or APK writes."""
import os
import mmap
from pathlib import Path
import re
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
NDK_BIN = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
with (ROOT / "apk_decoded/lib/arm64-v8a/libUE4.so").open("rb") as stream:
    with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        assert data[:6] == b"\x7fELF\x02\x01"
        phoff, shoff = struct.unpack_from("<QQ", data, 0x20)
        phsize, phcount, shsize, shcount = struct.unpack_from("<HHHH", data, 0x36)
        segments = [struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize) for i in range(phcount)]
        sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shsize) for i in range(shcount)]

        def read(address, size):
            for segment in segments:
                if segment[0] == 1 and segment[3] <= address and address + size <= segment[3] + segment[5]:
                    offset = segment[2] + address - segment[3]
                    return data[offset:offset + size]
            raise AssertionError(f"Unmapped address: {address:x}")

        expected = {
            0x633d030: (0xf9417800, 0xd65f03c0),
            0x6340b0c: (0x52800028, 0x39130008, 0xd65f03c0),
            0x6341174: (0x39550000, 0xd65f03c0),
            0x54ad814: (0xd104c3ff, 0xa90f5ffc, 0xa91057f6, 0xa9114ff4),
        }
        for address, words in expected.items():
            assert read(address, len(words) * 4) == struct.pack("<" + "I" * len(words), *words)
        tick_table = read(0x3fb6200, 16)
        assert {state: 0x54ad170 + tick_table[state] * 4 for state in (3, 6, 8, 10)} == {
            3: 0x54ad218, 6: 0x54ad1e8, 8: 0x54ad284, 10: 0x54ad364}
        cmd_table = read(0x42fd98a, 15)
        assert {cmd: 0x54be144 + cmd_table[cmd - 1] * 4 for cmd in (7, 13, 14)} == {
            7: 0x54be218, 13: 0x54be4b8, 14: 0x54be19c}
        inherited = {
            0x9d1efc8 + 16 + 0x458: 0x6340ca0,
            0x9d34640 + 16 + 0x458: 0x6340ca0,
            0x9d38fa8 + 16 + 0x458: 0x6340ca0,
            0x9d38850 + 16 + 0x458: 0x6310630,
        }
        found = {}
        for section in sections:
            if section[1] == 4:
                for offset in range(section[4], section[4] + section[5], section[9]):
                    address, info, addend = struct.unpack_from("<QQq", data, offset)
                    if address in inherited:
                        assert info == 1027
                        found[address] = addend
        assert found == inherited

header = ROOT / "analysis/re4fix_pause.h"
source = f'''#include <string.h>
static volatile int g_touch_pause;
static volatile int g_app_input_focus = 1;
static void touch_reset(void) {{}}
static int patch_code(void* a, void* b) {{ return 0; }}
#define LOGI(...) ((void)0)
#define LOGF(...) ((void)0)
#include "{header.as_posix()}"
void check_install(unsigned long base) {{ install_touch_pause(base); }}
void check_hand(void* hand) {{ touch_pause_tick(hand); }}
'''
with tempfile.TemporaryDirectory(prefix="pause-check-", dir=ROOT / "analysis/research") as temporary:
    temp = Path(temporary).resolve()
    assert temp.is_relative_to((ROOT / "analysis/research").resolve())
    cfile, ofile = temp / "pause.c", temp / "pause.o"
    cfile.write_text(source, encoding="utf-8")
    subprocess.run([str(NDK_BIN / "clang.exe"), "--target=aarch64-linux-android23", "-O2", "-fPIC", "-c", str(cfile), "-o", str(ofile)], check=True)
    assembly = subprocess.check_output([str(NDK_BIN / "llvm-objdump.exe"), "-d", str(ofile)], text=True)
    gate = assembly.split("<gate_touch_app_event>:", 1)[1]
    assert tuple(int(word, 16) for word in re.findall(r"^\s*[0-9a-f]+:\s+([0-9a-f]{8})", gate, re.M)[:4]) == expected[0x54ad814]
print("PASS: native preimages, lifecycle enums, pawn/pause-hand inheritance, AArch64 compile and gate ABI")
