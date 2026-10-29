"""Check the original keyboard delegate graph and execute actual typewriter touch C."""
import os
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import inspect_flat_bp as bp
from inspect_ui_package import package

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'analysis/research/typewriter_originals'
header = (ROOT / 'analysis/re4fix_typewriter_touch.h').read_text()
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest() == '6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
ranges = (
    (0x87037cc, 0x870388c, '5b24c779e8101500e3ca7ea56dab5672a364aa35513da5d3d72e1daf966d161b'),
    (0x63380dc, 0x633813c, 'a88845849bfd2955538f10764dbebaee0e564cac8e7a7ee9e03f02002dfc9e81'),
    (0x6338aec, 0x6338b4c, 'c87933053272191a2fc96a433d815808082eadc6f410fd95f296875836aaceab'),
)
for low, high, expected in ranges:
    assert hashlib.sha256(native[low:high]).hexdigest() == expected
checks = [tuple(int(v, 16) for v in row) for row in re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}', header)]
assert len(checks) == 4
for address, first, second in checks:
    assert struct.unpack_from('<II', native, address) == (first, second)
receipt = json.loads((DATA / 'receipt.json').read_text())
rows = receipt['files'] + receipt['keyboard_files']
assert len(rows) == 30 and len({row['path'] for row in rows}) == 30
identity = '\n'.join(sorted(row['path'] + ':' + row['sha256'] for row in rows))
assert hashlib.sha256(identity.encode()).hexdigest() == '362ff75c0880cd0b03863ee121a7004795e292b6767d73b0671699971a745a51'
for row in rows:
    blob = (DATA / Path(row['path']).name).read_bytes()
    assert len(blob) == row['bytes'] and hashlib.sha256(blob).hexdigest() == row['sha256'] and row['repeats'] == 2
stems = sorted({Path(row['path']).stem for row in rows})
for stem in stems:
    assert package((DATA / (stem + '.uasset')).read_bytes(), (DATA / (stem + '.uexp')).read_bytes()) == json.loads((DATA / (stem + '.package.json')).read_text())

class KeyboardScript(bp.Script):
    def expression(self, depth=0):
        # Original EX_IntZero / EX_IntOne. Both are one-byte immediate constants.
        if self.data[self.at] in (0x2c, 0x2d):
            return {'int': self.take(1)[0] - 0x2c}
        return super().expression(depth)

bp.Script = KeyboardScript
bp.DIRECTORY = DATA
parsed = {}
for stem, names in {
    'VR4PlayerPawn_Typewriter': ['ExecuteUbergraph_VR4PlayerPawn_Typewriter'],
    'TypewriterKeyBase': ['ExecuteUbergraph_TypewriterKeyBase', 'SetKeyActivated', 'IsKeyEnabled', 'KeyActivated', 'GetFingerMagnetCollision'],
    'TypewriterKeyStandard': ['ExecuteUbergraph_TypewriterKeyStandard', 'GetFingerMagnetCollision'],
    'TypewriterKeyEnter': ['ExecuteUbergraph_TypewriterKeyEnter', 'GetFingerMagnetCollision'],
    'TypewriterKeySpace': ['ExecuteUbergraph_TypewriterKeySpace', 'GetFingerMagnetCollision'],
    'TypewriterKeyBck': ['GetFingerMagnetCollision'],
    'TypewriterKeyClr': ['GetFingerMagnetCollision'],
    'TypewriterKeyOverwrite': ['ExecuteUbergraph_TypewriterKeyOverwrite', 'KeyActivated', 'GetFingerMagnetCollision'],
    'TypewriterTheatreBox': ['ExecuteUbergraph_TypewriterTheatreBox', 'RegisterKeys', 'UpdateHighlights', 'Overwritepressed', 'ProcessKey', 'IsTypingAllowed', 'OnRing', 'IsVRFocusStateOK'],
    'TypewriterManager_BP': ['ExecuteUbergraph_TypewriterManager_BP'],
}.items():
    for name in names:
        parsed[stem, name] = bp.read_script(stem, name, 32 if name in ('KeyActivated', 'OnRing') else None)
