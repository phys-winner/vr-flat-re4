"""Reproduce the paired-grip holster edge in original ARM64 input code.

The Oculus HandTrigger comparison/dispatch window, both InputGrip callbacks,
both GamePlayerHand implementations, TryGrabNearestGrip and the complete
IsGrabbingEnabled guard execute unchanged. Engine event routing, gameplay
permission, class lookup and the terminal grab/release callbacks are explicit
stubs. This does not simulate IK release boxes or the authored puzzle solver.
"""
from pathlib import Path
import hashlib
import struct

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm64_const as arm

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
BODIES=(
    (0x5c4ff80,0x5c50168,'1e97845944e870609131052d9b5e105498df7d701dfc42c1b18a95d368b7af5b'),
    (0x633c54c,0x633c560,'e872992f28223bf53fa6a4a4661ca1a480d9828d899e1d9db847ab73c8d3d969'),
    (0x629caec,0x629cbd8,'96f5a096bc3812fca474d301a2664c1696c7fb9452d9c8f6810e7e486c57b6e1'),
    (0x629acec,0x629aec4,'1cd23b2211a872b87174d874668f39363045f44a9131e652c5a2a086745696cc'),
    (0x629c7a0,0x629c9b4,'9c3fe5ed43794b1a6a71b946e3429d44b7adb521762afcba9a5e9a515a87974d'),
    (0x633d030,0x633d038,'4da80996965955e1fb2467b95f0f9e3ceffad91bd4bc2126573b790919eeaa11'),
)
for lo,hi,digest in BODIES:assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
DATA,STACK,STOP=0x20000000,0x30000000,0x40000000
HAND,PAWN,GRIP,CLASS,CHAIN,HV,PV,TLS=(DATA+i*0x1000 for i in range(8))
OBJECTS,CHUNKS,CHUNK,RECORD,TRIGGER,MESSAGE,MV,CONTROL=(DATA+i*0x1000 for i in range(8,16))
PRESS,RELEASE,PERMITTED=STOP+0x100,STOP+0x104,STOP+0x108


