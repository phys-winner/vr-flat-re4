"""Read-only native pause page/menu contracts; no production or device actions."""
from pathlib import Path
import hashlib
import mmap
import struct

root = Path(__file__).resolve().parents[2]
with (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
    with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as b:
        for lo, hi, digest in (
            (0x630f158, 0x630f1d0, '18ac7765ffe1e34a5e895b19ceddfd39c272562004fd24fe46dcfd5dd5c311e1'),
            (0x630f5c4, 0x630f64c, '7372a12db22951ce662733e72a0764b538c85d976bd1621fdb03b5e9ef68dc20'),
            (0x64a7f18, 0x64a7fd4, '841187efbe238ba883c6c59d0aa7a8827deafcb1bf7271dc2acd74e10500a357'),
            (0x64a8138, 0x64a81f8, '332fe066e56f357e8b99a6c6c806cfed06806463e962c2f14c8624132734727c'),
            (0x6333144, 0x633314c, 'bc3bb93c4628093835e1048be14578dd397a3398732843a9dfa1367c689ad15c'),
            (0x6333160, 0x6333388, '39869f5524a6f9a361f6e73b3ba3fe11b72ba85b1b9a5a33a310021cb5fc510b'),
            (0x64a81f8, 0x64a8258, 'cf6ac8beb15e4458a7468c85581c0263d9ffc729cd8e66fcc71237ca5574ac83'),
            (0x6372cec, 0x6372d44, 'a72f6026af7537104b8618a931b0683a8d187fb93cb1c0213575579f5101ce4c'),
            (0x64db2e4, 0x64db35c, '28f9e37fa8774b78e5786ff2e7d383ca1a9b69a00bb978fae8d9d7911ba0b0f8'),
        ):
            assert hashlib.sha256(b[lo:hi]).hexdigest() == digest, hex(lo)
        for a, words in (
            (0x64a7fc8, (0x52800022, 0x910183ff)),  # One registered native callback.
            (0x64a81c0, (0x52804b04, 0x52800105)),  # Native class size 258, align8.
            (0x6333144, (0x79634800, 0xd65f03c0)),  # StartingInfo scalar getter.
            (0x630f5d0, (0xb9811809, 0x7100053f)),
            (0x630f5f0, (0xf940016c, 0xeb08019f)),
            (0x630f620, (0xaa1303e0, 0x2a1f03e2)),
            (0x630f62c, (0xa9417bfd, 0x91038260)),
            (0x630f1ac, (0x3944454a, 0x340000ea)),  # Entry enabled, otherwise return.
            (0x630f1bc, (0xb940a902, 0x9103a100)),
        ):
            assert struct.unpack_from('<II', b, a) == words, hex(a)
        for a, target in (
            (0x630f628, 0x630e400), (0x630f63c, 0x630ea60),
            (0x630f1c8, 0x630f034), (0x6474008, 0x630f5c4),
            (0x64740c0, 0x630f158), (0x64a81e0, 0x687bd44),
            (0x6372d04, 0x64db2e4), (0x64db2fc, 0x698e1dc),
            (0x64a8254, 0x6333d9c),
        ):
            op = struct.unpack_from('<I', b, a)[0]
            assert op & 0x7c000000 == 0x14000000, hex(a)
            imm = op & 0x3ffffff
            if imm & 0x2000000:
                imm -= 0x4000000
            assert a + 4 * imm == target, hex(a)
        for metadata, offset in (
            (0x9df6a28, 0x250), (0x9df1948, 0x258),
            (0x9df6400, 0x10f0), (0x9df63d0, 0x1118),
            (0x9df63a0, 0x1140), (0x9ddbf30, 0x110),
            (0x9ddbff0, 0xe0),
        ):
            assert struct.unpack_from('<I', b, metadata + 0x24)[0] == offset, hex(metadata)
        shoff = struct.unpack_from('<Q', b, 40)[0]
        shsize, shnum = struct.unpack_from('<HH', b, 58)
        rel = {}
        for i in range(shnum):
            section = struct.unpack_from('<IIQQQQIIQQ', b, shoff + i * shsize)
            if section[1] != 4:
                continue
            for a, info, addend in struct.iter_unpack('<QQq', memoryview(b)[section[4]:section[4] + section[5]]):
                if a in (0x9df6970, 0x9df6978):
                    assert info & 0xffffffff == 1027
                    rel[a] = addend
        assert rel[0x9df6978] == 0x64a7e60
        a = rel[0x9df6970]
        assert bytes(b[a:a + len(b'TryStartUnpause\0')]) == b'TryStartUnpause\0'

members = {line.split(' T ', 1)[1] for line in
           (root / 'analysis/ue4_syms.txt').read_text().splitlines()
           if ' T AVR4PauseUIScreen::' in line}
assert members == {
    'AVR4PauseUIScreen::TryStartUnpause()',
    'AVR4PauseUIScreen::execTryStartUnpause(UObject*, FFrame&, void*)',
    'AVR4PauseUIScreen::GetPrivateStaticClass()',
    'AVR4PauseUIScreen::StaticRegisterNativesAVR4PauseUIScreen()',
    'AVR4PauseUIScreen::AVR4PauseUIScreen(FVTableHelper&)',
}
print('PASS: 9 original native full ranges, 9 opcode pairs, 9 ordinary call targets, 7 property offsets')
print('PASS: native PauseUIScreen registrar has one TryStartUnpause callback, class258 and StartInfo250')
print('PASS: distinct map/entry/menu ABI contracts and bounded PauseUIScreen native symbol table')
print('LIMIT: actual Blueprint Inventory page binding and StartingInfo policy still require evidence; no production selection route asserted')
