"""Check the approved mobile locres patch, byte preservation, references and malformed inputs."""
from pathlib import Path
import hashlib
import json
import re
import struct
import sys

root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'analysis'))
from mobile_locres import MOBILE_TEXT, ORIGINAL_SHA256, build_mobile_locres
from research.locres_read import Reader, read_locres

original=(root/'analysis/research/VR4_patch_en_original.locres').read_bytes()
assert hashlib.sha256(original).hexdigest()==ORIGINAL_SHA256
before=read_locres(original)
assert (before['entry_count'],before['namespace_count'],before['localized_string_count'])==(4131,23,3175)
assert build_mobile_locres(original,{})==original
patched=build_mobile_locres(original)
assert build_mobile_locres(patched)==patched and build_mobile_locres(patched,{})==patched
after=read_locres(patched)
assert (after['entry_count'],after['namespace_count'])==(4131,23)
changed=[]
for old,new in zip(before['entries'],after['entries']):
    name=old['namespace']+'/'+old['key']
    for field in ('namespace','namespace_hash','key','key_hash','source_hash'):
        assert old[field]==new[field],(name,field)
    assert new['text']==MOBILE_TEXT.get(name,old['text']),name
    if old['text']!=new['text']:
        changed.append(name)
    if name not in MOBILE_TEXT:
        assert old==new,name
assert set(changed)<=MOBILE_TEXT.keys() and set(MOBILE_TEXT)<=set(e['namespace']+'/'+e['key'] for e in before['entries'])
assert len(MOBILE_TEXT)==238 and len(changed)==236
prior_keys=(
    'Tutorials/WepManTypes','Tutorials/WepManImIntro','Tutorials/WepManImKnifeInfo','Tutorials/WepManImPistolInfo',
    'Tutorials/WepManImComplete','Tutorials/WepManImWatchTitle','Tutorials/WepManImWatchInfo','Tutorials/Swing',
    'Tutorials/GrabWeapon','Tutorials/AButton','Tutorials/XButton','UIStrings/CustomizeHandedness',
    'UIStrings/CustomizeSitStandDesc','UIStrings/CustomizeComfortModeDesc',
)
historical_prior={key:MOBILE_TEXT[key] for key in prior_keys}
historical_prior.update({
    'Tutorials/WepManImKnifeInfo':'Нажмите значок <Red>ножа</>, затем значок выстрела, чтобы нанести удар.',
    'Tutorials/WepManImWatchInfo':'Следите за состоянием персонажа: здоровьем, боеприпасами и другими показателями.',
    'UIStrings/CustomizeSitStandDesc':'<Default.Text>Выберите положение виртуального персонажа: сидя или стоя. Настройка влияет на расположение снаряжения.</>',
    'UIStrings/CustomizeComfortModeDesc':'<Default.Text>Режим перемещения с телепортом.</>\n<Default.Text>Для управления плавающим стиком выберите полное перемещение.</>',
})
prior=build_mobile_locres(original,historical_prior)
assert hashlib.sha256(prior).hexdigest()=='fcf73afab94168e0a43dc72fe1a23054fad7d57b381cb3f9679484a7fa98f70a'
new_hashes={
    'DeathStrings/MainAshleyHealQuick':0x06ca2f1a,'DeathStrings/MainAshleyDumpster':0x100f2dbc,
    'DeathStrings/MainAshleyCommand':0x3e12b411,'DeathStrings/MainHealingItems':0x28f0647a,
    'DeathStrings/MercHealSlots':0x28f0647a,'DeathStrings/MainQuickTeleportBack':0xc8398230,
    'DeathStrings/MercTeleport':0xc8398230,'DeathStrings/MainQuick180Turn':0x5c11dc45,
    'DeathStrings/MercQuickTurn':0x5c11dc45,'Options/CutsceneQTEsDesc':0xecf5d675,
    'PlayingManual/QTEInfo':0xcc1f84f3,'UIStrings/CustomizePopupComfortDesc':0xfb517015,
    'Tutorials/HelpInfoInventory':0xab07cbb7,
}
assert set(prior_keys)|new_hashes.keys() <= set(MOBILE_TEXT)
old_entries={e['namespace']+'/'+e['key']:e for e in before['entries']}
for key,value in new_hashes.items():assert old_entries[key]['source_hash']==value,key
assert old_entries['Tutorials/WepManImWatchInfo']['source_hash']==27889858
assert '{WatchHandedness:' in old_entries['Tutorials/WepManImWatchInfo']['text']
# Pin the full approved scope and the original source hashes independently of
# replacement wording. No namespace, key or source hash may be invented.
approved_sources={key:old_entries[key]['source_hash'] for key in sorted(MOBILE_TEXT)}
assert hashlib.sha256(json.dumps(approved_sources,separators=(',',':'),sort_keys=True).encode()).hexdigest()==\
    'ffa96dc6f85489fbe63d73d1b8f67191158b206812efb231abb91134676eb7d7'
