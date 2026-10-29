"""Consume the exact original cooked save scripts, resolving every call operand."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[2]

class Script:
    def __init__(self, data, names, imports, exports):
        self.data, self.names, self.imports, self.exports = data, names, imports, exports
        self.at = 0
        self.calls = []
        self.strings = []
    def take(self, count):
        end = self.at + count
        assert 0 <= self.at <= end <= len(self.data)
        result = self.data[self.at:end]; self.at = end
        return result
    def integer(self): return struct.unpack('<i', self.take(4))[0]
    def fname(self):
        index, number = struct.unpack('<II', self.take(8))
        assert 0 <= index < len(self.names) and number == 0
        return self.names[index]
    def ref(self):
        index = self.integer()
        assert index and (0 < index <= len(self.exports) or 0 < -index <= len(self.imports))
        return self.imports[-index-1]['path'] if index < 0 else self.exports[index-1]['name']
    def field(self):
        count = self.integer(); assert 0 <= count <= 8
        names = [self.fname() for _ in range(count)]
        owner = self.integer()
        assert owner == 0 or -len(self.imports) <= owner <= len(self.exports)
        return {'field': names, 'owner': owner}
    def expression(self, depth=0):
        assert depth < 32
        at = self.at; token = self.take(1)[0]
        if token in (0x00, 0x01, 0x48): return self.field()
        if token in (0x06,): return {'jump': self.integer()}
        if token == 0x07:
            target = self.integer()
            return {'jump_if_not': target, 'condition': self.expression(depth+1)}
        if token == 0x04: return {'return': self.expression(depth+1)}
        if token == 0x14: return {'let_bool': [self.expression(depth+1), self.expression(depth+1)]}
        if token == 0x19:
            obj = self.expression(depth+1); skip = self.integer(); field = self.field()
            return {'context': obj, 'skip': skip, 'result_field': field, 'call': self.expression(depth+1)}
        if token in (0x45, 0x68):
            name = self.fname() if token == 0x45 else self.ref()
            arguments = []
            while self.data[self.at] != 0x16:
                assert len(arguments) < 32
                arguments.append(self.expression(depth+1))
            self.take(1)
            self.calls.append({'disk_offset': at, 'call': name, 'arguments': arguments})
            return {'call': name, 'arguments': arguments}
        if token == 0x1d: return {'int': self.integer()}
        if token == 0x1e: return {'float': struct.unpack('<f', self.take(4))[0]}
        if token == 0x1f:
            end = self.data.index(0, self.at)
            assert end-self.at <= 4096
            value = self.take(end-self.at+1)[:-1].decode('utf8'); self.strings.append(value)
            return {'string': value}
        if token == 0x20: return {'object': self.ref()}
        if token == 0x2f:
            name, size = self.ref(), self.integer()
            assert 0 <= size <= 1024
            values = []
            while self.data[self.at] != 0x30:
                assert len(values) < 128
                values.append(self.expression(depth+1))
            self.take(1)
            return {'struct': name, 'runtime_size': size, 'values': values}
        if token in (0x0b, 0x17, 0x27, 0x28, 0x53): return {'literal_token': token}
        raise AssertionError(f'Unknown token {token:02x} at {at:x}')

def script(directory, stem, function, offset, raw_count, expected):
    package = json.loads((directory/(stem+'.package.json')).read_text())
    item = next(e for e in package['exports'] if e['name'] == function)
    body = (directory/(stem+'.uexp')).read_bytes()
    payload = body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]
    assert hashlib.sha256(payload).hexdigest() == item['sha256']
    assert struct.unpack_from('<I', payload, offset-4)[0] == raw_count
    assert offset+raw_count+12 == len(payload)
    parser = Script(payload[offset:offset+raw_count], package['names'], package['imports'], package['exports'])
    tree = []
    while parser.at < len(parser.data): tree.append(parser.expression())
    assert tree[-1] == {'literal_token': 0x53}
    assert [c['call'] for c in parser.calls] == expected
    return {'export': item, 'script_disk_offset': offset, 'script_raw_bytes': raw_count,
            'script_sha256': hashlib.sha256(parser.data).hexdigest(), 'tree': tree,
            'calls': parser.calls, 'strings': parser.strings}

first = ROOT/'analysis/research/extras_save_graph'
second = ROOT/'analysis/research/extras_utils_graph'
gi = script(first, 'GameInstance', 'CallSaveGame', 0x8a, 100, ['SaveGame'])
assert gi['tree'][0]['context']['object'] == '/Game/Blueprints/Utils/VR4Utils.Default__VR4Utils_C'
utils = script(second, 'Utils', 'SaveGame', 0xf0, 302, ['GetSaveGame', '/Script/Engine.GameplayStatics.SaveGameToSlot', 'SetLaserColor', '/Script/Engine.KismetSystemLibrary.PrintString'])
write = utils['calls'][1]
assert write['arguments'][1:] == [{'string': 'System'}, {'int': 0}]
assert write['arguments'][0] == {'field': ['CallFunc_GetSaveGame_VR4SaveGame'], 'owner': 3}
forbidden = ('AutoSave', 'CardSave', 'CardSysSave')
assert not any(any(x in i['path'] for x in forbidden) for i in json.loads((second/'Utils.package.json').read_text())['imports'])
result = {'GameInstance_CallSaveGame': gi, 'Utils_SaveGame': utils,
          'conclusion': 'The exact original Blueprint saves the reflected UE4 object to System.sav. Native pSys persists through a separate cCard serializer.'}
(second/'save_graph.json').write_text(json.dumps(result, indent=2)+'\n')
print('PASS: complete bounded GI100B + Utils302B original scripts; all call operands resolved; System slot0 metadata route, no native system-save call')
