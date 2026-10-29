"""Feed actual C pair-pose samples into original mirror, IK and dog solvers.

Engine ownership/event routing, hand-position input and model render updates are
explicit stubs. Distance/curve/IK timing, mirror Tick and jaw angle/saved-state
math execute unchanged ARM64. Dog InitializeGrab/UpdateGrab execute their exact
decoded Blueprint expression trees with controlled scene services.
"""
from pathlib import Path
import hashlib
import json
import math
import struct

from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn import arm64_const as arm
import check_mirror_solver as mirror
import check_beartrap_native_paths as dog
from inspect_ui_package import package
from pak9 import Pak9

ROOT=Path(__file__).resolve().parents[2]
native=mirror.native
FIXTURE=ROOT/'analysis/research/interaction_originals'
receipt=json.loads((FIXTURE/'pair_pose_receipt.json').read_text())
assert receipt['native_sha256']==hashlib.sha256(native).hexdigest()
pak=Pak9(ROOT/'VRLOC.com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
for row in receipt['files']:
    raw=(FIXTURE/Path(row['source']).name).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==row['sha256'] and len(raw)==row['bytes']
    entry=pak.entries[row['source']];assert entry['hash'].hex()==row['compressed_sha1']
    pak.f.seek(entry['off']+entry['header_size']);assert hashlib.sha1(pak.f.read(entry['size'])).hexdigest()==row['compressed_sha1']
pak.f.close();pak.z.close()
curve_body=(FIXTURE/'IK_DistanceReleaseTime.uexp').read_bytes()
curve=package((FIXTURE/'IK_DistanceReleaseTime.uasset').read_bytes(),curve_body)
original_names=dog.names;dog.names=curve['names']
try:
    outer,_=dog.tags(curve_body);properties,_=dog.tags(outer['FloatCurve'][2]);value=properties['Keys'][2]
    descriptor,_=dog.tag(value,4);assert struct.unpack_from('<I',value)[0]==3
    KEYS=[struct.unpack_from('<BBB6f',descriptor[3],27*i) for i in range(3)]
finally:dog.names=original_names
assert KEYS==[(1,0,0,0,-1,0,0,0,0),(2,0,0,5,1,0,0,0,0),(2,0,0,20,0,0,0,0,0)]

RANGES=(
    (0x62815ec,0x6281774,'9b5a2aa21568aa2f542adf0cec3b5d9eb6f7dd603a80d17b3b8865cfb87dd2ca'),
    (0x628193c,0x6281ad0,'a75c3fa6574fd33658e4b1ed6d0d89ad7801a323a1da7a542305bec6b4f6c969'),
    (0x628851c,0x6288700,'c668aa3fef34ef2a5948d17ba15002fd741b9ea7e6af1ce1d7ba5743595f1e8e'),
    (0x6210ff4,0x6211160,'c2d6833d1946a44ceac3255299f9da370913fa5d5f915d9e4fff37ce9210c1d4'),
    (0x61c401c,0x61c404c,'aad9818d3af1c03472988ada3c4ec895266d3bcb4dd9ddc0ab3aa9333b10f663'),
    (0x61c4318,0x61c4358,'3e87ef1b1086821e8c14f342c0c3b497aca5326881329f254d2b7016e2db2f48'),
)+mirror.BODIES[6:9]
for lo,hi,digest in RANGES:assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
# UCurveFloat's original constructor supplies constant pre/post extrapolation.
assert struct.unpack_from('<II',native,0x83fae64)==(0x52808089,0x79013a69)
DATA,STACK,STOP=0x20000000,0x30000000,0x40000000
GRIP,BOX,HAND,IK,CURVE,KEYDATA,TLS,OBJECTS,CHUNKS,CHUNK,VTABLE,POINT,TRAP,JAWS,MODEL,JOINT0,JOINT1=(DATA+i*0x1000 for i in range(17))


class NativePhysics:
    def __init__(self):
        self.vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.pages=set();self.calls=[];self.point=(0,0,0);self.released=False
        for address,length in ((DATA,0x20000),(STACK,0x20000),(STOP,4096),(0xa352000,4096)):self.vm.mem_map(address,length)
        for lo,hi,_ in RANGES:
            for page in range(lo&~4095,(hi+4095)&~4095,4096):self.code(page)
        for address in (0x6299ee8,0x62825b0,0x54e83b0,0x6281f08,0x5f40a3c,0x5f414e4,0x5f4145c,0x5d3510c,0x5f53b98):self.code(address&~4095)
        self.qword(0xa352308,OBJECTS);self.qword(OBJECTS+0x10,CHUNKS);self.qword(CHUNKS,CHUNK)
        for index,address in enumerate((GRIP,BOX,HAND,IK,CURVE)):self.vm.mem_write(address+0xc,struct.pack('<I',index))
        self.qword(GRIP+0x220,BOX);self.qword(IK+0x208,CURVE);self.vm.mem_write(IK+0x1f1,b'\x01')
        self.qword(HAND,VTABLE);self.qword(VTABLE+0x648,STOP+0x100);self.qword(VTABLE+0x650,STOP+0x104)
        self.floats(HAND+0x34c,8);self.qword(TLS+0x28,0x1122334455667788)
        self.vm.reg_write(arm.UC_ARM64_REG_TPIDR_EL0,TLS);self.vm.reg_write(arm.UC_ARM64_REG_CPACR_EL1,3<<20)
        for i,key in enumerate(KEYS):self.vm.mem_write(KEYDATA+28*i,struct.pack('<BBBx6f',*key))
        self.qword(CURVE+0xa0,KEYDATA);self.vm.mem_write(CURVE+0xa8,struct.pack('<II',3,3));self.vm.mem_write(CURVE+0x9c,b'\x04\x04')
        self.qword(TRAP+0x218,JAWS);self.vm.mem_write(TRAP+0x220,struct.pack('<I',2));self.qword(TRAP+0x268,MODEL)
        self.qword(MODEL,VTABLE);self.qword(VTABLE+0x30,STOP+0x108)
        for index,joint in enumerate((JOINT0,JOINT1)):
            self.qword(JAWS+index*0x28+0x18,joint);self.floats(JAWS+index*0x28+4,(55,-60)[index])
            self.vm.mem_write(JAWS+index*0x28+0xc,bytes([index==0]))
        self.vm.hook_add(UC_HOOK_CODE,self.instruction)
    def code(self,page):
        if page not in self.pages:self.vm.mem_map(page,4096);self.vm.mem_write(page,native[page:page+4096]);self.pages.add(page)
    def qword(self,address,value):self.vm.mem_write(address,struct.pack('<Q',value))
    def floats(self,address,*values):self.vm.mem_write(address,struct.pack('<'+'f'*len(values),*values))
    def scalar(self,value=None):
        if value is not None:self.vm.reg_write(arm.UC_ARM64_REG_S0,struct.unpack('<I',struct.pack('<f',value))[0])
        return struct.unpack('<f',struct.pack('<I',self.vm.reg_read(arm.UC_ARM64_REG_S0)))[0]
    def returned(self):self.vm.reg_write(arm.UC_ARM64_REG_PC,self.vm.reg_read(arm.UC_ARM64_REG_LR))
    def instruction(self,vm,address,size,user):
        if address==0x6299ee8:
            for i,value in enumerate(self.point):vm.reg_write(getattr(arm,'UC_ARM64_REG_S'+str(i)),struct.unpack('<I',struct.pack('<f',value))[0])
            self.returned()
        elif address==0x62825b0:vm.reg_write(arm.UC_ARM64_REG_X0,HAND);self.returned()
        elif address==0x54e83b0:self.calls.append('auto-release-event');self.returned()
        elif address==0x6281f08:self.released=True;self.calls.append('release');self.returned()
        elif address in (STOP+0x100,STOP+0x104,STOP+0x108,0x5f40a3c,0x5f414e4,0x5f4145c,0x5d3510c,0x5f53b98):self.returned()
        else:assert any(lo<=address<hi for lo,hi,_ in RANGES),hex(address)
    def execute(self,address):
        self.vm.reg_write(arm.UC_ARM64_REG_SP,STACK+0x1f000);self.vm.reg_write(arm.UC_ARM64_REG_LR,STOP)
        self.vm.emu_start(address,STOP,count=10000);assert self.vm.reg_read(arm.UC_ARM64_REG_PC)==STOP
    def curve(self,excess):self.vm.reg_write(arm.UC_ARM64_REG_X0,CURVE);self.scalar(excess);self.execute(0x83faeb8);return self.scalar()
    def box(self,center,extent):self.floats(BOX+0x1c0,0,0,0,1,*center,1,1,1,0,0);self.floats(BOX+0x420,*extent)
    def distance(self,point):
        self.floats(POINT,*point);self.vm.reg_write(arm.UC_ARM64_REG_X0,GRIP);self.vm.reg_write(arm.UC_ARM64_REG_X1,POINT)
        self.execute(0x628193c);return self.scalar()
    def tick(self,point,dt=1/60):
        self.point=point;self.vm.reg_write(arm.UC_ARM64_REG_X0,IK);self.vm.reg_write(arm.UC_ARM64_REG_X1,GRIP);self.vm.reg_write(arm.UC_ARM64_REG_X2,HAND)
        self.scalar(dt);self.execute(0x628851c)
    def joint(self,index,degrees):
        self.vm.reg_write(arm.UC_ARM64_REG_X0,TRAP);self.vm.reg_write(arm.UC_ARM64_REG_W1,index);self.scalar(degrees);self.execute(0x6210ff4)
    def saved(self):return self.vm.mem_read(TRAP+0x260,1)[0]


class FullScript(dog.Script):
    def expression(self,depth=0):
        if self.data[self.at]==0x6b:self.take(1);return {'array_get_by_ref':self.expression(depth+1),'index':self.expression(depth+1)}
        return super().expression(depth)
old_parser=dog.Script;dog.Script=FullScript
try:PROGRAMS={name:dog.script(name)[0] for name in ('InitializeGrab','UpdateGrab')}
finally:dog.Script=old_parser


class DogBlueprint:
    def __init__(self,native_solver):
        self.native=native_solver;self.locals={};self.function=0;self.angles=[]
        self.state={'HandsGrabbedCount':2,'RotationDeltas':[0,0],'TowedHandPosition':[None,None],
                    'ClosedTransform':[None,None],'InitialPositions':[None,None],'HasBeenGrabbed':[False,False]}
        self.trap={'TrapJaws':[{'CloseAngle':{'Degrees':85}},{'CloseAngle':{'Degrees':-78}}]};self.state['VR4BearTrap']=self.trap
        self.grips=[{'IKTarget':{}},{'IKTarget':{}}];self.state['Grip1']=self.grips[0];self.state['Grip2']=self.grips[1]
    def get(self,x,receiver=None):
        if 'field' in x:
            name=x['field'][0]
            source=self.locals if x['owner']==self.function else self.state if x['owner']==23 else receiver
            if name not in source:source[name]={'Degrees':0} if 'MakeStruct_Angle' in name else 0
            return source[name]
        if 'struct_member' in x:return self.get(x['value'])[x['struct_member']['field'][0]]
        if 'array_get_by_ref' in x:return self.get(x['array_get_by_ref'])[self.get(x['index'])]
        for kind in ('float','int','byte_int'):
            if kind in x:return x[kind]
        if 'literal_token' in x:return {0x27:True,0x28:False,0x17:self.state,0xb:None,0x53:None}[x['literal_token']]
        if 'object' in x:return x['object']
        if 'context' in x:return self.get(x['call'],self.get(x['context']))
        if 'switch_value' in x:
            value=self.get(x['switch_value']);return self.get(next((case['result'] for case in x['cases'] if self.get(case['match'])==value),x['default']))
        if 'let' in x:self.set(x['let'][1],self.get(x['let'][2]));return None
        if 'let_bool' in x:self.set(x['let_bool'][0],self.get(x['let_bool'][1]));return None
        if 'call' in x:return self.call(x,receiver)
        raise AssertionError(x)
    def set(self,x,value):
        if 'array_get_by_ref' in x:self.get(x['array_get_by_ref'])[self.get(x['index'])]=value
        elif 'struct_member' in x:self.get(x['value'])[x['struct_member']['field'][0]]=value
        else:
            source=self.locals if x['owner']==self.function else self.state;source[x['field'][0]]=value
    def call(self,x,receiver):
        args=x['arguments'];values=[self.get(a) for a in args];name=x['call'].rsplit('.',1)[-1]
        if name=='K2_GetActorLocation':return receiver['position']
        if name=='GetHandedness':return receiver['side']
        if name in ('GetInitialLocalGrabTransform','GetLocalEarlyGrabTransformDelta'):return {'position':(0,0,0),'rotation':(0,0,0),'scale':(1,1,1)}
        if name=='BreakTransform':
            for arg,key in zip(args[1:],('position','rotation','scale')):self.set(arg,values[0][key])
            return None
        if name in ('BreakRotator','BreakVector'):
            for arg,value in zip(args[1:],values[0]):self.set(arg,value)
            return None
        if name=='MakeRotator':return tuple(values)
        if name=='K2_SetRelativeRotation':receiver['rotation']=values[0];return None
        if name=='GetJointTransform':return {'position':(0,0,0)}
        if name=='Array_Get':self.set(args[2],values[0][values[1]]);return None
        if name=='InverseTransformLocation':return tuple(a-b for a,b in zip(values[1],values[0]['position']))
        if name=='Vector_Distance':return math.dist(*values)
        if name=='Multiply_VectorFloat':return tuple(v*values[1] for v in values[0])
        if name=='Multiply_FloatFloat':return values[0]*values[1]
        if name=='Add_FloatFloat':return values[0]+values[1]
        if name=='Subtract_FloatFloat':return values[0]-values[1]
        if name=='DegAtan2':return math.degrees(math.atan2(*values))
        if name=='FMin':return min(values)
        if name=='FMax':return max(values)
        if name=='ClampAngle':return max(values[1],min(values[2],values[0])) # cosmetic IK roll, controlled zero delta
        if name in ('EqualEqual_ObjectObject','EqualEqual_ByteByte','EqualEqual_IntInt'):return values[0]==values[1]
        if name=='Greater_IntInt':return values[0]>values[1]
        if name=='SetJointAngle':
            index,angle=values;self.angles.append((index,angle['Degrees']));self.native.joint(index,angle['Degrees']);return None
        if name=='UpdateGrip':return None # renderer/grip-following scene service only
        raise AssertionError((name,values))
    def run(self,name,hand,index):
        self.function=17 if name=='InitializeGrab' else 19;self.locals={'Hand':hand,'Grip':self.grips[index],'JawIndex':index}
        tree=PROGRAMS[name];positions=sorted(tree);pc=0
        for _ in range(300):
            x=tree[positions[pc]];jump=None
            if 'return' in x:return
            if 'jump' in x:jump=x['jump']
            elif 'jump_if_not' in x:
                if not self.get(x['condition']):jump=x['jump_if_not']
            else:self.get(x)
            pc=positions.index(jump) if jump is not None else pc+1
        raise AssertionError('Blueprint instruction bound')


def verify(samples):
    assert len(samples)==15
    physics=NativePhysics()
    for excess,expected in ((0,-1),(4.99,-1),(5,1),(10,20/27),(12.5,.5),(14,.352),(15,7/27),(17.5,2/27),(20,0)):
        assert abs(physics.curve(excess)-expected)<1e-5,(excess,physics.curve(excess))
    # Same original release box: remote accepted hand immediately drops; an
    # anchored hand survives the complete unchanged native distance/curve timer.
    for position,should_release in (((-108,0,113),True),((-46.5,0,0),False)):
        physics=NativePhysics();physics.box((5.005554,-38.775063,47.645920),(60,60,150))
        for _ in range(60):physics.tick(position)
        assert physics.released==should_release,(position,physics.calls)
    physics=NativePhysics();physics.box((0,0,0),(30,30,30))
    for _ in range(15):physics.tick((53,0,0))
    assert not physics.released
    physics.tick((53,0,0));assert physics.released # excess15cm, native delay7/27seconds
    print('PASS: original authored distance-curve keys and complete ARM64 release-box/IK timer; remote pose drops, anchored pose holds60frames and fractional delay remains active')
    mirror_vm=mirror.MirrorVM();mirror_vm.begin_reference();mirror_cases=0;dog_cases=0
    baseline=next(s['positions'] for s in samples if s['kind']==2 and s['control']==0 and s['sign']==0)
    for sample in samples:
        positions=sample['positions'];active=sample['sign']!=0
        if sample['kind']==1:
            for variant in (0,1):
                moves=mirror_vm.tick(variant,positions);mirror_cases+=1
                assert bool(moves)==active,(sample,moves)
                if moves:assert abs(moves[0][2]-mirror.expected_error(positions)/60)<1e-5
            mirror_vm.grabbed=(1,0);assert not mirror_vm.tick(0,positions)
            mirror_vm.grabbed=(0,1);assert not mirror_vm.tick(1,positions);mirror_vm.grabbed=(1,1)
            for point,grip in zip(positions,mirror.GRIPS):
                physics=NativePhysics();physics.box(tuple(a+b for a,b in zip(grip,(-3.724636,0,3.027382))),(30,30,30))
                for _ in range(30):physics.tick(point)
                assert not physics.released
        else:
            physics=NativePhysics();bp=DogBlueprint(physics)
            hands=[{'position':tuple(p),'side':i} for i,p in enumerate(baseline)]
            for i in (0,1):bp.run('InitializeGrab',hands[i],i)
            assert bp.state['InitialPositions']==[tuple(p) for p in baseline]
            for i,p in enumerate(positions):hands[i]['position']=tuple(p);bp.run('UpdateGrab',hands[i],i)
            should_save=sample['sign']!=0 and (sample['control']==0 or sample['sign']<0)
            assert bool(physics.saved())==should_save,(sample,bp.angles,physics.saved());dog_cases+=1
            single=DogBlueprint(NativePhysics());single.state['HandsGrabbedCount']=1
            for i in (0,1):single.run('InitializeGrab',{'position':tuple(baseline[i]),'side':i},i)
            for i,p in enumerate(positions):single.run('UpdateGrab',{'position':tuple(p),'side':i},i)
            assert not single.angles and not single.native.saved()
            for index,point in enumerate(positions):
                physics=NativePhysics();physics.box(((5.005554,-38.775063,47.645920),(-.000275,43.138641,48.158997))[index],(60,60,150))
                for _ in range(30):physics.tick(point)
                assert not physics.released
    print(f'PASS: actual C scoped pose samples drive BOTH original mirror Tick variants ({mirror_cases} cases); complete original dog InitializeGrab/UpdateGrab -> original native SetJointAngle/saved flag ({dog_cases} cases), neutral input does not complete; original release volumes remain satisfied')
    print('LIMIT: synthetic actor placement/ownership/model-render services; physical stage traversal, moving model/root feedback and actual device acceptance remain separate gameplay QA')
