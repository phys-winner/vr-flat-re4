"""Run the actual sampled presenter C and validate its deployed Vulkan 1.0 shaders."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
ndk=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018'))
clang=ndk/'toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
source=root/'analysis/research/test_overlay_present.c'
presenter=(root/'analysis/re4fix_overlay.c').read_text()
assert 'vkGetPhysicalDeviceProperties' not in presenter, 'presenter still queries GPU identity'
assert not re.search(r'VK_COMPONENT_SWIZZLE_[RB]\b',presenter), 'presenter still swaps sampled channels'
overlay=root/'analysis/overlay'
assets=(overlay/'overlay_assets.h').read_text()
with tempfile.TemporaryDirectory(prefix='re4-overlay-present-') as temporary:
    temporary=Path(temporary)
    shim=temporary/'android';shim.mkdir()
    (shim/'log.h').write_text('#ifndef RE4_TEST_ANDROID_LOG_H\n#define RE4_TEST_ANDROID_LOG_H\n'
        '#define ANDROID_LOG_INFO 4\nint __android_log_print(int,const char*,const char*,...);\n#endif\n')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',
        '-Wno-missing-field-initializers','-fsyntax-only',str(source)],check=True)
    obj=temporary/'present.obj';exe=temporary/'present.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-missing-field-initializers','-I',str(temporary),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-idirafter',str(clang.parents[1]/'sysroot/usr/include'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    for stage in ('vert','frag'):
        binary=temporary/(stage+'.spv')
        subprocess.run([str(ndk/'shader-tools/windows-x86_64/glslc.exe'),
            '--target-env=vulkan1.0','-O','-o',str(binary),str(overlay/('overlay.'+stage))],check=True)
        subprocess.run([str(ndk/'shader-tools/windows-x86_64/spirv-val.exe'),
            '--target-env','vulkan1.0',str(binary)],check=True)
        data=binary.read_bytes();assert data==(overlay/('overlay.'+stage+'.spv')).read_bytes(),stage
        embedded=re.search(r'ovl_'+stage+r'_spv\[(\d+)\] = \{(.*?)\};',assets,re.S)
        words=tuple(int(v,16) for v in re.findall(r'0x([0-9a-f]+)',embedded[2]))
        assert len(words)==int(embedded[1]) and struct.pack('<%dI'%len(words),*words)==data,stage
print('PASS: Android ARM64 -Werror and actual production-C host fixture; both glslc Vulkan1.0 -O outputs byte-identical to embedded/deployed SPIRV and spirv-val clean',flush=True)
frag=(overlay/'overlay.frag').read_text()
assert re.search(r'if\s*\(vColor\.a\s*>\s*1\.5\)',frag)
assert 'vec4(texture(atlas, vUV).rgb, 1.0)' in frag
assert 'outColor = texture(atlas, vUV) * vColor;' in frag
assert '.bgra' not in frag and '.bgr' not in frag
comp=(root/'analysis/re4fix_comp.h').read_text()
assert re.search(r'ovl_init\([^;]*g_own_imgs\s*,\s*g_comp_img\s*,\s*g_comp_mutable\s*\)',comp,re.S)
record=comp[comp.index('static const void* comp_present'):]
assert 'VK_LAYOUT_SHADER_READ' in record or 'VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL' in record
assert 'ovl_record(cmd, i, oq, nq);' in record
assert 'c_CmdBlit(cmd' not in record, 'raw-blit production fallback remains in sampled presentation'
print('PASS: logical RGB/opaque scene branch, existing atlas branch; root compositor proxy-array integration and sampled presentation without raw blit',flush=True)
