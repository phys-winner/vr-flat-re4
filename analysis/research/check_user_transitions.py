"""Verify exact original result-screen flows and QA transition ABIs without a device."""
from pathlib import Path
import contextlib
import hashlib
import io
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

import inspect_flat_bp as bp

ROOT = Path(__file__).resolve().parents[2]
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest() == '6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
directory = ROOT / 'analysis/research/user_render_originals'
for item in json.loads((directory / 'receipt.json').read_text()):
    data = (directory / Path(item['source']).name).read_bytes()
    assert len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256'] and item['repeats'] == 2
bp.DIRECTORY = directory
for stem, offset, possess, push, widget in (
    ('UI_MercenariesResults_BP', 11727, 5316, 5955, 'Widget_Results'),
    ('UI_ChapterSummary_BP', 3887, 1327, 2548, 'Widget_Unlocks'),
):
    graph = bp.read_script(stem, 'ExecuteUbergraph_' + stem, offset)
    nodes = {node['runtime']: node['expression'] for node in graph['tree']}
    assert nodes[possess]['let_object'][1] == {
        'call': '/Script/VR4.VR4PlayerController.PossessPlayerPawn', 'arguments': [{'byte_int': 4}]}
    assert nodes[push]['let_bool'][1] == {
        'call': '/Script/VR4.VR4UIManager.PushUIScreen', 'arguments': [{'literal_token': 23}]}
    package = json.loads((directory / (stem + '.package.json')).read_text())
    assert any(e['name'] == widget + '_GEN_VARIABLE' and e['class_path'] == '/Script/UMG.WidgetComponent'
               for e in package['exports'])
    assert any(e['name'] == 'NextButton_GEN_VARIABLE' and e['class_path'] == '/Script/Engine.ChildActorComponent'
               for e in package['exports'])
print('PASS: original Mercenaries/ChapterSummary both possess native menu pawn4, push their actual UI, retain original widget and ChildActor Next')

for stem, offset, pawn_type in (
    ('EndGameFlow_CreditsUnlocks_BP', 2685, 4),
    ('AshleyFirstTimeTutorial', 2654, 7), ('AshleyFoundTutorial', 5752, 7),
    ('BoatPreferenceTutorial', 2441, 7), ('WeapManTutorialStart', 24876, 7),
    ('MercTutorialsInGame', 27823, 7),
):
    with contextlib.redirect_stdout(io.StringIO()):
        graph = bp.read_script(stem, 'ExecuteUbergraph_' + stem, offset)
    assert any(c['call'] == 'SwapPawns' and c['arguments'] == [{'byte_int': pawn_type}] for c in graph['calls'])
    assert any(c['call'] == '/Script/VR4.VR4PlayerController.PossessPlayerPawn' for c in graph['calls'])
    assert any(c['call'] == 'SetAnchor' for c in graph['calls'])
print('PASS: original game-ending results/unlocks select Menu4; five physical/tutorial scenes select InWorldMenu7 and use native pawn/anchor pipelines')

relocations = {a: value for a, kind, value in struct.iter_unpack('<QQq', native[0x2e6c028:0x2e6c028 + 0x1146540]) if kind == 1027}
enums = {}
for address in range(0x9df91a8, 0x9df9248, 16):
    value = relocations[address]
    name = native[value:native.index(b'\0', value)].decode()
    assert name.startswith('EPlayerPawn::')
    enums[name.split('::')[1]] = struct.unpack_from('<q', native, address + 8)[0]
assert enums == {
    'None': 0, 'Bio4': 1, 'Pause': 2, 'Cutscene': 3, 'Menu': 4, 'Communicator': 5,
    'Typewriter': 6, 'InWorldMenu': 7, 'PauseMerchant': 8, 'Count': 9}
symbols = {}
dependents = {}
for line in (ROOT / 'analysis/ue4_syms.txt').read_text().splitlines():
    fields = line.split(' ', 2)
    if len(fields) != 3:
        continue
    address, _, name = fields
    symbols[int(address, 16)] = name
    match = re.fullmatch(r'Z_Construct_UClass_A(.+)_Statics::DependentSingletons', name)
    if match:
        dependents[match[1]] = int(address, 16)
parents = {}
for name, address in dependents.items():
    parent = symbols.get(relocations.get(address, 0), '')
    match = re.fullmatch(r'Z_Construct_UClass_A(.+)\(\)', parent)
    if match:
        parents[name] = match[1]
for name, parent in (('VR4DeathMenu', 'VR4UIScreen'), ('VR4PauseUIScreen', 'VR4UIScreen'),
                     ('VR4InventoryUIScreen', 'VR4PauseUIScreen'), ('VR4MerchantUIScreen', 'VR4PauseUIScreen')):
    assert parents[name] == parent
