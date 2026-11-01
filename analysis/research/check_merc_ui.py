"""Prove original Merc ending dependencies and execute the actual guarded visibility repair."""
from pathlib import Path
import hashlib,json,os,re,struct,subprocess,sys,tempfile

ROOT=Path(__file__).resolve().parents[2]
header=ROOT/'analysis/re4fix_merc_ui.h'
code=header.read_text()
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
checks=[]
for row in re.finditer(r'\{(0x[0-9a-f]+),\{([^}]+)\}\}',code):
    address=int(row[1],16);words=tuple(int(x,0) for x in row[2].split(','));length=4 if words[2] else 2
    assert struct.unpack_from('<'+'I'*length,native,address)==words[:length],hex(address)
    checks.append((address,words,length))
assert len(checks)==9
assert struct.unpack_from('<I',native,0x9f77968+0x24)[0]==0x498
playing_name=b'EMovieScenePlayerStatus::Playing'
assert native[0x4287bfb:0x4287bfb+len(playing_name)]==playing_name
assert struct.unpack_from('<q',native,0x9ee97a0+8)[0]==1
assert struct.unpack_from('<I',native,0x7dd501c)[0]==0x7100041f # Original IsAnimationPlaying compares native status to Playing=1.
assert struct.unpack_from('<II',native,0x7cfce78)==(0xb9475800,0xd65f03c0)
assert 'pthread_mutex_' not in code and code.index('if(!cinema_on_game_thread())return;')<code.index('phone_type()!=1')
assert code.count('mv_visible(component,1,0)')<=1
print('PASS: immutable native engine, nine installer preimages, original widget+498 getter, Playing enum1 and actual IsAnimationPlaying status comparison',flush=True)

sdk=Path(os.environ['ANDROID_SDK_ROOT'])
clang=sdk/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
win=Path('C:/Program Files (x86)/Windows Kits/10')
source=ROOT/'analysis/research/test_merc_ui.c'
android=[str(clang),'--target=aarch64-linux-android23','-std=c11','-O2','-Werror']
for diagnostic in (False,True):
    subprocess.run(android+(['-DRE4_QA_USER_TRANSITIONS=1'] if diagnostic else [])+['-fsyntax-only',str(source)],check=True)
    preprocessor='#define LOGF(...) emit(__VA_ARGS__)\n#include "'+header.as_posix()+'"\n'
    expanded=subprocess.run(android+(['-DRE4_QA_USER_TRANSITIONS=1'] if diagnostic else [])+['-E','-P','-x','c','-'],input=preprocessor,text=True,capture_output=True,check=True).stdout
    assert ('MERC UI original ending visibility' in expanded)==diagnostic
print('PASS: actual normal/diagnostic aarch64 syntax and diagnostic-only bounded visibility logging',flush=True)
with tempfile.TemporaryDirectory(prefix='re4-merc-ui-') as temporary:
    d=Path(temporary);fixture=d/'native_fixture.h'
    fixture.write_text('''static void native_merc_bindings(void){
    cu_get_widget=(void*(*)(const void*))(image+0x7db1a28);
    mv_visible=(void(*)(void*,int,int))(image+0x83ad964);
    fv_title_actor=(void*(*)(const void*,void*))(image+0x88945d8);
}
static void verify_merc_ui_installer(void){
    ready();
'''+''.join('    {const unsigned w[]={'+','.join(hex(x) for x in words[:length])+'};memcpy(image+0x%x,w,sizeof w);}\n'%address for address,words,length in checks)+'''
    *(unsigned*)(image+0x9f77968+0x24)=0x498;
    native_merc_bindings();install_touch_merc_ui((uintptr_t)image);assert(merc_ui_base==(uintptr_t)image);
    assert(merc_ui_playing==(int(*)(const void*,const void*))(image+0x7dd4fbc));
'''+''.join('    image[0x%x]^=1;install_touch_merc_ui((uintptr_t)image);assert(!merc_ui_base);image[0x%x]^=1;\n'%(address,address) for address,_,_ in checks)+'''
    *(unsigned*)(image+0x9f77968+0x24)=0x440;install_touch_merc_ui((uintptr_t)image);assert(!merc_ui_base);
    *(unsigned*)(image+0x9f77968+0x24)=0x498;
    mv_visible=visible;install_touch_merc_ui((uintptr_t)image);assert(!merc_ui_base);
    native_merc_bindings();cu_get_widget=get_widget;install_touch_merc_ui((uintptr_t)image);assert(!merc_ui_base);
    native_merc_bindings();phone_inspect_ready=0;install_touch_merc_ui((uintptr_t)image);assert(!merc_ui_base);phone_inspect_ready=1;
    install_touch_merc_ui(0);assert(!merc_ui_base);
    native_merc_bindings();install_touch_merc_ui((uintptr_t)image);assert(merc_ui_base==(uintptr_t)image);
    puts("PASS: actual installer exact dependencies, all9 opcode corruptions, widget offset/binding/base/readiness refusals");
}
''')
    obj,exe=d/'merc_ui.obj',d/'merc_ui.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-std=c11','-O2','-Werror',
        '-DMERC_UI_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-isystem',str(vc/'include'),
        '-isystem',str(win/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO','/OUT:'+str(exe),str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        '/LIBPATH:'+str(vc/'lib/x64'),'/LIBPATH:'+str(win/'Lib/10.0.26100.0/ucrt/x64'),
        '/LIBPATH:'+str(win/'Lib/10.0.26100.0/um/x64')],check=True)
    subprocess.run([str(exe)],check=True)

