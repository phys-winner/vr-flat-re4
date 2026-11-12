"""Prove native click/script ABI and execute actual toolbar/Start-confirmation code."""
import os
from pathlib import Path
import hashlib,json,re,struct,subprocess,tempfile
from inspect_flat_bp import read_script,DIRECTORY
import inspect_flat_bp as bp
from inspect_ui_package import package
from check_curved_menu_bounds import original_curved_vertices,capture_header
root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert struct.unpack_from('<II',native,0x63168dc)==(0x394c5000,0xd65f03c0) # Original merchant GetCurrentState: byte314, no writes.
curve_capture=capture_header(original_curved_vertices(native))
assert struct.unpack_from('<4I',native,0x6371bcc)==(0xd101c3ff,0xa90457f6,0xa9054ff4,0xa9067bfd)
for address,word in ((0x6371c54,0xf94006a8),(0x6371c64,0xf90003f3),(0x6371c68,0xad408281),
                     (0x6371c6c,0x3dc00282),(0x6371c84,0x97c5d9cb),(0x698e348,0xb9406a88),(0x698e450,0xf9403289)):
    assert struct.unpack_from('<I',native,address)[0]==word,hex(address)
print('PASS: original Click(component,Object24,ClickState48) first16/actual multicast operands; actual UStruct.Script Data60/Num68 reads',flush=True)
for stem,digests in (
    ('UI_PauseMenu_Content_BP',('54104674e2daa60f82da6481d81cc5494de2a05cbd6190d46bfddb0617d467b9','b8fffe1c0e3852676394262306c2adb3c0d9239230dda14ed5610ffd6468efc8')),
    ('UI_PauseMenu_Content_Trifold_Base_BP',('354ead27426c8cdb99474568855cce146f22d063c68cfcd64ffe04cf7d742ca2','781e4aac1d8d5f708c57a20999237a4a3814747e81f2d7d9a57db1284cb1ae68'))):
    for extension,digest in zip(('.uasset','.uexp'),digests):assert hashlib.sha256((DIRECTORY/(stem+extension)).read_bytes()).hexdigest()==digest
    for action,entry in zip(('A','B'),(139,144) if stem=='UI_PauseMenu_Content_BP' else (4360,4445)):
        script=read_script(stem,'SubAction_'+action,32)
        assert script['runtime_bytes']==18 and script['calls'][0]['arguments']==[{'int':entry}]
base=read_script('UI_PauseMenu_Content_BP','ExecuteUbergraph_UI_PauseMenu_Content_BP',232)
base={node['runtime']:node['expression'] for node in base['tree']}
assert base[139]==base[144]=={'jump':188} and base[188]=={'return':{'literal_token':11}}
trifold=read_script('UI_PauseMenu_Content_Trifold_Base_BP','ExecuteUbergraph_UI_PauseMenu_Content_Trifold_Base_BP')
trifold={node['runtime']:node['expression'] for node in trifold['tree']}
assert trifold[4360]['call']=='IntSubActionEventStart'
assert trifold[4374]['let_bool'][1]['call'].endswith('.PopUIScreen')
assert trifold[4374]['let_bool'][1]['arguments'][0]['field']==['CentralCoverScreen']
assert trifold[4786]['let_bool'][1]['arguments'][0]['field']==['UIScreen']
print('PASS: exact original cooked A/B18-byte wrappers; both base handlers jump to empty return, Trifold A opens cover while B closes actual panels',flush=True)
title=root/'analysis/research/title_ui_originals'
for extension,digest in (('.uasset','cd934ff9d084903059cd66fc14ec3001b61471a12f5fe2e4c6614350e76bafb0'),
                         ('.uexp','8d9145ff2c9cee049cde8d67ac473f83d36513ab2990d39305fa921cb6ec2c44')):
    assert hashlib.sha256((title/('FrontendManager'+extension)).read_bytes()).hexdigest()==digest
