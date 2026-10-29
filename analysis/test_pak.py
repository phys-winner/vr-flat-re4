"""Run from the project root: python analysis/test_pak.py."""
import hashlib
from pathlib import Path
import struct
import tempfile
import zipfile

from pak9 import Pak9
from pak_writer import build_pak


with tempfile.TemporaryDirectory() as td:
    pak_path = Path(td) / 'test.pak'
    zip_path = Path(td) / 'test.zip'
    expected = {'Engine/Config/Test.ini': b'[ConsoleVariables]\nr.MaxAnisotropy=8\n', 'empty': b''}
    build_pak(expected, pak_path)
    with zipfile.ZipFile(zip_path, 'w') as z:
        z.write(pak_path, 'test.pak')
    pak = Pak9(zip_path, 'test.pak')
    try:
        assert {n: pak.read_entry(n) for n in pak.entries} == expected
        for entry in pak.entries.values():
            assert struct.unpack_from('<Q', pak_path.read_bytes(), entry['off'])[0] == 0
    finally:
        pak.f.close()
        pak.z.close()
    bad = bytearray(pak_path.read_bytes())
    bad[53] ^= 1
    with zipfile.ZipFile(zip_path, 'w') as z:
        z.writestr('test.pak', bad)
    pak = Pak9(zip_path, 'test.pak')
    try:
        try:
            pak.read_entry('Engine/Config/Test.ini')
            raise AssertionError('corrupted payload was accepted')
        except ValueError as e:
            assert 'SHA1' in str(e)
    finally:
        pak.f.close()
        pak.z.close()
    bad[-1] ^= 1
    with zipfile.ZipFile(zip_path, 'w') as z:
        z.writestr('test.pak', bad)
    try:
        Pak9(zip_path, 'test.pak')
        raise AssertionError('corrupted index checksum was accepted')
    except ValueError as e:
        assert 'index SHA1' in str(e)

count = 0
for obb in Path('VRLOC.com.Armature.VR4').glob('*.obb'):
    with zipfile.ZipFile(obb) as z:
        members = [i.filename for i in z.infolist() if i.filename.endswith('.pak')]
    for member in members:
        pak = Pak9(obb, member)
        try:
            count += 1
            if member.endswith('pakchunk0-Android_ETC2.pak'):
                assert len(pak.entries) == 275215
                for name in pak.entries:
                    if name.endswith('.ini'):
                        data = pak.read_entry(name)
                        if name == 'VR4/Config/DefaultEngine.ini':
                            assert hashlib.sha256(data).hexdigest() == '8d8f154078f8781d800ee428ef58d038130dbc6b8604ccd97748f0163beedbae'
            if member.endswith('pakchunk0-Android_ETC2_p.pak'):
                assert pak.version == 3 and len(pak.entries) == 34
                for name in pak.entries:
                    pak.read_entry(name)
        finally:
            pak.f.close()
            pak.z.close()
assert count == 21, f'expected 21 original PAKs, tested {count}'
print('PASS: roundtrip, payload/index corruption, 21 original indices, 30 configs, 34 localization records')
