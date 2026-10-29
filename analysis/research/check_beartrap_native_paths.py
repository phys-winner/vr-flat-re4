"""Prove original dog-trap two-hand gates and the flat pair's movement input."""
from pathlib import Path
import hashlib
import math
import struct
import sys
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_W1,UC_ARM64_REG_W2,UC_ARM64_REG_LR

ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'analysis'),str(ROOT/'analysis/research')]
from inspect_ui_package import package
from inspect_flat_bp import Script
from pak9 import Pak9

native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for lo,hi,digest in (
    (0x6210e80,0x6210ea8,'8e9f9b35e46bb403547a5701ebbf27ae3d00973127d10b9b79c9eaaba69dc7fd'),
    (0x6210fac,0x6210ff4,'4346c383a676ea9689e744c64cf218207f1dc53b2514bb40d3b27e7fd6036648'),
    (0x62109e8,0x6210c1c,'0f87baadc3e520b90acd97b340d258d446f95ad9082ee1bf419cb52e5606a8d1'),
):assert hashlib.sha256(native[lo:hi]).hexdigest()==digest
fixture=ROOT/'analysis/research/interaction_originals'
expected={
    'em2a_trap_BP.uasset':('bf2011863fb17f39e7755731bc92b576cdf66d030f8dea47be339c9425dd20c9','4c28690333fc36b3f6dac5fbd4a7fe53fb1ced41'),
    'em2a_trap_BP.uexp':('2c93538b8e1740e3823ba1a01022feca2bb519433612d690904a7e0c6764431a','afee0c18a8c25fc98954a65b5889732cc0e5dcb9'),
}
pak=Pak9(ROOT/'VRLOC.com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
data={}
for name,(digest,compressed) in expected.items():
    raw=(fixture/name).read_bytes();assert hashlib.sha256(raw).hexdigest()==digest;data[name]=raw
    entry=pak.entries['VR4/Content/Blueprints/Props/Traps/'+name]
    assert entry['hash'].hex()==compressed
    pak.f.seek(entry['off']+entry['header_size']);assert hashlib.sha1(pak.f.read(entry['size'])).hexdigest()==compressed
pak.f.close();pak.z.close()
pkg=package(data['em2a_trap_BP.uasset'],data['em2a_trap_BP.uexp']);body=data['em2a_trap_BP.uexp'];names=pkg['names']
def fname(raw,at=0):
    index,number=struct.unpack_from('<II',raw,at);assert number==0;return names[index]
def tag(raw,at):
    name=fname(raw,at);at+=8
    if name=='None':return None,at
    kind=fname(raw,at);at+=8;size,index=struct.unpack_from('<II',raw,at);at+=8;extra=None
    if kind=='StructProperty':extra=fname(raw,at);at+=24
    elif kind in ('ByteProperty','EnumProperty','ArrayProperty'):extra=fname(raw,at);at+=8
    elif kind=='BoolProperty':extra=raw[at];at+=1
    assert raw[at]==0;at+=1
    return (name,kind,extra,raw[at:at+size]),at+size
def tags(raw,at=0):
    result={}
    while at<len(raw):
        item,at=tag(raw,at)
        if item is None:break
        name,kind,extra,value=item;result[name]=(kind,extra,value)
    return result,at
def structs(value):
    count=struct.unpack_from('<I',value)[0];descriptor,_=tag(value,4)
    assert descriptor[1]=='StructProperty';raw=descriptor[3];at=0;result=[]
    for _ in range(count):item,at=tags(raw,at);result.append(item)
    return result
def payload(name):
    entry=next(e for e in pkg['exports'] if e['name']==name)
    return body[entry['uexp_offset']:entry['uexp_offset']+entry['serial_size']]
def script(name):
    raw=payload(name)
    for at in range(4,len(raw)-16):
        count=struct.unpack_from('<I',raw,at)[0]
        if at+4+count+12!=len(raw) or raw[-13]!=0x53:continue
        parser=Script(raw[at+4:at+4+count],names,pkg['imports'],pkg['exports']);tree=[]
        try:
            while parser.at<len(parser.data):
                runtime=parser.at+parser.runtime_delta;tree.append((runtime,parser.expression()))
            assert parser.at+parser.runtime_delta==struct.unpack_from('<I',raw,at-4)[0]
        except (AssertionError,IndexError,KeyError,struct.error):continue
        return dict(tree),parser.calls
    raise AssertionError(name)
bindings,_=tags(payload(next(e['name'] for e in pkg['exports'] if e['class_path']=='/Script/Engine.ComponentDelegateBinding')))
events={}
for row in structs(bindings['ComponentDelegateBindings'][2]):
    values={key:fname(value[2]) for key,value in row.items()};events[(values['ComponentPropertyName'],values['DelegatePropertyName'])]=values['FunctionNameToBind']
tree,calls=script('ExecuteUbergraph_em2a_trap_BP')
for grip,entrypoint,callpoint,index in (('Grip1',221,317,0),('Grip2',458,554,1)):
    _,delegate=script(events[(grip,'OnPreGripReleased')])
    assert delegate[0]['call']=='ExecuteUbergraph_em2a_trap_BP' and delegate[0]['arguments']==[{'int':entrypoint}]
    call=tree[callpoint]['call'];assert call['call']=='/Script/VR4.VR4BearTrapComponent.SetJawReachedTarget'
    assert call['arguments']==[{'int':index},{'literal_token':0x28}] # EX_False
    assert all('jump' not in expression and 'jump_if_not' not in expression for at,expression in tree.items() if entrypoint<=at<callpoint)
update,ucalls=script('UpdateGrab')
assert update[182]['let_bool'][1]=={'call':'/Script/Engine.KismetMathLibrary.Greater_IntInt','arguments':[{'field':['HandsGrabbedCount'],'owner':23},{'int':1}]}
assert update[216]=={'jump_if_not':927,'condition':{'field':['CallFunc_Greater_IntInt_ReturnValue'],'owner':19}}
assert [at for at,expr in update.items() if expr.get('context') and isinstance(expr.get('call'),dict) and expr['call']['call']=='/Script/VR4.VR4BearTrapComponent.SetJointAngle']==[836,2397]
assert update[264]['jump_if_not']==1839 and update[1834]=={'jump':2452} and update[2447]=={'jump':886}
for at in (278,1839):assert update[at]['let'][2]['arguments']==[{'field':['distanceFromHand'],'owner':19},{'float':1.2000000476837158}]
component,_=tags(payload('VR4BearTrap_GEN_VARIABLE'));jaws=structs(component['TrapJaws'][2])
angles=[]
for jaw in jaws:
    values=[]
    for key in ('CloseAngle','DogSavedAngle','OpenAngle'):
        angle,_=tags(jaw[key][2]);values.append(struct.unpack('<f',angle['Degrees'][2])[0])
    angles.append(values)
assert angles==[[85,55,8],[-78,-60,-8]]
# Both flat hands move through >25cm with a 45-degree look turn. Walking adds
# translation too; original Blueprint applies opposite signs to the two jaws.
required=max(abs(close-goal)/1.2 for close,goal,_ in angles)
for forward,right in ((30,-20),(36,20)):
    displacement=2*math.hypot(forward,right)*math.sin(math.radians(45)/2)
    assert displacement>required
print('PASS: original dog-trap release clears its jaw; physical SetJointAngle has a strict two-hand Blueprint gate; original 55/-60-degree goals and flat movement input',flush=True)

# Execute actual original IsDogTrap and SetJawReachedTarget machine code with no
# native-call stubs. The saved flag is set only by BOTH reached jaw records.
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM);u.mem_map(0x6210000,0x2000);u.mem_write(0x6210000,native[0x6210000:0x6212000])
obj,jawdata,trap,ret=0x20000000,0x20001000,0x20002000,0x30000000
u.mem_map(obj,0x10000);u.mem_map(ret,0x1000)
def run(address,index=0,reached=0):
    u.reg_write(UC_ARM64_REG_X0,obj);u.reg_write(UC_ARM64_REG_W1,index);u.reg_write(UC_ARM64_REG_W2,reached);u.reg_write(UC_ARM64_REG_LR,ret)
    u.emu_start(address,ret,count=100);return u.reg_read(UC_ARM64_REG_X0)
