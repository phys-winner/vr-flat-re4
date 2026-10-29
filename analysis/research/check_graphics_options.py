"""Prove original console ABIs and execute production graphics/menu/persistence paths on host."""
import os
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LLVM = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64'))
VC = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK = Path(r'C:\Program Files (x86)\Windows Kits\10')
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest() == '6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
relocations = {a: c for a, kind, c in struct.iter_unpack('<QQq', native[0x2e6c028:0x2e6c028 + 0x1146540]) if kind == 1027}
graphics = (ROOT / 'analysis/re4fix_graphics.h').read_text(encoding='utf-8')
menu = (ROOT / 'analysis/re4fix_mobile_menu.h').read_text(encoding='utf-8')
guards = re.findall(r'\{(0x[0-9a-f]+), \{(0x[0-9a-f]+), (0x[0-9a-f]+)\}\}', graphics)
assert len(guards) == 9
for address, first, second in guards:
    assert struct.unpack_from('<II', native, int(address, 16)) == (int(first, 16), int(second, 16)), address
classes = re.findall(r'\{(0x[0-9a-f]+), (0x[0-9a-f]+), (0x[0-9a-f]+), ([01]), 0, 0, 0\}', graphics)
assert len(classes) == 4
for vtable, getter, setter, floating in classes:
    vtable, getter, setter = map(lambda value: int(value, 16), (vtable, getter, setter))
    assert relocations[vtable + 0x20] == 0x65ece54
    assert relocations[vtable + 0x88] == setter
    assert relocations[vtable + (0xa0 if floating == '1' else 0x98)] == getter
names = ('r.MaxAnisotropy', 'r.MipMapLODBias', 'r.ViewDistanceScale', 'r.StaticMeshLODDistanceScale',
         'r.SkeletalMeshLODBias', 'r.Shadow.MaxResolution')
for name in names:
    assert (name + '\0').encode('utf-16-le') in native and '"' + name + '"' in graphics
assert 'r.ScreenPercentage' not in graphics
assert 'mn.audio = mm_fname("AudioOptions")' in menu
assert 'if (injected && mm_owner_is_options(self)) { mm_inject_extras(self); mm_inject_graphics(self); }' in menu
# The original actor Blueprint places rows with one screen-wide ValidIndex, reset only by elementIndex zero.
spawn = json.loads((ROOT / 'analysis/research/flat_ui_graph/OptionsUIScreen_BP.SpawnMenuEntry.script.json').read_text(encoding='utf-8'))
tree = {node['runtime']: node['expression'] for node in spawn['tree']}
assert tree[5]['let_bool'][1]['arguments'] == [{'field': ['elementIndex'], 'owner': 17}, {'int': 0}]
assert tree[53]['let'][0] == {'field': ['ValidIndex'], 'owner': 33} and tree[53]['let'][2] == {'int': 0}
assert tree[1337]['arguments'][0] == {'field': ['ValidIndex'], 'owner': 33}
assert tree[1369]['call']['call'] == '/Script/VR4.VR4UIScrollComponent.AttachActor'
assert tree[1812]['let'][2]['arguments'] == [{'field': ['ValidIndex'], 'owner': 33}, {'int': 1}]
# The original footer first opens its Yes/No confirmation; only Yes resets CurrentMenu.MenuName.
directory = ROOT / 'analysis/research/flat_ui_graph'
footer_body = (directory / 'OptionsConfirmScreen_BP.uexp').read_bytes()
assert hashlib.sha256(footer_body).hexdigest() == 'cf2e2d7afbd99dc7f252f3062115dfaaa08b537e6dc089ef48c58d263aa18713'
footer_package = json.loads((directory / 'OptionsConfirmScreen_BP.package.json').read_text(encoding='utf-8'))
for label, entrypoint in (('Confirm', 369), ('Reject', 182)):
    item = next(item for item in footer_package['exports'] if f'VR4UIButton_{label}_' in item['name'] and 'OnUIComponentClicked' in item['name'])
    start = item['uexp_offset'] + 134 + 68
    assert footer_body[start:start + 14] == b'\x46\x01\x00\x00\x00\x1d' + struct.pack('<I', entrypoint) + b'\x16\x04\x0b\x53'
