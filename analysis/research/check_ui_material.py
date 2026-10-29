"""Prove original widget/MID/depth ABI and run the actual guarded material lifecycle on host."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LLVM = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
VC = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK = Path(r'C:\Program Files (x86)\Windows Kits\10')
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header = (ROOT / 'analysis/re4fix_ui_material.h').read_text(encoding='utf-8')
for first, last, digest in (
    (0x7dacffc, 0x7dad1a8, '90f452b914454dfe0ebe02899c19b4a7893cd525b1d1e8ebe0d7a2e7b9434fc2'),
    (0x7db2130, 0x7db21d4, '413683bc2a0a61c7289572ed7a5a0355996d661f5c5204c06467d2c362d0bb6c'),
    (0x8478088, 0x8478108, '8507903f3373346880dc11731820d82a1a6b5ff3d03193345dd61d81432d953c'),
    (0x8478af0, 0x8478b80, '02157f68a410d866b4cc503c0b383958c9c25a865f9610ad041c117f2c204d11'),
    (0x8e480c0, 0x8e480d0, '5ab05185a3f6bddf0bced13b490455444a362eb9b052db85350640218933ac0a'),
    (0x8385604, 0x838561c, '70890dcff84f0e02238fe6c96379170e2db71c53ed1857d818c85a8b4727e9be'),
):
    assert hashlib.sha256(native[first:last]).hexdigest() == digest, hex(first)
for caller, target in ((0x7dad00c, 0x8375c74), (0x7dad044, 0x8478088), (0x7dad068, 0x7dad0d4),
                       (0x7dad120, 0x847870c), (0x7dad154, 0x84781ac), (0x7dad17c, 0x8478338)):
    instruction = struct.unpack_from('<I', native, caller)[0]
    assert instruction & 0x7c000000 == 0x14000000
    offset = instruction & 0x3ffffff
    if offset & 0x2000000: offset -= 0x4000000
    assert caller + offset * 4 == target, hex(caller)
assert native[0x42f3032:0x42f303a] == b'SlateUI\0'
assert native[0x4241b67:0x4241b67 + len(b'TintColorAndOpacity\0')] == b'TintColorAndOpacity\0'
assert native[0x4287f40:0x4287f53] == b'OpacityFromTexture\0'
guards = re.findall(r'\{(0x[0-9a-f]+), \{(0x[0-9a-f]+), (0x[0-9a-f]+)\}\}', header)
fields = re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}', header)
assert len(guards) == 13 and len(fields) == 6
fixture = 'static void fixture_image(unsigned char* image){\n'
for address, first, second in guards:
    address, first, second = map(lambda value: int(value, 16), (address, first, second))
    assert struct.unpack_from('<II', native, address) == (first, second), hex(address)
    fixture += f'*(unsigned*)(image+{address})={first};*(unsigned*)(image+{address}+4)={second};\n'
for address, offset in fields:
    address, offset = int(address, 16), int(offset, 16)
    assert struct.unpack_from('<I', native, address + 36)[0] == offset, hex(address)
    fixture += f'*(unsigned*)(image+{address}+36)={offset};\n'
fixture += '}\nstatic const uintptr_t fixture_checks[]={' + ','.join(a for a, _, _ in guards) + ','
fixture += ','.join(f'{a}+36' for a, _ in fields) + '};\n'
core = (ROOT / 'analysis/re4fix_cinema.h').read_text(encoding='utf-8')
guard = core[core.index('static int cinema_object('):core.index('static int cinema_on_game_thread(')]
with tempfile.TemporaryDirectory(prefix='re4-ui-material-') as temporary:
    work = Path(temporary)
    # Match ARM64 pointer-width unsigned long while executing the production header on Windows LLP64.
    material = work / 'ui_material.h'
    material.write_text(re.sub(r'\bunsigned long\b(?! long)', 'uintptr_t', header), encoding='utf-8')
    proof = work / 'guard.h'
    proof.write_text(guard + fixture, encoding='utf-8')
    source = ROOT / 'analysis/research/test_ui_material.c'
    defines = ['-DMATERIAL_HEADER="' + material.as_posix() + '"', '-DGUARD_HEADER="' + proof.as_posix() + '"']
    subprocess.run([str(LLVM / 'clang.exe'), '--target=aarch64-linux-android23', '-O2', '-Werror',
                    *defines, '-fsyntax-only', str(source)], check=True)
    obj, exe = work / 'material.obj', work / 'material.exe'
    subprocess.run([str(LLVM / 'clang.exe'), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', *defines,
                    '-isystem', str(VC / 'include'), '-isystem', str(SDK / 'Include/10.0.26100.0/ucrt'),
                    '-c', str(source), '-o', str(obj)], check=True)
    subprocess.run([str(VC / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{VC / "lib/x64"}', f'/LIBPATH:{SDK / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{SDK / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: original SetMaterial creates component MID and binds SlateUI/tint/opacity; native sort setter marks render state dirty; 13 native guards, 6 fields, depth bit and actual ARM64/host lifecycle')
