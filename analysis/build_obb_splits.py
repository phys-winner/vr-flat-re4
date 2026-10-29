"""Package each immutable whole OBB as one stored ZIP32 asset split; no extraction/install."""
import os
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import zipfile
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parent / 'research'))
from audit_large_apk_signature import audit

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ['ANDROID_SDK_ROOT'])
SIGNER = Path(os.environ['RE4_TOOLS_DIR'])
INPUTS = {
    'main': (4278831240, 'c1d9ec3b9575870fa58f3cf651bff478a4bf69977747db47c15dafebf80aee16'),
    'patch': (4237413580, '4e6873968c7a1eda463cfdb8ed3633faee6a8a5bf99c3563181efce3e19c7eb8'),
}


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''): result.update(block)
    return result.hexdigest()


def stored_zip32(destination, manifest, obb, size, expected):
    entries = []
    with destination.open('w+b') as output:
        for name, payload, length in (('AndroidManifest.xml', manifest, len(manifest)), ('assets/' + obb.name, obb, size)):
            encoded = name.encode('utf-8')
            offset = output.tell()
            padding = (-(offset + 30 + len(encoded) + 4)) & 3
            extra = struct.pack('<HH', 0xd935, padding) + b'\0' * padding
            output.write(struct.pack('<IHHHHHIIIHH', 0x04034b50, 20, 0, 0, 0, 33, 0, length, length, len(encoded), len(extra)))
            output.write(encoded); output.write(extra)
            data_offset = output.tell()
            assert data_offset % 4 == 0 and length < 0xffffffff
            checksum = 0
            sha = hashlib.sha256()
            if isinstance(payload, bytes):
                output.write(payload); checksum = zlib.crc32(payload); sha.update(payload)
            else:
                with payload.open('rb') as source:
                    for block in iter(lambda: source.read(1024 * 1024), b''):
                        output.write(block); checksum = zlib.crc32(block, checksum); sha.update(block)
                assert sha.hexdigest() == expected, 'Original OBB input changed'
            end = output.tell()
            assert end - data_offset == length
            output.seek(offset + 14); output.write(struct.pack('<I', checksum)); output.seek(end)
            entries.append((encoded, offset, length, checksum))
        central = output.tell()
        for name, offset, length, checksum in entries:
            output.write(struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 20, 20, 0, 0, 0, 33, checksum, length, length, len(name), 0, 0, 0, 0, 0, offset))
            output.write(name)
        central_size = output.tell() - central
        output.write(struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, len(entries), len(entries), central_size, central, 0))
        assert output.tell() < 0xffffffff
        output.flush()
    with zipfile.ZipFile(destination) as archive:
        assert archive.namelist() == ['AndroidManifest.xml', 'assets/' + obb.name]
        entry = archive.getinfo('assets/' + obb.name)
        assert entry.file_size == size and entry.compress_type == zipfile.ZIP_STORED and entry.extract_version == 20
        assert entry.CRC == entries[1][3]


def build(name):
    size, expected = INPUTS[name]
    directory = ROOT / 'analysis/build/obb_splits_noverity'
    directory.mkdir(parents=True, exist_ok=True)
    obb = ROOT / 'VRLOC.com.Armature.VR4' / (name + '.201.com.Armature.VR4.obb')
    assert obb.stat().st_size == size
    final = directory / ('config.mobile_' + name + '.apk')
    receipt = directory / ('config.mobile_' + name + '.json')
    if final.exists() and receipt.exists():
        old = json.loads(receipt.read_text())
        if old.get('package') == 'com.Armature.VR4' and old['obb_sha256'] == expected and old['apk_sha256'] == digest(final) and not audit(final)['apk_verity_present']:
            print('PASS: exact cached whole OBB split', final, flush=True); return
    source_manifest = directory / ('manifest_' + name + '.xml')
    source_manifest.write_text('<?xml version="1.0" encoding="utf-8"?>\n'
        '<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.Armature.VR4" '
        'android:versionCode="201" android:versionName="2.1" split="config.mobile_' + name + '" android:isFeatureSplit="false">'
        '<uses-sdk android:minSdkVersion="23" android:targetSdkVersion="37" />'
        '<application android:hasCode="false" /></manifest>\n', encoding='utf-8')
    compiled = directory / ('manifest_' + name + '.apk')
    subprocess.run([str(SDK / 'build-tools/36.0.0/aapt2.exe'), 'link', '--manifest', str(source_manifest),
                    '-I', str(SDK / 'platforms/android-36/android.jar'), '-o', str(compiled)], check=True)
    with zipfile.ZipFile(compiled) as archive: manifest = archive.read('AndroidManifest.xml')
    stored_zip32(final, manifest, obb, size, expected)
    # Android APK-verity casts the signing-block offset to int for these >2 GiB assets.
    signer_jar = SDK / 'build-tools/36.0.0/lib/apksigner.jar'
    subprocess.run(['java', '--enable-native-access=ALL-UNNAMED', '-jar', str(signer_jar), 'sign',
                    '--key', str(SIGNER / 'testkey.pk8'), '--cert', str(SIGNER / 'testkey.x509.pem'),
                    '--v1-signing-enabled', 'true', '--v2-signing-enabled', 'true', '--v3-signing-enabled', 'true',
                    '--v4-signing-enabled', 'false', '--verity-enabled', 'false', str(final)], check=True)
    algorithms = audit(final)
    assert not algorithms['apk_verity_present'] and set(algorithms['schemes']) == {'v2', 'v3'}
    assert all(group == [['0x103']] for group in algorithms['schemes'].values())
    verified = subprocess.check_output(['java', '--enable-native-access=ALL-UNNAMED', '-jar', str(signer_jar),
                                        'verify', '--verbose', '--print-certs', str(final)], text=True)
    assert 'a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc' in verified
    subprocess.run([str(SDK / 'build-tools/36.0.0/zipalign.exe'), '-c', '4', str(final)], check=True)
    result = {'package': 'com.Armature.VR4', 'version_code': 201, 'split': 'config.mobile_' + name,
              'obb_path': obb.relative_to(ROOT).as_posix(), 'obb_bytes': size, 'obb_sha256': expected,
              'asset': 'assets/' + obb.name, 'compression': 'stored', 'zip': 'ZIP32', 'automatic_extraction': False,
              'apk_bytes': final.stat().st_size, 'apk_sha256': digest(final),
              'certificate_sha256': 'a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc',
              'apk_verity_enabled': False, 'signature_audit': algorithms}
    receipt.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: signed/aligned whole OBB asset split', final, final.stat().st_size, result['apk_sha256'], flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', choices=INPUTS)
    args = parser.parse_args()
    for name in ([args.name] if args.name else INPUTS): build(name)