entries={e['namespace']+'/'+e['key']:e for e in after['entries']}
assert entries['Tutorials/AButton']['text']==entries['Tutorials/XButton']['text']=='Действие'
assert entries['Tutorials/AButton']['string_index']==entries['Tutorials/XButton']['string_index']
for first,second in (('MainHealingItems','MercHealSlots'),('MainQuickTeleportBack','MercTeleport'),('MainQuick180Turn','MercQuickTurn')):
    assert entries['DeathStrings/'+first]['string_index']==entries['DeathStrings/'+second]['string_index']
assert '180' not in entries['DeathStrings/MainQuick180Turn']['text']
assert 'правой части экрана' in entries['DeathStrings/MainQuick180Turn']['text']
for name in new_hashes:
    assert not any(token in entries[name]['text'].lower() for token in ('контроллер','курок','захват','[a]','[b]','[x]','[y]','правый стик','левый стик')),name
for name in ('Options/CutsceneQTEsDesc','PlayingManual/QTEInfo'):
    assert 'отключ' in entries[name]['text'].lower()
    assert not any(word in entries[name]['text'].lower() for word in ('кнопк','жест','встрях','действие знач')),name

# Scan offsets independently: only selected indices and affected reference counts may change.
r=Reader(original);r.pos=17;string_offset=r.unpack('<Q');r.unpack('<I');namespace_count=r.unpack('<I')
allowed=set()
for _ in range(namespace_count):
    namespace=r.key()['text']
    for _ in range(r.unpack('<I')):
        key=r.key()['text'];r.unpack('<I')
        if namespace+'/'+key in MOBILE_TEXT:
            allowed.update(range(r.pos,r.pos+4))
        r.unpack('<i')
assert r.pos==string_offset
assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(original[:string_offset],patched[:string_offset])))
old_count=r.unpack('<I');allowed.update(range(string_offset,string_offset+4))
old_strings=[]
for _ in range(old_count):
    old_strings.append(r.string());allowed.update(range(r.pos,r.pos+4));r.unpack('<i')
assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(original,patched[:len(original)])))
rr=Reader(patched);rr.pos=string_offset;new_count=rr.unpack('<I');new_strings=[];new_references=[]
for _ in range(new_count):
    new_strings.append(rr.string());new_references.append(rr.unpack('<i'))
assert new_strings[:old_count]==old_strings and rr.pos==len(patched)
expected=[0]*new_count
for entry in after['entries']:
    expected[entry['string_index']]+=1
assert new_references==expected and all(ref>=0 for ref in new_references)

empty=build_mobile_locres(original,{'Tutorials/Swing':'','Tutorials/GrabWeapon':''})
empty_entries={e['namespace']+'/'+e['key']:e for e in read_locres(empty)['entries']}
assert empty_entries['Tutorials/Swing']['text']==empty_entries['Tutorials/GrabWeapon']['text']==''
assert empty_entries['Tutorials/Swing']['string_index']==empty_entries['Tutorials/GrabWeapon']['string_index']
assert build_mobile_locres(empty,{'Tutorials/Swing':'','Tutorials/GrabWeapon':''})==empty
unicode_text='Тест \U0001f600'
unicode_result=read_locres(build_mobile_locres(original,{'Tutorials/Swing':unicode_text}))
assert next(e for e in unicode_result['entries'] if e['namespace']=='Tutorials' and e['key']=='Swing')['text']==unicode_text
for broken in (b'',original[:-1],b'X'+original[1:],original[:16]+b'\x03'+original[17:]):
    try:
        build_mobile_locres(broken,{})
        raise AssertionError('malformed locres accepted')
    except ValueError:
        pass
for replacements in ({'Tutorials/ShotgunInfo':'mobile'}, {'Tutorials/Swing':'bad\0text'}, {'Tutorials/Swing':None}):
    try:
        build_mobile_locres(original,replacements)
        raise AssertionError('invalid replacement accepted')
    except ValueError:
        pass