assert hashlib.sha256(native[0x88945d8:0x88946ec]).hexdigest()=='3ccf560746dfb1881dff795c9496d05b215fea0038590cd9eb2eacba7289dcfc'
assert hashlib.sha256(native[0x694a528:0x694a554]).hexdigest()=='f11b72bf3e74adb2ea9afbfc9083f04f1f819d8247e1ccec084b3f23972af5a2'
bp.DIRECTORY=title
title_graph=read_script('FrontendManager','ExecuteUbergraph_FrontendManager',10878)
title_graph={node['runtime']:node['expression'] for node in title_graph['tree']}
assert title_graph[11555]=={'jump':5080}
assert title_graph[5080]['pop_execution_if_not']['field']==['allowButtonInput?']
assert title_graph[5101]['jump_if_not']==5425 and title_graph[5101]['condition']['field']==['ReadyForStart']
assert title_graph[5115]['let_bool']==[{'field':['ReadyForStart'],'owner':73},{'literal_token':40}]
assert title_graph[5351]['delegate']['field']==['FrontendStart']
assert title_graph[5090]['let_bool']==[{'field':['SkipLogo'],'owner':73},{'literal_token':39}]
assert title_graph[5425]['jump_if_not']==5440 and title_graph[5425]['condition']['field']==['LogoIntroComplete']
assert title_graph[5571]['call_delegate']=='LogoSkip__DelegateSignature'
title_input=read_script('FrontendManager','InpActEvt_Button A_K2Node_InputActionEvent_9',83)
assert title_input['runtime_bytes']==36 and title_input['tree'][0]['expression']['let_persistent'][1]['field']==['Key']
assert title_input['calls'][0]['arguments']==[{'int':11555}]
bp.DIRECTORY=DIRECTORY
print('PASS: exact authored title A route checks allowButtonInput?/ReadyForStart and consumes once; original World-only GetActorOfClass and FBoolProperty ByteOffset79/Mask7b native ABI',flush=True)
actions=root/'analysis/research/action_ui_originals';packages={};rows=[]
for asset in sorted(actions.glob('*.uasset')):
    body=asset.with_suffix('.uexp');raw=(asset.read_bytes(),body.read_bytes())
    rows.append(asset.stem+':'+hashlib.sha256(raw[0]).hexdigest()+':'+hashlib.sha256(raw[1]).hexdigest())
    packages[asset.stem]=package(*raw)
assert len(rows)==25 and hashlib.sha256('\n'.join(rows).encode()).hexdigest()=='2e0caec60f2dbf3b3c200852d022ef83311590e9eb59d24af6deca1305ea66be'
a_owners=[stem for stem,pkg in packages.items() if any(e['class_path']=='/Script/CoreUObject.Function' and e['name']=='SubAction_A' for e in pkg['exports'])]
assert sorted(a_owners)==['UI_PauseMenu_Content_BP','UI_PauseMenu_Content_Trifold_Base_BP']
for stem in ('UI_PauseMenu_Content_NewGame_BP','UI_PauseMenu_Content_ShootingRange_BP'):
    pkg=packages[stem];cls=next(e for e in pkg['exports'] if e['class_path']=='/Script/Engine.BlueprintGeneratedClass')
    assert next(i['path'] for i in pkg['imports'] if i['index']==cls['super']).endswith('.UI_PauseMenu_Content_Trifold_Base_BP_C')
pkg=packages['ShootingRange_Configuration_Screen_BP'];cls=next(e for e in pkg['exports'] if e['class_path']=='/Script/Engine.BlueprintGeneratedClass')
assert cls['name']=='ShootingRange_Configuration_Screen_BP_C'
start=next(e for e in pkg['exports'] if e['name']=='StartButton_GEN_VARIABLE');body=(actions/'ShootingRange_Configuration_Screen_BP.uexp').read_bytes()
payload=body[start['uexp_offset']:start['uexp_offset']+start['serial_size']]
assert payload[:8]==struct.pack('<II',pkg['names'].index('ChildActorClass'),0)
child=struct.unpack_from('<i',payload,25)[0]
assert next(i['path'] for i in pkg['imports'] if i['index']==child).endswith('.ShootingRange_Panel_Button_BP_C')
bp.DIRECTORY=actions
start_init=read_script('ShootingRange_Configuration_Screen_BP','Initialize',json.loads((actions/'ShootingRange_Configuration_Screen_BP.Initialize.script.json').read_text())['script_offset'])
start_init={n['runtime']:n['expression'] for n in start_init['tree']}
assert start_init[627]['let'][2]['value']['context']['field']==['StartButton']
assert start_init[724]['bind_delegate']=='HandleOnStartClick' and start_init[747]['target']['call']['field']==['OnClick']
range_start=read_script('ShootingRange_Configuration_Screen_BP','HandleOnStartClick')
assert range_start['tree'][0]['expression']['delegate']['field']==['OnStart']
button=read_script('UI_Button_BP','ExecuteUbergraph_UI_Button_BP',json.loads((actions/'UI_Button_BP.ExecuteUbergraph_UI_Button_BP.script.json').read_text())['script_offset'])
button={n['runtime']:n['expression'] for n in button['tree']}
assert button[603]['delegate']['field']==['OnClick']
clicked='BndEvt__VRUIButton_K2Node_ComponentBoundEvent_2_OnUIComponentClicked__DelegateSignature'
assert read_script('UI_Button_BP',clicked,134)['calls'][0]['arguments']==[{'int':494}]
confirm='BndEvt__VR4UIButton_Confirm_K2Node_ComponentBoundEvent_5_OnUIComponentClicked__DelegateSignature'
assert read_script('NewGameConfirmScreen_BP',confirm,134)['calls'][0]['arguments']==[{'int':437}]
bp.DIRECTORY=DIRECTORY
assert 'mobile_prefs_start_confirm' not in (root/'analysis/re4fix_input.h').read_text()
assert 'mobile_android_confirm_request(6' not in (root/'analysis/re4fix_input.h').read_text()
print('PASS:25 immutable original packages: all pause-content/NewGame/Range A routes are empty or cover-only; both actual game starts use native Click/OnClicked, exact Range class and StartButton child actor binding; toolbar A remains immediate',flush=True)
death=root/'analysis/research/death_ui_originals'
rows=[p.name+':'+hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(death.iterdir()) if p.suffix in ('.uasset','.uexp')]
assert len(rows)==12 and hashlib.sha256('\n'.join(rows).encode()).hexdigest()=='8eb973b69a1bae9213c9c3326778b70d44ca9a87e3debcda1a8597a6ff073bf5'
for stem in ('DeathMenu_BP','Button_Death','CinemaScreen_A'):
    pkg=package((death/(stem+'.uasset')).read_bytes(),(death/(stem+'.uexp')).read_bytes())
    assert pkg==json.loads((death/(stem+'.package.json')).read_text())