inventory = json.loads((ROOT / 'analysis/research/user_flat_screen_inventory.json').read_text())
classes = {c['name']: (row, c) for row in inventory['rows'] for c in row['classes']}
def native_screen(name, depth=0):
    assert depth < 32
    return name == 'VR4UIScreen' or name in parents and native_screen(parents[name], depth + 1)
def blueprint_screen(name, depth=0):
    assert depth < 32
    if name not in classes:
        return False
    _, item = classes[name]
    parent = item['super'] or ''
    return native_screen(parent.split('.', 1)[1]) if parent.startswith('/Script/VR4.') else blueprint_screen(parent.rsplit('.', 1)[-1], depth + 1)
coverage = [{'class': name, 'parent': item['super'], 'path': row['path'],
             'widget_components': [p['name'] for p in row['components'] if p['class'] == '/Script/UMG.WidgetComponent'],
             'names': row['call_or_property_names']}
            for name, (row, item) in sorted(classes.items()) if blueprint_screen(name)]
(ROOT / 'analysis/research/user_flat_screen_coverage.json').write_text(json.dumps({
    'native_sha256': hashlib.sha256(native).hexdigest(), 'native_pawn_enum': enums,
    'native_parents': parents, 'native_uiscreen_blueprints': coverage,
    'scope': inventory['scope'], 'selected_headers': sum(c['selected_headers'] for c in inventory['containers']),
    'containers': len(inventory['containers']), 'runtime_coverage_claimed': False}, indent=2) + '\n')
print('PASS: exact native pawn enum and native parent-constructor relocations; UIScreen hierarchy blueprints', len(coverage))

qa = (ROOT / 'analysis/research/qa_user_transitions.h').read_text()
for address, first, second in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\}', qa):
    assert struct.unpack_from('<II', native, int(address, 16)) == (int(first, 16), int(second, 16))
def branch(address):
    word = struct.unpack_from('<I', native, address)[0]
    assert word & 0xfc000000 == 0x94000000
    delta = word & 0x3ffffff
    if delta & 0x2000000:
        delta -= 0x4000000
    return address + 4 * delta
assert branch(0x60fe174) == branch(0x615e6b0) == 0x61e5ca4
assert struct.unpack_from('<II', native, 0x60fe16c) == (0x2a1f03e0, 0x2a1f03e1)
assert branch(0x615e9f4) == 0x61e5e20
assert struct.unpack_from('<II', native, 0x61e5ce0) == (0x2a0103f4, 0x12000017)
assert branch(0x600d83c) == 0x61553e8  # R106Init registers authored AEV2 with level20.
assert struct.unpack_from('<4I', native, 0x600d820) == (0xb0000003, 0x910a5063, 0x52800040, 0x52800281)
assert branch(0x600e38c) == 0x5f058e8  # EventMgr::SetEvt(original evd data).
assert branch(0x600e3d4) == 0x5f047f0  # Wait for the original event to end.
assert branch(0x600e424) == 0x615e8f8
assert struct.unpack_from('<II', native, 0x600e41c) == (0x52800061, 0x2a1f03e0)
assert branch(0x615e9e8) == 0x615e620
assert branch(0x615e6b0) == 0x61e5ca4
assert native[0x42e72ed:0x42e72ed + 16] == b'evd/r106s00.evd\0'
assert relocations[0x9cee040] == 0x615bc6c  # Native SceAt type2 dispatcher.
assert branch(0x615bca0) == 0x61612dc
assert native[0x61612dc:0x61612e0] == bytes.fromhex('68 ff ff 17')
assert branch(0x61611e4) == 0x61627bc
assert qa.startswith('// Diagnostic builds only:') and '#ifdef RE4_QA_USER_TRANSITIONS' in qa
print('PASS: original R106S00 AEV2 -> event completion -> SceSetChapterEnd(0,3) -> native chapter summary; exact static ABIs and QA preimages')
subprocess.run([sys.executable, str(ROOT / 'analysis/research/check_luis_route_native.py')], check=True)

