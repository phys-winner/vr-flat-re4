"""Exact native UI-plane ABI proof and execution of the production framing geometry."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
native = (ROOT / 'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
for first, last, digest in (
    (0x6378330, 0x63784c4, '65902014996ace5d5dc08b6f89a70d9c6339aeec171f9b3147cd6510eb139627'),
    (0x63784c4, 0x6378f48, 'd79791a49b6d3a84123970a7fbb24b4a093f2c03219d6f5465c45dd6d1a1f632'),
    (0x6378f48, 0x63795c0, 'e3bf472d026c974ca12aa9269da20effd5c0a5cc38160353374f4cace21a920b'),
    (0x637a830, 0x637aa8c, 'a378ffec776bd5a9d0a84a095686099b8a902fc25fb0b60d3ea70333501d4fa8'),
    (0x63795c0, 0x6379b84, '698987bbe32c946a78888df7088f322f2c0f5b6db63308f67c5e139c864da902'),
    (0x637b594, 0x637bd30, 'ceb8e5d15e182d883508c09f87461244670f92f8b9a90244e3efab2c4b3ded95'),
):
    assert hashlib.sha256(native[first:last]).hexdigest() == digest, hex(first)
assert struct.unpack_from('<II', native, 0x63713b4) == (0xf9018801, 0xd65f03c0)  # SetParent only310, not registration308
for caller, target in ((0x6378550, 0x6378f48), (0x6378564, 0x63795c0), (0x63789c0, 0x8712470),
                       (0x6378afc, 0x63713b4), (0x6378ef8, 0x637a664), (0x637a9d8, 0x63713b4),
                       (0x637b5ec, 0x63795c0), (0x637bb1c, 0x8712470)):
    op = struct.unpack_from('<I', native, caller)[0]
    assert op & 0xfc000000 == 0x94000000
    offset = op & 0x3ffffff
    if offset & 0x2000000: offset -= 0x4000000
    assert caller + 4*offset == target, hex(caller)
for address, words in (
    (0x637116c, (0xbd031800, 0xbd031c01, 0xd65f03c0)),
    (0x6371184, (0xbd431800, 0xbd431c01, 0x91070008, 0xaa0103e0, 0xaa0803e1, 0x14000416)),
    (0x63721f0, (0xd10203ff, 0x6d032beb, 0x6d0423e9, 0xf9002bf5)),
    (0x637abcc, (0xbd431800, 0xbd431c01, 0x91070008, 0xaa0103e0, 0xaa0803e1, 0x17ffdd84)),
):
    assert struct.unpack_from('<' + 'I' * len(words), native, address) == words, hex(address)
for address, offset in ((0x9e0fbb0, 0x318), (0x9e0fb88, 0x31c), (0xa0b7530, 0x130),
                        (0xa235ba8, 0xc0), (0x9e11c48, 0x238), (0x9e10ed8, 0x308),
                        (0x9e12a08, 0xa0), (0x9f77e20, 0x429), (0x9f77c28, 0x45c), (0x9f77698, 0x518), (0x9e10ea8, 0x310),
                        (0x9e14028, 0x318), (0x9e14000, 0x31c), (0x9e0fb28, 0x320), (0xa1087b8, 0x1f8)):
    assert struct.unpack_from('<I', native, address + 36)[0] == offset, hex(address)
header = (ROOT / 'analysis/re4fix_flat_view.h').read_text()
installer = header.split('static void install_flat_view(')[1]
for address, values in re.findall(r'\{(0x[0-9a-f]+), \{([^}]+)\}\}', installer):
    words = [int(v.strip(), 16) for v in values.split(',')]
    assert struct.unpack_from('<' + 'I' * len(words), native, int(address, 16)) == tuple(words), address
print('PASS: native plane Width318/Height31c, ComponentToWorld1c0, exact IntersectBoundedPlane call and Root/AttachParent/registered-controls offsets', flush=True)
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
source = ROOT / 'analysis/research/test_ui_frame.c'
subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(source)], check=True)
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-ui-frame-') as td:
    obj, exe = Path(td) / 'frame.obj', Path(td) / 'frame.exe'
    fragment = Path(td) / 'native_frame.h'
    fragment.write_text(header[header.index('static volatile float g_menu_yaw_off'):header.index('// ---- UI screen diagnostics')])
    widget = Path(td) / 'native_widget.h'
    widget.write_text(header[header.index('static struct cinema_ui_vec2 (*fv_current_draw_size)'):header.index('#include "re4fix_ui_footer.h"')])
    wizard = Path(td) / 'native_wizard.h'
    wizard.write_text(header[header.index('static struct cinema_ui_weak fv_wizard_finished'):header.index('// GameThread (menu hand tick): publish')])
    defines = ['-DFLAT_VIEW_FRAGMENT="' + fragment.as_posix() + '"', '-DWIDGET_FRAME_FRAGMENT="' + widget.as_posix() + '"',
               '-DWIZARD_FRAME_FRAGMENT="' + wizard.as_posix() + '"']
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-I', str(ROOT/'analysis'), *defines, '-fsyntax-only', str(source)], check=True)
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', '-isystem', str(vc/'include'),
                    '-isystem', str(sdk/'Include/10.0.26100.0/ucrt'), '-I', str(ROOT/'analysis'), *defines, '-c', '-o', str(obj), str(source)], check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{vc / "lib/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
    footer_source = ROOT / 'analysis/research/test_ui_footer.c'
    footer_obj, footer_exe = Path(td) / 'footer.obj', Path(td) / 'footer.exe'
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(footer_source)], check=True)
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', '-isystem', str(vc/'include'),
                    '-isystem', str(sdk/'Include/10.0.26100.0/ucrt'), '-c', '-o', str(footer_obj), str(footer_source)], check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{footer_exe}', str(footer_obj),
                    'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                    f'/LIBPATH:{vc / "lib/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
                    f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(footer_exe)], check=True)
print('PASS: Android23 ARM64 -Werror syntax and actual production C host execution', flush=True)
