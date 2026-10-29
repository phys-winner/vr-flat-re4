"""Validate the original native producer/caller and execute the actual CI flag repair."""
import os
from pathlib import Path
import hashlib
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
llvm=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
original=root/'apk_decoded/lib/arm64-v8a/libUE4.so'
native=original.read_bytes()
assert hashlib.sha256(native[0x65455b4:0x65458ac]).hexdigest()=='658f17ea969881ed6bf72d268030500aa022af06201717cfa2ac3d56aea3a1ac'
assert struct.unpack_from('<10I',native,0x6545960)==(0x97ffff15,0x9001f168,0xf946c508,0xf9400680,0x910123e1,0xaa1f03e2,0xf9400108,0xaa1703e3,0xd63f0100,0x340000e0)
symbols=subprocess.check_output([str(llvm/'llvm-nm.exe'),'-D','-S','-C',str(original)],text=True)
match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+B\s+VulkanDynamicAPI::vkCreateImage\s*$',symbols,re.M)
assert match and int(match[1],16)==0xa5e2570 and int(match[2],16)==8
print('PASS: immutable original GenerateImageCreateInfo producer fingerprint, exact native CreateImage caller LR0x6545984 and 8-byte dynamic API slot0xa5e2570',flush=True)
source=root/'analysis/research/test_comp_image_flags.c'
subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-image-flags-') as temporary:
    obj,exe=Path(temporary)/'test.obj',Path(temporary)/'test.exe'
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-idirafter',str(llvm.parent/'sysroot/usr/include'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Android ARM64 -Werror, Vulkan SDK structs/function ABI and actual production C execution',flush=True)
