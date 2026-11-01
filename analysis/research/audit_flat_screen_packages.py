"""Inventory original UI/pawn/level headers across every supplied PAK; no game writes."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
loader = ROOT / 'analysis/research/host_oodle_decode.py'
prefix = loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'", 1)[0]
environment = {'__file__': str(loader), '__name__': 'original_native_loader'}
exec(compile(prefix, str(loader), 'exec'), environment)
u, buffers, call = (environment[name] for name in ('u', 'buffers', 'call'))
sys.path.insert(0, str(ROOT / 'analysis'))
from pak9 import Pak9


def read_entry(pak, name):
    entry = pak.entries[name]
    if not entry['method'] or pak.methods[entry['method']] != 'Oodle':
        return pak.read_entry(name)
    assert not entry['encrypted'] and len(entry['blocks']) == 1
    assert 0 < entry['unc'] < 0x400000 and entry['size'] < 0x400000
    pak.f.seek(entry['off'])
    local, _ = pak.rentry(pak.f.read(entry['header_size']), 0)
    assert all(local[k] == entry[k] for k in ('size', 'unc', 'method', 'hash', 'blocks', 'encrypted', 'cbs'))
    raw = pak.f.read(entry['size'])
    assert hashlib.sha1(raw).digest() == entry['hash']
    assert entry['blocks'] == [(entry['header_size'], entry['header_size'] + len(raw))]
    u.mem_write(buffers, raw)
    compressor = call(0x96b6f6c, [buffers, len(raw), 0])
    needed = call(0x96b4b88, [compressor, entry['unc']])
    assert 0 < needed < 0x1000000
    result = call(0x96b64e0, [buffers, len(raw), buffers + 0x400000, entry['unc'],
                  1, 1, 0, 0, 0, 0, 0, buffers + 0x800000, needed, 3])
    assert result == entry['unc']
    return bytes(u.mem_read(buffers + 0x400000, result))


def metadata(asset):
    assert asset[:4] == bytes.fromhex('c1832a9e') and struct.unpack_from('<i', asset, 4)[0] == -7
    assert struct.unpack_from('<I', asset, 24)[0] == len(asset) and struct.unpack_from('<i', asset, 20)[0] == 0
    def string(at):
        n = struct.unpack_from('<i', asset, at)[0]
        length = n if n > 0 else -n * 2
        assert 0 < abs(n) < 65536 and at + 4 + length <= len(asset)
        raw = asset[at + 4:at + 4 + length]
        unit = 1 if n > 0 else 2
        assert raw[-unit:] == bytes(unit)
        return raw[:-unit].decode('utf-8' if n > 0 else 'utf-16-le'), at + 4 + length
    _, at = string(28)
    _, names_count, names_offset, _, _, export_count, export_offset, import_count, import_offset = struct.unpack_from('<9I', asset, at)
    assert max(names_count, export_count, import_count) < 20000
    names, at = [], names_offset
    for _ in range(names_count):
        name, at = string(at)
        names.append(name)
        at += 4
    assert at <= import_offset
    def fname(at):
        index, number = struct.unpack_from('<II', asset, at)
        assert index < len(names) and number < 100000
        return names[index] + ('_' + str(number - 1) if number else '')
    imports = [{'outer': struct.unpack_from('<i', asset, import_offset + i * 28 + 16)[0],
                'name': fname(import_offset + i * 28 + 20)} for i in range(import_count)]
    exports = []
    for i in range(export_count):
        at = export_offset + i * 104
        cls, super_, _, outer = struct.unpack_from('<4i', asset, at)
        exports.append({'class_index': cls, 'super_index': super_, 'outer': outer, 'name': fname(at + 16)})
    def path(index, depth=0):
        assert depth < 32 and index and -import_count <= index <= export_count
        item = imports[-index - 1] if index < 0 else exports[index - 1]
        return (path(item['outer'], depth + 1) + '.' if item['outer'] else '') + item['name']
    for item in exports:
        item['class'] = path(item['class_index']) if item['class_index'] else None
        item['super'] = path(item['super_index']) if item['super_index'] else None
    return names, exports


fixture = ROOT / 'analysis/research/flat_ui_graph/OptionsConfirmScreen_BP'
golden = json.loads(fixture.with_suffix('.package.json').read_text())
names, exports = metadata(fixture.with_suffix('.uasset').read_bytes())
assert names == golden['names']
assert [(e['name'], e['class']) for e in exports] == [(e['name'], e['class_path']) for e in golden['exports']]

roots = ('VR4/Content/Blueprints/', 'VR4/Content/UI/VR4/')
excluded = ('/Textures/', '/Materials/', '/Geometry/', '/Icons/', '/Fonts/', '/Meshes/', '/Material/',
            '/Animations/', '/Anim/', '/Props/', '/MeshSets/', '/AnimGraphs/', '/Animation/')
keys = ('PossessPlayerPawn', 'GetActivePlayerPawnType', 'SetAnchor', 'PushUIScreen', 'PopUIScreen',
        'SetViewTargetWithBlend', 'SetViewTarget', 'Camera', 'TheatreBox', 'Widget_Unlocks', 'Widget_Results',
        'StartChapterSummary', 'AdvanceToSummary', 'JumpToRoomNo')
rows, containers = [], []
for obb in sorted((ROOT / 'com.Armature.VR4').glob('*.obb')):
    with zipfile.ZipFile(obb) as archive:
        members = [entry.filename for entry in archive.infolist() if entry.filename.endswith('.pak')]
    for member in members:
        pak = Pak9(obb, member)
        selected = [name for name in pak.entries if name.endswith('.umap') or name.endswith('.uasset')
                    and name.startswith(roots) and not any(term in name for term in excluded)]
        containers.append({'obb': obb.name, 'pak': member, 'entries': len(pak.entries), 'selected_headers': len(selected)})
        for name in selected:
            asset = read_entry(pak, name)
            names, exports = metadata(asset)
            classes = [{'name': e['name'], 'super': e['super']} for e in exports
                       if e['class'] in ('/Script/Engine.BlueprintGeneratedClass', '/Script/Engine.WidgetBlueprintGeneratedClass')]
            relevant = [key for key in keys if key in names]
            components = [{'name': e['name'], 'class': e['class']} for e in exports
                          if e['class'] in ('/Script/UMG.WidgetComponent', '/Script/Engine.ChildActorComponent')]
            if classes or relevant:
                rows.append({'obb': obb.name, 'pak': member, 'path': name, 'sha256': hashlib.sha256(asset).hexdigest(),
                             'compressed_sha1': pak.entries[name]['hash'].hex(), 'classes': classes,
                             'call_or_property_names': relevant, 'components': components})
        pak.f.close()
        pak.z.close()
        print(obb.name, Path(member).name, 'headers', len(selected), 'cumulative rows', len(rows), flush=True)
result = {'scope': {'roots': roots, 'excluded_path_segments': excluded, 'all_maps': True, 'metadata_only': True},
          'native_sha256': environment['native_hash'], 'decoder': 'original Oodle ARM64 in Unicorn',
          'containers': containers, 'rows': rows}
(ROOT / 'analysis/research/user_flat_screen_inventory.json').write_text(json.dumps(result, indent=2) + '\n')
print('PASS: header metadata parity and original checked decoding; containers', len(containers),
      'headers', sum(c['selected_headers'] for c in containers), 'blueprint/maps rows', len(rows))