class NativeInput:
    def __init__(self):
        self.vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM);self.pages=set();self.calls=[];self.edges=[]
        for address,length in ((DATA,0x10000),(STACK,0x20000),(STOP,0x1000),(0xa352000,0x1000),(0xa35f000,0x1000)):
            self.vm.mem_map(address,length)
        for lo,hi,_ in BODIES:
            for page in range(lo&~4095,(hi+4095)&~4095,4096):self.code(page)
        for address in (0x64b07a8,0x64b07e4,0x642c718,0x629a930,0x6281ce4,0x629b240,0x6280170,0x629b4f0):self.code(address&~4095)
        self.qword(HAND,HV);self.qword(HV+0x740,0x629caec)
        self.qword(HAND+0x2f0,PAWN);self.vm.mem_write(HAND+0x505,b'\x01')
        self.qword(PAWN,PV);self.qword(PV+0x7c0,PERMITTED);self.qword(PAWN+0x10,CLASS)
        self.qword(CLASS+0x30,CHAIN);self.qword(CHAIN,CLASS+0x30)
        self.qword(0xa352308,OBJECTS);self.qword(OBJECTS+0x10,CHUNKS);self.qword(CHUNKS,CHUNK)
        self.vm.mem_write(GRIP+0xc,struct.pack('<I',1));self.vm.mem_write(HAND+0xc,struct.pack('<I',2))
        self.qword(TLS+0x28,0x1122334455667788)
        self.vm.reg_write(arm.UC_ARM64_REG_TPIDR_EL0,TLS);self.vm.reg_write(arm.UC_ARM64_REG_CPACR_EL1,3<<20)
        self.qword(MESSAGE,MV);self.qword(MV+0x98,PRESS);self.qword(MV+0xa0,RELEASE)
        self.qword(0xa35fed0,CONTROL+8);self.vm.mem_write(CONTROL+8,struct.pack('<f',.2))
        self.vm.hook_add(UC_HOOK_CODE,self.instruction)

    def code(self,page):
        if page not in self.pages:self.vm.mem_map(page,4096);self.vm.mem_write(page,native[page:page+4096]);self.pages.add(page)
    def qword(self,address,value):self.vm.mem_write(address,struct.pack('<Q',value))
    def current(self):return struct.unpack('<Q',self.vm.mem_read(HAND+0x428,8))[0]
    def down(self):return self.vm.mem_read(HAND+0x33d,1)[0]
    def returned(self,value):
        self.vm.reg_write(arm.UC_ARM64_REG_X0,value);self.vm.reg_write(arm.UC_ARM64_REG_PC,self.vm.reg_read(arm.UC_ARM64_REG_LR))

    def instruction(self,vm,address,size,user):
        x0=vm.reg_read(arm.UC_ARM64_REG_X0)
        if address in (PRESS,RELEASE):self.edges.append('pressed' if address==PRESS else 'released');self.returned(1)
        elif address==PERMITTED:self.returned(1)
        elif address==0x642c718:self.returned(CLASS)
        elif address==0x64b07a8:vm.reg_write(arm.UC_ARM64_REG_PC,0x629c7a0)
        elif address==0x64b07e4:vm.reg_write(arm.UC_ARM64_REG_PC,0x629c84c)
        elif address==0x629a930:self.calls.append('find');self.returned(GRIP)
        elif address==0x6281ce4:self.calls.append('try');self.returned(0)
        elif address==0x629b240:
            assert x0==HAND and vm.reg_read(arm.UC_ARM64_REG_X1)==GRIP
            self.calls.append('grab');self.qword(HAND+0x428,GRIP);self.returned(0)
        elif address==0x6280170:assert x0==GRIP;self.returned(0)
        elif address==0x629b4f0:
            assert x0==HAND;self.calls.append('release');self.qword(HAND+0x428,0);self.returned(0)
        else:assert any(lo<=address<hi for lo,hi,_ in BODIES),hex(address)

    def execute(self,address,end=STOP):
        self.vm.reg_write(arm.UC_ARM64_REG_SP,STACK+0x1f000);self.vm.reg_write(arm.UC_ARM64_REG_LR,STOP)
        self.vm.emu_start(address,end,count=2000)
        assert self.vm.reg_read(arm.UC_ARM64_REG_PC)==end
        assert self.vm.reg_read(arm.UC_ARM64_REG_SP)==STACK+0x1f000

    def input(self,pressed):
        self.vm.reg_write(arm.UC_ARM64_REG_X0,HAND);self.execute(0x633c54c if pressed else 0x633c558)

    def controller_frame(self,previous,value):
        self.edges=[];self.qword(RECORD,123);self.qword(RECORD+8,0)
        self.vm.mem_write(RECORD+0x10,bytes([previous]));self.vm.mem_write(TRIGGER,struct.pack('<f',value))
        self.qword(STACK+0x1f000+0x38,TRIGGER);self.qword(CONTROL,MESSAGE)
        for name,value in (('X20',RECORD+0x18),('X26',CONTROL),('X24',PAWN)):
            self.vm.reg_write(getattr(arm,'UC_ARM64_REG_'+name),value)
        self.vm.reg_write(arm.UC_ARM64_REG_S10,struct.unpack('<I',struct.pack('<f',.5))[0])
        self.execute(0x5c4ff80,0x5c50168)
        for edge in self.edges:self.input(edge=='pressed')
        return self.vm.mem_read(RECORD+0x10,1)[0]


# Positive control: the full native press really reaches one authored grab when
# free, then its unchanged current-grip guard rejects a repeated controller press.
test=NativeInput();test.input(True)
assert test.down()==1 and test.current()==GRIP and test.calls==['find','try','grab']
test.input(True);assert test.calls==['find','try','grab'] and test.down()==1 and test.current()==GRIP

# Before fix: the holstered weapon's last RG=1 followed by flat input RG=0
# delivers an original up edge, despite the direct native pair press being down.
test=NativeInput();test.input(True);test.calls=[]
assert test.controller_frame(1,0)==0 and test.edges==['released']
assert test.down()==0 and test.current()==0 and test.calls==['release']

# Owned input: old right RG remains down; the initially free left 0->1 edge
# cannot reinitialize its accepted grip. No original release is suppressed.
for previous in (0,1):
    test=NativeInput();test.input(True);test.calls=[]
    for frame in range(60):previous=test.controller_frame(previous,1)
    assert test.down()==1 and test.current()==GRIP and not test.calls
    test.controller_frame(previous,0)
    assert test.down()==0 and test.current()==0 and test.calls==['release']
print('PASS: original ARM64 HandTrigger edge reproduces holster RG-up drop; owned hold survives 60 frames; exact CurrentGrip guard prevents duplicate grab; original release remains active')
print('LIMIT: this proves input continuity only; native distance-release volumes and physical dog/mirror completion require separate checks')
