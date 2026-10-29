"""Execute original caller/getters/LOS in Unicorn; no APK or device changes.

Actual GetBestGrip argument-production and FindBestGrip call windows execute.
The complete Bio4/GamePawn ray-origin getters, body/head getters and LOS execute.
UObject class/interface/target services and world collision are controlled runtime
services. The collision fixture is one plane; this is not a village collision dump.
"""
from pathlib import Path
import hashlib
import struct

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_UNMAPPED
from unicorn import arm64_const as arm

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
phoff=struct.unpack_from('<Q',native,32)[0];stride,count=struct.unpack_from('<HH',native,54)
segments=[struct.unpack_from('<IIQQQQQQ',native,phoff+i*stride) for i in range(count)]
def read(address,size):
    p=next(p for p in segments if p[0]==1 and p[3]<=address and address+size<=p[3]+p[5])
    start=p[2]+address-p[3];return native[start:start+size]
for lo,hi,digest in (
    (0x621cb6c,0x621cc40,'c0374cfe2a5c7398a9da3c9119abf3417768eeff5cc10e8fafc67254a1cecd83'),
    (0x6342490,0x6342628,'59e676009f5afc9591248859eaf16e5f138d9ec59d2dfd425de6d670144eb89a'),
    (0x6341b18,0x6341b2c,'485a0902a21b4e5a71801efb6b4cf0fa939ddffb37af060f0f03565851fc0479'),
    (0x62cd570,0x62cd7ac,'4c3d24d1a275a0d48ba41a5cfd95d2ef7aaf11e9709fc77a22dc541be7f9c71a'),
    (0x628220c,0x62824b8,'f2d46b54a628f7ca9982e43b6e479d7c06c069d542fa5b9387c38e6aa616efcb'),
    (0x629a930,0x629ad20,'55d2e315791c3c6072c282f15b364e61fcf65a7092c61c742fabbd6824b561e4'),
):assert hashlib.sha256(read(lo,hi-lo)).hexdigest()==digest,hex(lo)
# UHT offsets and the original caller agree: view is first, body/ray origin second.
assert struct.unpack('<I',read(0x9d9d080+0x24,4))[0]==0
assert struct.unpack('<I',read(0x9d9d050+0x24,4))[0]==12
assert struct.unpack('<I',read(0x629ab70+4,4))[0]==0x910123e2 # x2=body location (radius only)
assert struct.unpack('<I',read(0x62cd63c,4))[0]==0xa9420be1 # x1=view, x2=ray origin
shoff=struct.unpack_from('<Q',native,40)[0];sz,num=struct.unpack_from('<HH',native,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*sz) for i in range(num)]
slot=0x9cffa48+16+0x888
relocs=[struct.unpack_from('<QQq',native,at) for sec in sections if sec[1]==4
        for at in range(sec[4],sec[4]+sec[5],sec[9]) if struct.unpack_from('<Q',native,at)[0]==slot]
assert relocs==[(slot,1027,0x621cb6c)]

DATA,STACK,STOP=0x20000000,0x40000000,0x30000000
PAWN,HEAD,MODEL,VTABLE,GRIP=DATA+0x1000,DATA+0x3000,DATA+0x4000,DATA+0x6000,DATA+0x7000
PARENT,IFACE,CLASS,REGISTRY,CHUNKS,OBJECTS=DATA+0x8000,DATA+0x9000,DATA+0xa000,DATA+0xb000,DATA+0xc000,DATA+0xd000
ZERO,UP,IDENTITY,PLAYER_PTR,VIEW,BODY=DATA+0xe000,DATA+0xe020,DATA+0xe040,DATA+0xe080,DATA+0xf000,DATA+0xf020
TLS,SP=DATA+0x10000,STACK+0x18000

