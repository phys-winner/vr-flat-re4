"""Read only the selected original cooked script streams for flat UI evidence."""
from pathlib import Path
import ast
import json
import struct

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = ROOT / 'analysis/research/flat_ui_graph'
source = ast.parse((ROOT / 'analysis/research/check_extras_save_bp.py').read_text())
definition = next(node for node in source.body if isinstance(node, ast.ClassDef) and node.name == 'Script')
namespace = {'struct': struct}
exec(compile(ast.Module(body=[definition], type_ignores=[]), '<original Script parser>', 'exec'), namespace)
BaseScript = namespace['Script']


class Script(BaseScript):
    def __init__(self, *args):
        super().__init__(*args)
        self.runtime_delta = 0

    def field(self):
        before = self.at
        delta = self.runtime_delta
        value = super().field()
        self.runtime_delta = delta + 8 - (self.at-before)
        return value

    def ref(self):
        value = super().ref()
        self.runtime_delta += 4
        return value

    def fname(self):
        index, number = struct.unpack('<II', self.take(8))
        assert 0 <= index < len(self.names) and number < 10000
        self.runtime_delta += 4
        return self.names[index] + ('_' + str(number-1) if number else '')

    def expression(self, depth=0):
        assert depth < 64
        at = self.at
        token = self.data[self.at]
        if token in (0x22, 0x23, 0x2b):
            self.take(1)
            count = 10 if token == 0x2b else 3
            return {'vector_token': token, 'values': struct.unpack('<'+'f'*count, self.take(count*4))}
        if token == 0x69:
            self.take(1)
            count = struct.unpack('<H', self.take(2))[0]
            assert count <= 256
            end = self.integer()
            index = self.expression(depth+1)
            cases = []
            for _ in range(count): cases.append({'match': self.expression(depth+1), 'next': self.integer(), 'result': self.expression(depth+1)})
            return {'switch_value': index, 'end': end, 'cases': cases, 'default': self.expression(depth+1)}
        if token == 0x61:
            self.take(1)
            return {'bind_delegate': self.fname(), 'delegate': self.expression(depth+1), 'object': self.expression(depth+1)}
        if token in (0x5c, 0x62):
            self.take(1)
            return {'delegate_operation': token, 'target': self.expression(depth+1), 'value': self.expression(depth+1)}
        if token == 0x63:
            self.take(1)
            signature = self.ref()
            delegate = self.expression(depth+1)
            arguments = []
            while self.data[self.at] != 0x16: arguments.append(self.expression(depth+1))
            self.take(1)
            return {'call_delegate': signature, 'delegate': delegate, 'arguments': arguments}
        if token == 0x64:
            self.take(1)
            return {'let_persistent': [self.field(), self.expression(depth+1)]}
        if token == 0x67:
            self.take(1)
            return {'soft_object': self.expression(depth+1)}
        if token in (0x0f, 0x13, 0x1b, 0x1c, 0x26, 0x2e, 0x31, 0x38, 0x42, 0x43, 0x44, 0x46, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x51, 0x5f):
            self.take(1)
            if token == 0x0f: return {'let': [self.field(), self.expression(depth+1), self.expression(depth+1)]}
            if token in (0x1b, 0x1c, 0x46):
                name = self.fname() if token == 0x1b else self.ref()
                arguments = []
                while self.data[self.at] != 0x16: arguments.append(self.expression(depth+1))
                self.take(1)
                call = {'disk_offset': at, 'call': name, 'arguments': arguments}
                self.calls.append(call)
                return call
            if token == 0x26: return {'byte': self.take(1)[0]}
            if token in (0x13, 0x2e): return {'cast': self.ref(), 'value': self.expression(depth+1)}
            if token == 0x42: return {'struct_member': self.field(), 'value': self.expression(depth+1)}
            if token == 0x31:
                target = self.expression(depth+1)
                values = []
                while self.data[self.at] != 0x32: values.append(self.expression(depth+1))
                self.take(1)
                return {'set_array': target, 'values': values}
            if token == 0x38: return {'primitive_cast': self.take(1)[0], 'value': self.expression(depth+1)}
            if token == 0x4b: return {'instance_delegate': self.fname()}
            if token == 0x4c: return {'push_execution': self.integer()}
            if token == 0x4d: return {'pop_execution': True}
            if token == 0x4e: return {'computed_jump': self.expression(depth+1)}
            if token == 0x4f: return {'pop_execution_if_not': self.expression(depth+1)}
            if token == 0x51: return {'interface_context': self.expression(depth+1)}
            if token in (0x43, 0x44, 0x5f):
                return {('let_object' if token == 0x5f else 'let_delegate'): [self.expression(depth+1), self.expression(depth+1)]}
        if token in (0x21, 0x24, 0x25, 0x29, 0x2a, 0x36, 0x3a, 0x52, 0x5b):
            self.take(1)
            if token == 0x21: return {'name': self.fname()}
            if token in (0x24, 0x25): return {'byte_int': self.take(1)[0]}
            if token == 0x29:
                kind = self.take(1)[0]
                if kind == 4: return {'text_table': self.ref(), 'table': self.expression(depth+1), 'key': self.expression(depth+1)}
                assert kind in (0, 1, 2, 3), kind
                return {'text_kind': kind, 'parts': [self.expression(depth+1) for _ in range(3 if kind == 1 else 0 if kind == 0 else 1)]}
            if token == 0x36: return {'size': self.expression(depth+1)}
            if token == 0x3a: return {'double': struct.unpack('<d', self.take(8))[0]}
            if token == 0x52: return {'interface_cast': self.ref(), 'value': self.expression(depth+1)}
            if token == 0x5b: return {'skip_offset': self.integer()}
            return {'literal_token': token}
        return super().expression(depth)


