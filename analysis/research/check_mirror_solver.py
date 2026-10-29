"""Execute the original ARM64 mirror solver and movement clamps in Unicorn.

No APK/device/source writes. BeginPlay's reference-normal instruction window and
the complete Tick are native. FindBetweenNormals, FAngle, curve evaluation,
FixedFrame and MirrorA/BMove also execute unchanged. Ownership queries, Smd
object lookup, inactive audio and libc acosf are explicit runtime stubs.
The curve has synthetic constant default data; this is not a cooked speed-curve
or room traversal/completion test. Root-to-Smd visual feedback is not simulated.
The standalone cases below measure unassisted phone poses. Actual scoped-pose
C output is checked against the same native solver by check_mirror_touch.py.
"""
from pathlib import Path
import hashlib
import json
import math
import re
import struct

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn import arm64_const as arm
from inspect_ui_package import package


ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
phoff=struct.unpack_from('<Q',native,32)[0]
stride,count=struct.unpack_from('<HH',native,54)
segments=[struct.unpack_from('<IIQQQQQQ',native,phoff+i*stride) for i in range(count)]


def read(address,size):
    part=next(p for p in segments if p[0]==1 and p[3]<=address and address+size<=p[3]+p[5])
    start=part[2]+address-part[3]
    return native[start:start+size]


BODIES=(
    (0x6354b14,0x6354ebc,'d466dfa3c3c59311ffabca438e1530ebe9025a530a9922d0cd8470521234df5a'),
    (0x6354eec,0x63554bc,'12a3989c900cabe8853d74bf44b1552db9d9326f744be81935f70874c0dd8fae'),
    (0x60a67f4,0x60a68bc,'0e2c37033e3b872a7d2067c7ee2ab9f7fca543911d63f35c6db06045c6253843'),
    (0x60a68bc,0x60a6984,'aaf58755f69404da61a83725a54c73ab45dadc9e6ed6fc6877762a7b8a67ef57'),
    (0x66e2524,0x66e261c,'862449be9f64186ba7fa21335da0578adae82d24791f3e1370f90cc3b97076f9'),
    (0x61c4034,0x61c404c,'4bc57e56b344397d63c963d7d43eda510830f5700e8fec02b67be4b1fd2a5ea8'),
    (0x83faeb8,0x83faec4,'218c480273317aef013fa0a6a777e4582dd050807c08abe8d019e7e0e3bd1617'),
    (0x8404f24,0x84051e4,'e28f502f676583502d80ff59e6b40f0fd437d1381e1282bd15372bcd0b77c01c'),
    (0x8405bd0,0x8405d9c,'3cf323685a0eb4d4c89e25bd402200445ce4a3818c94f13bce80365cd81e7f29'),
    (0x5f13a48,0x5f13aa0,'641aa81f1fce4b4cb4e8676653aa567056de31dceda4530ebc1bb627e1dd1a02'),
)
for start,end,digest in BODIES:
    assert hashlib.sha256(read(start,end-start)).hexdigest()==digest,hex(start)

# The scoped adapter supplies controller input without replacing the solver.
adapter=(ROOT/'analysis/re4fix_mirror_touch.h').read_text()
assert not any(token in adapter for token in ('patch_code(', 'MirrorAMove(', 'MirrorBMove(', 'PuzzleComplete(', 'g_hand_ovr'))
producer=(ROOT/'analysis/re4fix.c').read_text()
offsets=re.search(r'g_hand_off\[2\]\[3\]\s*=\s*\{\s*\{([^}]+)\},[^{}]*\{([^}]+)\}',producer)
assert offsets
idle=[tuple(float(n) for n in re.findall(r'[-+]?\d+\.\d+f?',row.replace('f',''))) for row in offsets.groups()]
assert idle==[(-.2,-.3,-.3),(.2,-.26,-.36)]
assert 'q_rot(position_q, off, w); pos[0] += w[0]; pos[1] += w[1]; pos[2] += w[2];' in producer
assert 'off[0] = 0; off[1] = -0.14f; off[2] = -0.42f;' in producer
knife_source=(ROOT/'analysis/re4fix_weapon_pose.h').read_text()
knife_match=re.search(r'g_knife_keys\[KP_COUNT\]\s*=\s*\{\s*\{([^}]+)\}',knife_source)
assert knife_match
knife=tuple(float(n) for n in re.findall(r'[-+]?\d+\.\d+',knife_match[1])[:3])
assert knife==(.21,-.18,-.44)
hand_setter=struct.unpack('<I',read(0x633ab34,4))[0]
assert hand_setter&0xfc000000==0x94000000 and 0x633ab34+4*(hand_setter&0x3ffffff)==0x8712470

