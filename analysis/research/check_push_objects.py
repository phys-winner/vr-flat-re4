"""Host-execute the actual Push6 publisher and the pinned original ARM64 push flow.

The ARM64 fixture stubs scene/animation providers. State selection, held input tests,
range clamps, stopTarget and pushTarget collision-result handling are original code.
It does not claim to simulate an entire room or authored animation/collision data.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'analysis'),str(ROOT/'analysis/research')]
from inspect_ui_package import package
from capstone import Cs,CS_ARCH_ARM64,CS_MODE_ARM
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import *

native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
ranges=(
 (0x5fad7e0,0x5fadb98,'658ca97c4e2674fcb00ef8e679bbc1e52c74dcaa84589af968ba6ba7b86934bd'),
 (0x5fb8444,0x5fb845c,'bfdb54e1e434ba9f1a31cf47ab9f00184a291def1507e8ebfa46c9de9c499ce6'),
 (0x5fd707c,0x5fd7324,'e614de40b963e11efae403870a016f2ea7a8911e7c77c5ca9a14958941b9edea'),
 (0x5fd7324,0x5fd7380,'be83810ba702172993f94dd1dd0b834415bf9a1b5b8d8ccf83aa0c2051b7440e'),
 (0x5fd7380,0x5fd7568,'7820595ee56a51a0887e96d5397b2fd220ff99c1afd5c9a7814248e4c2c8a46f'),
 (0x5ea9dac,0x5ea9ed0,'da1fa418ed1341bbe207e2edf996ec6dcfef50ffb9d258c74194e699ffb781ef'),
 (0x62a52f4,0x62a5a14,'abbdfd26f08a4c5fc1acb2c6e9a0c44f32b19115ae40efb4bffc254ea165e446'),
)
for start,end,digest in ranges:assert hashlib.sha256(native[start:end]).hexdigest()==digest,hex(start)
source=(ROOT/'analysis/re4fix_push_objects.h').read_text()
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source)
assert len(checks)==13
for address,a,b in checks:assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16))
md=Cs(CS_ARCH_ARM64,CS_MODE_ARM)
def instruction(address,mnemonic,operand):
    item=next(md.disasm(native[address:address+4],address));assert (item.mnemonic,item.op_str)==(mnemonic,operand),(hex(address),item)
for address,target in ((0x5fad974,0x5fd7324),(0x5fada18,0x5fd7928),(0x5fada70,0x5fd7380),
 (0x5fd741c,0x5f58370),(0x5fd7488,0x5ea9dac),(0x5fd74a4,0x5d48a18),(0x5fd74d8,0x5fd7568),(0x5fd74e4,0x5fd779c)):
    instruction(address,'bl',f'#{hex(target)}')
instruction(0x5fb7a18,'bl','#0x5fd707c')
instruction(0x5fb7a5c,'adrp','x3, #0x5fb8000');instruction(0x5fb7a64,'add','x3, x3, #0x444')
instruction(0x5fb7a68,'mov','w1, #6');instruction(0x5fb7a8c,'bl','#0x5d37e14')
assert struct.unpack_from('<II',native,0x42fb3e8)==(0,0x111c) # cEmMgr constructor's exact slot geometry.
instruction(0x5ea9084,'mov','w20, w5');instruction(0x5ea9114,'strb','w20, [x19, #0x61e]')
for address in (0x5ef2914,0x5ef2a9c,0x5ef7214):instruction(address,'ldrb','w5, [x20, #2]')
instruction(0x5ea9134,'str','w8, [x19, #0xac]');instruction(0x5ea9138,'stur','x9, [x19, #0xa4]')

clang=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
with tempfile.TemporaryDirectory(prefix='re4-push-') as temporary:
    folder=Path(temporary);obj=folder/'push.obj';exe=folder/'push.exe';test=ROOT/'analysis/research/test_push_objects.c'
    for options in ([],['-DRE4_QA_USER_TRANSITIONS']):
        subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror',*options,'-fsyntax-only',str(test)],check=True)
        subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',*options,
            '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c',str(test),'-o',str(obj)],check=True)
        subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
            'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
            f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
        subprocess.run([str(exe)],check=True)

fixture=ROOT/'analysis/research/push_objects_originals'
receipt=json.loads((fixture/'receipt.json').read_text())
for row in receipt:
    filename=row.get('output',row['source'].rsplit('/',1)[-1]);data=(fixture/filename).read_bytes()
    assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256'],filename
    assert row['decoder_repeats']==2 and len(row['compressed_sha1'])==40
    if 'header_sha256' in row:assert hashlib.sha256((fixture/'r101.das.header').read_bytes()).hexdigest()==row['header_sha256']
def original(stem):
    body=(fixture/(stem+'.uexp')).read_bytes();metadata=package((fixture/(stem+'.uasset')).read_bytes(),body)
    assert metadata==json.loads((fixture/(stem+'.package.json')).read_text())
    return metadata,body
mapping,body=original('Bio4KeyMapping_DT');names=mapping['names']
def fname(data,at,ns):
    index,number=struct.unpack_from('<II',data,at);assert number==0 and index<len(ns);return ns[index]
def properties(data,at,ns):
    result={}
    while True:
        name=fname(data,at,ns);at+=8
        if name=='None':return result,at
        kind=fname(data,at,ns);size,index=struct.unpack_from('<II',data,at+8);assert index==0;at+=16
        structure=None
        if kind=='StructProperty':structure=fname(data,at,ns);assert data[at+8:at+24]==bytes(16);at+=24
        assert data[at]==0;at+=1
        result[name]=(kind,structure,data[at:at+size]);at+=size
fields,at=properties(body,0,names);assert fields['RowStruct'][0]=='ObjectProperty'
assert struct.unpack_from('<I',body,at)[0]==0;at+=4
count=struct.unpack_from('<I',body,at)[0];at+=4;assert count==13
rows={}
for _ in range(count):
    name=fname(body,at,names);fields,at=properties(body,at+8,names);rows[name]=fields
assert at==mapping['exports'][0]['serial_size']
ck=rows['KEY_CK'];assert ck['Bio4Key'][0]=='Int64Property' and struct.unpack('<Q',ck['Bio4Key'][2])[0]==0x400
for field,key in (('OculusKeyLeftHanded','OculusTouch_Left_X_Click'),('OculusKeyRightHanded','OculusTouch_Right_A_Click')):
    kind,structure,payload=ck[field];assert (kind,structure)==('StructProperty','Key')
    entry,end=properties(payload,0,names);assert end==len(payload) and fname(entry['KeyName'][2],0,names)==key
models,body=original('EtcModelData_DA');names=models['names'];assert models['exports'][0]['class_path']=='/Script/VR4.VR4ModelData'
assert fname(body,0,names)=='ModelClassList' and fname(body,8,names)=='ArrayProperty' and fname(body,24,names)=='SoftObjectProperty'
assert struct.unpack_from('<II',body,16)==(1348,0) and body[32]==0 and struct.unpack_from('<I',body,33)[0]==112
classes=[]
for index in range(112):
    at=37+12*index;classes.append(fname(body,at,names));assert struct.unpack_from('<I',body,at+8)[0]==0
for index,stem in ((4,'et0400_dresser_BP'),(5,'et0500_Cabinet_BP'),(0x30,'et3000_knightStatue_BP')):
    assert classes[index].endswith('/'+stem+'.'+stem+'_C');original(stem)
for start,arg,call,class_call in ((0x5ef2924,0,0x5ef2928,0x5ef2938),(0x5ef2aa8,1,0x5ef2ab0,0x5ef2ac0),(0x5ef7220,3,0x5ef7228,0x5ef7238)):
    instruction(start,'mov',f'w4, #{arg}' if arg else 'w4, wzr')
    instruction(call,'bl','#0x5ea9038');instruction(class_call,'bl','#0x62462e4')
instruction(0x5ea90d8,'mov','w2, #0x55');instruction(0x5ea91d0,'strb','w21, [x19, #0x119]')
def ets_records(data):
    count=struct.unpack_from('<H',data)[0];assert 16+count*40<=len(data)
    return [(struct.unpack_from('<H',data,16+40*i)[0],data[16+40*i+2],struct.unpack_from('<3f',data,16+40*i+28)) for i in range(count)]
r101=ets_records((fixture/'r101.das.ets').read_bytes())
assert len(r101)==28 and [(kind,ident) for kind,ident,_ in r101 if kind in (4,5)]==[(5,13),(4,15),(4,17)]
for address,ident,call,set_range in ((0x6002c84,13,0x6002c8c,0x6002cb0),(0x6002cb8,15,0x6002cc0,0x6002ce8),(0x6002d20,17,0x6002d28,0x6002d6c)):
    instruction(address,'mov',f'w0, #{hex(ident)}');instruction(call,'bl','#0x5efccec');instruction(set_range,'bl','#0x5ea9d0c')
das=(fixture/'r212.das').read_bytes();assert hashlib.sha256(das).hexdigest()=='81ecff89b6a62a1308985eca6c1081df57398f109c808e692942b1f45e1b0531'
archive=struct.unpack_from('<I',das,0x2c)[0];assert archive==0x400
count=struct.unpack_from('<I',das,archive)[0];offsets=struct.unpack_from('<'+'I'*count,das,archive+16);assert count==37
ets_at=archive+offsets[11];ets_end=archive+offsets[12];assert (ets_at,ets_end-ets_at)==(0x8931e0,0x200)
r212=ets_records(das[ets_at:ets_end]);assert len(r212)==12 and [(kind,ident) for kind,ident,_ in r212 if kind==0x30]==[(0x30,5),(0x30,6)]
for address,ident,call,set_range in ((0x606a3c8,5,0x606a3d0,0x606a410),(0x606a418,6,0x606a424,0x606a464)):
    instruction(address,'mov',f'w0, #{ident}');instruction(call,'bl','#0x5efccec');instruction(set_range,'bl','#0x5ea9d0c')
print('PASS: original KEY_CK held mask0x400/A-or-X; original model-class bindings and room ETS/native range callers prove R101 furniture and R212 puzzle statues share type0x55/cPlPush',flush=True)

# Reuse the established original-ELF PT_LOAD/RELA loader, stopping before its decoder work.
loader=ROOT/'analysis/research/host_oodle_decode.py';environment={'__file__':str(loader),'__name__':'push_original_loader'}
exec(compile(loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0],str(loader),'exec'),environment)
u,base,buffers,ret,call=(environment[name] for name in ('u','base','buffers','ret','call'))
def read(at,size):return bytes(u.mem_read(at,size))
def qword(at):return struct.unpack('<Q',read(at,8))[0]
def write(at,fmt,*values):u.mem_write(at,struct.pack(fmt,*values))
assert qword(base+0xa361d78)==base+0xa4e9b40 and qword(base+0xa361d90)==base+0xa4e9b68 and qword(base+0xa361d98)==base+0xa501aa0
assert qword(base+0xa3f9c98+9*8)==base+0x5fad7e0
enum=base+0x9d4ecf0+6*16;name=b'EActionButtonType::Push\0';assert qword(enum+8)==6 and read(qword(enum),len(name))==name
player,push,target,pg,motion,aux,parts,vtable,row=(buffers+i for i in (0x10000,0x12000,0x14000,0x18000,0x19000,0x1a000,0x1b000,0x1c000,0x1d000))
current=None
stub_addresses={0x5d54590,0x5d545d0,0x5f575fc,0x5f556cc,0x5fd7eb4,0x5f53c40,0x616c50c,0x5f42860,
    0x5f40a3c,0x5f414e4,0x5f4145c,0x5d3510c,0x5d48a18,0x5fd7568,0x5fd779c,0x621984c,0x5f58370,0x9bf2ec0}
def scene_provider(uc,address,size,user):
    global current
    if address==ret+0x100:return
    offset=address-base
    if offset not in stub_addresses:return
    x0,x1=(uc.reg_read(UC_ARM64_REG_X0+i) for i in range(2));result=0
    current['calls'][offset]=current['calls'].get(offset,0)+1
    if offset==0x5f575fc:
        write(x1,'<Q',motion)
        if x0==player and uc.reg_read(UC_ARM64_REG_W4)==6:current['lean_started']=current['now']
    elif offset==0x5f556cc:
        state=read(player+0x116,1)[0]
        result=int(state==1 and current['now']-current['lean_started']>=320)
    elif offset==0x5fd7eb4:result=1 # Valid native initial alignment is an explicit fixture input.
    elif offset==0x5f53c40:result=parts
    elif offset==0x5f58370:
        assert x0==target and x1==0
        position=list(struct.unpack('<3f',read(target+0xa4,12)));side=current['side']
        position[0 if side in (1,3) else 2]+=1 if side in (1,2) else -1
        write(target+0xa4,'<3f',*position);current['steps']+=1
    elif offset==0x5fd7568:result=int(current['collision']=='scene')
    elif offset==0x5fd779c:result=int(current['collision']=='object')
    elif offset==0x9bf2ec0:raise AssertionError('native stack canary failed')
    # LIMIT_ANGLE gets small fixture yaw values; returning its incoming s0 is exact here.
    uc.reg_write(UC_ARM64_REG_X0,result);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_LR))
hook=u.hook_add(UC_HOOK_CODE,scene_provider)
u.mem_write(ret+0x100,bytes.fromhex('c0035fd6'))
def setup(subtype,side,collision=''):
    global current
    u.mem_write(buffers+0x10000,bytes(0x10000))
    write(base+0xa4e9b40,'<Q',player);write(base+0xa3f5e48,'<Q',pg);write(pg+0x68,'<Q',motion)
    write(player+0x9b8,'<Q',push);write(player+0x990,'<Q',aux);write(player+0x580,'<Q',aux)
    write(push,'<QQ',player,target);write(push+0x14,'<I',side)
    write(target,'<Q',vtable);write(vtable+0x30,'<Q',ret+0x100)
    write(target+8,'<I',1);write(target+0x118,'<BB',0x55,subtype);write(target+0x3e8,'<h',1)
    identity=(1,0,0,0,0,1,0,0,0,0,1,0)
    for offset in (0x109c,0x10cc):write(target+offset,'<12f',*identity)
    write(target+0x10fc,'<4f',3,3,3,3);write(target+0x110c,'<B',0x10)
    current={'now':0,'lean_started':0,'side':side,'calls':{},'steps':0,'collision':collision,'states':[]}
    call(0x5fb8444,[]);assert read(player+0x114,4)==b'\x00\x09\x00\x00'
def frame(now,held):
    current['now']=now;write(base+0xa501aa0+0x18,'<Q',0x400 if held else 0)
    if read(player+0x115,1)==b'\x09':call(0x5fad7e0,[player])
    state=read(player+0x116,1)[0];current['states'].append(state);return state
def run(pulse,subtype,side,collision='',release_after=None):
    setup(subtype,side,collision)
    for now in range(0,1200,16):
        held=now<(180 if pulse else 1000) and read(player+0x116,1)!=b'\x16'
        if release_after is not None and current['steps']>=release_after:held=False
        frame(now,held)
        if read(player+0x115,1)!=b'\x09':break
    assert read(player+0x115,1)==b'\0' and qword(target+0x228)==0
    return current.copy(),struct.unpack('<3f',read(target+0xa4,12))
for subtype in (0,1,3):
    for side in range(4):
        failed,position=run(True,subtype,side);assert failed['steps']==0 and 50 in failed['states'] and 20 not in failed['states'] and position==(0,0,0)
        fixed,position=run(False,subtype,side);axis=0 if side in (1,3) else 2;expected=3 if side in (1,2) else -3
        assert fixed['steps']==4 and position[axis]==expected and {1,20,21,22,40}<=set(fixed['states'])
        assert fixed['calls'][0x5fd7568]==fixed['calls'][0x5fd779c]==4
        released,position=run(False,subtype,side,release_after=1);assert released['steps']==1 and 40 in released['states'] and 22 not in released['states']
    for collision in ('scene','object'):
        stopped,_=run(False,subtype,0,collision);assert stopped['steps']==1 and 22 in stopped['states']
        assert stopped['calls'][0x5fd7568]==stopped['calls'][0x5fd779c]==1
# Execute the original held-IsPressed result -> row mask bridge, for both input levels.
write(row+0x38,'<Q',0x400)
for held in (False,True):
    write(base+0xa501aa0+0x18,'<Q',0x80);u.reg_write(UC_ARM64_REG_X24,row);u.reg_write(UC_ARM64_REG_W22,int(held))
    u.emu_start(base+0x62a54c0,base+0x62a54e0,count=30)
    assert qword(base+0xa501aa0+0x18)==(0x480 if held else 0x80)
u.hook_del(hook)
print('PASS: original ARM64 Push callback/state machine, native range and input bridge: 180ms pulse aborts lean; held input moves furniture/statue subtypes on all4 sides; release/scene/object stop retain native termination',flush=True)
print('LIMIT: authored MotionMove/animation timing, scene collision providers and initial alignment are controlled inputs; real room presentation and device behavior require later targeted verification.',flush=True)