print(f'PASS: {len(changed)} changed approved keys; 4131 entries/23 namespaces/hashes/raw unselected strings preserved')
print('PASS: references, appended unique strings, no-op/repeat/empty/non-BMP UTF16 and malformed/unauthorized input guards')
print('PASS: historical original14 output SHA unchanged; all238 approved original key/source hashes pinned, shared translations and no unsupported QTE/instant180 claims')

from build_mobile_text import ENGINE_PATH, ENGINE_SHA256, build_mobile_engine
engine=(root/'analysis/research/graphics-evidence'/ENGINE_PATH).read_bytes()
assert hashlib.sha256(engine).hexdigest()==ENGINE_SHA256
mobile_engine=build_mobile_engine(engine)
setting=b'r.DefaultBackBufferPixelFormat=0\r\n'
assert mobile_engine.count(setting)==1 and mobile_engine.replace(setting,b'',1)==engine
assert b'[/Script/Engine.RendererSettings]\r\n'+setting in mobile_engine
for broken in (b'',engine[:-1],engine.replace(b'r.MobileHDR=False',b'r.MobileHDR=True')):
    try:
        build_mobile_engine(broken)
        raise AssertionError('unverified engine config accepted')
    except ValueError:
        pass
print('PASS: verified original engine config preserved byte-for-byte except one BGRA8 backbuffer setting; changed inputs refused')

from build_mobile_text import build_locale_resources, LOCALE_SHA256
locale_files,manifest=build_locale_resources()
assert len(locale_files)==len(manifest)==6
assert {item['locale'] for item in manifest}=={'ru',*LOCALE_SHA256}
for item in manifest:
    missing_by_locale={
        'ru':[], 'en':[], 'es-419':[],
        'de-DE':['Options/MoveStyleHybrid','Options/SteeringSettingHand','Tutorials/Trigger','Tutorials/TriggerU'],
        'fr-FR':['Options/MoveStyleFPS','Tutorials/Grip','Tutorials/GripU'],
        'ja-JP':['Options/MoveStyleFPS'],
    }
    missing=missing_by_locale[item['locale']]
    assert item['approved_mobile_keys_present']==len(MOBILE_TEXT)-len(missing)
    assert item['missing_mobile_keys_use_native_en_fallback']==missing
    payload=locale_files[item['path']]
    assert hashlib.sha256(payload).hexdigest()==item['mobile_sha256']
    parsed=read_locres(payload)
    assert (parsed['entry_count'],parsed['namespace_count'])==(item['entries'],item['namespaces'])
    source=original if item['locale']=='ru' else (root/'analysis/research/localization_originals'/(item['locale']+'_VR4.locres')).read_bytes()
    rr=Reader(source);rr.pos=17;old_offset=rr.unpack('<Q');rr.unpack('<I');ns_count=rr.unpack('<I');selected=set()
    for _ in range(ns_count):
        namespace=rr.key()['text']
        for _ in range(rr.unpack('<I')):
            name=namespace+'/'+rr.key()['text'];rr.unpack('<I')
            if name in MOBILE_TEXT:selected.update(range(rr.pos,rr.pos+4))
            rr.unpack('<i')
    assert rr.pos==old_offset
    assert all(a==b or position in selected for position,(a,b) in enumerate(zip(source[:old_offset],payload[:old_offset])))
    old_count=rr.unpack('<I');old_pool=[(rr.string(),rr.unpack('<i')) for _ in range(old_count)]
    new_reader=Reader(payload);new_reader.pos=old_offset;new_count=new_reader.unpack('<I');new_pool=[(new_reader.string(),new_reader.unpack('<i')) for _ in range(new_count)]
    assert [text for text,_ in old_pool]==[text for text,_ in new_pool[:old_count]]
    assert new_reader.pos==len(payload) and build_mobile_locres(payload, {})==payload
    # The culture's original metadata and every unselected translation stay
    # unchanged, including entries whose text was shared with a selected key.
    culture_before=read_locres(source)
    for first,second in zip(culture_before['entries'],parsed['entries']):
        name=first['namespace']+'/'+first['key']
        assert all(first[field]==second[field] for field in ('namespace','namespace_hash','key','key_hash','source_hash'))
        if name not in MOBILE_TEXT:assert first==second,name
assert not any('телепорт' in MOBILE_TEXT[name].lower() or 'виртуальн' in MOBILE_TEXT[name].lower() for name in ('UIStrings/CustomizeSitStandDesc','UIStrings/CustomizeComfortModeDesc','UIStrings/CustomizePopupComfortDesc'))
print('PASS: six real decoded locales, 1420 selected translations plus8 native English fallback keys; every original metadata/key/source hash/text-pool entry and remaining translation retained, no fake foreign keys')