for stem in ('TypewriterKeyStandard', 'TypewriterKeySpace', 'TypewriterKeyEnter', 'TypewriterKeyBck', 'TypewriterKeyClr', 'TypewriterKeyOverwrite'):
    getter = parsed[stem, 'GetFingerMagnetCollision']['tree'][0]['expression']['let_object']
    assert getter[0]['field'] == ['Collision'] and getter[1]['field'] == ['FingerMagnetCollision']
for stem in ('TypewriterKeyBck', 'TypewriterKeyClr'):
    inherited = json.loads((DATA / (stem + '.package.json')).read_text())
    assert any(e['name'] == stem + '_C' and any(i['index'] == e['super'] and i['name'] == 'TypeWriterKeyBase_Rotating_C' for i in inherited['imports']) for e in inherited['exports'])
assert parsed['TypewriterKeyBase', 'GetFingerMagnetCollision']['tree'][0]['expression']['let_object'][1] == {'literal_token': 42}
press = parsed['TypewriterKeyBase', 'SetKeyActivated']
tree = {node['runtime']: node['expression'] for node in press['tree']}
assert tree[72]['let_bool'][1]['arguments'][1]['field'] == ['KeyActivationEnabled']
assert tree[110]['pop_execution_if_not']['field'] == ['CallFunc_BooleanAND_ReturnValue']
assert tree[263] == {'call': 'KeyActivated', 'arguments': []}
register = parsed['TypewriterTheatreBox', 'RegisterKeys']
assert any(call['call'].endswith('.GetAllChildActors') for call in register['calls'])
assert any(node['expression'].get('bind_delegate') == 'Keypressed' for node in register['tree'])
assert any(call['call'].endswith('.Array_Add') and call['arguments'][0]['field'] == ['AllKeys'] for call in register['calls'])
register_tree = {node['runtime']: node['expression'] for node in register['tree']}
excluded = register_tree[68]['let_bool'][1]['call']['arguments']
assert excluded[0]['field'] == ['LocalChildActors'] and excluded[1]['context']['field'] == ['ChildActorOverwrite']
assert register_tree[1321]['let_object'][0]['field'] == ['OverwriteKey']
assert register_tree[1340]['bind_delegate'] == 'Overwritepressed'
assert register_tree[1363]['target']['context']['field'] == ['OverwriteKey']
assert parsed['TypewriterTheatreBox', 'Overwritepressed']['calls'][0]['arguments'] == [{'int': 26669}]
highlights = {node['runtime']: node['expression'] for node in parsed['TypewriterTheatreBox', 'UpdateHighlights']['tree']}
assert highlights[3235]['let_bool'][1]['arguments'][1] == {'byte_int': 13}
assert highlights[3266]['let_bool'][1]['arguments'][1] == {'byte_int': 6}
assert highlights[3335]['context']['field'] == ['OverwriteKey'] and highlights[3335]['call']['call'] == 'SetKeyEnabled'
theatre = {node['runtime']: node['expression'] for node in parsed['TypewriterTheatreBox', 'ExecuteUbergraph_TypewriterTheatreBox']['tree']}
assert theatre[18654]['call'] == 'ProcessKey'
assert theatre[26674] == {'jump': 11103}
assert theatre[11103]['let_bool'][1]['arguments'][1] == {'byte_int': 2}
assert theatre[11134]['let_bool'][1]['arguments'][1] == {'byte_int': 4}
assert theatre[11165]['call'] == 'IsVRFocusStateOK'
assert theatre[26669] == {'jump': 26631} and theatre[26631]['call'] == 'IsVRFocusStateOK'
assert theatre[26664] == {'jump': 18498} and theatre[18498]['let_bool'][1]['arguments'][1] == {'byte_int': 6}
assert theatre[18649] == {'jump': 18317} and theatre[18317]['let'][2] == {'byte_int': 9}
process = parsed['TypewriterTheatreBox', 'ProcessKey']
assert process['calls'][0]['call'] == 'IsTypingAllowed'
assert any(call['call'] == 'SaveActivated' for call in process['calls'])
allowed = parsed['TypewriterTheatreBox', 'IsTypingAllowed']['calls']
assert sorted(call['arguments'][1]['byte_int'] for call in allowed if call['call'].endswith('.EqualEqual_ByteByte')) == [7, 8, 9, 14]
manager_calls = parsed['TypewriterManager_BP', 'ExecuteUbergraph_TypewriterManager_BP']['calls']
assert all(any(call['call'].endswith(name) for call in manager_calls) for name in ('NotifyConfirmed', 'NotifyCanceled', 'LastSaveSucceeded'))
assert any(call['call'] == 'SaveGame' for call in manager_calls)
print('PASS: 30 immutable original members /15 package maps; complete relevant native BP streams; original enabled key / registered delegate / typed ProcessKey / manager flow and cancel state/VR-focus guards', flush=True)

