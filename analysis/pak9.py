"""UE4 pak v3/v8/v9 index parser and stored/Zlib reader (pak inside OBB zip)."""
import hashlib
import struct
import sys
import zipfile
import zlib


class Pak9:
    def __init__(self, obb_path, member):
        self.z = zipfile.ZipFile(obb_path)
        self.info = self.z.getinfo(member)
        self.f = self.z.open(member)
        self.size = self.info.file_size
        self.f.seek(max(0, self.size - 1024))
        tail = self.f.read(1024)
        m = tail.rfind(b'\xe1\x12\x6f\x5a')
        if m < 0:
            raise ValueError('pak magic not found')
        self.version, self.idx_off, self.idx_size = struct.unpack_from('<IQQ', tail, m + 4)
        if self.version not in (3, 8, 9) or self.idx_off + self.idx_size > self.size:
            raise ValueError('unsupported version or invalid index bounds')
        self.methods = ([''] + [tail[m + 45 + i * 32:m + 77 + i * 32].split(b'\0')[0].decode()
                               for i in range(5)] if self.version >= 8 else ['', 'Zlib'])
        self.f.seek(self.idx_off)
        self.idx = self.f.read(self.idx_size)
        if hashlib.sha1(self.idx).digest() != tail[m + 24:m + 44]:
            raise ValueError('pak index SHA1 mismatch')
        self.mount, p = self.rstr(self.idx, 0)
        n, = struct.unpack_from('<i', self.idx, p)
        p += 4
        self.entries = {}
        for _ in range(n):
            name, p = self.rstr(self.idx, p)
            e, p = self.rentry(self.idx, p)
            if e['off'] + e['header_size'] + e['size'] > self.idx_off or name in self.entries:
                raise ValueError('invalid entry bounds or duplicate name')
            self.entries[name] = e
        if p != len(self.idx):
            raise ValueError('index was not consumed exactly')

    @staticmethod
    def rstr(b, p):
        n, = struct.unpack_from('<i', b, p)
        p += 4
        size = n if n >= 0 else -2 * n
        if not n or p + size > len(b):
            raise ValueError('invalid FString bounds')
        unit = 1 if n > 0 else 2
        if b[p + size - unit:p + size] != b'\0' * unit:
            raise ValueError('unterminated FString')
        return b[p:p + size - unit].decode('utf-8' if n > 0 else 'utf-16-le'), p + size

    @staticmethod
    def rentry(b, p):
        start = p
        off, size, unc, method = struct.unpack_from('<QQQI', b, p)
        p += 28
        h = b[p:p + 20]
        p += 20
        blocks = []
        if method:
            nblocks, = struct.unpack_from('<I', b, p)
            p += 4
            if nblocks > (len(b) - p) // 16:
                raise ValueError('invalid compression block count')
            blocks = [struct.unpack_from('<QQ', b, p + i * 16) for i in range(nblocks)]
            p += 16 * nblocks
        encrypted, cbs = struct.unpack_from('<BI', b, p)
        p += 5
        return dict(off=off, size=size, unc=unc, method=method, blocks=blocks,
                    encrypted=encrypted, cbs=cbs, hash=h, header_size=p - start), p

    def read_entry(self, name):
        e = self.entries[name]
        if e['encrypted']:
            raise ValueError('encrypted entry unsupported')
        if e['method'] and (e['method'] >= len(self.methods) or self.methods[e['method']] != 'Zlib'):
            # shortcut: Oodle entries remain compressed; add a licensed/local decoder when needed.
            raise ValueError('only stored/Zlib entries supported')
        self.f.seek(e['off'])
        local, _ = self.rentry(self.f.read(e['header_size']), 0)
        if any(local[k] != e[k] for k in ('size', 'unc', 'method', 'hash', 'blocks', 'encrypted', 'cbs')):
            raise ValueError('entry header differs from index')
        raw = self.f.read(e['size'])
        if len(raw) != e['size'] or hashlib.sha1(raw).digest() != e['hash']:
            raise ValueError('entry data SHA1 mismatch')
        data = raw
        if e['method']:
            chunks = []
            for s, t in e['blocks']:
                if self.version < 5:
                    s -= e['off']
                    t -= e['off']
                s -= e['header_size']
                t -= e['header_size']
                if not 0 <= s <= t <= len(raw):
                    raise ValueError('invalid compression block bounds')
                chunks.append(zlib.decompress(raw[s:t]))
            data = b''.join(chunks)
        if len(data) != e['unc']:
            raise ValueError('entry uncompressed size mismatch')
        return data


if __name__ == '__main__':
    pak = Pak9(sys.argv[1], sys.argv[2])
    print(f'version={pak.version} entries={len(pak.entries)}')
    pat = sys.argv[3] if len(sys.argv) > 3 else ''
    for name in pak.entries:
        if pat.lower() in name.lower():
            print(name)
