"""Compile/execute the actual owned logical-surface header; verify original API slots."""
import os
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
llvm=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
symbols=subprocess.check_output([str(llvm/'llvm-nm.exe'),'-D','-S','-C',str(root/'apk_decoded/lib/arm64-v8a/libUE4.so')],text=True)
for name,address in (('vkGetPhysicalDeviceSurfaceFormatsKHR',0xa5e2848),('vkCreateAndroidSurfaceKHR',0xa5e2870),('vkDestroyImageView',0xa5e2590)):
    match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+B\s+VulkanDynamicAPI::'+name+r'\s*$',symbols,re.M)
    assert match and int(match[1],16)==address and int(match[2],16)==8,name
print('PASS: original ELF dynamic API symbols, exact 8-byte SurfaceFormats/AndroidSurface/DestroyImageView slots',flush=True)
main=(root/'analysis/re4fix.c').read_text()
comp=(root/'analysis/re4fix_comp.h').read_text()
assert main.index('#include "re4fix_comp.h"')<main.index('#include "re4fix_comp_surface.h"')<main.index('#include "re4fix_comp_views.h"')
for name,hook in (('vkGetPhysicalDeviceSurfaceFormatsKHR','h_vkSurfaceFormats'),('vkCreateAndroidSurfaceKHR','h_vkCreateAndroidSurface'),('vkDestroyImageView','h_vkDestroyImageView')):
    assert main.count('!strcmp(name, "'+name+'")')>=2,name # original GIPA and dlsym routes
    assert 'return (void*)&'+hook in main
swap_start=re.search(r'int h_vkCreateSwapchain\([^;{]*\)\s*\{',main).end()
swap=main[swap_start:main.index('int h_vkQueueSubmit(',swap_start)]
assert 'r_vkSurfaceFormats(g_phys_dev, surface' in swap and 'h_vkSurfaceFormats(' not in swap
assert 'g_comp_mutable=__atomic_load_n(&g_comp_format_list,__ATOMIC_ACQUIRE);' in comp
assert '_Alignas(8) unsigned char list[32]={0};unsigned view_formats[2]={44,50};' in comp
assert 'if (g_comp_mutable)' in comp and '*(unsigned*)(ic+16)=8;' in comp
assert '*(unsigned*)list=1000147000u;*(unsigned*)(list+16)=2;' in comp
assert '*(unsigned**)(list+24)=view_formats;' in comp and '*(void**)(ic+8)=list;' in comp
assert '*(unsigned*)(ic + 24) = g_comp_mutable ? 44u : g_comp_fmt;' in comp
device_start=re.search(r'int h_vkCreateDevice\([^;{]*\)\s*\{',main).end()
device=main[device_start:main.index('#include "re4fix_comp.h"',device_start)]
assert 'unsigned char copied_ci[72] __attribute__((aligned(8)));' in device and 'const char* names[257];' in device
assert device.index('comp_prepare_device_formats((void*)phys,ci,copied_ci,names)')<device.index('r_vkCreateDevice(phys, selected_ci, ac, out)')<device.index('comp_capture_device_formats(selected_ci,r);')
assert 'graphics_audit_device(phys, selected_ci, r);' in device
fb_start=re.search(r'int h_vkCreateFB\([^;{]*\)\s*\{',main).end()
fb=main[fb_start:main.index('static long g_exec_n;',fb_start)]
assert fb.index('comp_linear_framebuffer(')<fb.index('ci=selected;')<fb.index('r_vkCreateFB(dev, ci, ac, out)')
assert 'g_views[i].view=0' in main and 'comp_linear_view_destroyed(view);' in main and 'r_vkDestroyImageView(dev,view,allocator);' in main
print('PASS: actual root include/procedure routes, aligned selected device CI prepare/original/capture/audit sequence, physical WSI bypass, capability-gated format-list44/50 and effective framebuffer CI/trace integration',flush=True)
sources=[root/'analysis/research/test_comp_surface.c',root/'analysis/research/test_comp_factory.c']
clang=llvm/'clang.exe'
for source in sources:
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-comp-surface-') as temporary:
    for source in sources:
        obj,exe=Path(temporary)/(source.stem+'.obj'),Path(temporary)/(source.stem+'.exe')
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
            '-idirafter',str(llvm.parent/'sysroot/usr/include'),'-c','-o',str(obj),str(source)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
            f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)
print('PASS: Android ARM64 -Werror, original Vulkan SDK ABI sizes and actual C host execution',flush=True)