animation=json.loads((DATA/'animation_receipt.json').read_text())
assert animation['native_sha256']==hashlib.sha256(native).hexdigest()
assert [(row['bytes'],row['sha256']) for row in animation['files']]==[
    (9449,'c7a2fdc74c1283765d940bde17e1319abe7695df8c6977e752efbcd350acebaa'),
    (12016,'8cd5e8c4f1177727c0468d26e2736b645db66a56edc9eba591c827ec1fecd81e')]
for row in animation['files']:
    data=(DATA/Path(row['path']).name).read_bytes()
    assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'] and row['repeats']==2
rotating='TypeWriterKeyBase_Rotating'
assert package((DATA/(rotating+'.uasset')).read_bytes(),(DATA/(rotating+'.uexp')).read_bytes())==json.loads((DATA/(rotating+'.package.json')).read_text())
for stem,method,writer in [('TypeWriterKeyBase_Translating','SetAnimBPKeyTranslation','SetKeyTranslation'),
                           (rotating,'SetAnimBPKeyAngle','SetKeyAngle')]:
    visual=bp.read_script(stem,method)
    assert [call['call'] for call in visual['calls']]==['/Script/Engine.KismetSystemLibrary.IsValid',
        '/Script/Engine.KismetMathLibrary.Subtract_FloatFloat',writer]
    assert visual['calls'][0]['arguments'][0]['field']==['AnimBP']
    assert visual['calls'][2]['arguments'][0]['field']==['KeyAlphaIndex']
    assert all('Activated' not in call['call'] for call in visual['calls'])
for stem in ('TypewriterKeyStandard','TypewriterKeyEnter','TypewriterKeySpace','TypewriterKeyBck','TypewriterKeyClr','TypewriterKeyOverwrite'):
    asset=json.loads((DATA/(stem+'.package.json')).read_text())
    owner=next(e for e in asset['exports'] if e['name']==stem+'_C')
    assert next(i['name'] for i in asset['imports'] if i['index']==owner['super']) in (rotating+'_C','TypeWriterKeyBase_Translating_C')
assert any(call['call']=='SetText' for call in parsed['TypewriterTheatreBox','ProcessKey']['calls'])
assert 'WidgetList' in json.dumps(parsed['TypewriterTheatreBox','ProcessKey']['tree'])
print('PASS: original rotating/translating AnimBP visual-only writers and all six key-family ancestors; native ProcessKey writes typed text to WidgetList',flush=True)

attach=bp.read_script('TypewriterKeyBase','GetAttachPoint')
assert [call['call'] for call in attach['calls']]==[
    '/Script/Engine.Actor.GetParentComponent','/Script/Engine.KismetSystemLibrary.IsValid',
    '/Script/Engine.Actor.GetParentComponent','/Script/Engine.SceneComponent.GetAttachSocketName',
    '/Script/Engine.SceneComponent.GetAttachParent','GetNonForwardingComponent']