parser = qa[qa.index('static unsigned qa_transition_request;'):qa.index('static void qa_user_transitions_tick(')]
fixture = '''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
''' + parser + '''
int main(void){
    assert(qa_user_transitions_command("mercend") && qa_transition_request==1);
    assert(qa_user_transitions_command("inspect") && qa_transition_request==2);
    assert(qa_user_transitions_command("chapter") && qa_transition_request==3);
    assert(qa_user_transitions_command("luis") && qa_transition_request==4);
    assert(qa_user_transitions_command("watchleft") && qa_transition_request==5);
    assert(qa_user_transitions_command("watchright") && qa_transition_request==6);
    assert(qa_user_transitions_command("capoff") && qa_transition_request==7);
    assert(qa_user_transitions_command("cap60") && qa_transition_request==8);
    assert(qa_user_transitions_command("door101") && qa_transition_request==9);
    assert(qa_user_transitions_command("door212") && qa_transition_request==10);
    assert(qa_user_transitions_command("spinel") && qa_transition_request==11);
    assert(qa_user_transitions_command("merchant") && qa_transition_request==12);
    assert(qa_user_transitions_command("treasurestate") && qa_transition_request==13);
    assert(qa_user_transitions_command("merchantintro") && qa_transition_request==14);
    assert(qa_user_transitions_command("houseintro") && qa_transition_request==15);
    for(unsigned room=0;room<0x500;room++){
        char text[16];snprintf(text,sizeof text,"room%03x",room);
        int expected=(room>=0x100&&room<=0x120)||(room>=0x200&&room<=0x22c)
            ||(room>=0x300&&room<=0x333)||(room>=0x400&&room<=0x404&&room!=0x401);
        assert(qa_user_transitions_command(text)==expected);
        if(expected)assert(qa_transition_request==(0x10000u|room));
    }
    const char* rejected[]={"","unknown","room","room1000","room-10","roomxyz","room10g","roomfff","mercend ","chapterx","Luis","luis ","luis106"};
    for(unsigned i=0;i<sizeof rejected/sizeof *rejected;i++)assert(!qa_user_transitions_command(rejected[i]));
    puts("PASS: actual QA command parser exhaustive 1280 room IDs and malformed/bounds refusals");
    static unsigned char game[0x5400],manager[0x200],header[16+17*0x100],tasks[12*0x40];
    const uintptr_t base=0x10000000;unsigned short room=0x106,version=0x104,count=17;
    unsigned char* entries=header+16;unsigned char* entry=entries+2*0x100;
    uintptr_t callback=base+0x600e294,argument=0;const unsigned char* start=header;
    memcpy(game+0x5038,&room,sizeof room);memcpy(header,"AEV",4);
    memcpy(header+4,&version,2);memcpy(header+6,&count,2);
    memcpy(manager,&start,sizeof start);memcpy(manager+8,&entries,sizeof entries);
    entry[0x38]=3;entry[0x39]=2;entry[0x3a]=2;entry[0x3e]=20;entry[0x50]=2;entry[0x51]=1;
    memcpy(entry+0x48,&callback,sizeof callback);memcpy(entry+0x40,&argument,sizeof argument);
    unsigned char original[sizeof header];memcpy(original,header,sizeof original);
    assert(qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    assert(qa_luis_fixture_ready(game,manager,base,0,(const void*)(base+0xa40a0f8),tasks));
    assert(qa_luis_fixture_ready(game,manager,base,0,(const void*)(uintptr_t)-1,tasks));
    assert(!qa_luis_fixture_ready(0,manager,base,0,0,tasks));
    assert(!qa_luis_fixture_ready(game,0,base,0,0,tasks));
    assert(!qa_luis_fixture_ready(game,manager,base,0,0,0));
    assert(!qa_luis_fixture_ready(game,manager,base,1,0,tasks));
    assert(!qa_luis_fixture_ready(game,manager,base,0,(const void*)1,tasks));
    game[0x5038]=5;assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));game[0x5038]=6;
    game[0x535a]=0x20;assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));game[0x535a]=0;
    const unsigned fields[]={0,4,6,16+2*0x100+0x38,16+2*0x100+0x39,16+2*0x100+0x3a,
        16+2*0x100+0x3e,16+2*0x100+0x40,16+2*0x100+0x48,16+2*0x100+0x50,16+2*0x100+0x51};
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
        header[fields[i]]^=1;assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));header[fields[i]]^=1;
    }
    entry[0x3e]=0;assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));entry[0x3e]=20;
    entries++;memcpy(manager+8,&entries,sizeof entries);assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    entries--;memcpy(manager+8,&entries,sizeof entries);
    for(unsigned i=0;i<12;i++)tasks[i*0x40]=1;
    assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    tasks[0]=0;assert(qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    memcpy(tasks+0x40+0x18,&callback,sizeof callback);
    assert(!qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    memset(tasks,0,sizeof tasks);assert(qa_luis_fixture_ready(game,manager,base,0,0,tasks));
    assert(!memcmp(original,header,sizeof original));
    puts("PASS: actual Luis QA guard refuses wrong room/progression/replay/task ownership, malformed AEV, synchronous level0, duplicate/full native tasks; native records unchanged");
}
'''
clang = Path(os.environ['ANDROID_SDK_ROOT']) / 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-transitions-') as temporary:
    folder = Path(temporary)
    source, obj, exe = folder / 'test.c', folder / 'test.obj', folder / 'test.exe'
    source.write_text(fixture)
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(source)], check=True)
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
        '-isystem', str(vc / 'include'), '-isystem', str(sdk / 'Include/10.0.26100.0/ucrt'),
        '-c', '-o', str(obj), str(source)], check=True)
    subprocess.run([str(vc / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
        'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
