"""Original Load cover delegates and actual projected native Click publication regression."""
import os
from pathlib import Path
import contextlib
import hashlib
import io
import json
import struct
import subprocess
import tempfile
from inspect_ui_package import package
import inspect_flat_bp as bp

ROOT=Path(__file__).resolve().parents[2]
directory=ROOT/'analysis/research/continue_cover_originals'
asset=(directory/'LoadGameCoverScreen_BP.uasset').read_bytes()
body=(directory/'LoadGameCoverScreen_BP.uexp').read_bytes()
assert hashlib.sha256(asset).hexdigest()=='7368e1e2256f3dcab86d08307d21ca02c170dcbf61129102c7b6a985693dadaa'
assert hashlib.sha256(body).hexdigest()=='8e1d7ff2a4ddb55bcd9ad86f40d2c12a1c4bce03ee1ee1a75ed48fa7e1aa6aa5'
metadata=package(asset,body)
assert metadata==json.loads((directory/'LoadGameCoverScreen_BP.package.json').read_text())
bp.DIRECTORY=directory
def script(name):
    item=next(e for e in metadata['exports'] if e['name']==name)
    raw=body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]
    candidates=[(at+4,struct.unpack_from('<I',raw,at)[0]) for at in range(4,len(raw)-16)
        if at+4+struct.unpack_from('<I',raw,at)[0]+12==len(raw) and raw[-13]==0x53]
    for at,count in sorted(candidates,key=lambda x:x[1],reverse=True):
        try:
            with contextlib.redirect_stdout(io.StringIO()):result=bp.read_script('LoadGameCoverScreen_BP',name,at)
        except (AssertionError,IndexError,KeyError,struct.error):continue
        return result
    raise AssertionError(name)

for button,event,entry in (('continue',2,742),('loadSave',5,1012)):
    wrapper=script(f'BndEvt__VR4UIButton_{button}_K2Node_ComponentBoundEvent_{event}_OnUIComponentClicked__DelegateSignature')
    assert wrapper['calls']==[{'disk_offset':68,'call':'ExecuteUbergraph_LoadGameCoverScreen_BP','arguments':[{'int':entry}]}]
    component=next(e for e in metadata['exports'] if e['name']==f'VR4UIButton_{button}_GEN_VARIABLE')
    assert component['class_path']=='/Script/VR4.VR4UIButtonComponent'
    raw=body[component['uexp_offset']:component['uexp_offset']+component['serial_size']]
    assert 'AllowedAction' not in metadata['names'] or struct.pack('<II',metadata['names'].index('AllowedAction'),0) not in raw
graph={node['runtime']:node['expression'] for node in script('ExecuteUbergraph_LoadGameCoverScreen_BP')['tree']}
assert graph[742]['let_object'][1]['call']=='/Script/Engine.Actor.GetOwner'
assert graph[837]['call']=={'call':'PlayOutro','arguments':[]}
assert graph[873]['call']=='/Script/Engine.KismetSystemLibrary.Delay'
assert graph[873]['arguments'][1]=={'float':.5}
assert graph[873]['arguments'][2]['values'][0]=={'skip_offset':15}
assert graph[132]['call']=={'call':'LoadGame','arguments':[{'field':['MostCurrentSave'],'owner':18}]}
print('PASS: exact original cover packages; Continue Click -> original outro/delay/LoadGame(MostCurrentSave), both original button delegates, no authored AllowedAction override',flush=True)

native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for address,word in ((0x6371148,0x390c827f),(0x6371fdc,0xf81e0ff3),(0x6371fe8,0x39499008),
                     (0x6371ff8,0xf9423908),(0x6371ffc,0xd63f0100),
                     (0x6371bcc,0xd101c3ff),(0x6371c54,0xf94006a8),(0x6371c64,0xf90003f3),
                     (0x6371c68,0xad408281),(0x6371c6c,0x3dc00282)):
    assert struct.unpack_from('<I',native,address)[0]==word,hex(address)
for address,offset in ((0x9e0fb28,0x320),(0x9e10ed8,0x308)):
    assert struct.unpack_from('<I',native,address+36)[0]==offset
print('PASS: native button Click action0 constructor, Enabled visibility virtual call and original Click(Object24,ClickState48) ABI/metadata',flush=True)

header=(ROOT/'analysis/re4fix_flat_view.h').read_text()
source=(ROOT/'analysis/re4fix.c').read_text()
world=(ROOT/'analysis/re4fix_world_touch.h').read_text()
fragments={
    'FRAME_OWNED_FRAGMENT':header[header.index('static int fv_frame_owned('):header.index('static void fv_frame_restore(')],
    'FRAME_DESCENDS_FRAGMENT':header[header.index('static int fv_descends_from('):header.index('static int fv_component_plane(')],
    'UI_CLICK_FRAGMENT':'static void (*fv_ui_click)(void*,const void*,const void*);\n'+header[header.index('static int fv_click_component('):header.index('// Native widget rectangles')],
    'LOAD_COVER_FRAGMENT':header[header.index('static int fv_load_cover_button('):header.index('static void touch_ui_targets_tick(')],
    'UI_TARGET_TICK_FRAGMENT':header[header.index('static void touch_ui_targets_tick('):header.index('static long fv_hint_since;')],
    'Q_ROT_FRAGMENT':source[source.index('static void q_rot('):source.index('static float g_eye_height')],
    'UI_PROJECT_FRAGMENT':world[world.index('static int wt_project('):world.index('// Input thread: a short still tap')],
}
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
test=ROOT/'analysis/research/test_ui_load_cover.c'
with tempfile.TemporaryDirectory(prefix='re4-load-cover-') as temp:
    out=Path(temp);defines=[]
    for name,code in fragments.items():
        path=out/(name.lower()+'.h');path.write_text(code);defines.append('-D'+name+'="'+path.as_posix()+'"')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',*defines,'-fsyntax-only',str(test)],check=True)
    obj,exe=out/'load.obj',out/'load.exe'
    compile_cmd=[str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-isystem',str(vc/'include'),
        '-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),*defines,'-c','-o',str(obj),str(test)]
    link_cmd=[str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib',
        'libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}']
    subprocess.run(compile_cmd,check=True);subprocess.run(link_cmd,check=True);subprocess.run([str(exe)],check=True)
    # Reproduce the old omission: type7 receives no Load cover targets despite its live button.
    target=out/'ui_target_tick_fragment.h';original=target.read_text()
    target.write_text(original.replace('type==1 || load_cover ? screen:0','type==1 ? screen:0'))
    subprocess.run(compile_cmd,check=True);subprocess.run(link_cmd,check=True)
    rejected=subprocess.run([str(exe)],capture_output=True)
    assert rejected.returncode!=0 and b'g_ui_target_count==2' in rejected.stderr+rejected.stdout
print('PASS: Android23 ARM64 -Werror syntax, actual target/Click C host regression, old type7 omission rejected by negative fixture',flush=True)
