"""Read-only original reload cache ABI/layout proof; no helper/device execution."""
from pathlib import Path
import hashlib
import struct


root = Path(__file__).resolve().parents[2]
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
assert native[:6] == b"\x7fELF\x02\x01"
assert struct.unpack_from("<H", native, 18)[0] == 183
phoff = struct.unpack_from("<Q", native, 32)[0]
phsize, phnum = struct.unpack_from("<HH", native, 54)
segments = [struct.unpack_from("<IIQQQQQQ", native, phoff + i * phsize)
            for i in range(phnum)]


def data(address, size):
    for kind, flags, offset, virtual, physical, filesz, memsz, align in segments:
        if kind == 1 and virtual <= address and address + size <= virtual + filesz:
            start = offset + address - virtual
            return native[start:start + size]
    raise AssertionError(f"not file-backed PT_LOAD: {address:#x}+{size:#x}")


def words(address, expected):
    actual = struct.unpack("<" + "I" * len(expected), data(address, 4 * len(expected)))
    assert actual == expected, (hex(address), actual)


ranges = (
    (0x61e3c4c, 0x61e4288, "42a290d051b4e8f7a6e6681f7fa1a8044228711ef9512cb84a052f24bb524c17"),
    (0x61e33c8, 0x61e33fc, "1492b04bdca6ae82d15b0f2c39b0e3b6526590867bfdcb092f3c000507922ecc"),
    (0x61e369c, 0x61e36d8, "1353f3322861de48db2d22ed3f6f9db5f7bbfc630b0afcdd4a1f987c0ad200a4"),
    (0x629130c, 0x6291358, "8d8def4ef8617814e91e5a5b54592ddaacadb8e866e81f68701d668b78c3f7a1"),
    (0x629032c, 0x6290414, "40fd58c63763898cf32a34265e5f791c21ebada599bb87746814a099f04cb113"),
    (0x6273b84, 0x6273d90, "9a3c79ae6eaba8ab906552813fb3340a433af7f946ffd6ae4a650438324bd648"),
    (0x6271d00, 0x6271d40, "75dafeb4dcdf4afc9a6cdded0df5d2a33e1bf71d49b656c47e7801743dcd5bc1"),
    (0x628b120, 0x628b1b4, "a05f2e86574e00f17c182a097f52a3a723e9beba5d223631391c3f498076a312"),
    (0x83a1bc8, 0x83a1cf8, "c90502359c400355f50d9ff55cc2cdf8a973729ff2a4cd0e8866a4123670f847"),
    (0x83a96f4, 0x83a984c, "d708e4541074d441f2b4a6a1daa72dc956476015bfa63b33b34a7103e46687f7"),
    (0x8337b58, 0x8337bc0, "d09bdd2d5423f80611175cd60e4ab22f5aa29986c0940674cfae4c05975a62de"),
    (0x83364a4, 0x8336504, "ac34b1015c8c99567c125e634ac4ee0a772034938cd917ce847ffef17cee0a2f"),
)
for start, end, digest in ranges:
    assert hashlib.sha256(data(start, end - start)).hexdigest() == digest, hex(start)

# Hidden x8 return destination and scalar-packed output stores.
words(0x61e3cb4, (0xaa0803f3,))
words(0x61e3d14, (0xb9002668, 0xf801c269))
words(0x61e4184, (0x2d038e64,))
words(0x61e4190, (0xbd002662,))
words(0x61e426c, (0x2d020660, 0xbd001a62))
words(0x629134c, (0xad008500, 0x3d800102))
# Cache constructor: attach parent, socket name, relative TF into cache+0x10.
words(0x61e3c54, (0xf9406028, 0xf9000008, 0xf9406428, 0xf9000408,
                  0x91004008, 0xaa0103e0, 0x948716a2))
# Geometry follows the base cache; it is not Scale3D.
words(0x61e33dc, (0x9400021c, 0xf9421268, 0xb9442a69, 0xf9002288, 0xb9004a89))
words(0x61e36b0, (0x94000167, 0xb9442668, 0xb9004288, 0xbd442260,
                  0xbd442661, 0x1e213800, 0xbd004680))
for params, size in ((0x9d59548, 0x40), (0x9d59060, 0x50), (0x9d591f0, 0x50)):
    assert struct.unpack("<I", data(params + 32, 4))[0] == size, hex(params)
    assert struct.unpack("<I", data(params + 40, 4))[0] == 16, hex(params)
for params, offset in ((0x9da4558, 0x580), (0x9da4528, 0x588),
                       (0x9da3c10, 0xc80), (0x9da3be0, 0xcd0),
                       (0x9d96b38, 0x510), (0x9d96810, 0x5b0), (0xa235a30, 0x134)):
    assert struct.unpack("<I", data(params + 36, 4))[0] == offset, hex(params)
    assert struct.unpack("<I", data(params + 32, 4))[0] == 1, hex(params)

# Independently prove the original Identity constant reached by the cache ctor.
dynamic = next(p for p in segments if p[0] == 2)
tags = {}
for address in range(dynamic[3], dynamic[3] + dynamic[5], 16):
    tag, value = struct.unpack("<QQ", data(address, 16))
    if not tag:
        break
    tags[tag] = value
relocations = {}
for address in range(tags[7], tags[7] + tags[8], 24):
    target, info, addend = struct.unpack("<QQq", data(address, 24))
    if target in (0xa3527f0, 0xa3527e0):
        relocations[target] = (info, addend)
assert relocations[0xa3527f0] == (0x403, 0x4315110)
assert relocations[0xa3527e0] == (0x403, 0xa6be1bc)  # FVector::OneVector
assert struct.unpack("<12f", data(0x4315110, 48)) == (0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0)
# USceneComponent native defaults: RelativeScale.X=1 and Y/Z=1.
words(0x83a1ca0, (0x52a7f009,))
words(0x83a1cb0, (0xb9013669,))
words(0x83a1cdc, (0x0f03f600,))
words(0x83a1ce4, (0xfd009e60,))
assert struct.unpack("<2f", data(0x42fabf8, 8)) == (44, 22)
words(0x83364c0, (0x52a84009, 0xb9042a69, 0x0f026440))

print("PASS: 12 original native ranges; x8 scalar FTransform48 return, cache base64/derived80 layouts")
print("PASS: 7 reflected offsets, ammo/gun BeginPlay callers, native target/capsule overlap paths")
print("PASS: original Identity relocation/bytes, native unit scene scale and capsule/box geometry defaults")
print("LIMIT: cooked Blueprint overrides and actual runtime scale are not measured by this check")
