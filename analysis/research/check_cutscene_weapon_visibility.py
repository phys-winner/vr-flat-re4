"""Read-only proof of exact held-weapon ownership and authored visibility recovery."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
b = (root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for lo, hi, digest in (
    (0x628d024, 0x628d0dc, '3f38b4e9581dba9a0b02ea77bc5a516bc9269bc524129b6b54b6f5d9355db45a'),
    (0x62b985c, 0x62b99c4, 'a9f1a2fe691a4c6d50a6d453f2dc1e05d08181ba56fff0ad9067fa0f9a99ee22'),
    (0x62b371c, 0x62b3dc0, 'ffbc698099251c4fe6d742b0cfc7319b469b0ba5a383d4f427efb05de090af9a'),
    (0x62b9764, 0x62b9858, '00bb195ea66b210ef720090e538223c99d1b8cf6fb241d77dd27f9ca9f8e8de3'),
    (0x62446bc, 0x62448e0, '47a74bd3eae360ac9b087f1f9a0864882e8a53fa0b7cc79e3e8a4c965f8dd93c'),
    (0x62ac2c8, 0x62ac368, '8627a2eb1a78982b7611725c2125cac5702b2fdf644b88b2f1654442eddbff0c'),
    (0x628b0c8, 0x628b104, '652b7636a39cf4d9bed62aeee6be7a158f4c4898cb8f8103ec6011fd25405427'),
    (0x62b90c8, 0x62b90fc, '9e7bbd5ec0e2a6de2aa285e81b68224119bd489811bca3ebb90c5453aa0a11e9'),
    (0x628af0c, 0x628af2c, '1c341cc7cb10a10d36bd2d274f7ced188cc0fb1efd238cc6b1301d229ecd3198'),
    (0x62a65e0, 0x62a677c, '324ca13539726afa350dd83d4b3338e3e2f8e62b4950fd03b7ef83e1394536db'),
    (0x62af678, 0x62af6ec, 'a03afe797f321c3972993d5ac12ab80c684428de42b32d65dfe9bd6632e2af08'),
    (0x6424b5c, 0x6424c1c, '54a5ae25155cfb34f9a3e212ec907b8ac9072f8f6f422088ae057ce68e95a85e'),
    (0x62afb44, 0x62afc40, 'e18b6e0b108ce3f7613cf22ace69587da4a980a932212d1d1791cbe0099afa80'),
):
    assert hashlib.sha256(b[lo:hi]).hexdigest() == digest, hex(lo)

for caller, target in ((0x6244524, 0x62a2f74), (0x6244758, 0x62b371c),
                       (0x6244880, 0x62b371c), (0x62b3d68, 0x62a6110),
                       (0x628ac90, 0x8f4e5bc), (0x628acac, 0x6922368),
                       (0x628acbc, 0x83aa6f8), (0x628c5d4, 0x62b985c),
                       (0x62451a0, 0x62a677c), (0x628d038, 0x62bc4a0),
                       (0x628d08c, 0x83ad964), (0x628d098, 0x64200f8),
                       (0x628b100, 0x83a2ed8), (0x62b90e8, 0x6922368),
                       (0x62b90f8, 0x83aa6f8), (0x628af28, 0x62b9764),
                       (0x62a59d4, 0x62a65e0), (0x62a669c, 0x83ad964),
                       (0x62a6734, 0x83ad964), (0x6424c04, 0x687bd44)):
    op = struct.unpack_from('<I', b, caller)[0]
    assert op & 0xfc000000 == 0x94000000, hex(caller)
    imm = op & 0x3ffffff
    if imm & 0x2000000:
        imm -= 0x4000000
    assert caller + 4*imm == target, hex(caller)

for address, words in (
    (0x628d024, (0xf81d0ff5, 0xa9014ff4)),
    (0x628d064, (0xf942a260, 0xf9400008)),
    (0x62b9958, (0xf9436d08, 0xd63f0100)),
    (0x628acb8, (0xf902a260, 0x94847e8f)),
    (0x62b977c, (0xf901e261, 0xb00204d5)),
):
    assert struct.unpack_from('<II', b, address) == words, hex(address)
for metadata, offset in ((0x9da4618, 0x540), (0x9db0b18, 0x3c0)):
    assert struct.unpack_from('<I', b, metadata+0x24)[0] == offset, hex(metadata)

shoff = struct.unpack_from('<Q', b, 40)[0]
shsize, shnum = struct.unpack_from('<HH', b, 58)
relocations = {}
expected = {0x9d1b3e0: 0x628c5bc, 0x9d1b458: 0x628d024}
name_address = 0x9df91a8+3*16
for i in range(shnum):
    s = struct.unpack_from('<IIQQQQIIQQ', b, shoff+i*shsize)
    if s[1] != 4:
        continue
    for o in range(s[4], s[4]+s[5], s[9]):
        address, info, addend = struct.unpack_from('<QQq', b, o)
        if address in expected or address == name_address:
            assert info & 0xffffffff == 1027
            relocations[address] = addend
for address, target in expected.items():
    assert relocations[address] == target, hex(address)
name = relocations[name_address]
assert b[name:b.index(b'\0', name)] == b'EPlayerPawn::Cutscene'
assert struct.unpack_from('<q', b, name_address+8)[0] == 3
print('PASS: 13 original native ranges, 20 exact normal caller targets, 5 opcode pairs and 2 reflected fields')
print('PASS: Gun Post/UpdateVisibility vtable targets and native Cutscene3 name/value')
header = (root/'analysis/re4fix_cutscene_weapon.h').read_text()
checks = re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}', header)
assert len(checks) == 29
for address, first, second in checks:
    assert struct.unpack_from('<II', b, int(address,16)) == (int(first,16),int(second,16)), address
fields_text = header.split('} fields[] = {',1)[1].split('};',1)[0]
fields = re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}', fields_text)
assert len(fields) == 10
for metadata, offset in fields:
    assert struct.unpack_from('<I',b,int(metadata,16)+0x24)[0] == int(offset,16), metadata
assert header.index('gate_cutscene_weapon(gun);') < header.index('cutscene_weapon_post_visibility(gun);')
assert header.index('gate_cutscene_avatar(pawn);') < header.index('cutscene_avatar_post_visibility(pawn);')
assert struct.unpack_from('<I',b,0x6424be4)[0]==0x5280a204 # native class size argument w4=0x510
print('PASS: production installer 29 opcode pairs, 10 fields, native Hand class510 and both original-first hooks',flush=True)

clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
objdump = clang.with_name('llvm-objdump.exe')
sources = [root/'analysis/research/test_cutscene_weapon.c',root/'analysis/research/test_cutscene_avatar.c']
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-cutscene-weapon-') as td:
    for index,source in enumerate(sources):
        android_obj = Path(td)/f'android{index}.o'
        subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',
                        '-c','-o',str(android_obj),str(source)],check=True)
        assembly = subprocess.check_output([str(objdump),'-d',str(android_obj)],text=True)
        for name,words in (
            ('gate_cutscene_weapon',('f81d0ff5','a9014ff4','a9027bfd','910083fd')),
            ('gate_cutscene_avatar',('f81c0ff7','a90157f6','a9024ff4','a9037bfd')),
        ):
            if index==1 and name=='gate_cutscene_weapon':
                continue # Both gates are already proved in the weapon object; this fixture exercises avatar.
            assert f'<{name}>:' in assembly,(source.name,name)
            gate = assembly.split(f'<{name}>:',1)[1].split('\n\n',1)[0]
            for word in words:
                assert word in gate, word
            assert 'br\tx16' in gate
        print(f'PASS: {source.name} actual ARM64 object -Werror, native displaced prologue/x16 gates',flush=True)
        obj,exe = Path(td)/f'test{index}.obj',Path(td)/f'test{index}.exe'
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-c','-o',str(obj),str(source)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)
print('LIMIT: class510 correction is new for p248; prior p247 movie acceptance is root-supplied evidence')