assert attach['tree'][-4]['expression']['let_object'][0]['field']==['PseudoRootComponent']
for stem,threshold in [('TypeWriterKeyBase_Rotating','KeyPressThresholdDegrees'),('TypeWriterKeyBase_Translating','KeyPressThreshold')]:
    init=bp.read_script(stem,'InitKeyTransformData')
    assert any(call['call']=='GetKeyHierarchy' for call in init['calls'])
    assert 'KeyPlaneMarker' in json.dumps(init['tree']) and 'LocalKeySurfaceNormal' in json.dumps(init['tree'])
    setter=bp.read_script(stem,'SetKeyDepressAngle')
    assert any(threshold in json.dumps(call) for call in setter['calls'])
    metadata=json.loads((DATA/(stem+'.package.json')).read_text());blob=(DATA/(stem+'.uexp')).read_bytes()
    default=next(row for row in metadata['exports'] if row['name']=='Default__'+stem+'_C')
    raw=blob[default['uexp_offset']:default['uexp_offset']+default['serial_size']]
    at=raw.index(struct.pack('<II',metadata['names'].index('LocalKeySurfaceNormal'),0))
    assert struct.unpack_from('<3f',raw,at+49)==(0.,0.,1.)
scroll=bp.read_script('TypewriterTheatreBox','ScrollList')
assert scroll['calls'][0]['arguments']==[{'int':17218}]
for name,entry in [('BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_5_GripHandEventMulticastDelegate__DelegateSignature',26531),
                   ('BndEvt__DialPlayerGrip_K2Node_ComponentBoundEvent_6_GripHandEventMulticastDelegate__DelegateSignature',26536)]:
    event=bp.read_script('TypewriterTheatreBox',name)
    assert event['tree'][0]['expression']['let_persistent'][1]['field']==['Hand']
    assert event['calls'][0]['arguments']==[{'int':entry}]
assert theatre[26531]=={'jump':17792} and theatre[17806]['let'][2]=={'byte_int':5}
assert theatre[26536]=={'jump':17746} and theatre[17721]['let'][2]=={'byte_int':0}
assert theatre[17251]['let_bool'][0]['field']==['ScrollActive']
assert theatre[257]['let_bool'][1]['arguments'][0]['field']==['ScrollActive']
assert theatre[17438]['let'][0]['field']==theatre[21513]['let'][0]['field']==['CurrentSlotIndex']
assert theatre[17385]['let_bool'][1]['arguments'][1]=={'int':0}
assert theatre[21460]['let_bool'][1]['arguments'][1]=={'int':19}
update=bp.read_script('TypewriterTheatreBox','UpdateList')
assert update['tree'][1]['expression']['let'][2]=={'int':-2}
assert update['tree'][3]['expression']['let_bool'][1]['arguments'][1]=={'int':4}
assert theatre[21589]['let'][2]['arguments'][1]=={'float':32.0}
states=bp.read_script('TypewriterTheatreBox','InitHandStates')
spawns=[call for call in states['calls'] if call['call'].endswith('.SpawnObject')]
assert len(spawns)==2 and all(call['arguments'][1]=={'literal_token':23} for call in spawns)
print('PASS: immutable original visible surface hierarchy/normal and distinct family thresholds; original dial delegate entries, native ScrollActive/index wrap/32-unit rows and theatre-owned hand helper creation',flush=True)

capture_path=DATA/'p347_touch_capture.json'
assert hashlib.sha256(capture_path.read_bytes()).hexdigest()=='f285186e7d19472395f5575583b1b979339ae57c264eb009c83d9e612803528a'
capture=json.loads(capture_path.read_text())
assert capture['pid']==32414 and capture['width']==2400 and capture['height']==1080 and len(capture['keys'])==41
assert capture['theatre_state']['currentState']['raw']=='07' and capture['theatre_state']['bLastVRFocusState']['value'] is True
state = capture['theatre_state']['currentState']
assert (state['kind'], state['offset'], state['size'], state['dim']) == ('ByteProperty', 0x5d0, 1, 1)
assert struct.unpack_from('<I',native,0xa0b7448+0x24)[0]==0x160
for key in capture['keys']:
    parent=key['parent_property'];assert (parent['kind'],parent['offset'],parent['size'],parent['dim'])==('WeakObjectProperty',0x160,8,1)
    assert key['parent']['outer']==capture['theatre']['address'] and key['collision']['outer']==key['address'] and key['collision']['owner']==key['address']
    assert key['state']['KeyEnabled']['value'] is True and key['state']['KeyActivationEnabled']['value'] is True and key['state']['IsKeyActivated']['value'] is False
    for name,kind,size,output in [('IsKeyEnabled','BoolProperty',1,True),('GetFingerMagnetCollision','ObjectProperty',8,True),('SetKeyActivated','BoolProperty',1,False)]:
        function=key['functions'][name];assert (function['num_parms'],function['parms_size'],function['return_offset'])==(1,size,65535)
        params=[p for p in function['parameters'] if int(p['flags'],16)&0x80];assert len(params)==1
        param=params[0];flags=int(param['flags'],16)
        assert (param['kind'],param['offset'],param['size'],param['dim'])==(kind,0,size,1) and not flags&0x400 and bool(flags&0x100)==output
