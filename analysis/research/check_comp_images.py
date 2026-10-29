"""Compile and execute the actual native proxy-image barrier/transfer wrappers."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
llvm=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
source=root/'analysis/research/test_comp_images.c'
clang=llvm/'clang.exe'
symbols=subprocess.check_output([str(llvm/'llvm-nm.exe'),'-D','-S','-C',str(root/'apk_decoded/lib/arm64-v8a/libUE4.so')],text=True)
for name,address in (('vkCmdPipelineBarrier',0xa5e27b0),('vkCmdCopyImage',0xa5e2748),
                     ('vkCmdBlitImage',0xa5e2750),('vkCmdResolveImage',0xa5e2790)):
    match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+B\s+VulkanDynamicAPI::'+name+r'\s*$',symbols,re.M)
    assert match and int(match[1],16)==address and int(match[2],16)==8,name
print('PASS: original ELF exact 8-byte PipelineBarrier/CopyImage/BlitImage/ResolveImage dynamic API slots',flush=True)
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-comp-images-') as temporary:
    obj,exe=Path(temporary)/'test.obj',Path(temporary)/'test.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-idirafter',str(llvm.parent/'sysroot/usr/include'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: actual production proxy image header, Android ARM64 -Werror and Windows C execution',flush=True)
