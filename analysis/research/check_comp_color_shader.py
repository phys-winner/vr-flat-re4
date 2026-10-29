"""Recompile retained probe GLSL, validate embedded SPIR-V, and execute the actual diagnostic header."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

VERTEX='''#version 450
layout(location=0) out vec2 uv;
void main() {
    vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);
    uv=p;
    gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
'''
FRAGMENT='''#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    color=uv.x<0.25?vec4(1,0,0,1):uv.x<0.5?vec4(0,1,0,1):uv.x<0.75?vec4(0,0,1,1):vec4(0.25,0.25,0.25,1);
}
'''
root=Path(__file__).resolve().parents[2]
ndk=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018'))
llvm=ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin'
shader_tools=ndk/'shader-tools/windows-x86_64'
header=(root/'analysis/re4fix_comp_color_diag.h').read_text()
with tempfile.TemporaryDirectory(prefix='re4-color-shader-') as temporary:
    output=Path(temporary)
    for stage,name,source in [('vert','vertex',VERTEX),('frag','fragment',FRAGMENT)]:
        glsl,spv=output/f'probe.{stage}',output/f'probe.{stage}.spv'
        glsl.write_text(source)
        subprocess.run([str(shader_tools/'glslc.exe'),'--target-env=vulkan1.0','-O','-o',str(spv),str(glsl)],check=True)
        subprocess.run([str(shader_tools/'spirv-val.exe'),'--target-env','vulkan1.0',str(spv)],check=True)
        match=re.search(r'static const uint32_t comp_color_'+name+r'_spv\[\]=\{([^}]+)\};',header)
        assert match,name
        embedded=bytes().join(struct.pack('<I',int(word,16)) for word in re.findall(r'0x[0-9a-f]+',match[1]))
        assert embedded==spv.read_bytes(),f'{name} embedded bytes differ from retained GLSL'
    print('PASS: retained descriptor-free fullscreen RGB/linear-gray0.25 GLSL recompiles byte-for-byte; spirv-val Vulkan1.0 passes',flush=True)
    vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
    sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
    for filename in ['test_comp_color_diag.c','test_comp_color_shader_diag.c']:
        source=root/'analysis/research'/filename
        subprocess.run([str(llvm/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
        obj,exe=output/(source.stem+'.obj'),output/(source.stem+'.exe')
        subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-idirafter',str(llvm.parent/'sysroot/usr/include'),'-c','-o',str(obj),str(source)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)
print('PASS: original clear probes and actual shader/resolve probe header; Android ARM64 -Werror and Windows execution',flush=True)