# Read the original native-grip CDO data, not SCS indicator or IK-target offsets.
fixture=ROOT/'analysis/research/interaction_originals'
asset=(fixture/'PuzzleMirror.uasset').read_bytes();body=(fixture/'PuzzleMirror.uexp').read_bytes()
assert hashlib.sha256(asset).hexdigest()=='34e7399e8c83b24cb271c4a93cd354a58452df45c64e4d522b78c39b3182bf27'
assert hashlib.sha256(body).hexdigest()=='df16caaf3d1135bf6e16c4b9ba61bef856b4604b7249a052a7b0694e3e328049'
metadata=package(asset,body);names=metadata['names']


def export_data(name):
    item=next(e for e in metadata['exports'] if e['name']==name)
    return body[item['uexp_offset']:item['uexp_offset']+item['serial_size']]


def property_data(data,name,kind,size,structure=None):
    marker=struct.pack('<II',names.index(name),0);at=data.index(marker)
    assert data.count(marker)==1 and struct.unpack_from('<II',data,at+8)==(names.index(kind),0)
    assert struct.unpack_from('<II',data,at+16)==(size,0)
    header=49 if structure else 25
    if structure:
        assert struct.unpack_from('<II',data,at+24)==(names.index(structure),0)
        assert data[at+32:at+48]==bytes(16)
    assert data[at+header-1]==0 and at+header+size<=len(data)
    return data[at+header:at+header+size]


GRIPS=tuple(struct.unpack('<3f',property_data(export_data(name),'RelativeLocation','StructProperty',12,'Vector'))
            for name in ('LeftGrip','RightGrip'))
angle=property_data(export_data('Default__PuzzleMirror_C'),'DeadZone','StructProperty',37,'Angle')
DEAD_ZONE=struct.unpack('<f',property_data(angle,'Degrees','FloatProperty',4))[0]
assert GRIPS==((-47.15716552734375,-19.56024742126465,107),(-47.15716552734375,20.29507064819336,107))
assert abs(DEAD_ZONE-.45)<1e-7
reference_angle=math.atan2(sum(p[1] for p in GRIPS),sum(p[0] for p in GRIPS))
DATA,STACK,STOP=0x20000000,0x40000000,0x30000000
MIRROR,ROOT_COMPONENT,LEFT_GRIP,RIGHT_GRIP=DATA+0x1000,DATA+0x2000,DATA+0x3000,DATA+0x4000
HANDS=(DATA+0x5000,DATA+0x6000)
HAND_ROOTS=(DATA+0x7000,DATA+0x8000)
R300,CURVE,AUDIO,VTABLE,TLS=DATA+0x9000,DATA+0xa000,DATA+0xb000,DATA+0xc000,DATA+0xd000
MODELS=(DATA+0xe000,DATA+0xf000)
ZERO,UP,IDENTITY=DATA+0x11000,DATA+0x11020,DATA+0x11040
LIMITS=((2.5899999141693115,3.9200000762939453),(.6700000166893005,1.2599999904632568))


