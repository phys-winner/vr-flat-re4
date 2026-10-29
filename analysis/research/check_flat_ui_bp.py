"""Original authored-plane and one-step comfort-finish proof; no game asset edits."""
import hashlib
import json
import struct
from inspect_flat_bp import DIRECTORY, read_script

expected = {
    'ComfortCustomizeCoverScreen_BP': (
        'a096b3a3b63877b0171aeb96ac7063c429062eb01b081ce6baed26fbd4e9275e',
        'ca728a01ca00846027db2404902e7dedc1a15a92070e47256cf91f7b90e117b7'),
    'CutsceneTheatreBox': (
        '1565d70c1fa8e363512f908f84818aa09b262ae3ba89b66d07fc00db3035e2f4',
        'c8ed269a8a5a18f457f4de26b8a498fa7e86fe9f287814dd0df97c8794bc78dd'),
    'OptionsConfirmScreen_BP': (
        '065b74f868ac7734bfc2f9e87ce9e408cb19d120c4d7b968875850912905436f',
        'cf2e2d7afbd99dc7f252f3062115dfaaa08b537e6dc089ef48c58d263aa18713'),
    'NewGameConfirmScreen_BP': (
        '1d6a3e784e4e1ec66253589786309a20245f9cb49e0713e5fab12323c850d558',
        'fee980c94fd0e2e7080cb778f62762472d54a6d4ff0960b3324e1f486d0a0ccc'),
}
for stem, hashes in expected.items():
    for extension, digest in zip(('uasset', 'uexp'), hashes):
        assert hashlib.sha256((DIRECTORY / (stem + '.' + extension)).read_bytes()).hexdigest() == digest


def expressions(script):
    return {item['runtime']: item['expression'] for item in script['tree']}


wizard = 'ComfortCustomizeCoverScreen_BP'
event = 'BndEvt__VR4UIButton_NewGamePlus_K2Node_ComponentBoundEvent_11_OnUIComponentClicked__DelegateSignature'
package = json.loads((DIRECTORY / (wizard + '.package.json')).read_text())
binding = next(e for e in package['exports'] if e['name'] == 'ComponentDelegateBinding_0')
body = (DIRECTORY / (wizard + '.uexp')).read_bytes()
binding = body[binding['uexp_offset']:binding['uexp_offset']+binding['serial_size']]
assert len(binding) == 3308 and struct.unpack_from('<I', binding, 33)[0] == 30
records = []
for i in range(30):
    names = []
    for k in range(3):
        at = 86 + 107*i + 33*k
        assert struct.unpack_from('<I', binding, at+16)[0] == 8
        index, number = struct.unpack_from('<II', binding, at+25)
        assert index < len(package['names']) and number == 0
        names.append(package['names'][index])
    records.append(names)
assert records[20] == ['VR4UIButton_Next', 'OnClicked', event]
wrapper = read_script(wizard, event)
assert wrapper['calls'][-1]['call'] == 'ExecuteUbergraph_ComfortCustomizeCoverScreen_BP'
assert wrapper['calls'][-1]['arguments'] == [{'int': 2050}]
graph = expressions(read_script(wizard, 'ExecuteUbergraph_ComfortCustomizeCoverScreen_BP'))
assert graph[2050] == {'jump': 1433}
assert graph[1433]['let_bool'][1]['call'].endswith('.NotEqual_IntInt')
assert graph[1433]['let_bool'][1]['arguments'][0]['field'] == ['currentScreen']
assert graph[1433]['let_bool'][1]['arguments'][1] == {'int': 4}
assert graph[1467]['jump_if_not'] == 1626
assert graph[1626]['let_bool'] == [{'field': ['confirmedChoices?'], 'owner': 51}, {'literal_token': 0x27}]
assert graph[1637] == {'jump': 15}
assert graph[15]['call'] == 'SaveComfortCompleted' and graph[29]['call'] == 'RefreshOptionScreen'
assert graph[138]['call']['call'] == 'UpdateCoverScreen'
assert graph[138]['call']['arguments'][:3] == [{'literal_token': 0x2a}, {'literal_token': 0x17}, {'byte_int': 1}]
save = read_script(wizard, 'SaveComfortCompleted', 375)
assert any(c['call'].endswith('.SetGamePlayerSettings') for c in save['calls'])
assert any(c['call'] == 'SaveGame' for c in save['calls'])

theatre = expressions(read_script('CutsceneTheatreBox', 'ExecuteUbergraph_CutsceneTheatreBox'))
assert theatre[9980]['let_bool'][1]['call'].endswith('.Not_PreBool')
assert theatre[9980]['let_bool'][1]['arguments'][0]['call']['field'] == ['UseFlatCutsceneScreen']
assert theatre[10031]['jump_if_not'] == 12341
for runtime, component, visible in ((12341, 'FlatScreen', 0x27), (12375, 'Screen', 0x28)):
    expression = theatre[runtime]
    assert expression['context']['field'] == [component]
    assert expression['call']['call'].endswith('.SetVisibility')
    assert expression['call']['arguments'] == [{'literal_token': visible}, {'literal_token': 0x28}]
assert theatre[12409] == {'jump': 10045}
for first, second in ((1521, 1571), (17280, 17330)):
    assert theatre[first]['context']['field'] == ['Screen']
    assert theatre[second]['context']['field'] == ['FlatScreen']
    assert theatre[first]['call']['call'] == theatre[second]['call']['call'] == 'SetMaterial'
    assert theatre[first]['call']['arguments'] == theatre[second]['call']['arguments'] == [{'int': 0}, {'field': ['VideoMaterial'], 'owner': 77}]
options_footer = expressions(read_script('OptionsConfirmScreen_BP', 'SetButtons', 187))
for runtime, field, key in ((510, 'ConfimButton', 'AcceptChanges'), (606, 'RejectButton', 'ResetToDefault')):
    assert options_footer[runtime]['context']['field'] == [field]
    assert options_footer[runtime]['call']['call'] == 'SetButtonText'
    assert options_footer[runtime]['call']['arguments'][0]['key'] == {'string': key}
options_actions = read_script('OptionsConfirmScreen_BP', 'ExecuteUbergraph_OptionsConfirmScreen_BP', 903)
assert any(c['call'].endswith('.ResetSettingsOfMenu') for c in options_actions['calls'])
new_footer = expressions(read_script('NewGameConfirmScreen_BP', 'SetInitialButtons'))
for runtime, field, key in ((0, 'ConfimButton', 'Start'), (88, 'RejectButton', 'Cancel')):
    assert new_footer[runtime]['context']['field'] == [field]
    assert new_footer[runtime]['call']['call'] == 'SetButtonText'
    assert new_footer[runtime]['call']['arguments'][0]['key'] == {'string': key}
assert new_footer[302]['call']['call'].endswith('.SetEnabled')
assert new_footer[302]['context']['field'] == ['VR4UIButton_Back']
assert new_footer[302]['call']['arguments'] == [{'literal_token': 0x28}]
print('PASS: exact original cooked SHA256s; complete disk/runtime script consumption; Next binding -> stage4 original save/finish, GameInstance flag -> real flat mesh with identical video, and exact Accept/Reset/Start/Cancel footer owners')
