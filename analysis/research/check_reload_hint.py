"""Check native GrabAmmo label contracts and execute actual caption/reload-predicate C."""
import os
from pathlib import Path
import re
import json
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
tutorial=(root/'analysis/re4fix_tutorial.h').read_text(encoding='utf8')
preimages=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',tutorial)
assert len(preimages)==10
for address,first,second in preimages:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
for address,offset in ((0x9d87c60,0x2b0),(0x9d87d20,0x298),(0x9e0e920,0x228),(0x9e0e8f0,0x230)):
    assert struct.unpack_from('<I',native,address+36)[0]==offset
# Native constructor stores WidgetComponent in ButtonLabel228, LineComponent in Line230.
for address,word in ((0x636e798,0x946b7551),(0x636e7c0,0xf9011660),(0x636e7e0,0x9400e3b0),(0x636e808,0xf9011a60),
                     (0x625707c,0xf9401681),(0x6257084,0x94045e28),(0x6257090,0x9409f8bc),
                     (0x61eaecc,0x9487fa0d)):
    assert struct.unpack_from('<I',native,address)[0]==word,hex(address)
assert "TUTORIAL instruction 'GrabAmmoVignetteSit'" in (root/'analysis/logs/p320_mobile_ui/p322_final_native.log').read_text()
print('PASS:10 native tutorial/Slate geometry preimages,4 label fields, exact authored WidgetComponent/StaticMesh Line/TrackComponent/SetLabel producer and captured GrabAmmo instruction',flush=True)

def function(text,signature):
    match=re.search(re.escape(signature)+r'\s*\{',text)
    assert match,signature
    pos=text.index('{',match.start())+1;depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[match.start():pos]

cinema=(root/'analysis/re4fix_cinema.h').read_text()
hud=(root/'analysis/re4fix_hud_extra.h').read_text()
helpers='\n'.join((function(cinema,'static int cinema_object(const void* object,const void* target,int minimum)'),
    function(hud,'static int hs_reload_possible(void* hand,void* gun)'),
    function(hud,'static int touch_hud_reload_needed(long now)')))
capture=json.loads((root/'analysis/logs/p324_ui/p327_hint_geometry.json').read_text())
rows=[entry['slate'][0]['float14'] for entry in capture]
def c_float(value):
    literal=format(value,'.17g')
    return literal+('f' if '.' in literal or 'e' in literal else '.0f')
helpers+='\nstatic const float caption_capture[3][14]={'+','.join('{'+','.join(map(c_float,row))+'}' for row in rows)+'};\n'
widget_log=(root/'analysis/logs/p324_ui/p327_live_hint_widget_inspect.log').read_text()
for name,offset in (('ButtonLabel','240'),('ButtonLabelAlt','248')):
    assert re.search("name='"+name+"' type='ObjectProperty' offset="+offset+" element=8 dim=1 value=.*class='TextBlock'",widget_log)
assert 'loaded==0 && hs_reload_possible(hand,gun) ? now : 0' in hud
# AmmoAvailable accepts positive reserves, or an ejected magazine with current(true)>0.
for address,word in ((0x628d30c,0xf943ed08),(0x628d338,0x3974b268),(0x628d344,0x52800021),(0x628d350,0xf943dd08)):
    assert struct.unpack_from('<I',native,address)[0]==word
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-reload-hint-') as temporary:
    d=Path(temporary);support=d/'helpers.h';support.write_text(helpers,encoding='utf8')
    source=root/'analysis/research/test_reload_hint.c';obj=d/'hint.obj';exe=d/'hint.exe'
    common=['-O2','-Werror','-I',str(root/'analysis'),'-DRELOAD_HINT_HELPERS="'+support.as_posix()+'"']
    subprocess.run([str(clang),'--target=aarch64-linux-android23',*common,'-fsyntax-only',str(source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc',*common,'-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast','-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
