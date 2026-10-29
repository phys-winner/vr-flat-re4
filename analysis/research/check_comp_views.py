"""Execute the actual owned linear-BGRA framebuffer helper without device writes."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=root/'analysis/research/test_comp_views.c'
vulkan_header=clang.parents[1]/'sysroot/usr/include/vulkan/vulkan_core.h'
sdk_text=vulkan_header.read_text(encoding='utf-8')
sdk_definitions=[]
for name,expected in [('IDENTITY',0),('R',3),('G',4),('B',5),('A',6)]:
    value=int(re.search(rf'\bVK_COMPONENT_SWIZZLE_{name}\s*=\s*(\d+)\s*,',sdk_text).group(1))
    assert value==expected,(name,value,expected)
    sdk_definitions.append(f'-DRE4_SDK_VK_SWIZZLE_{name}={value}')
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',*sdk_definitions,'-fsyntax-only',str(source)],check=True)
with tempfile.TemporaryDirectory(prefix='re4-comp-views-') as temporary:
    obj=Path(temporary)/'comp_views.obj'; exe=Path(temporary)/'comp_views.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',*sdk_definitions,
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: official NDK VkComponentSwizzle values, Android ARM64 -Werror syntax and actual helper host execution',flush=True)
