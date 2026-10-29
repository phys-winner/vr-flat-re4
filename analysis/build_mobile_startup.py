"""Retain native shader completion while removing headset-only startup dependencies."""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
ORIGINALS = ROOT / 'analysis/research/splash_originals'
HASHES = {
    'SplashScreenMap.umap': '7c0b4fa848b7ed6896b647a9a02436a65b1c196421f865928882d947b55bbb7f',
    'SplashScreenMap.uexp': '75282a527764f9475e406cea64ebf49ffd2b0d132a5355a31191e33207b05577',
    'OptimizingScreen.uasset': '7cae4acc6c19ad6ece3b423a923909a1a18493c21ce8b7cb661615b7953e4d1f',
    'OptimizingScreen.uexp': '884f1cc8f1de0f341db8c6f2bd4d653e2d66181fe66a228ec569aad4748d6633',
}
sys.path.insert(0, str(ROOT / 'analysis/research'))
from inspect_flat_bp import Script


def build_mobile_startup():
    receipt = json.loads((ORIGINALS/'receipt.json').read_text())
    files, audit = {}, {}
    for stem in ('SplashScreenMap', 'OptimizingScreen'):
        extension = '.umap' if stem == 'SplashScreenMap' else '.uasset'
        package = json.loads((ORIGINALS/(stem+'.package.json')).read_text())
        for suffix in (extension, '.uexp'):
            raw = (ORIGINALS/(stem+suffix)).read_bytes()
            evidence = next(e for e in receipt['files'] if e['source'].endswith('/'+stem+suffix))
            assert hashlib.sha256(raw).hexdigest() == evidence['sha256'] == HASHES[stem+suffix]
            files[evidence['source']] = raw
        key = next(k for k in files if k.endswith('/'+stem+'.uexp'))
        body = bytearray(files[key])
        graph = json.loads((ORIGINALS/(stem+'.ExecuteUbergraph_'+stem+'.script.json')).read_text())
        start = graph['export']['uexp_offset']+graph['script_offset']
        if stem == 'SplashScreenMap':
            # Check actual pipelines each launch; saved GPU stamps can outlive the real driver cache.
            assert body[start+1500:start+1505] == bytes.fromhex('068f030000')
            struct.pack_into('<I', body, start+1501, 401)
            audit[stem] = {'entitlement_jump': {'disk': start+1501, 'from': 911, 'to': 401}}
        else:
            tree = graph['tree']
            original = bytes(body[start:start+graph['script_bytes']])
            assert len(original) == 4268 and graph['runtime_bytes'] == 3308
            # Copy original native calls/fields verbatim, retaining their linker relocations.
            order = (0, 5, 2477, 2531, 1342, 1352, 1380, 1977, 37, 65, 99, 119, 148, 186, 489, 543, 639, 15, 862)
            script, runtime, locations = bytearray(), 0, {}
            for old in order:
                index = next(i for i, node in enumerate(tree) if node['runtime'] == old)
                node, following = tree[index:index+2]
                locations[old] = (len(script), runtime)
                script += original[node['offset']:following['offset']]
                runtime += following['runtime']-old
            padding = len(original)-len(script)-3
            assert padding > 0
            return_at = runtime
            # Return before the unreachable EX_Nothing padding that preserves package/export sizes.
            script += b'\x04\x0b'+b'\x0b'*padding+b'\x53'
            runtime += padding+3
            struct.pack_into('<I', script, locations[0][0]+1, return_at)
            struct.pack_into('<I', script, locations[186][0]+1, locations[639][1])
            delay_at = locations[489][0]
            delay_end = locations[543][0]
            delay = bytes(script[delay_at:delay_end])
            assert delay.count(b'\x5b'+struct.pack('<I', 37)) == 1
            linkage = delay_at+delay.index(b'\x5b'+struct.pack('<I', 37))+1
            struct.pack_into('<I', script, linkage, locations[37][1])
            # Yield before even a zero-work completion so FinishSpawning can bind CacheComplete.
            delay_at, delay_end = locations[2477][0], locations[2531][0]
            delay = bytes(script[delay_at:delay_end])
            assert delay.count(b'\x5b'+struct.pack('<I', 1342)) == 1
            linkage = delay_at+delay.index(b'\x5b'+struct.pack('<I', 1342))+1
            struct.pack_into('<I', script, linkage, locations[1342][1])
            assert len(script) == len(original)
            body[start:start+len(script)] = script
            struct.pack_into('<I', body, start-8, runtime)
            wrappers = {}
            for path in ORIGINALS.glob(stem+'.*.script.json'):
                wrapper = json.loads(path.read_text())
                if wrapper['export']['name'].startswith('ExecuteUbergraph_'):
                    continue
                for call in wrapper['calls']:
                    if call.get('call') != 'ExecuteUbergraph_OptimizingScreen':
                        continue
                    at = wrapper['export']['uexp_offset']+wrapper['script_offset']+call['disk_offset']+6
                    assert body[at-1] == 0x1d and struct.unpack_from('<I', body, at)[0] == call['arguments'][0]['int']
                    target = locations[2477][1] if wrapper['export']['name'] == 'ReceiveBeginPlay' else return_at
                    struct.pack_into('<I', body, at, target)
                    wrappers[wrapper['export']['name']] = target
            assert len(wrappers) == 6
            parsed = Script(bytes(script), package['names'], package['imports'], package['exports'])
            while parsed.at < len(script):
                parsed.expression()
            assert parsed.at+parsed.runtime_delta == runtime
            calls = [c.get('call') for c in parsed.calls]
            assert not any('Widget' in str(c) or 'RenderTarget' in str(c) or 'Entitlement' in str(c) for c in parsed.calls)
            assert all(any(name in str(c) for c in calls) for name in ('PauseShaderCacheCompilation', 'GetNumShaderCompilesRemaining', 'IsShaderCacheReady', 'SetShaderCacheModeFast', 'SetShaderCacheModeBackground', 'DestroyOptimizingShadersFile'))
            audit[stem] = {'serialized_bytes': len(script), 'runtime_bytes': runtime, 'return_at': return_at,
                           'original_statements': list(order), 'wrappers': wrappers, 'calls': calls}
        assert len(body) == len(files[key])
        files[key] = bytes(body)
    audit['files'] = {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()} for name, data in files.items()}
    return files, audit


if __name__ == '__main__':
    files, audit = build_mobile_startup()
    print(json.dumps(audit, indent=2))