footer = {item['runtime']: item['expression'] for item in json.loads((directory / 'OptionsConfirmScreen_BP.ExecuteUbergraph_OptionsConfirmScreen_BP.script.json').read_text(encoding='utf-8'))['tree']}
assert footer[182]['let_bool'][1]['arguments'] == [{'field': ['isConfirmMenu?'], 'owner': 11}]
assert footer[230]['call'] == 'SetButtons'
assert footer[369] == {'jump_if_not': 684, 'condition': {'field': ['isConfirmMenu?'], 'owner': 11}}
reset = footer[583]['call']
assert reset['call'].endswith('.ResetSettingsOfMenu')
assert reset['arguments'][0]['context']['call']['field'] == ['CurrentMenu']
assert reset['arguments'][0]['call']['field'] == ['MenuName']
assert footer[783]['call']['call'] == 'SubAction_B'
spawn_menu = {item['runtime']: item['expression'] for item in json.loads((directory / 'OptionsUIScreen_BP.Spawn Menu.script.json').read_text(encoding='utf-8'))['tree']}
assert spawn_menu[109]['arguments'][1] == {'name': 'MenuName'}
assert spawn_menu[109]['arguments'][2]['struct_member']['field'] == ['Name']
# RefreshMenu performs UpdateMenu and its refresh delegates; it does not call SetCurrentMenu.
for start, end, digest in ((0x630f64c, 0x630f6a4, 'bc3fa4c91a8a00a57498bd05638a72bedb86db4b32d5b935577eae8cf3ccff76'),
                           (0x630e400, 0x630e854, '846547fe5004d80499efde9534597cc8aa6f4b04f12c500710d35dc2ca461d44')):
    assert hashlib.sha256(native[start:end]).hexdigest() == digest
op = struct.unpack_from('<I', native, 0x630f69c)[0]
assert op & 0xfc000000 == 0x14000000
offset = op & 0x3ffffff
if offset & 0x2000000: offset -= 0x4000000
assert 0x630f69c + 4*offset == 0x630e400
extra_dir = ROOT/'analysis/research/extras_ui_originals'
extra_package = json.loads((extra_dir/'OptionsScreen_Menu.package.json').read_text())
extra_asset, extra_body = ((extra_dir/('OptionsScreen_Menu'+ext)).read_bytes() for ext in ('.uasset', '.uexp'))
assert hashlib.sha256(extra_asset).hexdigest() == 'f815c0ef879316e03effc474ea0e4e65499f1e3ae7ef6c245c34247a7081de76'
assert hashlib.sha256(extra_body).hexdigest() == '065af738d202ea409b388d3352bdc523444042d7e36a5a297b88dd74ae726138'
owner = next(item for item in extra_package['exports'] if item['name'] == 'OptionsScreen_Menu_C')
field = owner['uexp_offset']+0x240
assert struct.unpack_from('<4I', extra_body, field) == (extra_package['names'].index('NameProperty'), 0, extra_package['names'].index('MenuName'), 0)
assert struct.unpack_from('<I', extra_body, field+20)[0] == 1
assert struct.unpack_from('<3I', native, 0x8976e18) == (0xb9804d08, 0xf9400269, 0xf8286a89)  # Native SetNameProperty copies8 bytes at reflected offset.
assert hashlib.sha256(native[0x8c703e4:0x8c70400]).hexdigest() == '0a2ef7932ce5a8c5395051c993e3a8fce443b65c7d6d84936e6f946a98462232'
assert struct.unpack_from('<I', native, 0x8c703f4)[0] == 0xf9405008  # Original UActorComponent::GetOwner reads+a0.
binding = json.loads((extra_dir/'runtime_binding.json').read_text())
assert (binding['property'],binding['offset'],binding['element_size'],binding['array_dim']) == ('menuName','0x258',8,1)
assert struct.unpack_from('<I', native, 0x630df04)[0] == 0x51000518  # AddEntry elementIndex = entries.Num() - 1.
assert 'r.MobileHDR=False' in (ROOT / 'analysis/research/graphics-evidence/VR4/Config/DefaultEngine.ini').read_text()
call = struct.unpack_from('<I', native, 0x8ac5a90)[0]
assert call & 0xfc000000 == 0x94000000
immediate = call & 0x03ffffff
if immediate & 0x02000000: immediate -= 0x04000000
assert 0x8ac5a90 + immediate * 4 == 0x8abd2e0  # BL IsMobileHDR in SupportsScreenPercentage.
assert struct.unpack_from('<I', native, 0x8ac5aa4)[0] == 0x2a1f03e0  # Mobile LDR unsupported -> false.

def function(name):
    match = re.search(r'^(?:static )?[^\n]+\b' + re.escape(name) + r'\([^;\n]*\) \{', menu, re.M)
    assert match, name
    opening = menu.index('{', match.start())
    depth = 1
    index = opening + 1
    while depth:
        if menu[index] == '{': depth += 1
        elif menu[index] == '}': depth -= 1
        index += 1
    return menu[match.start():index] + '\n'

