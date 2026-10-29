"""Build a UE4 v3-style pak matching the VRLOC _p.pak layout (verified byte-for-byte)."""
import struct, hashlib, sys, os

def build_pak(files: dict, out_path: str):
    """files: {name_in_pak: bytes}"""
    body = bytearray()
    entries = []
    for name, data in files.items():
        h = hashlib.sha1(data).digest()
        off = len(body)
        entry = struct.pack('<qqqI', 0, len(data), len(data), 0) + h + b'\0' * 5
        body += entry + data
        entries.append((name, off, len(data), h))
    idx = bytearray()
    mnt = b'../../../\x00'
    idx += struct.pack('<i', len(mnt)) + mnt
    idx += struct.pack('<i', len(entries))
    for name, off, size, h in entries:
        nb = name.encode() + b'\x00'
        idx += struct.pack('<i', len(nb)) + nb
        idx += struct.pack('<qqqI', off, size, size, 0) + h + b'\x00' * 5
    idx_off = len(body)
    footer = struct.pack('<IIqq', 0x5A6F12E1, 3, idx_off, len(idx)) + hashlib.sha1(idx).digest()
    with open(out_path, 'wb') as f:
        f.write(body)
        f.write(idx)
        f.write(footer)
    print(f'wrote {out_path}: {len(entries)} entries, {os.path.getsize(out_path)} bytes')

if __name__ == '__main__':
    out = sys.argv[1]
    files = {}
    for spec in sys.argv[2:]:
        name, path = spec.split('=', 1)
        files[name] = open(path, 'rb').read()
    build_pak(files, out)
