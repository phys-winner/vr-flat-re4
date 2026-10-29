"""Verify body helper preimages and run its actual guards on a host mock."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header = root / 'analysis/re4fix_body.h'
native = (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
checks = re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}', header.read_text(encoding='utf8'))
assert len(checks) == 10
for addr, first, second in checks:
    assert struct.unpack_from('<II', native, int(addr, 16)) == (int(first, 16), int(second, 16)), addr
# EPlayerPawn native EnumeratorParam values: None/Bio4/Pause/Cutscene/Menu/Communicator/etc.
assert [struct.unpack_from('<q', native, 0x9df91a8 + i * 16 + 8)[0] for i in range(10)] == list(range(10))
section_offset = struct.unpack_from('<Q', native, 40)[0]
section_stride, section_count = struct.unpack_from('<HH', native, 58)
bio4_name = None
for i in range(section_count):
    section = struct.unpack_from('<IIQQQQIIQQ', native, section_offset + i * section_stride)
    if section[1] != 4:
        continue
    for offset in range(section[4], section[4] + section[5], section[9]):
        address, info, addend = struct.unpack_from('<QQq', native, offset)
        if address == 0x9df91a8 + 16:
            assert info & 0xffffffff == 1027
            bio4_name = native[addend:native.index(b'\0', addend)]
assert bio4_name == b'EPlayerPawn::Bio4'
# Native exec wrappers store the three returned HFA floats in pitch/yaw/roll order.
for result_store in (0x64b4bc8, 0x64b4c00, 0x64b4f34):
    assert struct.unpack_from('<II', native, result_store) == (0x2d000660, 0xbd000a62), hex(result_store)
print('PASS: ten body/target-angle preimages, FRotator HFA returns and EPlayerPawn enum values', flush=True)

clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
source = root / 'analysis/research/test_body.c'
with tempfile.TemporaryDirectory(prefix='re4-body-check-') as temporary:
    path = Path(temporary)
    test = source
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(test)], check=True)
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
        '-Wno-int-to-pointer-cast', '-Wno-int-to-void-pointer-cast',
        '-isystem', str(vc / 'include'), '-isystem', str(sdk / 'Include/10.0.26100.0/ucrt'),
        '-c', '-o', str(path / 'body.obj'), str(test)], check=True)
    subprocess.run([str(vc / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{path / "body.exe"}',
        str(path / 'body.obj'), 'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(path / 'body.exe')], check=True)
