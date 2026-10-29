"""Execute original AArch64 framebuffer deletion before/after the actual compiled conditional scan gate."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X30, UC_ARM64_REG_SP, UC_ARM64_REG_PC, UC_ARM64_REG_TPIDR_EL0, UC_ARM64_REG_X19

ROOT=Path(__file__).resolve().parents[2]
LLVM=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
NATIVE=ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so'
HEADER=ROOT/'analysis/re4fix_comp_framebuffer_lifecycle.h'

class Elf:
    def __init__(self,path):
        self.data=Path(path).read_bytes()
        self.loads=[]
        phoff=struct.unpack_from('<Q',self.data,32)[0]
        phsize,phcount=struct.unpack_from('<HH',self.data,54)
        for i in range(phcount):
            p=struct.unpack_from('<IIQQQQQQ',self.data,phoff+i*phsize)
            if p[0]==1:self.loads.append(p)
        shoff=struct.unpack_from('<Q',self.data,40)[0]
        shsize,shcount=struct.unpack_from('<HH',self.data,58)
        self.sections=[struct.unpack_from('<IIQQQQIIQQ',self.data,shoff+i*shsize) for i in range(shcount)]
        self.tables={}
        self.symbols={}
        for i,section in enumerate(self.sections):
            if section[1] not in (2,11):continue
            strings=self.sections[section[6]]
            text=self.data[strings[4]:strings[4]+strings[5]]
            entries=[]
            for at in range(section[4],section[4]+section[5],24):
                name,info,other,index,value,size=struct.unpack_from('<IBBHQQ',self.data,at)
                name=text[name:text.find(b'\0',name)].decode()
                entries.append((name,value,size,index))
                if name and value:self.symbols[name]=(value,size)
            self.tables[i]=entries
    def read(self,address,size):
        for p in self.loads:
            if p[3]<=address and address+size<=p[3]+p[5]:
                start=p[2]+address-p[3]
                return self.data[start:start+size]
        raise AssertionError(('unmapped ELF address',hex(address),size))
    def load(self,uc,base):
        size=max(p[3]+p[6] for p in self.loads)
        uc.mem_map(base,(size+4095)&~4095)
        for p in self.loads:uc.mem_write(base+p[3],self.data[p[2]:p[2]+p[5]])
        for section in self.sections:
            if section[1]!=4:continue
            table=self.tables[section[6]]
            for at in range(section[4],section[4]+section[5],24):
                offset,info,addend=struct.unpack_from('<QQq',self.data,at)
                kind,symbol=info&0xffffffff,info>>32
                if kind==1027:value=base+addend
                elif kind in (1025,1026):
                    assert table[symbol][1],table[symbol]
                    value=base+table[symbol][1]+addend
                else:raise AssertionError(('unexpected test-gate relocation',kind))
                uc.mem_write(base+offset,struct.pack('<Q',value))

original=Elf(NATIVE)
checks=[]
for address,count,words in re.findall(r'\{0x([0-9a-f]+),(\d+),\{([^}]+)\}\}',HEADER.read_text()):
    words=[int(word,16) for word in re.findall(r'0x[0-9a-f]+',words)]
    assert len(words)==int(count)
    address=int(address,16)
    assert original.read(address,len(words)*4)==struct.pack('<'+'I'*len(words),*words),hex(address)
    checks.append((address,words))
assert len(checks)==9
assert struct.unpack('<I',original.read(0x6525904,4))[0]==0x3400010a
assert 0x6525904+(((0x3400010a>>5)&0x7ffff)*4)==0x6525924
print('PASS: original native entry/neighbor/branch/initialized source+resolve arrays/Matches/deferred-delete preimages',flush=True)

NBASE=0x100000000
GBASE=0x200000000
DATA=0x400000000
MANAGER=DATA+0x1000
DEVICE=DATA+0x2000
TLS=DATA+0x2800
STOP=NBASE+0x1000
TARGET=0xb40000779f58ea30
SOURCE=0xb4000077dead0001
RESOLVE=0xb4000077dead0002
DEPTH=0xb4000077dead0003
RET=struct.pack('<I',0xd65f03c0)

class NativeDeletion:
    def __init__(self,gate,patched,groups,current=None):
        self.uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        self.uc.mem_map(DATA,0x10000)
        self.uc.mem_map(STOP,4096)
        self.pages={STOP}
        self.events=[]
        self.framebuffers=[]
        self.reads=[]
        self.trace=[]
        for page in (0x6525000,0x655b000):
            self.map_page(NBASE+page)
            self.uc.mem_write(NBASE+page,original.read(page,4096))
        external=(0x652db68,0x63892d4,0x6615fd0,0x9bf2f30,0x6505478,0x650553c,0x9bf2ec0)
        for address in external:
            self.map_page(NBASE+(address&~4095));self.uc.mem_write(NBASE+address,RET)
        self.externals={NBASE+address:address for address in external}
        if patched:
            gate.load(self.uc,GBASE)
            for name,target in [('g_tgt_comp_fb_match',0x6525930),('g_tgt_comp_fb_depth',0x6525924)]:
                self.put64(GBASE+gate.symbols[name][0],NBASE+target)
            address=GBASE+gate.symbols['gate_comp_fb_resolve_scan'][0]
            self.uc.mem_write(NBASE+0x6525900,struct.pack('<IIQ',0x58000051,0xd61f0220,address))
        self.put64(MANAGER+0x50,DATA+0x5000)
        self.put32(MANAGER+0x60,(1<<len(groups))-1)
        self.put32(MANAGER+0x78,len(groups))
        self.put64(TLS+0x28,0x1122334455667788)
        index=0
        for group_index,group in enumerate(groups):
            listing=DATA+0x4000+group_index*0x80
            vector=DATA+0x5400+group_index*0x100
            self.put32(DATA+0x5000+group_index*24,group_index+1)
            self.put64(DATA+0x5000+group_index*24+8,listing)
            self.put64(listing,vector);self.put32(listing+8,len(group));self.put32(listing+12,len(group))
            for j,item in enumerate(group):
                fb=DATA+0x3000+index*0x200
                views=DATA+0x6000+index*0x40
                texture_views=DATA+0x7000+index*0x100
                self.framebuffers.append(fb)
                self.put64(vector+j*8,fb)
                self.put64(fb,texture_views);self.put32(fb+8,2);self.put32(fb+12,2)
                self.put64(fb+0x28,views);self.put32(fb+0x30,2);self.put32(fb+0x34,2)
                self.put64(fb+0x48,0xabc000+index)
                self.put64(views,0xdef000+index*2);self.put64(views+8,0xdef001+index*2)
                self.put32(fb+0x58,item.get('count',1));self.put32(fb+0x5c,item.get('count',1))
                for k,image in enumerate(item.get('source',[SOURCE])):self.put64(fb+0x60+k*8,image)
                for k,image in enumerate(item.get('resolve',[RESOLVE])):self.put64(fb+0xa0+k*8,image)
                self.put64(fb+0xe0,item.get('depth',DEPTH))
                index+=1
        if current is not None:self.put64(MANAGER+0x1d0,self.framebuffers[current])
        self.uc.hook_add(UC_HOOK_CODE,self.code)
        self.uc.hook_add(UC_HOOK_MEM_READ,self.read)
    def map_page(self,address):
        if address not in self.pages:self.uc.mem_map(address,4096);self.pages.add(address)
    def put32(self,address,value):self.uc.mem_write(address,struct.pack('<I',value))
    def put64(self,address,value):self.uc.mem_write(address,struct.pack('<Q',value))
    def get32(self,address):return struct.unpack('<I',self.uc.mem_read(address,4))[0]
    def get64(self,address):return struct.unpack('<Q',self.uc.mem_read(address,8))[0]
    def read(self,uc,kind,address,size,value,user):
        for fb in self.framebuffers:
            if fb+0x60<=address<fb+0xe0:self.reads.append((fb,address-fb,size))
    def code(self,uc,address,size,user):
        self.trace.append(address)
        operation=self.externals.get(address)
        if operation is None:return
        a,b,c=[uc.reg_read(reg) for reg in (UC_ARM64_REG_X0,UC_ARM64_REG_X1,UC_ARM64_REG_X2)]
        if operation==0x652db68:
            assert a==MANAGER+0x50
            self.put32(MANAGER+0x60,self.get32(MANAGER+0x60)&~(1<<b));self.events.append(('remove',b))
        elif operation==0x63892d4:self.events.append(('delete',a))
        elif operation==0x6615fd0:self.events.append(('free',a))
        elif operation==0x9bf2f30:uc.mem_write(a,bytes(uc.mem_read(b,c)))
        elif operation==0x6505478:
            assert a==DEVICE+0x3b0 and b in (4,7)
            self.events.append(('enqueue',b,c))
        elif operation==0x650553c:raise AssertionError('unexpected resource allocation in fixture')
        elif operation==0x9bf2ec0:raise AssertionError('native stack guard changed')
        uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
    def execute(self,image):
        self.uc.reg_write(UC_ARM64_REG_SP,DATA+0xf000)
        self.uc.reg_write(UC_ARM64_REG_TPIDR_EL0,TLS)
        self.uc.reg_write(UC_ARM64_REG_X30,STOP)
        self.uc.reg_write(UC_ARM64_REG_X0,MANAGER)
        self.uc.reg_write(UC_ARM64_REG_X1,DEVICE)
        self.uc.reg_write(UC_ARM64_REG_X2,image)
        for i in range(10):self.uc.reg_write(UC_ARM64_REG_X19+i,0x12340000+i)
        try:self.uc.emu_start(NBASE+0x65257a0,STOP,count=100000)
        except Exception:
            pc=self.uc.reg_read(UC_ARM64_REG_PC)
            print('native failure',hex(pc),[(i.mnemonic,i.op_str) for i in Cs(CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN).disasm(bytes(self.uc.mem_read(pc,4)),pc)],flush=True)
            print([(hex(at),[(i.mnemonic,i.op_str) for i in Cs(CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN).disasm(bytes(self.uc.mem_read(at,4)),at)]) for at in self.trace[-20:]],flush=True)
            raise
        assert self.uc.reg_read(UC_ARM64_REG_PC)==STOP
        assert self.uc.reg_read(UC_ARM64_REG_SP)==DATA+0xf000
        for i in range(10):assert self.uc.reg_read(UC_ARM64_REG_X19+i)==0x12340000+i
        removed=[event[2]-0xabc000 for event in self.events if event[0]=='enqueue' and event[1]==7]
        for i in removed:
            assert ('enqueue',4,0xdef000+i*2) in self.events and ('enqueue',4,0xdef001+i*2) in self.events
            assert ('free',DATA+0x6000+i*0x40) in self.events and ('free',DATA+0x7000+i*0x100) in self.events
            assert ('delete',self.framebuffers[i]) in self.events
        return sorted(removed)

def native_checks(gate):
    scenarios=[]
    for slot in range(8):
        colors=[SOURCE+i for i in range(slot+1)];colors[slot]=TARGET
        scenarios.append((f'source{slot}',[[{'count':slot+1,'source':colors}]],TARGET,[0],[0]))
        resolves=[RESOLVE+i for i in range(slot+1)];resolves[slot]=TARGET
        scenarios.append((f'resolve{slot}',[[{'count':slot+1,'source':[SOURCE+i for i in range(slot+1)],'resolve':resolves}]],TARGET,[],[0]))
    scenarios.extend([
        ('depth',[[{'depth':TARGET}]],TARGET,[0],[0]),
        ('nonmatching',[[{}]],TARGET,[],[]),
        ('null-with-depth',[[{}]],0,[],[]),
        ('null-no-depth',[[{'depth':0}]],0,[0],[0]),
        ('null-zero-color-count',[[{'count':0,'source':[],'resolve':[],'depth':DEPTH}]],0,[],[]),
        ('zero-color-resolve',[[{'count':0,'source':[],'resolve':[TARGET]}]],TARGET,[],[0]),
        ('multiple',[[{'source':[TARGET]},{},{'resolve':[TARGET]},{'depth':TARGET}]],TARGET,[0,3],[0,2,3]),
        ('multiple-lists',[[{'resolve':[TARGET]},{}],[{'source':[TARGET]},{'resolve':[TARGET]}]],TARGET,[2],[0,2,3]),
    ])
    for name,groups,image,before,after in scenarios:
        for patched,expected in [(False,before),(True,after)]:
            test=NativeDeletion(gate,patched,groups,current=0)
            actual=test.execute(image)
            assert actual==expected,(name,patched,actual,expected)
            assert test.get64(MANAGER+0x1d0)==(0 if 0 in expected else test.framebuffers[0]),(name,patched)
            old_events=list(test.events);test.execute(image)
            assert test.events==old_events,(name,patched,'duplicate deletion')
            for fb,offset,size in test.reads:assert 0x60<=offset and offset+size<=0xe0
    print(f'PASS: {len(scenarios)*2} actual original AArch64 Notify/Delete/Destroy/destructor executions: all8source+8resolve slots, depth/nonmatch/NULL exact semantics, multipleFB/cache lists, repeated deletion, callee registers/stack, original cache removal/currentFB reset/deferred framebuffer+view enqueue and container frees',flush=True)

with tempfile.TemporaryDirectory(prefix='re4-fb-lifecycle-') as temporary:
    output=Path(temporary)
    # This translation unit compiles the actual naked gate without the host fixture seam.
    mini=output/'gate.c'
    mini.write_text('#include <string.h>\n#define LOGF(...) ((void)0)\nstatic int patch_code(void*a,void*b){return 0;}\n#include "'+HEADER.as_posix()+'"\nvoid* test_gate(void){return (void*)gate_comp_fb_resolve_scan;}\n')
    shared=output/'gate.so'
    subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fPIC','-shared','-nostdlib',str(mini),'-o',str(shared)],check=True)
    gate=Elf(shared)
    address,size=gate.symbols['gate_comp_fb_resolve_scan']
    code=gate.read(address,size)
    assert code[:4]==struct.pack('<I',0xd503249f)
    assembly=list(Cs(CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN).disasm(code,address))
    assert any(i.mnemonic=='cbz' and i.op_str.startswith('x19,') for i in assembly)
    assert any(i.mnemonic=='mov' and i.op_str=='w10, #0x10' for i in assembly)
    print('PASS: actual compiled BTIj gate, NULL conditional count and explicit stolen cbz relocation to original depth/match resumes',flush=True)
    native_checks(gate)
    source=ROOT/'analysis/research/test_comp_framebuffer_lifecycle.c'
    subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
    vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
    sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
    obj,exe=output/'test.obj',output/'test.exe'
    subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: actual production installer C and AArch64 gate plus immutable native lifecycle execution; no device or original ELF writes',flush=True)