english=json.loads((root/'analysis/mobile_text_en.json').read_text('utf-8'))
assert set(english)==set(MOBILE_TEXT)
for locale in ('ru',*LOCALE_SHA256):
    replacement=MOBILE_TEXT if locale=='ru' else json.loads((root/('analysis/mobile_text_'+locale+'.json')).read_text('utf-8'))
    assert set(replacement)==set(english)
    for key,text in replacement.items():
        assert isinstance(text,str) and '\0' not in text and '\ufffd' not in text,(locale,key)
        assert re.findall(r'<[^>]*>',text)==re.findall(r'<[^>]*>',english[key]),(locale,key)
print('PASS: all1428 supplied translations have valid Unicode and exactly matched rich-text markup across six cultures')
hold_terms={
    'ru':('удерживайте','отпустите','запясть','аптеч'),
    'en':('hold','release','wrist','medical bag'),
    'de-DE':('halte','lass los','handgelenk','medizintaschen'),
    'es-419':('mantén','suelta','muñeca','botiquín'),
    'fr-FR':('maintenez','relâchez','poignet','trousse de soins'),
    'ja-JP':('押し続け','離す','手首','救急バッグ'),
}
for locale,terms in hold_terms.items():
    text=MOBILE_TEXT if locale=='ru' else json.loads((root/('analysis/mobile_text_'+locale+'.json')).read_text('utf-8'))
    watch=text['Tutorials/WepManImWatchInfo'].lower()
    for term in terms[:3]:assert term in watch,(locale,term)
    for key in ('PlayingManual/HealingConsumablesImageText0','PlayingManual/HealingConsumablesInfo','Tutorials/WepManImFirstAidInfo'):
        assert terms[3] in text[key].lower(),(locale,key)
assert 'one press' in english['Tutorials/WepManImKnifeInfo'].lower() and 'holding repeats' in english['Tutorials/WepManImKnifeInfo'].lower()
assert 'full slash' in english['PlayingManual/KnifeInfo'].lower() and 'finish' in english['PlayingManual/KnifeInfo'].lower()
assert 'only on your wrist' in english['PlayingManual/WatchInfo'].lower()
assert 'health icon' not in english['PlayingManual/HealingConsumablesImageText0'].lower()
print('PASS: six cultures describe held health dial/release/wrist, separate medical bag selection, full knife cycle/hold-repeat and wrist-only gameplay ammo')
for key,text in english.items():
    assert not re.search(r'\b(VR|controllers?|thumbsticks?|teleport|waggle)\b',text,re.I),key
    assert not re.search(r'\b(grip|triggers?)\b',text,re.I),key
    assert not re.search(r'<img\s+id="Button|\{(?:Stick|Watch)?Handedness',text,re.I),key
for key in ('Options/MoveStyleTeleport','Options/Snap','Options/Quick180Turn','Tutorials/ChooseBoatChoice1','Tutorials/ChooseBoatChoice2'):
    assert 'inactive' in english[key].lower(),key
assert 'additional original input action' in english['Options/InspectObjectDesc']
assert 'camera' in english['Options/PlayingPositionDesc'].lower() and 'anchor' in english['Options/PlayingPositionDesc'].lower()
assert 'camera height' in english['UIStrings/CustomizeSitStandDesc'].lower()
assert not any(word in english['Tutorials/HelpInfoMap'].lower() for word in ('pinch','both fingers','zoom'))
assert not any(word in english['Tutorials/AshFoundSelectFollow'].lower() for word in ('tap','button','icon'))
assert not any(word in english['PlayingManual/MercsCharactersKrauserSpecialAttack'].lower() for word in ('tap','button','trigger'))
original_english=read_locres((root/'analysis/research/localization_originals/en_VR4.locres').read_bytes())
for entry in original_english['entries']:
    key=entry['namespace']+'/'+entry['key']
    text=english.get(key,entry['text'])
    assert not re.search(r'\b(VR|controllers?|thumbsticks?|teleport|waggle|grip|triggers|Wii)\b',text,re.I),key
    assert not re.search(r'<img\s+id="Button',text,re.I),key
    assert not re.search(r'\b(?:A|B|C|X|Y|Z)\s+Button\b|\+Control Pad|Nunchuk',text),key
print('PASS: selected English controls contain no VR/controller/button-icon instructions; fixed legacy choices, real camera-height profile and unsupported map/Ashley/Krauser control limits described honestly')
