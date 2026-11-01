"""Read complaint-related cooked packages using the retained original Oodle decoder."""
from pathlib import Path
import hashlib
import json
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

output = ROOT / 'analysis/research/user_render_originals'
output.mkdir(exist_ok=True)
pak = Pak9(ROOT / 'com.Armature.VR4/patch.201.com.Armature.VR4.obb',
           'VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
receipt = []
for stem in ('UI_MercenariesResults_BP', 'UI_MercenariesResults_RetryButton_BP',
             'MercenariesResultsMap', 'VR4MenuPawn_BP', 'VR4MenuPawnInWorld_BP',
             'VR4CameraCover_BP', 'CutsceneTheatreBox', 'VideoManager_BP',
             'UI_ChapterSummary_BP', 'UI_ChapterSummary_NextButton_BP', 'ChapterSummaryMap',
             'EndGameFlow_CreditsUnlocks_BP', 'AshleyFirstTimeTutorial', 'AshleyFoundTutorial',
             'BoatPreferenceTutorial', 'WeapManTutorialStart', 'MercTutorialsInGame', 'ShowMenu'):
    name = next(n for n in pak.entries if n.endswith('/' + stem + '.uasset')
                or n.endswith('/' + stem + '.umap'))
    decoded = {}
    for entry in (name, name.rsplit('.', 1)[0] + '.uexp'):
        e = pak.entries[entry]
        assert e['method'] == 1 and pak.methods[1] == 'Oodle' and not e['encrypted']
        assert len(e['blocks']) == 1 and 0 < e['unc'] < 0x100000 and e['size'] < 0x10000
        pak.f.seek(e['off'])
        local, _ = pak.rentry(pak.f.read(e['header_size']), 0)
        assert all(local[k] == e[k] for k in ('size', 'unc', 'method', 'hash', 'blocks', 'encrypted', 'cbs'))
        raw = pak.f.read(e['size'])
        assert hashlib.sha1(raw).digest() == e['hash']
        assert e['blocks'] == [(e['header_size'], e['header_size'] + len(raw))]
        trials = []
        for _ in range(2):
            u.mem_write(buffers, raw)
            compressor = call(0x96b6f6c, [buffers, len(raw), 0])
            needed = call(0x96b4b88, [compressor, e['unc']])
            assert 0 < needed < 16 * 1048576
            u.mem_write(buffers + 0x10000, bytes(e['unc']))
            result = call(0x96b64e0, [buffers, len(raw), buffers + 0x10000, e['unc'],
                          1, 1, 0, 0, 0, 0, 0, buffers + 0x100000, needed, 3])
            assert result == e['unc']
            trials.append(bytes(u.mem_read(buffers + 0x10000, result)))
        assert trials[0] == trials[1]
        leaf = Path(entry).name
        decoded[Path(entry).suffix] = trials[0]
        (output / leaf).write_bytes(trials[0])
        receipt.append({'source': entry, 'sha256': hashlib.sha256(trials[0]).hexdigest(),
                        'bytes': len(trials[0]), 'compressed_sha1': e['hash'].hex(), 'repeats': 2})
    parsed = package(decoded[Path(name).suffix], decoded['.uexp'])
    (output / (stem + '.package.json')).write_text(json.dumps(parsed, indent=2) + '\n')
    print(stem, len(parsed['exports']), len(parsed['names']), flush=True)
pak.f.close()
pak.z.close()
(output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