u.mem_write(obj+0x268,struct.pack('<Q',trap))
u.mem_write(trap+0x475,b'\x01');assert run(0x6210e80)==1
u.mem_write(trap+0x119,b'\x01');assert run(0x6210e80)==0
u.mem_write(trap+0x119,b'\0');u.mem_write(trap+0x475,b'\x02');assert run(0x6210e80)==0
u.mem_write(obj+0x268,bytes(8));assert run(0x6210e80)==0
u.mem_write(obj+0x218,struct.pack('<Q',jawdata));u.mem_write(obj+0x220,struct.pack('<I',2))
for index,reached,expected_saved in ((0,1,0),(1,1,1),(0,0,0),(0,1,1),(1,0,0)):
    before=bytes(u.mem_read(obj,0x3000));run(0x6210fac,index,reached);after=bytes(u.mem_read(obj,0x3000))
    assert after[0x260]==expected_saved
    assert {i for i,(x,y) in enumerate(zip(before,after)) if x!=y}<={0x260,0x1000+index*0x28+0x20}
u.mem_write(obj+0x220,struct.pack('<I',1));before=bytes(u.mem_read(obj,0x3000));run(0x6210fac,0,1);assert before==bytes(u.mem_read(obj,0x3000))
print('PASS: actual ARM64 dog-type predicate and both-jaw saved-state semantics; single-jaw records rejected; no extra native memory writes',flush=True)
print('LIMIT: authored jaw/grip runtime transforms and saving the actual village dog require device gameplay QA',flush=True)
