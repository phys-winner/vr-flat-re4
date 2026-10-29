"""Build bounded ZIP/ZIP64 fixtures for the production cache parser; no original data is changed."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import warnings
import zipfile

work = Path(sys.argv[1])
root = work / 'cache-zips'
root.mkdir()
names = ['main.201.com.Armature.VR4.obb', 'patch.201.com.Armature.VR4.obb']
payloads = [b'PK\x03\x04' + bytes(range(80)), b'PK\x03\x04' + b'patch payload' * 9]
for label, method in [('stored', 0), ('compressed', 8), ('duplicate', 0), ('missing', 0), ('wrongsize', 0)]:
    with warnings.catch_warnings(), zipfile.ZipFile(root / (label + '.zip'), 'w', compression=method) as archive:
        warnings.simplefilter('ignore', UserWarning)
        archive.writestr(names[0], payloads[0])
        if label != 'missing':
            archive.writestr('cache/' + names[1], payloads[1] + (b'bad' if label == 'wrongsize' else b''))
        if label == 'duplicate': archive.writestr(names[0], payloads[0])
with zipfile.ZipFile(root / 'legacy.zip', 'w') as archive:
    for name, payload in zip(names, payloads):
        archive.writestr(name.replace('.201.com.', '.201.VRLOC.com.'), payload)

data = (root / 'stored.zip').read_bytes()
with zipfile.ZipFile(root / 'stored.zip') as archive:
    first = archive.infolist()[0]
    offset = first.header_offset + 30 + len(first.filename.encode()) + len(first.extra)
bad = bytearray(data); bad[offset + 7] ^= 1
(root / 'badcrc.zip').write_bytes(bad)
bad = bytearray(data); local = data.index(b'PK\x03\x04'); central = data.index(b'PK\x01\x02')
struct.pack_into('<H', bad, local + 6, 1); struct.pack_into('<H', bad, central + 8, 1)
(root / 'encrypted.zip').write_bytes(bad)
bad = bytearray(data); bad[30] ^= 1
(root / 'mismatched.zip').write_bytes(bad)
bad = bytearray(data); struct.pack_into('<I', bad, central + 42, 0xfffffff0)
(root / 'badbounds.zip').write_bytes(bad)
(root / 'truncated.zip').write_bytes(data[:-1])

# A sparse nine-gigabyte fixture proves ZIP64 directory and entry offsets without copying the OBBs.
sizes = [4278831240, 4237413580]
locations = [0, 5000000000]
directory = 9500000000
with (root / 'zip64.zip').open('w+b') as output:
    if os.name == 'nt':
        import ctypes
        import msvcrt
        count = ctypes.c_ulong()
        assert ctypes.windll.kernel32.DeviceIoControl(ctypes.c_void_p(msvcrt.get_osfhandle(output.fileno())),
            0x900c4, None, 0, None, 0, ctypes.byref(count), None), 'Cannot mark test fixture sparse'
    records = []
    for name, size, location in zip(names, sizes, locations):
        encoded = name.encode()
        output.seek(location)
        output.write(struct.pack('<IHHHHHIIIHH', 0x04034b50, 45, 0, 0, 0, 0, 0, size, size, len(encoded), 0) + encoded)
        output.write(b'PK\x03\x04')
        extra = struct.pack('<HHQ', 1, 8, location) if location > 0xffffffff else b''
        records.append(struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 45, 45, 0, 0, 0, 0, 0,
            size, size, len(encoded), len(extra), 0, 0, 0, 0, min(location, 0xffffffff)) + encoded + extra)
    output.seek(directory)
    central = b''.join(records)
    output.write(central)
    zip64 = output.tell()
    output.write(struct.pack('<IQHHIIQQQQ', 0x06064b50, 44, 45, 45, 0, 0, 2, 2, len(central), directory))
    output.write(struct.pack('<IIQI', 0x07064b50, 0, zip64, 1))
    output.write(struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 2, 2, len(central), 0xffffffff, 0))

subprocess.run([os.path.join(os.environ['JAVA_HOME'], 'bin/java.exe'), '-ea', '-cp', str(work),
    'com.epicgames.ue4.MobileCacheZipTest', str(root), str(len(payloads[0])), str(len(payloads[1]))], check=True)
print('PASS: production ZIP parser/extractor, sparse ZIP64 >9GB, malformed ZIPs and transactional recovery')
