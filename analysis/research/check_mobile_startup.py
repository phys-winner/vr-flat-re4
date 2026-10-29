"""Check the actual packaged startup graph, including its real counter/latent control flow."""
from pathlib import Path
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'analysis'), str(ROOT/'analysis/research')]
from build_mobile_startup import build_mobile_startup, ORIGINALS
from inspect_flat_bp import Script
from inspect_ui_package import package

files, audit = build_mobile_startup()
stem = 'OptimizingScreen'
asset = files[next(k for k in files if k.endswith('/'+stem+'.uasset'))]
body = files[next(k for k in files if k.endswith('/'+stem+'.uexp'))]
metadata = package(asset, body)
original = json.loads((ORIGINALS/(stem+'.ExecuteUbergraph_'+stem+'.script.json')).read_text())
at = original['export']['uexp_offset']+original['script_offset']
script = Script(body[at:at+4268], metadata['names'], metadata['imports'], metadata['exports'])
nodes = {}
while script.at < len(script.data):
    runtime = script.at+script.runtime_delta
    nodes[runtime] = script.expression()
assert script.at+script.runtime_delta == struct.unpack_from('<I', body, at-8)[0] == audit[stem]['runtime_bytes']
assert not any(word in json.dumps(nodes) for word in ('Widget', 'RenderTarget', 'Entitlement', 'StereoLayer'))
offsets = sorted(nodes)
next_at = dict(zip(offsets, offsets[1:]))
remaining, ready, values, events = 4, False, {}, []

def evaluate(expr):
    if 'field' in expr:
        return values[expr['field'][0]]
    for literal in ('int', 'float'):
        if literal in expr:
            return expr[literal]
    if 'call' in expr:
        name = expr['call'].split('.')[-1]
        arguments = [evaluate(a) for a in expr.get('arguments', [])]
        if name == 'GetNumShaderCompilesRemaining':
            return remaining
        if name == 'IsShaderCacheReady':
            return ready
        if name == 'Greater_IntInt':
            return arguments[0] > arguments[1]
        if name == 'Not_PreBool':
            return not arguments[0]
        if name == 'BooleanOR':
            return arguments[0] or arguments[1]
        assert name in ('PauseShaderCacheCompilation', 'SetShaderCacheModeFast', 'SetShaderCacheModeBackground', 'DestroyOptimizingShadersFile'), name
        events.append(name)
        return None
    raise AssertionError(expr)

def execute(start):
    pc = start
    for _ in range(100):
        expr = nodes[pc]
        if 'let' in expr:
            target = expr['let'][1]['field'][0]
            values[target] = evaluate(expr['let'][2])
        elif 'let_bool' in expr:
            target = expr['let_bool'][0]['field'][0]
            values[target] = evaluate(expr['let_bool'][1])
        elif 'jump_if_not' in expr:
            if not evaluate(expr['condition']):
                pc = expr['jump_if_not']
                continue
        elif 'call_delegate' in expr:
            assert expr['call_delegate'] == 'CacheComplete__DelegateSignature'
            events.append('CacheComplete')
        elif 'return' in expr:
            return None
        elif expr.get('call', '').endswith('.Delay'):
            duration = expr['arguments'][1]['float']
            assert duration in (0.0, 0.10000000149011612)
            events.append('Delay0' if duration == 0 else 'Delay')
            return expr['arguments'][2]['values'][0]['skip_offset']
        else:
            evaluate(expr)
        pc = next_at[pc]
    raise AssertionError('startup graph did not yield or return')

initial_resume = execute(audit[stem]['wrappers']['ReceiveBeginPlay'])
assert events == ['Delay0'] and not values
resume = execute(initial_resume)
assert values['TotalShadersToCache'] == 4
assert events == ['Delay0', 'PauseShaderCacheCompilation', 'SetShaderCacheModeFast', 'Delay']
remaining = 0
assert execute(resume) == resume  # A zero count alone must not override IsShaderCacheReady.
assert events[-1] == 'Delay' and 'CacheComplete' not in events
ready = True
assert execute(resume) is None
assert events[-3:] == ['SetShaderCacheModeBackground', 'DestroyOptimizingShadersFile', 'CacheComplete']
assert events.count('CacheComplete') == 1
for name, entry in audit[stem]['wrappers'].items():
    if name != 'ReceiveBeginPlay':
        before = list(events)
        assert execute(entry) is None and events == before
splash = files['VR4/Content/Levels/SplashScreenMap.uexp']
assert splash[2301:2306] == bytes.fromhex('0691010000')
source = json.loads((ORIGINALS/'SplashScreenMap.ExecuteUbergraph_SplashScreenMap.script.json').read_text())
spawn = [n['expression'] for n in source['tree'] if 401 <= n['runtime'] <= 739]
assert spawn[0]['let'][2]['call'].endswith('.MakeTransform')
assert spawn[1]['let_object'][1]['call'].endswith('.BeginDeferredActorSpawnFromClass')
assert spawn[-3]['bind_delegate'] == 'CacheComplete' and spawn[-2]['delegate_operation'] == 92
assert any(c.get('call', '').endswith('.SetShadersOptimized') for c in source['calls'])
remaining, ready, values, events = 0, True, {}, []
initial_resume = execute(audit[stem]['wrappers']['ReceiveBeginPlay'])
assert events == ['Delay0'] and 'CacheComplete' not in events
assert execute(initial_resume) is None and events[-1] == 'CacheComplete'
print('PASS: four hash-verified packages retain serialized sizes; startup jumps to initialized spawn/bind independently of saved shader flag')
print('PASS: zero-work cache yields before completion so deferred spawn can bind the delegate')
print('PASS: actual rebuilt/relocated Blueprint captures total, compiles, waits on both real readiness conditions, completes once; no widget, RT or entitlement expressions')
print('LIMIT: Blueprint VM deserialization and shader timing require the packaged hardware run')
