"""Read-only opcode and Android ABI compile checks for the one-shot pawn diagnostic."""
import os
from pathlib import Path
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[2]
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
header = (root / "analysis/re4fix_state_diag.h").read_text(encoding="utf-8")
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
checks = re.findall(r"\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}", header)
assert len(checks) == 10, f"expected 10 native preconditions, got {len(checks)}"
for address, word0, word1 in checks:
    actual = struct.unpack_from("<II", native, int(address, 16))
    assert actual == (int(word0, 16), int(word1, 16)), f"native opcode mismatch at {address}: {actual}"
print("PASS: 10 native diagnostic opcode preconditions", flush=True)
source = root / "analysis/re4fix.c"
assert '#include "re4fix_state_diag.h"' in source.read_text(encoding="utf-8")
subprocess.run([str(clang), "--target=aarch64-linux-android23", "-O2", "-fsyntax-only",
                str(source)], cwd=root, check=True)
print("PASS: integrated one-shot diagnostic compiled for Android aarch64")
