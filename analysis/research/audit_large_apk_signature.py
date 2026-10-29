"""Bounded APK signing-block algorithm audit; the multi-GB payload is never loaded."""
from pathlib import Path
import argparse
import json
import struct

SCHEMES = {0x7109871a: 'v2', 0xf05368c0: 'v3'}
VERITY_ALGORITHMS = {0x421, 0x423, 0x425}


def length_field(data, offset):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError('Truncated length-prefixed field')
    length, = struct.unpack_from('<I', data, offset)
    start, end = offset + 4, offset + 4 + length
    if end > len(data):
        raise ValueError('Length-prefixed field exceeds its owner')
    return data[start:end], end


def records(data):
    offset = 0
    while offset < len(data):
        record, offset = length_field(data, offset)
        yield record


def algorithms(sequence):
    result = []
    for record in records(sequence):
        if len(record) < 8:
            raise ValueError('Truncated algorithm record')
        algorithm, = struct.unpack_from('<I', record)
        _, end = length_field(record, 4)
        if end != len(record):
            raise ValueError('Algorithm record trailing bytes')
        result.append(algorithm)
    return result


def audit(path):
    size = path.stat().st_size
    with path.open('rb') as stream:
        stream.seek(max(0, size - 65557))
        tail = stream.read(65557)
        end = tail.rfind(b'PK\x05\x06')
        if end < 0 or end + 22 > len(tail):
            raise ValueError('No ZIP32 end record')
        eocd = tail[end:]
        if len(eocd) != 22 + struct.unpack_from('<H', eocd, 20)[0]:
            raise ValueError('Ambiguous ZIP end record')
        disk, central_disk, disk_entries, entries, central_size, central = struct.unpack_from('<HHHHII', eocd, 4)
        if disk or central_disk or entries != disk_entries or central + central_size != size - len(eocd):
            raise ValueError('Unsupported ZIP32 layout')
        if central < 24:
            raise ValueError('No APK signing footer')
        stream.seek(central - 24)
        block_length, magic = struct.unpack('<Q16s', stream.read(24))
        if magic != b'APK Sig Block 42' or not 24 <= block_length <= 1024 * 1024:
            raise ValueError('Invalid or oversized APK signing block')
        start = central - block_length - 8
        if start < 0:
            raise ValueError('Invalid signing-block offset')
        stream.seek(start)
        block = stream.read(block_length + 8)
    if len(block) != block_length + 8 or struct.unpack_from('<Q', block)[0] != block_length:
        raise ValueError('APK signing-block size disagreement')
    result = {'apk_bytes': size, 'central_directory_offset': central, 'signing_block_offset': start, 'schemes': {}}
    offset = 8
    while offset < len(block) - 24:
        if offset + 12 > len(block) - 24:
            raise ValueError('Truncated signing-block entry')
        length, identifier = struct.unpack_from('<QI', block, offset)
        end = offset + 8 + length
        if length < 4 or end > len(block) - 24:
            raise ValueError('Invalid signing-block entry bounds')
        value = block[offset + 12:end]
        offset = end
        if identifier not in SCHEMES:
            continue
        signers, consumed = length_field(value, 0)
        if consumed != len(value):
            raise ValueError('Signer sequence trailing bytes')
        groups = []
        for signer in records(signers):
            signed, cursor = length_field(signer, 0)
            digests, _ = length_field(signed, 0)
            if identifier == 0xf05368c0:
                cursor += 8
            signatures, cursor = length_field(signer, cursor)
            _, cursor = length_field(signer, cursor)
            if cursor != len(signer):
                raise ValueError('Signer trailing bytes')
            digest_ids, signature_ids = algorithms(digests), algorithms(signatures)
            if digest_ids != signature_ids or not signature_ids:
                raise ValueError('Digest/signature algorithms disagree')
            groups.append([hex(item) for item in signature_ids])
        if not groups or SCHEMES[identifier] in result['schemes']:
            raise ValueError('Missing or duplicate APK signing scheme')
        result['schemes'][SCHEMES[identifier]] = groups
    if offset != len(block) - 24 or not result['schemes']:
        raise ValueError('Invalid block consumption or absent v2/v3 signature')
    result['apk_verity_present'] = any(int(item, 16) in VERITY_ALGORITHMS
        for group in result['schemes'].values() for signer in group for item in signer)
    result['android_verity_offset_overflows'] = result['apk_verity_present'] and start > 0x7fffffff
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--require-no-verity', action='store_true')
    args = parser.parse_args()
    result = audit(args.apk)
    if args.require_no_verity and result['apk_verity_present']:
        raise SystemExit('Refusing APK-verity algorithm on this large APK')
    print(json.dumps(result, indent=2))
