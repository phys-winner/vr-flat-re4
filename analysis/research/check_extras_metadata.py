"""Verify the two retained device QA SaveGame metadata files, without changing a profile."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = ROOT/'analysis/research/extras_save_graph'

def read_metadata(path):
    data = path.read_bytes()
    def string(at):
        assert 0 <= at <= len(data)-4
        count = struct.unpack_from('<i', data, at)[0]
        assert 0 < count < 65536 and at+4+count <= len(data) and data[at+3+count] == 0
        return data[at+4:at+3+count].decode('utf8'), at+4+count
    assert data[:4] == b'GVAS' and struct.unpack_from('<II',data,4) == (2,518)
    branch, at = string(22)
    assert branch == '++depot+VR4'
    version, count = struct.unpack_from('<II',data,at)
    assert version == 3 and count == 52
    at += 8+20*count
    classname, at = string(at)
    assert classname == '/Game/Blueprints/Save/VR4SaveGame_BP.VR4SaveGame_BP_C'
    def properties(at, end, depth=0):
        assert depth < 8
        result = {}
        for _ in range(256):
            name, at = string(at)
            if name == 'None': return result, at
            kind, at = string(at)
            size, index = struct.unpack_from('<II',data,at); at += 8
            assert index == 0 and name not in result
            extra, value = None, None
            if kind == 'StructProperty':
                extra, at = string(at); at += 16
            elif kind in ('ArrayProperty','EnumProperty','ByteProperty'):
                extra, at = string(at)
            elif kind == 'BoolProperty':
                assert size == 0 and data[at] in (0,1)
                value = bool(data[at]); at += 1
            assert data[at] in (0,1)
            at += 1+16*data[at]
            assert at+size <= end
            if kind == 'StructProperty' and extra == 'VR4GamePlayerSettings':
                value, finish = properties(at,at+size,depth+1)
                assert finish == at+size
            result[name] = dict(kind=kind, struct=extra, value=value)
            at += size
        raise AssertionError('Unbounded property list')
    top, end = properties(at,len(data))
    assert end == len(data)-4 and data[end:] == bytes(4)
    settings = top['PlayerSettings']
    assert settings['kind'] == 'StructProperty' and settings['struct'] == 'VR4GamePlayerSettings'
    return data, settings['value']

before_data, before = read_metadata(DIRECTORY/'p335_System.sav')
after_data, after = read_metadata(DIRECTORY/'p337_System.sav')
assert hashlib.sha256(before_data).hexdigest() == '24eae009e46d6823c4019cf5f526610ac5cbf3ea92f140188ae6f71cd8e3f3f6'
assert hashlib.sha256(after_data).hexdigest() == '72406ecb4461a8eae80bcbcfe5e9cb2df59397baf05239f4768e385fa7a5d017'
unlocked = ['UnlockedGoldenGuns','UnlockedHorrorMode','UnlockedBigHeadMode','UnlockedHighSpeedMode']
enabled = ['EnableGoldenGuns','EnableHorrorMode','EnableBigHeadMode','EnableHighSpeedMode']
for name in unlocked:
    assert name not in before and after[name] == dict(kind='BoolProperty',struct=None,value=True)
for name in enabled:
    assert name not in before and name not in after
report = dict(before_build='p335',after_build='p337',
    before_sha256=hashlib.sha256(before_data).hexdigest(),after_sha256=hashlib.sha256(after_data).hexdigest(),
    explicit_unlocked={name:True for name in unlocked},
    enabled={name:'absent in both; unchanged inherited default' for name in enabled},
    method='Bounded original GVAS header, property tags, nested PlayerSettings and exact list terminators; read only')
(DIRECTORY/'metadata_device_proof.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: actual p337 System.sav contains all four Unlocked BoolProperties true; all four Enable properties retain the identical absent/default state from p335')