class MirrorVM:
    def __init__(self):
        self.vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        self.pages=set()
        for address,length in ((DATA,0x20000),(STACK,0x20000),(STOP,4096)):
            self.vm.mem_map(address,length)
        for start,end,_ in BODIES:
            for page in range(start&~4095,(end+4095)&~4095,4096):self.code_page(page)
        for address in (0x6282f28,0x62825b0,0x616415c,0x9bf3da0):self.code_page(address&~4095)
        for page in (0xa351000,0xa352000,0xa353000,0xa356000,0xa537000):self.vm.mem_map(page,4096)
        self.qword(MIRROR+0x130,ROOT_COMPONENT)
        self.qword(MIRROR+0x248,LEFT_GRIP);self.qword(MIRROR+0x260,RIGHT_GRIP)
        for grip,position in zip((LEFT_GRIP,RIGHT_GRIP),GRIPS):self.floats(grip+0x11c,*position)
        for hand,root in zip(HANDS,HAND_ROOTS):self.qword(hand+0x130,root)
        self.qword(MIRROR+0x290,CURVE);self.floats(MIRROR+0x28c,DEAD_ZONE)
        self.qword(MIRROR+0x280,AUDIO);self.qword(AUDIO,VTABLE);self.qword(VTABLE+0x568,STOP+0x100)
        self.vm.mem_write(STOP+0x100,bytes.fromhex('c0035fd6'))
        self.qword(0xa351bf8,ZERO);self.qword(0xa3527f0,IDENTITY);self.qword(0xa352808,IDENTITY)
        self.qword(0xa3539d8,ZERO);self.qword(0xa356f78,UP);self.qword(0xa537000,R300)
        self.floats(UP,0,0,1);self.floats(IDENTITY,0,0,0,1,0,0,0,1,1,1,0,0)
        self.qword(TLS+0x28,0x1122334455667788)
        self.vm.reg_write(arm.UC_ARM64_REG_TPIDR_EL0,TLS)
        self.vm.reg_write(arm.UC_ARM64_REG_CPACR_EL1,3<<20)
        self.grabbed=(1,1);self.calls=[];self.writes=[];self.expected_begin=False
        self.vm.hook_add(UC_HOOK_CODE,self.instruction)
        self.vm.hook_add(UC_HOOK_MEM_WRITE,self.write)

    def code_page(self,page):
        if page not in self.pages:
            self.vm.mem_map(page,4096);self.vm.mem_write(page,read(page,4096));self.pages.add(page)

    def qword(self,address,value):self.vm.mem_write(address,struct.pack('<Q',value))
    def floats(self,address,*values):self.vm.mem_write(address,struct.pack('<'+'f'*len(values),*values))
    def get_float(self,address):return struct.unpack('<f',self.vm.mem_read(address,4))[0]
    def scalar(self,index):return struct.unpack('<f',struct.pack('<I',self.vm.reg_read(getattr(arm,'UC_ARM64_REG_S'+str(index)))))[0]
    def set_scalar(self,index,value):self.vm.reg_write(getattr(arm,'UC_ARM64_REG_S'+str(index)),struct.unpack('<I',struct.pack('<f',value))[0])

    def write(self,vm,kind,address,size,value,user):
        allowed=((STACK,STACK+0x20000),(R300+0x1ac,R300+0x1b4),
                 (MODELS[0]+0xb4,MODELS[0]+0xb8),(MODELS[1]+0xb4,MODELS[1]+0xb8),
                 (ROOT_COMPONENT+0x180,ROOT_COMPONENT+0x19c),(MIRROR+0x2ac,MIRROR+0x2b0))
        if self.expected_begin:allowed+=((MIRROR+0x2a0,MIRROR+0x2ac),)
        assert any(lo<=address and address+size<=hi for lo,hi in allowed),(hex(address),hex(vm.reg_read(arm.UC_ARM64_REG_PC)))
        self.writes.append((address,size))

    def instruction(self,vm,address,size,user):
        x0=vm.reg_read(arm.UC_ARM64_REG_X0)
        external=True
        if address==0x6282f28:
            self.calls.append(('grabbed',x0));vm.reg_write(arm.UC_ARM64_REG_X0,self.grabbed[(LEFT_GRIP,RIGHT_GRIP).index(x0)])
        elif address==0x62825b0:
            self.calls.append(('hand',x0));vm.reg_write(arm.UC_ARM64_REG_X0,HANDS[(LEFT_GRIP,RIGHT_GRIP).index(x0)])
        elif address==0x616415c:
            assert x0 in (60,61);self.calls.append(('model',x0));vm.reg_write(arm.UC_ARM64_REG_X0,MODELS[x0-60])
        elif address==0x9bf3da0:self.set_scalar(0,math.acos(max(-1,min(1,self.scalar(0)))))
        elif address==STOP+0x100:
            assert x0==AUDIO;vm.reg_write(arm.UC_ARM64_REG_X0,0)
        else:external=False
        if external:
            vm.reg_write(arm.UC_ARM64_REG_PC,vm.reg_read(arm.UC_ARM64_REG_LR));return
        assert any(lo<=address<hi for lo,hi,_ in BODIES),hex(address)
        if address==0x63552a8:self.calls.append(('error',self.scalar(10)))
        if address in (0x60a67f4,0x60a68bc):self.calls.append(('move',0 if address==0x60a67f4 else 1,self.scalar(0)))
        if address==0x83faeb8:self.calls.append(('curve',self.scalar(0)))

    def execute(self,start,end=STOP):
        self.calls=[];self.writes=[]
        self.vm.reg_write(arm.UC_ARM64_REG_SP,STACK+0x1f000)
        self.vm.reg_write(arm.UC_ARM64_REG_LR,STOP)
        self.vm.emu_start(start,end,count=10000)
        assert self.vm.reg_read(arm.UC_ARM64_REG_PC)==end,hex(self.vm.reg_read(arm.UC_ARM64_REG_PC))
        if end==STOP:assert self.vm.reg_read(arm.UC_ARM64_REG_SP)==STACK+0x1f000

    def begin_reference(self):
        self.expected_begin=True;self.vm.reg_write(arm.UC_ARM64_REG_X19,MIRROR)
        self.execute(0x6354cd8,0x6354d74);self.expected_begin=False
        normal=struct.unpack('<3f',self.vm.mem_read(MIRROR+0x2a0,12))
        assert max(abs(a-b) for a,b in zip(normal,(math.cos(reference_angle),math.sin(reference_angle),0)))<1e-6

    def tick(self,index,positions,dt=1/60,rate=600,frame=(0,0,0,0)):
        x,y,z,yaw=frame;a=math.radians(yaw);q=(0,0,math.sin(a/2),math.cos(a/2))
        self.floats(ROOT_COMPONENT+0x1c0,*q,x,y,z,1,1,1,0,0)
        self.floats(ROOT_COMPONENT+0x180,*q,0,yaw,0);self.floats(MIRROR+0x2ac,yaw)
        for root,point in zip(HAND_ROOTS,positions):
            px=x+point[0]*math.cos(a)-point[1]*math.sin(a)
            py=y+point[0]*math.sin(a)+point[1]*math.cos(a)
            self.floats(root+0x1d0,px,py,z+point[2])
        self.vm.mem_write(MIRROR+0x288,bytes([index]))
        self.floats(CURVE+0x30+0x68,rate) # Zero-key FRichCurve default; actual Eval executes.
        initial=(3.2,.9)[index];self.floats(R300+0x1ac+4*index,initial)
        self.vm.reg_write(arm.UC_ARM64_REG_X0,MIRROR);self.set_scalar(0,dt)
        self.execute(0x6354eec)
        moves=[c for c in self.calls if c[0]=='move']
        if moves:
            assert len(moves)==1 and moves[0][1]==index
            actual=self.get_float(R300+0x1ac+4*index)
            assert abs(actual-max(LIMITS[index][0],min(LIMITS[index][1],initial-moves[0][2])))<1e-6
            assert actual==self.get_float(MODELS[index]+0xb4)
        return moves