prefix = graphics[:graphics.index('\nvoid graphics_audit_device')]
prefix = prefix.replace('#include <vulkan/vulkan_core.h>\n', '') + '\n#endif\n'
prefix += menu[menu.index('struct mobile_prefs'):menu.index('// ---------------- engine ABI')]
prefix += menu[menu.index('enum { L_TITLE'):menu.index('// ---------------- names')]
start = menu.index('static struct {', menu.index('// ---------------- names'))
prefix += menu[start:menu.index('} mn;', start) + len('} mn;')] + '\n'
prefix += menu[menu.index('static unsigned char mm_current_table_kind;'):menu.index('static void mm_row(')]
for name in ('mm_u64', 'mm_put', 'mm_language_autonym', 'mm_inject', 'mm_extras_value', 'mm_extras_values', 'mm_inject_extras', 'mm_inject_graphics_open', 'mm_inject_graphics', 'hook_menu_add_entries'):
    prefix += function(name)
prefix += menu[menu.index('static unsigned mm_row_logs;'):menu.index('void hook_menu_add_entry(')]
for name in ('hook_menu_add_entry', 'mm_own', 'mm_graphics_index', 'mm_bind_extras', 'hook_opt_get'):
    prefix += function(name)
start = menu.index('static void* volatile mm_pending_menu_component;')
prefix += menu[start:menu.index('void hook_opt_apply', start)]
for name in ('hook_opt_apply', 'hook_opt_get_slider', 'hook_opt_apply_slider', 'hook_opt_reset', 'mm_refresh_extras', 'touch_mobile_menu_tick'):
    prefix += function(name)
# Windows uses LLP64; retain the production ARM64 pointer width in this host-only translation.
prefix = re.sub(r'\bunsigned long\b(?! long)', 'uintptr_t', prefix)
fixture = 'static void fixture_image(unsigned char* p){\n*(void**)(p+0xa6b1ac0)=(void*)1;\n'
for address, first, second in guards:
    fixture += f'*(unsigned*)(p+{address})={first};*(unsigned*)(p+{address}+4)={second};\n'
fixture += '}\nstatic const uintptr_t fixture_guards[]={' + ','.join(a for a, _, _ in guards) + '};\n'
fixture += f'static const unsigned fixture_guard_count={len(guards)};\n'

with tempfile.TemporaryDirectory(prefix='graphics-options-', dir=ROOT / 'analysis/research') as temporary:
    work = Path(temporary)
    (work / 'graphics_fixture.h').write_text(fixture + prefix, encoding='utf-8')
    source = ROOT / 'analysis/research/test_graphics_options.c'
    obj, exe = work / 'graphics.obj', work / 'graphics.exe'
    subprocess.run([str(LLVM / 'bin/clang.exe'), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
                    '-I', str(work), '-isystem', str(VC / 'include'), '-isystem', str(SDK / 'Include/10.0.26100.0/ucrt'),
                    '-c', str(source), '-o', str(obj)], check=True)
    subprocess.run([str(VC / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{VC / "lib/x64"}', f'/LIBPATH:{SDK / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{SDK / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe), str(work / 'prefs.txt')], check=True)
    view = (ROOT / 'analysis/re4fix_view_extra.h').read_text(encoding='utf-8')
    guards = re.findall(r'\{(0x[0-9a-f]+), \{([^}]+)\}\}', view)
    assert len(guards) == 4
    for address, values in guards:
        words = [int(word.strip(), 16) for word in values.split(',')]
        if words[2] == 0: words = words[:2]
        assert struct.unpack_from('<' + 'I' * len(words), native, int(address, 16)) == tuple(words), address
    source = ROOT / 'analysis/research/test_view_extra.c'
    subprocess.run([str(LLVM / 'bin/clang.exe'), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(source)], check=True)
    subprocess.run([str(LLVM / 'bin/clang.exe'), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
                    '-Wno-int-to-pointer-cast', '-Wno-int-to-void-pointer-cast',
                    '-isystem', str(VC / 'include'), '-isystem', str(SDK / 'Include/10.0.26100.0/ucrt'),
                    '-c', str(source), '-o', str(obj)], check=True)
    subprocess.run([str(VC / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{VC / "lib/x64"}', f'/LIBPATH:{SDK / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{SDK / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
subprocess.run([str(LLVM / 'bin/clang.exe'), '--target=aarch64-linux-android23', '-O2', '-fPIC', '-fsyntax-only',
                '-I', str(ROOT / 'analysis'), str(ROOT / 'analysis/re4fix.c')], check=True)
print('PASS: all nine original preimages, four relocated int/float/value/ref vtable ABIs, actual cooked cvar names, mobile LDR screen-scale rejection, full ARM64 source syntax')