def number(n):return repr(float(n))+'f'
expected=['// Exact p347 native final POV and all 41 owned collision shapes. Checked against p347_touch_capture.json.',
    'static const unsigned char tt_capture_view[64]={'+','.join(str(x) for x in bytes.fromhex(capture['pov']))+'};',
    'static const struct {const char* name;float tf[12],shape[3];int box;} tt_capture_keys[]={']
for key in capture['keys']:
    shape=key['collision'];box=shape['cls']=='BoxComponent';extent=shape['BoxExtent'] if box else [shape['CapsuleRadius'][0],shape['CapsuleHalfHeight'][0],0]
    expected.append('{"'+key['parent']['name']+'",{'+','.join(number(x) for x in shape['transform'])+'},{'+','.join(number(x) for x in extent)+'},'+str(int(box))+'},')
expected+=['};','static const struct{int key;float x,y;} tt_capture_touches[]={']
for touch in capture['touches']:
    index=next(i for i,key in enumerate(capture['keys']) if key['parent']['name']==touch['key'])
    expected.append('{'+str(index)+','+number(touch['x'])+','+number(touch['y'])+'},')
expected+=['};','']
assert (ROOT/'analysis/research/typewriter_touch_capture.h').read_text()=='\n'.join(expected)
print('PASS: immutable frozen p347 capture; exact native weak parent offset/serial shape ownership, all41 enabled keys/state7, and actual named OUT/input UFunction metadata; C data matches final rendered POV and physical taps',flush=True)

frame_path = DATA / 'p347_frame_capture.json'
assert hashlib.sha256(frame_path.read_bytes()).hexdigest() == 'e73d3b7b0a68c7bc824aa3d694f2beaf7edded7ece05aa559a984c606eb2c67f'
frame = json.loads(frame_path.read_text())
assert frame['pid'] == capture['pid'] and frame['theatre'] == capture['theatre'] and frame['pawn'] == capture['pawn']
assert frame['native_ui_state'] == 7 and frame['widgets']['ActiveWidget']['cls'] == 'TypewriterEntry_C'
frame_data = ['// Exact original visible p347 paper/title/Cancel quads; checked against p347_frame_capture.json.',
    'static const struct{const char* name;float tf[12],size[2],pivot[2];unsigned short flags;} tt_capture_widgets[]={']
for name in ('WidgetTitle', 'WidgetList', 'CancelWidget', 'WidgetOverwrite'):
    widget = frame['components'][name]
    assert widget['cls'] == 'WidgetComponent' and widget['outer'] == widget['owner'] == frame['theatre']['address']
    assert int.from_bytes(bytes.fromhex(widget['flags14c']), 'little') & 0x420 == 0x20
    for field, offset, kind in [('CurrentDrawSize', 0x450, 'StructProperty'), ('Pivot', 0x45c, 'StructProperty'), ('GeometryMode', 0x518, 'EnumProperty'), ('Space', 0x429, 'EnumProperty')]:
        prop = widget[field]['property']; assert (prop['offset'], prop['dim'], prop['kind']) == (offset, 1, kind)
    assert widget['GeometryMode']['hex'] == widget['Space']['hex'] == '00'
    frame_data.append('{"'+name+'",{'+','.join(number(x) for x in widget['transform'])+'},{'+','.join(number(x) for x in widget['CurrentDrawSize']['int2'])+'},{'+','.join(number(x) for x in widget['Pivot']['float2'])+'},'+str(int.from_bytes(bytes.fromhex(widget['flags14c']),'little'))+'},')