bp.DIRECTORY=death
death_graph=read_script('DeathMenu_BP','ExecuteUbergraph_DeathMenu_BP',json.loads((death/'DeathMenu_BP.ExecuteUbergraph_DeathMenu_BP.script.json').read_text())['script_offset'])
parents=[c for c in death_graph['calls'] if c['call'].endswith('.SetObjectPropertyByName') and c['arguments'][1]=={'name':'ParentMenu'}]
assert len(parents)==2 and all(c['arguments'][2]=={'literal_token':23} for c in parents)
buttons=[c for c in death_graph['calls'] if c['call'].endswith('.Array_Add') and c['arguments'][0]['field']==['ButtonList']]
assert len(buttons)==2 and not any('Attach' in c['call'] for c in death_graph['calls'])
props=json.loads((death/'DeathMenu_BP.component_properties.json').read_text())
assert next(p['value'] for p in props if p['export']=='Screen_GEN_VARIABLE' and p['field']=='StaticMesh').endswith('/CinemaScreen_A.CinemaScreen_A')
assert next(p['value'] for p in props if p['export']=='Screen_GEN_VARIABLE' and p['field']=='RelativeScale3D')==[2.174999952316284,2.0,1.75]
button_props=json.loads((death/'Button_Death.component_properties.json').read_text())
assert next(p['value'] for p in button_props if p['field']=='Width')==120
assert next(p['value'] for p in button_props if p['field']=='Height')==30
bp.DIRECTORY=DIRECTORY
root_source=(root/'analysis/re4fix.c').read_text()
match=re.search(r'^static void head_angles_now\(float\* yaw, float\* pitch\) \{',root_source,re.M);assert match
end=root_source.index('\n}',match.end())+2
head_source=root_source[match.start():end]
pointer_start=root_source.index('    } else if (touch_menu_pointer_allowed((int)node)) {')+len('    } else if (touch_menu_pointer_allowed((int)node)) {')
pointer_end=root_source.index('    } else if ((node == 3 || node == 4)',pointer_start)
pointer_source='static void native_pointer(float direction[3],float position[3]) { qf_t q={0,0,0,1};float pos[3]={0};\n'+root_source[pointer_start:pointer_end]+'\nfloat forward[3]={0,0,-1};q_rot(q,forward,direction);memcpy(position,pos,sizeof pos);}\n'
world_source=(root/'analysis/re4fix_world_touch.h').read_text();projection_start=world_source.index('static int wt_project(')
pointer_source+=world_source[projection_start:world_source.index('\n}',projection_start)+2]+'\n'
# Preserve the exact live Merc scene topology, including both decorative capture/lighting actors.
graph_path=Path(os.environ['RE4_QA_CAPTURE_DIR'])/'p431_merc_live_graph.json'
assert hashlib.sha256(graph_path.read_bytes()).hexdigest()=='f667dc327d1a836a9f5aed142ec5cbe26b25e2f6ae439f2b0432169068468ac4'
graph=json.loads(graph_path.read_text());nodes=graph['nodes'];assert graph['read_only'] and graph['node_count']==len(nodes)==304
assert len({n['object'] for n in nodes})==304 and sum(len(n['children']) for n in nodes)==303
owners={'0x72349ce020':8,'0x72569e4a00':13,'0x72569e9000':16,'0x7258e83620':26,'0x72578840c0':27}
assert {o:sum(n['owner']==o for n in nodes) for o in owners}==dict(zip(owners,[32,4,4,132,132]))
indices=dict(zip(('0x721e377600','0x721cb99b10','0x723b94b400','0x7257a86dc0','0x72a47c7e00','0x72569eac00','0x721cb9e570',
    '0x7257a87b40','0x721e370a00','0x72569ecf00','0x721cb9dad0','0x72569e2000'),(9,10,20,23,14,15,21,25,17,18,24,6)))
