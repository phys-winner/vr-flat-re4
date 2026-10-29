"""Original scoped prompt hierarchy/mesh setter proof and actual C lifetime/visibility tests."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
b=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(b).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
rela={a:c for a,t,c in struct.iter_unpack('<QQq',b[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
s=(root/'analysis/re4fix_prompt_visual.h').read_text()
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',s)
assert len(checks)==10
for a,x,y in checks:assert struct.unpack_from('<II',b,int(a,16))==(int(x,16),int(y,16)),a
fields=[(0x9dbfb50,0x270,'MessagePromptUI'),(0x9d934c8,0x230,'mpPlayerPawn'),
    (0xa0b7530,0x130,'RootComponent'),(0xa235b20,0xd0,'AttachChildren'),
    (0xa235ba8,0xc0,'AttachParent'),(0xa1087b8,0x1f8,'ChildActor')]
for a,o,name in fields:
    p=rela[a];assert b[p:b.index(b'\0',p)].decode()==name and struct.unpack_from('<I',b,a+36)[0]==o
assert not any(x in s for x in ['SetHidden','HideUI','SetInteractionCommand','SpawnActor','mv_hidden('])
# Exact original SetVisibility changes bVisible14c bit5, preserves other bits,
# dirties the component and branches past child propagation for enum0.
setter_words={0x83ad998:0x39453809,0x83ad99c:0x79429808,
    0x83ad9b0:0x121a790b,0x83ad9c4:0x7900012a,
    0x83ad9cc:0x51000689,0x83ad9e0:0x71000adf,0x83ad9e4:0x54001002,
    0x83ad9d8:0xf9423d08,0x83ad9dc:0xd63f0100}
for a,w in setter_words.items():assert struct.unpack_from('<I',b,a)[0]==w,hex(a)
assert 'mv_visible(mesh,0,0)' in s and 'mv_visible(mesh,mask->visible[i],0)' in s
# Complete getter has a spawn path; the helper reads its existing cached property only.
assert struct.unpack_from('<I',b,0x62da6f0)[0]==0xf9013a60
assert 'manager+0x270' in s and 'ControllerIcon3d_C' in s
import json
snapshot=json.loads((root/'analysis/research/p272_prompt_parsed.json').read_text())
selection=re.findall(r'\{"(Quest[^\"]+)",(0x[0-9a-f]+)\}',s)
assert len(selection)==14
assert snapshot['owned_controller_pointers']==['0x79ac03f6e0']
controller=next(x for x in snapshot['controller_objects'] if x['pointer']=='0x79ac03f6e0')
assert controller['size']==0x3f8
widget1=next(x for x in controller['properties'] if x['name']=='Widget1')
assert widget1['offset']=='0x270' and widget1['type']=='ObjectProperty' and widget1['element']==8 and widget1['dim']==1
assert widget1['within_class'] and widget1['value_class']=='WidgetComponent'
for name,offset in selection:
    captured=next(x for x in controller['properties'] if x['name']==name)
    assert captured['offset']==offset and captured['type']=='ObjectProperty' and captured['element']==8 and captured['dim']==1
    assert captured['value_class']=='StaticMeshComponent' and captured['within_class']
print('PASS:10 native preimages/6 named fields; cached Floater/ChildActor ownership and native visibility-bit/no-propagation setter',flush=True)
glyph_header=(root/'analysis/re4fix_prompt_glyph.h').read_text()
originals=root/'analysis/research/prompt_ui_originals'
rows=[p.name+':'+hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(originals.iterdir()) if p.suffix in ('.uasset','.uexp')]
assert len(rows)==14 and hashlib.sha256('\n'.join(rows).encode()).hexdigest()=='c5f211d7046feb235c5319f5aa2ae12ba3366eb51f8c96e793ed31d0d359f21c'
from inspect_ui_package import package
for stem in ('InfoRing','NoteInstruction','QteArrows','TopRing','ControllerIcon3d'):
    pkg=package((originals/(stem+'.uasset')).read_bytes(),(originals/(stem+'.uexp')).read_bytes())
    assert pkg==json.loads((originals/(stem+'.package.json')).read_text())
    body=(originals/(stem+'.uexp')).read_bytes();glyphs=set()
    if stem=='ControllerIcon3d':
        for name,target in (('Widget1_GEN_VARIABLE','InfoRing.InfoRing_C'),('WidgetArrows_GEN_VARIABLE','QteArrows.QteArrows_C')):
            item=next(x for x in pkg['exports'] if x['name']==name)
            assert item['class_path']=='/Script/UMG.WidgetComponent'
            payload=body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]
            at=payload.index(struct.pack('<II',pkg['names'].index('WidgetClass'),0))
            assert struct.unpack_from('<4I',payload,at+8)==(pkg['names'].index('ObjectProperty'),0,4,0) and not payload[at+24]
            ref=struct.unpack_from('<i',payload,at+25)[0]
            assert ref<0 and pkg['imports'][-ref-1]['path']=='/Game/Blueprints/UI/FloaterUi/'+target
    for item in pkg['exports']:
        if item['class_path']!='/Script/UMG.Image':continue
        payload=body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]
        marker=struct.pack('<II',pkg['names'].index('ResourceObject'),0) if 'ResourceObject' in pkg['names'] else b''
        at=payload.find(marker) if marker else -1
        while at>=0:
            if at+29<=len(payload) and struct.unpack_from('<4I',payload,at+8)==(pkg['names'].index('ObjectProperty'),0,4,0) and not payload[at+24]:
                ref=struct.unpack_from('<i',payload,at+25)[0]
                if ref<0 and '/Textures/Button_' in pkg['imports'][-ref-1]['path']:glyphs.add(item['name'])
            at=payload.find(marker,at+1)
    if stem=='InfoRing':
        names=glyph_header[glyph_header.index('pv_glyph_names[]'):glyph_header.index('};',glyph_header.index('pv_glyph_names[]'))]
        assert glyphs==set(re.findall(r'"([^"]+)"',names)) and len(glyphs)==23
        # The caption and pad image share native layout; changing a glyph's Visibility can resize it.
        parents={}
        for slot in pkg['exports']:
            if not slot['class_path'].endswith('Slot'):continue
            payload=body[slot['uexp_offset']:slot['uexp_offset']+slot['serial_size']];refs={}
            for field in ('Parent','Content'):
                marker=struct.pack('<II',pkg['names'].index(field),0);at=payload.find(marker)
                while at>=0:
                    if at+29<=len(payload) and struct.unpack_from('<4I',payload,at+8)==(pkg['names'].index('ObjectProperty'),0,4,0) and not payload[at+24]:
                        refs[field]=struct.unpack_from('<i',payload,at+25)[0];break
                    at=payload.find(marker,at+1)
            if all(refs.get(field,0)>0 for field in ('Parent','Content')):parents[refs['Content']]=refs['Parent']
        commands=[e for e in pkg['exports'] if e['name']=='CommandText' and e['class_path']=='/Script/UMG.TextBlock']
        assert len(commands)==2
        for command in commands:
            parent=parents[command['index']];assert pkg['exports'][parent-1]['name']=='HorizontalBox_0'
            same_layout=False
            for image in (e for e in pkg['exports'] if e['name']=='Image_A' and e['class_path']=='/Script/UMG.Image'):
                node=image['index']
                for _ in range(24):
                    node=parents.get(node)
                    if node==parent:same_layout=True;break
                    if not node:break
            assert same_layout
    elif stem=='NoteInstruction':assert glyphs=={'Image_A_3','Image_A_6','Image_B_2','Image_X_2','Image_Y_2'}
    else:assert not glyphs
for address,words in ((0x7e01294,(0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd)),
                      (0x7e1495c,(0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd)),
                      (0x7da4524,(0xfc1c0fe8,0xa90157f6,0xa9024ff4,0xa9037bfd)),
                      (0x7da46d4,(0xfc1d0fe8,0xf90007f5,0xa9014ff4,0xa9027bfd))):
    assert struct.unpack_from('<4I',b,address)==words
for address,offset in ((0x9f6b718,0x1a8),(0x9f5abd8,0x108),(0x9f5a358,0x30),(0x9f7bf38,0x28),(0x9f6ff30,0xc4)):
    assert struct.unpack_from('<I',b,address+36)[0]==offset
assert hashlib.sha256(b[0x7da46d4:0x7da46d4+460]).hexdigest()=='7fe699b420396d0ca07f0a528218a48438a113c851b775369f4ae99d5c6a41ac'
assert hashlib.sha256(b[0x7e3d53c:0x7e3d53c+164]).hexdigest()=='06e77d0672d8e60eb6793f761c0d36d888283057f8227d4f532f77851dc3a6aa'
assert struct.unpack_from('<I',b,0x7da46f0)[0]==0xbd00c400 # str s0,[x0,#c4], RenderOpacity only
assert struct.unpack_from('<2I',b,0x7e3d5b0)==(0xbd4007e0,0x97fd9c48) # float argument -> original setter
print('PASS:14 immutable original prompt/widget members; exact InfoRing23/Note5 button textures, Controller Widget1 vs QteArrows, original CommandText/Image_A shared HorizontalBox; native render-opacity getter/setter/thunk hashes, float ABI/c4 store and tree ownership offsets',flush=True)
core=(root/'analysis/re4fix_cinema.h').read_text()
guard=core[core.index('static int cinema_object('):core.index('static int cinema_on_game_thread(')]
ui=(root/'analysis/re4fix_cinema_ui.h').read_text()
property_helpers=ui[ui.index('static int cinema_ui_named('):ui.index('// Explicit diagnostic seed only;')]
prefix='#include <string.h>\nstatic void phone_inspect_fname(const void*,char*,int);\n'+guard+property_helpers
prefix+='unsigned long fixture_prompt_code[]={'+','.join(a for a,_,_ in checks)+'};\n'
prefix+='unsigned long fixture_prompt_fields[]={'+','.join(hex(a) for a,_,_ in fields)+'};\n'
prefix+=f'unsigned fixture_prompt_count={len(checks)},fixture_prompt_field_count={len(fields)};\nvoid fixture_prompt_image(unsigned char* p){{\n'
for a,x,y in checks:prefix+=f'*(unsigned*)(p+{a})={x};*(unsigned*)(p+{a}+4)={y};\n'
for a,o,_ in fields:prefix+=f'*(unsigned*)(p+{a}+36)={o};\n'
prefix+='}\n'
world=(root/'analysis/re4fix_world_touch.h').read_text()
def world_function(name):
    match=re.search(r'^(?:static )?[^\n]+\b'+re.escape(name)+r'\([^;\n]*\) \{',world,re.M)
    assert match,name
    opening=world.index('{',match.start());at=opening+1;depth=1
    while depth:
        if world[at]=='{':depth+=1
        elif world[at]=='}':depth-=1
        at+=1
    return world[match.start():at]+'\n'
start=world.index('static volatile int g_prompt_tree_pending;')
prompt=world[start:world.index('static int wt_project(',start)]
prompt+=world_function('wt_project')+world_function('wt_component_location')+world_function('touch_world_tick')
world_fixture=(root/'analysis/research/test_world_prompt.inc').read_text().replace('/* WORLD_FUNCTIONS */',prompt)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-prompt-') as temporary:
    d=Path(temporary);source=d/'prompt.c';source.write_text(prefix+(root/'analysis/research/test_prompt_visual.c').read_text(),encoding='utf8')
    (d/'world_prompt_fixture.h').write_text(world_fixture,encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(root/'analysis'),'-fsyntax-only',str(source)],check=True)
    obj,exe=d/'prompt.obj',d/'prompt.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-I',str(root/'analysis'),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    glyph_source=root/'analysis/research/test_prompt_glyph.c'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(root/'analysis'),'-fsyntax-only',str(glyph_source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-I',str(root/'analysis'),
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-isystem',str(sdk/'Include/10.0.26100.0/um'),'-isystem',str(sdk/'Include/10.0.26100.0/shared'),
        '-c','-o',str(obj),str(glyph_source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: actual ARM64 Werror helper and executed mesh-only visibility/restore, metadata/ownership/lifetime/type/thread/array/installer guards',flush=True)
