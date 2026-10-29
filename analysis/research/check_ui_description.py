"""Original options binding evidence and execution of the live description/question helper."""
import os
from pathlib import Path
import json
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
native = (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert struct.unpack_from('<4I', native, 0x7da4280) == (0xd10183ff, 0xf90013f7, 0xa90357f6, 0xa9044ff4)
assert struct.unpack_from('<2I', native, 0x83ad964) == (0xd104c3ff, 0xf90073fc)
assert struct.unpack_from('<I', native, 0x9f6ff58+0x24)[0] == 0xc3
runtime = (root / 'analysis/logs/p320_mobile_ui/p322_footer_scroll.log').read_text(encoding='utf-8-sig')
for name, offset in (('DescriptionPanel', '438'), ('Description_Widget', '258'), ('DescriptionWidget', '268'),
                     ('DoorLeftScreen', '3a0'), ('DoorRightScreen', '398'), ('Header Widget', '258')):
    assert re.search(r"name='"+name+r"' type='ObjectProperty' offset="+offset+r" element=8 dim=1", runtime)
assert "class='DoorDescriptionPanel_BP_C' size=270" in runtime
assert "class='UI_MainMenu_Info_Widget_C' size=819" in runtime
assert "class='UI_MainMenu_Options_Confirm_Header_Widget_C' size=248" in runtime
assert "name='Text' type='TextProperty' offset=0 element=24 dim=1" in runtime
buttons = json.loads((root / 'analysis/research/flat_ui_graph/OptionsConfirmScreen_BP.SetButtons.script.json').read_text())
assert buttons['export']['sha256'] == '43b884214f2c94711014f64edd6996c6e81fc5ebad4de05f441a73311d4f46e1'
for address in (359, 703):
    expression = next(node['expression'] for node in buttons['tree'] if node['runtime'] == address)
    assert expression['context']['field'] == ['Header Widget']
    assert expression['call']['call'] == '/Script/Engine.SceneComponent.SetVisibility'
    assert expression['call']['arguments'][0]['field'] == ['isConfirmMenu?']
cinema = (root / 'analysis/re4fix_cinema_ui.h').read_text()
fragment = cinema[cinema.index('static int cinema_ui_named('):cinema.index('// Explicit diagnostic seed only;')]
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
source = root / 'analysis/research/test_ui_description.c'
with tempfile.TemporaryDirectory(prefix='re4-description-') as td:
    directory = Path(td)
    binding, obj, exe = directory/'binding.h', directory/'description.obj', directory/'description.exe'
    binding.write_text(fragment)
    define = '-DDESCRIPTION_BINDING_FRAGMENT="'+binding.as_posix()+'"'
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', define, '-fsyntax-only', str(source)], check=True)
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', define,
                    '-isystem', str(vc/'include'), '-isystem', str(sdk/'Include/10.0.26100.0/ucrt'),
                    '-c', '-o', str(obj), str(source)], check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{vc/"lib/x64"}', f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: immutable native visibility ABI, saved exact runtime description/header bindings and original confirmation visibility graph; Android ARM64 -Werror and actual production C host execution', flush=True)
