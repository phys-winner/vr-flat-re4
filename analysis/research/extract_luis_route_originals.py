"""Read only the original room/scene evidence needed for the Luis QA route."""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
loader = ROOT / 'analysis/research/host_oodle_decode.py'
prefix = loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'", 1)[0]
environment = {'__file__': str(loader), '__name__': 'original_native_loader'}
exec(compile(prefix, str(loader), 'exec'), environment)
u, buffers, call = (environment[name] for name in ('u', 'buffers', 'call'))
sys.path[:0] = [str(ROOT / 'analysis'), str(ROOT / 'analysis/research')]
from pak9 import Pak9
from inspect_ui_package import package

output = ROOT / 'analysis/research/luis_route_originals'
output.mkdir(exist_ok=True)
pak = Pak9(ROOT / 'com.Armature.VR4/patch.201.com.Armature.VR4.obb',
           'VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
receipt = []


def read_original(name, block_index=None, output_prefix=''):
    e = pak.entries[name]
    assert e['method'] == 1 and pak.methods[1] == 'Oodle' and not e['encrypted']
    assert 0 < e['cbs'] <= 0x100000
    pak.f.seek(e['off'])
    local, _ = pak.rentry(pak.f.read(e['header_size']), 0)
    assert all(local[k] == e[k] for k in ('size', 'unc', 'method', 'hash', 'blocks', 'encrypted', 'cbs'))
    raw = pak.f.read(e['size'])
    assert hashlib.sha1(raw).digest() == e['hash']
    blocks = list(enumerate(e['blocks']))
    if block_index is not None:
        blocks = [blocks[block_index]]
    decoded = []
    for index, (begin, end) in blocks:
        begin -= e['header_size']
        end -= e['header_size']
        assert 0 <= begin < end <= len(raw)
        compressed = raw[begin:end]
        length = min(e['cbs'], e['unc'] - index * e['cbs'])
        trials = []
        for _ in range(2):
            u.mem_write(buffers, compressed)
            compressor = call(0x96b6f6c, [buffers, len(compressed), 0])
            needed = call(0x96b4b88, [compressor, length])
            assert 0 < needed < 0x1000000
            result = call(0x96b64e0, [buffers, len(compressed), buffers + 0x400000, length,
                          1, 1, 0, 0, 0, 0, 0, buffers + 0x800000, needed, 3])
            assert result == length
            trials.append(bytes(u.mem_read(buffers + 0x400000, result)))
        assert trials[0] == trials[1]
        decoded.append(trials[0])
    result = b''.join(decoded)
    if block_index is None:
        assert len(result) == e['unc']
    leaf = output_prefix + Path(name).name + (f'.block{block_index}' if block_index is not None else '')
    (output / leaf).write_bytes(result)
    receipt.append({'obb': Path(pak.z.filename).name, 'pak': pak.info.filename,
                    'source': name, 'output': leaf, 'sha256': hashlib.sha256(result).hexdigest(),
                    'bytes': len(result), 'original_bytes': e['unc'],
                    'compressed_sha1': e['hash'].hex(), 'repeats': 2,
                    'block_index': block_index, 'uncompressed_offset': (block_index or 0) * e['cbs']})
    print(leaf, len(result), flush=True)
    return result


room = read_original('VR4/Content/BIO4/St1/r106.das', block_index=0)
container = struct.unpack_from('<I', room, 0x2c)[0]
assert container == 0x400
count = struct.unpack_from('<I', room, container)[0]
assert 0 < count < 512
offsets = struct.unpack_from('<' + str(count) + 'I', room, container + 16)
tags = [room[container + 16 + 4 * count + 4 * i:container + 20 + 4 * count + 4 * i] for i in range(count)]
print('DAS sections:', [(tag, hex(offset)) for tag, offset in zip(tags, offsets)], flush=True)
assert tags[15] == b'AEV\0' and tags[25] == b'ITA\0'
assert all((container + offsets[index]) // 0x100000 == 9 for index in (15, 16, 25, 26))
read_original('VR4/Content/BIO4/St1/r106.das', block_index=9)

stem = 'VR4/Content/Blueprints/Debug/DebugMenu/DebugMap/r106_s'
asset, body = (read_original(stem + extension) for extension in ('.uasset', '.uexp'))
(output / 'r106_s.package.json').write_text(json.dumps(package(asset, body), indent=2) + '\n')
stem = 'VR4/Content/Levels/Evil/1st/r106/r106'
asset, body = (read_original(stem + extension, output_prefix='patch_') for extension in ('.umap', '.uexp'))
(output / 'patch_r106.package.json').write_text(json.dumps(package(asset, body), indent=2) + '\n')
pak.f.close()
pak.z.close()
pak = Pak9(ROOT / 'com.Armature.VR4/main.201.com.Armature.VR4.obb',
           'VR4/Content/Paks/pakchunk1-Android_ETC2.pak')
asset, body = (read_original('r106/r106' + extension, output_prefix='main_') for extension in ('.umap', '.uexp'))
(output / 'main_r106.package.json').write_text(json.dumps(package(asset, body), indent=2) + '\n')
pak.f.close()
pak.z.close()
(output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