# Actual live capture, interpreted through the immutable getter's exact layout.
capture=Path(os.environ.get('RE4_QA_CAPTURE_DIR',ROOT/'analysis/build/qa412_motion_font_push_watch'))/'p433_merc_end_players_twice.json'
if capture.exists():
    samples=json.loads(capture.read_text());assert len(samples)==2 and samples[1]['time']-samples[0]['time']>3.5
    for first,second in zip(samples[0]['players'],samples[1]['players']):
        assert first['address']==second['address'] and first['bytes']==second['bytes']
        data=bytes.fromhex(first['bytes']);assert len(data)==0x7a8
        animation=struct.unpack_from('<Q',data,0x408)[0];status=struct.unpack_from('<I',data,0x758)[0]
        numerator,denominator=struct.unpack_from('<ii',data,0x738);frame,subframe=struct.unpack_from('<if',data,0x744)
        assert status==1 and hex(animation)==first['animation']
        assert (frame+subframe)*denominator/numerator==first['seconds']
    assert samples[0]['players'][1]['animation']=='0x726bf02e00'
    print('PASS: captured original live EndMessage and Intro are Playing=1 and byte-identical across3.557s; exact native time layout confirms frozen animations',flush=True)
print('PASS: Merc UI guarded actual C and original native dependencies',flush=True)

# Run immutable ARM64 APIs, with the original C++ vtables and data fixtures, not game-function stubs.
# A fresh non-isolated child can use the installed Unicorn package even when this checker uses -I.
native_proof=r'''
from pathlib import Path
import json,os,struct,sys
root=Path(sys.argv[1]);loader=root/'analysis/research/host_oodle_decode.py'
sys.argv=[str(loader)]
env={'__file__':str(loader),'__name__':'merc_original_api_proof'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),env)
u,base,call,buf=env['u'],env['base'],env['call'],env['buffers']
from unicorn.arm64_const import UC_ARM64_REG_S0
component,widget,players,array=buf+0x10000,buf+0x20000,[buf+0x30000,buf+0x31000],buf+0x40000
comp=bytearray(0x550);struct.pack_into('<Q',comp,0,base+0x9f78020+0x10)
struct.pack_into('<H',comp,0x14c,0x5043);comp[0x496]=1;struct.pack_into('<Q',comp,0x498,widget)
user=bytearray(0x281);struct.pack_into('<QII',user,0x178,array,2,2)
animations=[0x726bf09e00,0x726bf02e00]
data=[]
capture=Path(os.environ.get('RE4_QA_CAPTURE_DIR',root/'analysis/build/qa412_motion_font_push_watch'))/'p433_merc_end_players_twice.json'
observed=json.loads(capture.read_text())[0]['players'] if capture.exists() else None
for i in range(2):
    player=bytearray.fromhex(observed[i]['bytes']) if observed else bytearray(0x7a8)
    struct.pack_into('<Q',player,0,base+0x9f2fe48+0x10);struct.pack_into('<Q',player,0x20,widget)
    struct.pack_into('<Q',player,0x408,animations[i]);struct.pack_into('<I',player,0x758,1)
    if not observed:struct.pack_into('<iiif',player,0x738,60000,1,51742,.1922743022441864)
    u.mem_write(players[i],bytes(player));data.append(bytes(player))
u.mem_write(component,bytes(comp));u.mem_write(widget,bytes(user));u.mem_write(array,struct.pack('<2Q',*players))
before=[bytes(u.mem_read(widget,len(user)))]+[bytes(u.mem_read(p,len(d))) for p,d in zip(players,data)]
for i in range(2):
    assert call(0x7cfce78,[players[i]])==1
    assert call(0x7dd4fbc,[widget,animations[i]])==1
    call(0x7dd4f34,[widget,animations[i]])
    value=struct.unpack('<f',struct.pack('<I',u.reg_read(UC_ARM64_REG_S0)))[0]
    n,d=struct.unpack_from('<ii',data[i],0x738);frame,sub=struct.unpack_from('<if',data[i],0x744)
    assert abs(value-(frame+sub)*d/n)<.000001
assert call(0x7dd4fbc,[widget,0])==0 and call(0x7dd4fbc,[widget,0x12345678])==0
for status in (0,2,3,4,5):
    u.mem_write(players[1]+0x758,struct.pack('<I',status));assert call(0x7dd4fbc,[widget,animations[1]])==0
u.mem_write(players[1]+0x758,struct.pack('<I',1))
after=[bytes(u.mem_read(widget,len(user)))]+[bytes(u.mem_read(p,len(d))) for p,d in zip(players,data)]
assert before==after
assert call(0x7db1a28,[component])==widget
assert call(0x7daf2b4,[component])==0 # Explicitly invisible, despite native TickWhenOffscreen=1.
call(0x83ad964,[component,1,0])
changed=bytes(u.mem_read(component,len(comp)))
assert [(i,a,b) for i,(a,b) in enumerate(zip(comp,changed)) if a!=b]==[(0x14c,0x43,0x63)]
assert call(0x83ad924,[component])==1 and call(0x7daf2b4,[component])==1
call(0x83ad964,[component,1,0]);assert bytes(u.mem_read(component,len(comp)))==changed
assert before==[bytes(u.mem_read(widget,len(user)))]+[bytes(u.mem_read(p,len(d))) for p,d in zip(players,data)]
print('PASS: full original ARM64 SetVisibility(true,false) changes only visible bit14c and is idempotent; original ShouldDrawWidget0->1 despite unchanged offscreen1; original Widget getter, Playing/status/time/absent-animation getters with real vtables; widget/players unchanged, no substituted native game calls',flush=True)
'''
subprocess.run([sys.executable,'-B','-c',native_proof,str(ROOT)],check=True)