def read_script(stem, name, offset=None):
    package = json.loads((DIRECTORY / (stem+'.package.json')).read_text())
    body = (DIRECTORY / (stem+'.uexp')).read_bytes()
    item = next(e for e in package['exports'] if e['name'] == name)
    payload = body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]
    candidates = [(at+4, struct.unpack_from('<I', payload, at)[0]) for at in range(4, len(payload)-16)
                  if at+4+struct.unpack_from('<I', payload, at)[0]+12 == len(payload) and payload[-13] == 0x53]
    if offset is None:
        assert len(candidates) == 1, (name, candidates)
        offset, count = candidates[0]
    else:
        offset, count = next(c for c in candidates if c[0] == offset)
    parser = Script(payload[offset:offset+count], package['names'], package['imports'], package['exports'])
    tree = []
    try:
        while parser.at < len(parser.data):
            at = parser.at
            runtime = at + parser.runtime_delta
            tree.append({'offset': at, 'runtime': runtime, 'expression': parser.expression()})
    except AssertionError as exc:
        print(stem, name, 'PARSE FAIL at', hex(parser.at), str(exc), 'next', parser.data[parser.at:parser.at+32].hex())
        raise
    runtime_expected = struct.unpack_from('<I', payload, offset-8)[0]
    assert parser.at + parser.runtime_delta == runtime_expected, (name, parser.at + parser.runtime_delta, runtime_expected)
    result = dict(export=item, script_offset=offset, script_bytes=count, runtime_bytes=runtime_expected, tree=tree, calls=parser.calls)
    (DIRECTORY / (stem+'.'+name.replace('/', '_')+'.script.json')).write_text(json.dumps(result, indent=2)+'\n')
    print(stem, name, 'parsed', count, 'disk /', runtime_expected, 'runtime bytes;', len(parser.calls), 'resolved calls')
    return result


if __name__ == '__main__':
    read_script('ComfortCustomizeCoverScreen_BP', 'AdvanceScreen')
    read_script('ComfortCustomizeCoverScreen_BP', 'SaveComfortCompleted', 375)
    read_script('CutsceneTheatreBox', 'SetHeadlockedScreen')