frame_data += ['};', '']
assert (ROOT / 'analysis/research/typewriter_frame_capture.h').read_text() == '\n'.join(frame_data)
for address, offset in ((0x9f77e20, 0x429), (0x9f77c28, 0x45c), (0x9f77698, 0x518)):
    assert struct.unpack_from('<I', native, address+0x24)[0] == offset
flat_view = (ROOT / 'analysis/re4fix_flat_view.h').read_text()
assert 'fv_current_draw_size = (struct cinema_ui_vec2 (*)(const void*))(base + 0x7db1fc8);' in flat_view
assert struct.unpack_from('<4I',native,0x7db1fc8) == (0xbd445000,0xbd445401,0x5e21d800,0x5e21d821)
print('PASS: immutable original native visible paper/title/list/Cancel geometry, rendered desired sizes and pivots; exact getter field bindings and guarded existing native quad metadata', flush=True)

overwrite_path = ROOT / 'analysis/build/qa371/typewriter_contract.json'
assert hashlib.sha256(overwrite_path.read_bytes()).hexdigest() == '55a8c8be64cfaac1429d8e11058c26b6d1ed3348bfd682cd49fe37e78ef678f0'
overwrite = json.loads(overwrite_path.read_text())
assert overwrite['completed'] and overwrite['pid'] == 17090 and overwrite['candidate'] == 'p371'
assert overwrite['helper_sha256'] == 'e20d852daee98b645e27671700e5ba12f60007de2e29fb7aafcbdf1187143070'
assert overwrite['native_sha256'] == hashlib.sha256(native).hexdigest()
assert all(overwrite['ownership'].values()) and overwrite['key_collision_owned']
assert overwrite['theatre']['properties']['currentState']['value'] == 6
assert all(overwrite['key']['properties'][name]['value'] for name in ('KeyEnabled', 'KeyActivationEnabled'))
assert not overwrite['key']['properties']['IsKeyActivated']['value']
assert overwrite['key_collision']['class'] == overwrite['OvewriteCollision']['class'] == 'BoxComponent'
assert overwrite['key_collision']['properties']['BoxExtent']['raw'] == overwrite['OvewriteCollision']['properties']['BoxExtent']['raw']
overwrite_data = ['// Exact read-only p371 native POV and authored Overwrite geometry; checked against typewriter_contract.json.',
                  'static const unsigned char tt_overwrite_pov[64]={' + ','.join(str(x) for x in bytes.fromhex(overwrite['pov'])) + '};']
for name, key in [('magnet', 'key_collision'), ('button', 'OvewriteCollision'), ('widget', 'WidgetOverwrite')]:
    overwrite_data.append('static const float tt_overwrite_' + name + '_tf[12]={' + ','.join(number(x) for x in overwrite[key]['transform']) + '};')
overwrite_data.append('static const float tt_overwrite_extent[3]={' + ','.join(number(x) for x in struct.unpack('<3f', bytes.fromhex(overwrite['key_collision']['properties']['BoxExtent']['raw']))) + '};')
assert (ROOT / 'analysis/research/typewriter_overwrite_capture.h').read_text() == '\n'.join(overwrite_data) + '\n'
assert theatre[18539]['let'][2]['context']['field'] == ['OvewriteCollision']
assert theatre[9452]['values'][1]['cases'][0]['result']['field'] == ['OvewriteCollision']
print('PASS: immutable exact p371 weak-owned enabled Overwrite/native face-vs-finger geometry and final POV; C reproducer matches physical failed tap, original authored tutorial/sound anchor and Box extents', flush=True)

