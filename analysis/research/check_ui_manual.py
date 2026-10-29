"""Exact saved Manual sibling bindings and execution of the production native layout."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
runtime = (root / 'analysis/logs/p324_ui/p332_manual_owner_inspect.log').read_text(encoding='utf-8-sig')
for object_id, name, offset in (
    (0, 'UIScreen', '370'), (0, 'DoorRightScreen', '398'), (0, 'DoorLeftScreen', '3a0'),
    (3, 'Widget_Main', '258'), (3, 'Widget', '328'), (3, 'Owner', 'e0'),
    (4, 'Widget_InfoText', '258'), (4, 'Info', '278'), (4, 'Owner', 'e0'),
    (5, 'ContentParent', '290'), (5, 'main', '2c0'), (5, 'InfoText', '2b8'), (5, 'Owner', 'e0'),
):
    assert re.search(r"PHONEPROP id="+str(object_id)+r" name='"+name+r"' type='ObjectProperty' offset="+offset+r" element=8 dim=1", runtime)
for cls, size in (('UI_PauseMenu_Content_PlayingManual_BP_C','428'), ('PlayingManualEntrySelection_C','2e7'),
                  ('PlayingManualMain_C','3b0'), ('PlayingManualInfoText_C','299'), ('PlayingManualMainWidget_C','d24')):
    assert f"class='{cls}' size={size}" in runtime
assert re.search(r"PHONEPROP id=7 name='Widget'.*offset=498.*value=(0x[0-9a-f]+).*class='PlayingManualMainWidget_C'", runtime)
assert re.search(r"PHONEPROP id=9 name='Widget'.*offset=498.*value=(0x[0-9a-f]+).*class='PlayingManualInfoTextWidget_C'", runtime)
with (root / 'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as native:
    native.seek(0xa0b7828+0x24)
    assert struct.unpack('<I',native.read(4))[0] == 0xe0
    native.seek(0x6372050)
    assert struct.unpack('<5I',native.read(20)) == (0x39417268,0xb9405a69,0xb3601d09,0xf27b013f,0x1a9f17e0)
flat = (root / 'analysis/re4fix_flat_view.h').read_text()
menu = (root / 'analysis/re4fix_menu_visual.h').read_text()
cinema = (root / 'analysis/re4fix_cinema_ui.h').read_text()
footer = (root / 'analysis/re4fix_ui_footer.h').read_text()
fragments = {
    'BINDING': menu[menu.index('static int menu_visual_layout('):menu.index('static int menu_visual_component(')]
               + cinema[cinema.index('static int cinema_ui_named('):cinema.index('// Explicit diagnostic seed only;')],
    'FRAME': flat[flat.index('static volatile float g_menu_yaw_off'):flat.index('// ---- UI screen diagnostics')],
    'WIDGET': flat[flat.index('static struct cinema_ui_vec2 (*fv_current_draw_size'):flat.index('#include "re4fix_ui_footer.h"')],
    'SCROLL': footer[footer.index('static int fv_scroll_plane('):footer.index('static void fv_update_scrollers(')],
}
assert flat.index('fv_manual_tick(screen,&camera)') < flat.index('if (fv_frame_screen(screen, &camera))')
assert 'fv_manual_restore();fv_description_tick(0)' in flat
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
source = root / 'analysis/research/test_ui_manual.c'
with tempfile.TemporaryDirectory(prefix='re4-manual-') as td:
    directory = Path(td)
    defines = []
    for name, text in fragments.items():
        header = directory / (name.lower()+'.h')
        header.write_text(text)
        defines.append('-DMANUAL_'+name+'_FRAGMENT="'+header.as_posix()+'"')
    includes = ['-I',str(root/'analysis'),*defines]
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',*includes,'-fsyntax-only',str(source)],check=True)
    obj, exe = directory/'manual.obj',directory/'manual.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',*includes,
                    '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
                    '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
                    'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
                    f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: exact p332 native Manual property/widget identities, immutable Owner/IsEnabled ABI, Android ARM64 -Werror and actual production C host execution',flush=True)