next_index=30
for node in nodes:
    if node['object'] not in indices:indices[node['object']]=next_index;next_index+=1
edges=[];merc_rows=[]
for node in nodes:
    kind=1|({'0x72c6ce1a00':2,'0x72c6cfe700':4,'0x72c6cfdf80':8}.get(node['class'],0))
    first=len(edges);edges.extend(indices[c] for c in node['children'])
    assert node['class_size']>=0x1f0 and len(node['tf'])==12
    floats=','.join(format(x,'.9e')+'f' for x in node['tf'])
    outer=owners.get(node['outer'],indices.get(node['outer']));assert outer is not None
    merc_rows.append('{'+','.join(map(str,(indices[node['object']],owners[node['owner']],outer,indices.get(node['parent'],0),first,len(node['children']),node['flags14c'],kind)))+',{'+floats+'}}')
merc_capture='static const unsigned merc_edges[]={'+','.join(map(str,edges))+'};\n'
merc_capture+='static const struct{unsigned index,owner,outer,parent,first,count,flags,kind;float tf[12];}merc_nodes[]={\n'+',\n'.join(merc_rows)+'\n};\n'
merc_capture+='static const struct{unsigned actor,root;}merc_owners[]={'+','.join('{'+str(owners[o])+','+str(indices[next(n['object'] for n in nodes if n['owner']==o and (n['parent']=='0x0' or next(p['owner'] for p in nodes if p['object']==n['parent'])!=o))])+'}' for o in owners)+'};\n'
controls=('0x72569eac00','0x72569ecf00','0x72569e6600','0x72569e2000','0x72569e2380','0x72569e9700','0x736935c940')
merc_capture+='static const unsigned merc_controls[]={'+','.join(str(indices[c]) for c in controls)+'};\n'
raw=bytes.fromhex(next(n['widget_438_520'] for n in nodes if n['object']=='0x721cb99b10'))
assert struct.unpack_from('<2i',raw,0)==struct.unpack_from('<2i',raw,0x18)==(1920,1080) and raw[0x20]==0
merc_capture+='static const unsigned char merc_main_widget[]={'+','.join(map(str,raw))+'};\n'
print('PASS: immutable live Merc304-node/303-edge topology, exact32+4+4+132+132 ownership, original native widget bytes and registered-control order captured for actual C regression',flush=True)
print('PASS:12 immutable original DeathMenu/button/widget/surface members; native BP spawns2 independent ParentMenu-owned buttons without attachments and original cinematic screen bounds source; real current head_angles_now is compiled into camera regression',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
for fixture in ('test_ui_actions_start.c','test_ui_console.c'):
  source=root/'analysis/research'/fixture
  with tempfile.TemporaryDirectory(prefix='re4-actions-start-') as td:
    folder=Path(td);obj=folder/'test.obj';exe=folder/'test.exe'
    head_header=folder/'head.h';head_header.write_text(head_source)
    pointer_header=folder/'pointer.h';pointer_header.write_text(pointer_source)
    curve_header=folder/'curves.h';curve_header.write_text(curve_capture)
    merc_header=folder/'merc.h';merc_header.write_text(merc_capture)
    widget_header=folder/'widget.h'
    flat=(root/'analysis/re4fix_flat_view.h').read_text()
    assert '{0x9f77670, 0x51c}' in flat
    widget_header.write_text(flat[flat.index('static int fv_descends_from('):flat.index('static unsigned fv_screen_planes(')]
        +flat[flat.index('static struct cinema_ui_vec2 (*fv_current_draw_size)'):flat.index('#include "re4fix_ui_footer.h"')])
    includes=['-DUI_HEAD_FRAGMENT="'+head_header.as_posix()+'"','-DUI_POINTER_FRAGMENT="'+pointer_header.as_posix()+'"','-DUI_WIDGET_FRAGMENT="'+widget_header.as_posix()+'"','-DUI_CURVE_CAPTURE="'+curve_header.as_posix()+'"','-DUI_MERC_CAPTURE="'+merc_header.as_posix()+'"']
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror',*includes,'-fsyntax-only',str(source)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        *includes,'-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO','/STACK:4194304',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib',
        'libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',
        f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