def flat_hands(look,pose):
    offsets=list(idle)
    if pose.startswith('gun'):
        side=int(pose[-1]);offsets[side]=(0,-.14,-.42)
    if pose.startswith('knife'):
        side=int(pose[-1]);offsets[side]=((1 if side else -1)*knife[0],knife[1],knife[2])
    a=math.radians(-look) # OVR positive yaw is a left turn in the UE world frame.
    return [(-80+100*(-oz*math.cos(a)-ox*math.sin(a)),100*(-oz*math.sin(a)+ox*math.cos(a)),160+100*oy)
            for ox,oy,oz in offsets]


def expected_error(positions):
    x=sum(p[0] for p in positions);y=sum(p[1] for p in positions)
    return (math.atan2(y,x)-reference_angle+math.pi)%(2*math.pi)-math.pi


def main():
    vm=MirrorVM();vm.begin_reference()
    samples=[];cases=0
    for pose in ('idle','gun0','gun1','knife0','knife1'):
        for look in (-60,-30,0,30,60):
            positions=flat_hands(look,pose);error=expected_error(positions)
            for index in (0,1):
                for frame in ((0,0,0,0),(213,-57,19,37)):
                    moves=vm.tick(index,positions,frame=frame);cases+=1
                    if abs(math.degrees(error))<=DEAD_ZONE:assert not moves
                    else:
                        assert len(moves)==1 and abs(moves[0][2]-error/60)<1e-5,(pose,look,index,moves,error)
                        assert len([c for c in vm.calls if c[0]=='hand'])==2
                        assert abs(next(c[1] for c in vm.calls if c[0]=='curve')-abs(math.degrees(error)))<.01
            native_error=next(c[1] for c in vm.calls if c[0]=='error')
            assert abs(native_error-error)<1e-5
            samples.append({'pose':pose,'look_degrees':look,'expected_error_degrees':round(math.degrees(error),6),
                            'native_error_degrees':round(math.degrees(native_error),6)})

    positions=flat_hands(30,'idle')
    for grips in ((0,0),(0,1),(1,0)):
        vm.grabbed=grips;assert not vm.tick(0,positions)
        assert not any(c[0] in ('hand','curve','move') for c in vm.calls)
    vm.grabbed=(1,1)
    for rate in (0,1,600):
        for dt in (1/30,1/60,1/120):
            move=vm.tick(0,positions,dt=dt,rate=rate)[0]
            error=expected_error(positions)
            wanted=math.copysign(min(abs(error),math.radians(rate)),error)*dt
            assert abs(move[2]-wanted)<1e-5
    for index,start in enumerate((0x60a67f4,0x60a68bc)):
        for delta,limit in ((100,LIMITS[index][0]),(-100,LIMITS[index][1])):
            vm.floats(R300+0x1ac+4*index,(3.2,.9)[index]);vm.set_scalar(0,delta);vm.execute(start)
            assert abs(vm.get_float(R300+0x1ac+4*index)-limit)<1e-7
            assert vm.get_float(MODELS[index]+0xb4)==vm.get_float(R300+0x1ac+4*index)
        # Original fixed-frame gates, rather than render FPS, decide whether a move applies.
        initial=(3.2,.9)[index];vm.floats(R300+0x1ac+4*index,initial);vm.floats(R300+0x2e0,2,2)
        vm.set_scalar(0,.1);vm.execute(start)
        assert abs(vm.get_float(R300+0x1ac+4*index)-initial)<1e-6
        vm.floats(R300+0x2e0,0,0)

    print('PASS: pinned original BeginPlay reference window, full Tick, native quaternion/curve/FixedFrame and A/B movement clamps; no completion calls or unapproved data writes')
    print(json.dumps({'native_tick_cases':cases,'reference_angle_degrees':math.degrees(reference_angle),'samples':samples,
                      'limits_degrees':[[math.degrees(v) for v in pair] for pair in LIMITS]},indent=2))
    print('LIMIT: constant-default curve data and controlled ownership/audio/Smd lookup; no room traversal, cooked speed-curve parity, visual-root feedback or complete angular-range claim. These unassisted phone-pose reference cases predate the scoped paired-pose adapter tested by check_mirror_touch.py.')


if __name__=="__main__":main()