class VM:
    def __init__(self):
        self.vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.pages=set();self.calls=[];self.executed=set()
        self.target=(90.,0.,100.);self.wall=94.;self.mode=1
        for p,n in ((DATA,0x20000),(STACK,0x20000),(STOP,4096)):self.vm.mem_map(p,n)
        self.vm.hook_add(UC_HOOK_MEM_UNMAPPED,self.unmapped)
        for p in (0xa351000,0xa352000,0xa356000,0xa361000,0xa365000,0xa359000):self.page(p)
        self.vm.hook_add(UC_HOOK_CODE,self.instruction);self.vm.hook_add(UC_HOOK_MEM_WRITE,self.write)
        for p,fn in ((0xa351bf8,ZERO),(0xa3527f0,IDENTITY),(0xa356f78,UP),(0xa361d78,PLAYER_PTR),(0xa352308,REGISTRY)):self.qword(p,fn)
        self.qword(PLAYER_PTR,MODEL);self.qword(MODEL,VTABLE);self.qword(VTABLE+0x48,0x5f55e94)
        self.qword(PAWN,VTABLE);self.qword(VTABLE+0x888,0x621cb6c);self.qword(VTABLE+0x8a0,STOP+0x120)
        self.qword(PAWN+0x2d8,HEAD);self.floats(PAWN+0x80c,90);self.floats(HEAD+0x1d0,0,0,160)
        self.floats(PAWN+0x4dc,1);self.floats(PAWN+0x4ec,1,1,1)
        self.floats(MODEL+0x18,1,0,0,0,0,1,0,0,0,0,1,0)
        self.floats(UP,0,0,1);self.floats(IDENTITY,0,0,0,1,0,0,0,1,1,1,0,0)
        self.qword(GRIP+0x300,PARENT);self.qword(GRIP+0xa8,DATA+0x11000)
        self.qword(REGISTRY+0x10,CHUNKS);self.qword(CHUNKS,OBJECTS)
        self.qword(IFACE,VTABLE+0x1000);self.qword(VTABLE+0x1030,STOP+0x100)
        self.qword(TLS+0x28,0x1122334455667788)
        self.vm.reg_write(arm.UC_ARM64_REG_TPIDR_EL0,TLS);self.vm.reg_write(arm.UC_ARM64_REG_CPACR_EL1,3<<20)
    def page(self,p):
        if p in self.pages:return
        seg=next(s for s in segments if s[0]==1 and s[3]<=p<s[3]+s[6])
        self.vm.mem_map(p,4096);self.pages.add(p)
        length=max(0,min(4096,seg[3]+seg[5]-p))
        if length:self.vm.mem_write(p,native[seg[2]+p-seg[3]:seg[2]+p-seg[3]+length])
    def unmapped(self,vm,kind,address,size,value,user):
        self.page(address&~4095);return True
    def qword(self,p,n):self.vm.mem_write(p,struct.pack('<Q',n))
    def floats(self,p,*v):self.vm.mem_write(p,struct.pack('<'+'f'*len(v),*v))
    def vector(self,p):return struct.unpack('<3f',self.vm.mem_read(p,12))
    def scalar(self,i,v):self.vm.reg_write(getattr(arm,'UC_ARM64_REG_S'+str(i)),struct.unpack('<I',struct.pack('<f',v))[0])
    def result(self):return tuple(struct.unpack('<f',struct.pack('<I',self.vm.reg_read(getattr(arm,'UC_ARM64_REG_S'+str(i)))))[0] for i in range(3))
    def write(self,vm,kind,address,size,value,user):
        assert STACK<=address and address+size<=STACK+0x20000,(hex(address),hex(vm.reg_read(arm.UC_ARM64_REG_PC)))
    def instruction(self,vm,address,size,user):
        self.executed.add(address);x0=vm.reg_read(arm.UC_ARM64_REG_X0);external=True
        if address==0x643e924:vm.reg_write(arm.UC_ARM64_REG_X0,CLASS)
        elif address==0x69b68e8:
            assert x0==PARENT;vm.reg_write(arm.UC_ARM64_REG_X0,IFACE)
        elif address==STOP+0x100:
            assert x0==IFACE and vm.reg_read(arm.UC_ARM64_REG_X1)==GRIP
            for i,v in enumerate(self.target):self.scalar(i,v)
        elif address==STOP+0x120:vm.reg_write(arm.UC_ARM64_REG_X0,0) # No authored override of ray origin.
        elif address in (0x5d464f0,0x831dbb4):
            a=vm.reg_read(arm.UC_ARM64_REG_X1 if address==0x5d464f0 else arm.UC_ARM64_REG_X2)
            b=vm.reg_read(arm.UC_ARM64_REG_X2 if address==0x5d464f0 else arm.UC_ARM64_REG_X3)
            scale=.1 if address==0x5d464f0 else 1
            a,b=tuple(x*scale for x in self.vector(a)),tuple(x*scale for x in self.vector(b))
            self.calls.append((a,b));hit=min(a[0],b[0])<self.wall<=max(a[0],b[0])
            vm.reg_write(arm.UC_ARM64_REG_X0,int(hit))
        else:external=False
        if external:vm.reg_write(arm.UC_ARM64_REG_PC,vm.reg_read(arm.UC_ARM64_REG_LR))
    def call(self,address,args=(),stop=STOP):
        self.vm.reg_write(arm.UC_ARM64_REG_SP,SP);self.vm.reg_write(arm.UC_ARM64_REG_LR,STOP)
        for i,v in enumerate(args):self.vm.reg_write(arm.UC_ARM64_REG_X0+i,v)
        self.vm.emu_start(address,stop,count=1000000)
        assert self.vm.reg_read(arm.UC_ARM64_REG_PC)==stop
        return self.vm.reg_read(arm.UC_ARM64_REG_X0)
    def caller_vectors(self,tracked):
        self.vm.mem_write(PAWN+0xb89,bytes([tracked]));self.vm.reg_write(arm.UC_ARM64_REG_X20,PAWN)
        self.call(0x629aa20,(PAWN,),0x629aa54)
        radius_body,view,ray=self.vector(SP+0x48),self.vector(SP+0x38),self.vector(SP+0x28)
        assert radius_body==(0.,0.,0.) and view==(0.,0.,160.) and ray==(0.,0.,90.)
        return view,ray
    def los(self,view,body,mode):
        self.calls=[];self.vm.mem_write(GRIP+0x210,bytes([mode]));self.floats(VIEW,*view);self.floats(BODY,*body)
        self.qword(SP+0x20,VIEW);self.qword(SP+0x28,BODY);self.vm.reg_write(arm.UC_ARM64_REG_X19,GRIP)
        # Execute the actual LDP x1,x2 and BL PlayerHasLineOfSight from FindBestGrip.
        return self.call(0x62cd63c,(),0x62cd648)&1

v=VM()
for tracked in (0,1):
    view,body=v.caller_vectors(tracked)
    for mode in (1,2):
        v.target=(90.,0.,100.)
        assert v.los(view,body,mode)==1 and len(v.calls)==2
        assert v.los(view,(95,0,100),mode)==0 # Old production call supplied the grip center as body.
        v.target=(100.,0.,100.)
        assert v.los(view,body,mode)==0 # A real obstruction remains a rejection.
assert {0x621cb6c,0x62a9fac,0x6342490,0x6341b18,0x628220c,0x62cd63c,0x5f55e94}.issubset(v.executed)
print('PASS: six immutable native ranges, UHT view/body order and exact Bio4 vtable888 ray-origin target')
print('PASS: actual GetBestGrip getter window, both native ray-origin branches and actual FindBestGrip LOS call window')
print('PASS: complete native LOS modes1/2 accept the original view/ray-origin contract, reject the old grip-center argument and retain blocked-target rejection; stack-only writes')
print('LIMIT: controlled UObject/target services and one collision plane; actual village geometry/door state still require device acceptance')
