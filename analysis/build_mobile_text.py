"""Build the bounded localization patch embedded in the native phone helper."""
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile, ZIP_STORED
import hashlib
import json
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))

from mobile_locres import build_mobile_locres, ORIGINAL_SHA256, PAK_PATH, MOBILE_TEXT
from pak_writer import build_pak
from pak9 import Pak9
from research.locres_read import read_locres
from build_mobile_startup import build_mobile_startup


ENGINE_PATH = 'VR4/Config/DefaultEngine.ini'
ENGINE_SHA256 = '8d8f154078f8781d800ee428ef58d038130dbc6b8604ccd97748f0163beedbae'
LOCALE_SHA256 = {
    'en': '6399970dd3ffef5d3b45ee15c03d9284d03a918a0ed98cf38b538297f4312b0f',
    'de-DE': 'c20919343037359d0a321c418f6135b079861ccfc598e04f79969f37ae23c45e',
    'es-419': '4ca5de6f01a2049ea8bf0e30037c977ddbd0cce1eeb6c8207cf2c9730eca7f3d',
    'fr-FR': '74c5550a4e6c5a476cb17380e86899c6cd8330f668734d418d17dd7bba9a4fc3',
    'ja-JP': 'fe59da5119e299c124511550ffec428fb7aec9e11e8ff955088e5071d7b6a7c9',
}


def build_locale_resources():
    directory=Path(__file__).resolve().parent
    russian=(directory/'research/VR4_patch_en_original.locres').read_bytes()
    if hashlib.sha256(russian).hexdigest()!=ORIGINAL_SHA256:
        raise ValueError('verified Russian fan translation changed')
    originals={'ru':russian}
    for locale,expected in LOCALE_SHA256.items():
        original=(directory/'research/localization_originals'/(locale+'_VR4.locres')).read_bytes()
        if hashlib.sha256(original).hexdigest()!=expected:
            raise ValueError('verified original locale changed: '+locale)
        originals[locale]=original
    files={};manifest=[]
    for locale,original in originals.items():
        replacements=MOBILE_TEXT if locale=='ru' else json.loads((directory/('mobile_text_'+locale+'.json')).read_text(encoding='utf-8'))
        if set(replacements)!=set(MOBILE_TEXT):
            raise ValueError('locale must cover the same approved mobile keys: '+locale)
        before=read_locres(original)
        names={entry['namespace']+'/'+entry['key'] for entry in before['entries']}
        present={key:text for key,text in replacements.items() if key in names}
        mobile=build_mobile_locres(original,present)
        after=read_locres(mobile)
        for first,second in zip(before['entries'],after['entries']):
            name=first['namespace']+'/'+first['key']
            assert all(first[field]==second[field] for field in ('namespace','namespace_hash','key','key_hash','source_hash'))
            assert second['text']==(present[name] if name in present else first['text'])
        assert before['entry_count']==after['entry_count'] and before['namespace_count']==after['namespace_count']
        path='VR4/Content/Localization/VR4/'+locale+'/VR4.locres'
        files[path]=mobile
        manifest.append({'locale':locale,'path':path,'original_bytes':len(original),'original_sha256':hashlib.sha256(original).hexdigest(),
                         'mobile_bytes':len(mobile),'mobile_sha256':hashlib.sha256(mobile).hexdigest(),'entries':before['entry_count'],
                         'namespaces':before['namespace_count'],'approved_mobile_keys_present':len(present),
                         'missing_mobile_keys_use_native_en_fallback':sorted(set(replacements)-set(present)),
                         'other_translations_and_source_hashes':'unchanged','startup_selection':'Activity Configuration locale -> original Android startup locale -> original FInternationalization'})
    return files,manifest


def build_mobile_engine(original):
    if hashlib.sha256(original).hexdigest() != ENGINE_SHA256:
        raise ValueError('verified engine config input changed')
    section = b'[/Script/Engine.RendererSettings]\r\n'
    if original.count(section) != 1 or b'r.DefaultBackBufferPixelFormat=' in original:
        raise ValueError('unexpected backbuffer config')
    # Slate and the mobile scene must use the same BGRA8 proxy backbuffer format.
    return original.replace(section, section + b'r.DefaultBackBufferPixelFormat=0\r\n', 1)


def main():
    directory = Path(__file__).resolve().parent
    files,locales = build_locale_resources()
    engine = build_mobile_engine((directory / 'research/graphics-evidence' / ENGINE_PATH).read_bytes())
    files[ENGINE_PATH]=engine
    startup_files,startup_audit=build_mobile_startup()
    assert not set(files)&set(startup_files)
    files.update(startup_files)
    pak_path = directory / 'paks/pakchunk99-Android_ETC2_999_P.pak'
    build_pak(files, pak_path)
    with TemporaryDirectory(prefix='re4-mobile-pak-') as temporary:
        archive = Path(temporary) / 'check.zip'
        with ZipFile(archive, 'w', compression=ZIP_STORED) as output:
            output.write(pak_path, 'mobile.pak')
        pak = Pak9(archive, 'mobile.pak')
        try:
            assert pak.version == 3 and pak.mount == '../../../'
            assert set(pak.entries) == set(files)
            for name, payload in files.items():
                assert pak.read_entry(name) == payload
        finally:
            pak.f.close()
            pak.z.close()
    data = pak_path.read_bytes()
    header = ['// Generated by build_mobile_text.py; do not edit.',
              'static const unsigned char g_mobile_text_pak[] = {']
    header += [','.join(str(value) for value in data[index:index+24])+','
               for index in range(0, len(data), 24)]
    header += ['};', '']
    (directory / 'mobile_text_pak.h').write_text('\n'.join(header), encoding='ascii')
    (directory/'research/mobile_locale_manifest.json').write_text(json.dumps({'locales':locales,'pak_entries':len(files),
        'pak_bytes':len(data),'pak_sha256':hashlib.sha256(data).hexdigest(),'new_hot_language_switch':False,
        'engine_config':'original bytes plus existing p269 BGRA8 setting only','startup':startup_audit},indent=2)+'\n',encoding='utf-8')
    print(f'PASS: six verified locales with {len(MOBILE_TEXT)} approved mobile keys, native partial-locale fallback, one unchanged backbuffer config; embedded PAK {len(files)} entries/{len(data)} bytes/SHA256 {hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    main()