source = (ROOT / 'analysis/re4fix.c').read_text()
input_source = (ROOT / 'analysis/re4fix_input.h').read_text()
cinema = (ROOT / 'analysis/re4fix_cinema.h').read_text()
assert '#include "re4fix_typewriter_touch.h"' in source and 'install_touch_typewriter(base)' in source
assert 'touch_typewriter_event(h,type,x,y,g_tw,g_th,now_ms())' in input_source
assert input_source.index('touch_typewriter_event(h,type,x,y,g_tw,g_th,now_ms())') < input_source.index('struct vt* t = vt_find(h);', input_source.index('void touch_process('))
assert 'touch_typewriter_cancel();' in input_source
assert cinema.index('cinema_flat_view(camera,view);') < cinema.index('if (cinema_health_view) cinema_health_view(camera,view);') < cinema.index('if (cinema_typewriter_view) cinema_typewriter_view(camera,view);')
assert 'CallSaveGame' not in header and 'NotifyConfirmed' not in header and 'ProcessKey' not in header
assert 'g_menu_tick_ms' not in header and 'phone_player(' not in header
print('PASS: actual early queued input/focus-reset/final-view integration; no native save or native request under input mutex; owned active camera replaces stale gameplay-hand assumptions', flush=True)

clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
csource = ROOT / 'analysis/research/test_typewriter_touch.c'
subprocess.run([str(clang), '--target=aarch64-linux-android23', '-std=c11', '-O2', '-Werror', '-fsyntax-only', str(csource)], check=True)
with tempfile.TemporaryDirectory(prefix='re4-typewriter-') as temp:
    folder = Path(temp)
    (folder / 'pthread.h').write_text('''#pragma once
typedef struct { int locked; } pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
#define RE4_TEST_MUTEX_LOCKED(m) ((m).locked)
static int pthread_mutex_lock(pthread_mutex_t* m){assert(!m->locked);m->locked=1;
#ifdef RE4_TEST_MUTEX_AFTER_LOCK
RE4_TEST_MUTEX_AFTER_LOCK();
#endif
return 0;}
static int pthread_mutex_unlock(pthread_mutex_t* m){assert(m->locked);m->locked=0;return 0;}
''')
    fixture = folder / 'installer.h'
    fixture.write_text('''__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_installer(void){
 ready();unsigned char* memory=VirtualAlloc((void*)0x20000000,0x9000000,0x3000,4);assert(memory);
 static const struct{unsigned at,words[2];} code[]={
''' + '\n'.join('{0x%x,{0x%x,0x%x}},' % row for row in checks) + '''
 };
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(memory+code[i].at,code[i].words,8);
 phone_active=(void*(*)(void))(memory+0x63380dc);phone_type=(unsigned(*)(void))(memory+0x6338aec);
 uintptr_t base=(uintptr_t)memory;tt_ready=0;install_touch_typewriter(base);assert(tt_ready&&cinema_typewriter_view==touch_typewriter_view);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned k=0;k<2;k++){
  ((unsigned*)(memory+code[i].at))[k]^=1;install_touch_typewriter(base);assert(!tt_ready);((unsigned*)(memory+code[i].at))[k]^=1;
 }
 phone_inspect_ready=0;install_touch_typewriter(base);assert(!tt_ready);phone_inspect_ready=1;
 phone_active=get_active;install_touch_typewriter(base);assert(!tt_ready);
 phone_active=(void*(*)(void))(memory+0x63380dc);install_touch_typewriter(0);assert(!tt_ready);
 install_touch_typewriter(base);assert(tt_ready);tt_ready=0;assert(VirtualFree(memory,0,0x8000));
 puts("PASS: actual installer verified preimages/dependencies, each opcode corruption, callback installation and unsupported native ABI refusal");
}
''')
    obj, exe = folder / 'test.obj', folder / 'test.exe'
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-std=c11', '-O2', '-Werror', '-I', str(folder),
                    '-DTYPEWRITER_INSTALL_FIXTURE="' + fixture.as_posix() + '"',
                    '-isystem', str(vc / 'include'), '-isystem', str(sdk / 'Include/10.0.26100.0/ucrt'),
                    '-c', '-o', str(obj), str(csource)], check=True)
    subprocess.run([str(vc / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{vc / "lib/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: actual Typewriter touch C boundary fixtures executed; physical keyboard/confirm/cancel/save/reload remain device acceptance', flush=True)
