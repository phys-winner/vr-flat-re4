"""Prove the native window-format defect and execute the early CVar adapter, setter and AArch64 gate."""
from pathlib import Path
import hashlib,os,re,struct,subprocess,tempfile
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm64_const import *

ROOT=Path(__file__).resolve().parents[2]
LLVM=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'
VC=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK=Path(r'C:\Program Files (x86)\Windows Kits\10')
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
header=(ROOT/'analysis/re4fix_backbuffer.h').read_text()
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==7
for a,b,c in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(b,16),int(c,16))
relocations={a:c for a,kind,c in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if kind==1027}
assert relocations[0xa351ba0]==0xa6b1ac0
assert relocations[0xa352000]==0xa6eb068 and relocations[0xa352008]==0xa6eb058
assert relocations[0xa351be8]==0xa74e6f8
assert relocations[0x9e20d30+0x98]==0x65e7940 and relocations[0x9e20d30+0x88]==0x65ee9f0
assert relocations[0x9e20d30+0x60]==0x65e7a50
assert relocations[0xa372e90]+0x10==0x9e20d30 # Native int RegisterConsoleVariable uses this exact class.
assert struct.unpack_from('<II',native,0x65e7a50)==(0x9100d000,0xd65f03c0)
# Original PreInit sets GT identity before project settings and its RHIInit(false) call.
assert struct.unpack_from('<I',native,0x54c2b68)[0]==0x52800034
assert struct.unpack_from('<I',native,0x54c3068)[0]==0xb9000120
assert struct.unpack_from('<I',native,0x54c307c)[0]==0x39000134
assert 0x54c307c<0x54c4138<0x54c4c24
assert native[0x4002f64:0x4002f64+44].decode('utf-16-le')=='r.HDR.EnableHDROutput\0'
assert native[0x3feec10:0x3feec10+42].decode('utf-16-le')=='bUseHDRDisplayOutput\0'
for lo,hi,digest in (
    (0x75581b4,0x75581f4,'54ab81dcc119c6d89730f8f7e1fb83f5c806a20ba644d75542d600fbe4aedd18'),
    (0x88a350c,0x88a36e4,'9f97fd68b406f63befc7f1f3fa89c275ac79ef3ae17820c0e8bfd1fd7e6f0577'),
):assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
for address in (0x88a35bc,0x88a3624,0x88a3ea8):
    assert struct.unpack_from('<I',native,address)[0]==0x52a04002 # All original HDR-setting writers use GameSetting02.
assert native[relocations[0x9e211d0+8*8]:][:10].decode('utf-16-le')=='Code\0'
assert native[relocations[0x9e211d0+2*8]:][:24].decode('utf-16-le')=='GameSetting\0'
# The actual driver CI is tracked: no render-pass rewrite can make these p411 records false positives.
main=(ROOT/'analysis/re4fix.c').read_text()
crp=main[main.index('int h_vkCreateRenderPass(void* dev, const void* ci, const void* ac, void** out) {'):]
crp=crp[:crp.index('\nvoid h_vkCmdBeginRenderPass')]
assert 'int r = r_vkCreateRenderPass(dev, ci, ac, out);' in crp and 'ci=' not in crp and 'ci =' not in crp
views=(ROOT/'analysis/re4fix_comp_views.h').read_text()
assert 'rp->n!=1 || rp->fmt0!=44' in views and '*(unsigned*)(view_ci+36)=44' in views
run=(ROOT/'analysis/build/qa401_user_complaints/p411_route_first.log').read_text(encoding='utf-8-sig')
found=re.findall(r'FBSW[^\n]*att\[0\]=swapchain#([0-3]) viewfmt=50[^\n]*rpfmt\[0\]=64 MISMATCH',run)
assert set(found)==set('0123') and "key=r120s00_00" in run
print('PASS: p411 actual native Vulkan inputs disagree on all4 subtitle framebuffers (RP64/view50); no render-pass CI rewrite; exact early native caller/prologue, CVar int vtable and GT/RHI globals',flush=True)

def pages(u,addresses,source=native):
    for page in sorted({a&~4095 for a in addresses}):
        u.mem_map(page,4096)
        if source and page<len(source):u.mem_write(page,source[page:page+4096])

def call(u,address,args,ret=0x70000000):
    for i,a in enumerate(args):u.reg_write(UC_ARM64_REG_X0+i,a)
    u.reg_write(UC_ARM64_REG_SP,0x40010000);u.reg_write(UC_ARM64_REG_X30,ret)
    u.emu_start(address,ret,count=100000)
    assert u.reg_read(UC_ARM64_REG_PC)==ret
    return u.reg_read(UC_ARM64_REG_X0)

# Real CanChange -> Set -> OnChanged -> GetInt, including preservation of READONLY4/RenderThreadSafe32.
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
pages(u,[0x65e7940,0x65ee9f0,0x65e36f8,0xa351ba0,0xa352000,0xa6b1ac0,0xa6eb068,0x9bf2ee0,
    0x8a8834c,0x44aeb40])
for p,n in ((0x40000000,0x20000),(0x50000000,4096),(0x60000000,4096),(0x70000000,4096)):u.mem_map(p,n)
for address in (0xa351ba0,0xa352000,0xa352008):u.mem_write(address,struct.pack('<Q',relocations[address]))
u.reg_write(UC_ARM64_REG_TPIDR_EL0,0x60000000)
manager,variable,literal=0x50000100,0x50000400,0x50000800
u.mem_write(0xa6b1ac0,struct.pack('<Q',manager));u.mem_write(0xa6eb068,b'\1');u.mem_write(0xa6eb058,struct.pack('<I',123));u.mem_write(literal,b'0\0\0\0')
events=[]
def native_hook(uc,address,size,user):
    if address==0x9bf2ee0:
        uc.reg_write(UC_ARM64_REG_X0,123);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
    elif address==0x65e36f8:
        assert bytes(uc.mem_read(uc.reg_read(UC_ARM64_REG_X0),4))==b'0\0\0\0' and uc.reg_read(UC_ARM64_REG_X2)==10
        events.append('parse');uc.reg_write(UC_ARM64_REG_X0,0);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_X30))
    elif address==0x65eea54:events.append('native-notify')
    elif address==0x65e7d54:
        # Omit only denied-write diagnostic formatting. Native priority comparison/cset/return still execute.
        uc.reg_write(UC_ARM64_REG_PC,0x65e7e54)
u.hook_add(UC_HOOK_CODE,native_hook)
for flags in (4,0x03000004,0x06000004,0x08000004):
    data=bytearray(0x40);struct.pack_into('<I',data,0x18,flags);struct.pack_into('<ii',data,0x34,4,4);u.mem_write(variable,bytes(data));events.clear()
    call(u,0x65ee9f0,[variable,literal,0x08000000])
    result=bytes(u.mem_read(variable,0x40))
    assert struct.unpack_from('<I',result,0x18)[0]==0x08000004 and struct.unpack_from('<ii',result,0x34)==(0,0)
    assert events==['parse','native-notify'] and u.mem_read(manager+0xcc,1)==b'\1'
    assert call(u,0x65e7940,[variable])==0
for flags in (0x03000004,0x02000020):
    data=bytearray(0x40);struct.pack_into('<I',data,0x18,flags);struct.pack_into('<ii',data,0x34,0,0)
    u.mem_write(variable,bytes(data));events.clear();call(u,0x65ee9f0,[variable,literal,0x08000000])
    result=bytes(u.mem_read(variable,0x40));expected=bytearray(data);struct.pack_into('<I',expected,0x18,0x08000000|(flags&0xffff))
    assert result==expected and events==['parse','native-notify']
    for priority in range(10):assert call(u,0x65e7d18,[variable,priority<<24])==int(priority>=8)
    u.mem_write(literal,b'1\0\0\0');events.clear();call(u,0x65ee9f0,[variable,literal,0x02000000])
    assert bytes(u.mem_read(variable,0x40))==result and events==[]
    u.mem_write(literal,b'0\0\0\0')
for value,wanted in ((0,2),(1,2),(2,10),(3,10),(4,18)):
    converted=call(u,0x8a8838c,[value]);assert call(u,0x8a8834c,[converted])==wanted
print('PASS: original ARM64 setter/CanChange/OnChanged/GetInt: READONLY4/RenderThreadSafe32 preserved, GT+RT0 and native Code-priority promotion even for unchanged0; native later GameSetting Set(1) refused without writes; equal/stronger priorities preserved by native comparison; only literal parser/platform gettid/denied diagnostic formatting stubbed; CVar0->PF2 versus old4->PF18',flush=True)

with tempfile.TemporaryDirectory(prefix='re4-backbuffer-') as temporary:
    td=Path(temporary);source=ROOT/'analysis/research/test_backbuffer.c';obj=td/'test.obj';exe=td/'test.exe'
    subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
    subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-isystem',str(VC/'include'),'-isystem',str(SDK/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(VC/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib',
        'libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{VC/"lib/x64"}',
        f'/LIBPATH:{SDK/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{SDK/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
    arm_source=td/'arm.c';arm_elf=td/'arm.elf'
    arm_source.write_text('#include <stddef.h>\n#include <stdint.h>\n'
        'unsigned test_tid=123,test_patch_failure;\nint gettid(void){return test_tid;}\n'
        'int memcmp(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;}\n'
        'static int patch_code(void*t,void*h){if(test_patch_failure)return -1;unsigned*p=t;p[0]=0x58000051;p[1]=0xd61f0220;*(uintptr_t*)(p+2)=(uintptr_t)h;return 0;}\n'
        '#define LOGF(...) ((void)0)\n'+f'#include "{(ROOT/"analysis/re4fix_backbuffer.h").as_posix()}"\n'
        'int probe_install(uintptr_t base){return install_backbuffer(base);}\n',encoding='utf8')
    subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-ffreestanding',
        '-fno-builtin','-fno-stack-protector','-fuse-ld=lld','-nostdlib','-static','-Wl,-Ttext=0x10000,-e,probe_install',
        '-o',str(arm_elf),str(arm_source)],check=True)
    arm=arm_elf.read_bytes();phoff=struct.unpack_from('<Q',arm,32)[0];step,count=struct.unpack_from('<HH',arm,54)
    segments=[struct.unpack_from('<IIQQQQQQ',arm,phoff+i*step) for i in range(count)]
    shoff=struct.unpack_from('<Q',arm,40)[0];step,count=struct.unpack_from('<HH',arm,58)
    sections=[struct.unpack_from('<IIQQQQIIQQ',arm,shoff+i*step) for i in range(count)]
    symsec=next(s for s in sections if s[1]==2);strsec=sections[symsec[6]];strings=arm[strsec[4]:strsec[4]+strsec[5]]
    symbols={}
    for at in range(symsec[4],symsec[4]+symsec[5],symsec[9]):
        name,info,other,index,value,length=struct.unpack_from('<IBBHQQ',arm,at)
        if name:symbols[strings[name:strings.index(0,name)].decode()]=value
    native_base=0x10000000;native_entry=native_base+0x6ed4148;native_tail=native_entry+16;caller=native_base+0x54c4c2c
    hdr_variable=variable+0x100
    def fixture(late=False,bad=False,bad_hdr=False,fail=False,default_value=4,hdr_value=0):
        uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM);mapped=set()
        for seg in segments:
            if seg[0]!=1:continue
            for page in range(seg[3]&~4095,(seg[3]+seg[6]+4095)&~4095,4096):
                if page not in mapped:uc.mem_map(page,4096);mapped.add(page)
            uc.mem_write(seg[3],arm[seg[2]:seg[2]+seg[5]])
        offsets=[int(a,16) for a,_,_ in checks]+[0x6ed4158,0xa74e6f8,0xa6eb068,0xa6b1ac0]
        for page in sorted({(native_base+a)&~4095 for a in offsets}):
            uc.mem_map(page,4096);off=page-native_base
            if off<len(native):uc.mem_write(page,native[off:off+4096])
        for p,n in ((0x40000000,0x20000),(0x50000000,4096),(0x70000000,4096)):uc.mem_map(p,n)
        uc.mem_write(native_base+0xa74e6f8,struct.pack('<Q',manager if late else 0))
        uc.mem_write(native_base+0xa6b1ac0,struct.pack('<Q',manager));uc.mem_write(native_base+0xa6eb068,b'\1');uc.mem_write(native_base+0xa6eb058,struct.pack('<I',123))
        for object_,flags,value in ((variable,0x03000004,default_value),(hdr_variable,0x20,hdr_value)):
            data=bytearray(0x40);struct.pack_into('<Q',data,0,native_base+0x9e20d30);struct.pack_into('<I',data,0x18,flags);struct.pack_into('<ii',data,0x34,value,value);uc.mem_write(object_,bytes(data))
        if bad:uc.mem_write(native_entry,b'\0')
        if bad_hdr:uc.mem_write(native_base+0x75581cc,b'\0')
        if fail:uc.mem_write(symbols['test_patch_failure'],struct.pack('<I',1))
        return uc
    for key in ('late','bad','bad_hdr','fail'):
        uc=fixture(**{key:True});before=bytes(uc.mem_read(native_entry,16))
        assert call(uc,symbols['probe_install'],[native_base])==0 and bytes(uc.mem_read(native_entry,16))==before
    for argument,default_value,hdr_value in ((0,4,1),(1,4,1),(0,0,0),(1,0,0)):
        uc=fixture(default_value=default_value,hdr_value=hdr_value);assert call(uc,symbols['probe_install'],[native_base])==1
        assert call(uc,symbols['probe_install'],[native_base])==1 # No duplicate patch.
        # A simulated original continuation unwinds the exact stolen native prologue; no GPU initialization is simulated.
        uc.mem_write(native_tail,struct.pack('<5I',0xa94557f6,0xa9464ff4,0xa9477bfd,0x910203ff,0xd65f03c0))
        order=[]
        def adapter_hook(engine,address,size,user):
            if address==native_base+0x65e9ba0:
                raw=bytes(engine.mem_read(engine.reg_read(UC_ARM64_REG_X1),62));name=raw.decode('utf-16-le').split('\0',1)[0]
                assert name in ('r.DefaultBackBufferPixelFormat','r.HDR.EnableHDROutput');hdr=name=='r.HDR.EnableHDROutput'
                order.append('find:HDR' if hdr else 'find:default');engine.reg_write(UC_ARM64_REG_X0,hdr_variable if hdr else variable);engine.reg_write(UC_ARM64_REG_PC,engine.reg_read(UC_ARM64_REG_X30))
            elif address==native_base+0x65e7940:
                object_=engine.reg_read(UC_ARM64_REG_X0);assert object_ in (variable,hdr_variable)
                order.append('get:HDR' if object_==hdr_variable else 'get:default');engine.reg_write(UC_ARM64_REG_X0,struct.unpack('<I',engine.mem_read(object_+0x34,4))[0]);engine.reg_write(UC_ARM64_REG_PC,engine.reg_read(UC_ARM64_REG_X30))
            elif address==native_base+0x65ee9f0:
                assert engine.mem_read(engine.reg_read(UC_ARM64_REG_X1),4)==b'0\0\0\0' and engine.reg_read(UC_ARM64_REG_X2)==0x08000000
                assert order[:4]==['find:default','get:default','find:HDR','get:HDR']
                object_=engine.reg_read(UC_ARM64_REG_X0);assert object_ in (variable,hdr_variable)
                order.append('set:HDR' if object_==hdr_variable else 'set:default');engine.mem_write(object_+0x34,bytes(8));engine.mem_write(object_+0x18,struct.pack('<I',0x08000020 if object_==hdr_variable else 0x08000004));engine.reg_write(UC_ARM64_REG_PC,engine.reg_read(UC_ARM64_REG_X30))
            elif address==native_tail:
                assert engine.reg_read(UC_ARM64_REG_X0)==argument;order.append('original-rhi')
        uc.hook_add(UC_HOOK_CODE,adapter_hook)
        saved={globals()[f'UC_ARM64_REG_X{i}']:0x11000000+i for i in range(19,30)}
        for reg,value in saved.items():uc.reg_write(reg,value)
        call(uc,native_entry,[argument],caller)
        assert order==['find:default','get:default','find:HDR','get:HDR','set:default','get:default','set:HDR','get:HDR','original-rhi']
        assert uc.reg_read(UC_ARM64_REG_SP)==0x40010000
        assert all(uc.reg_read(reg)==value for reg,value in saved.items())
        assert struct.unpack('<ii',uc.mem_read(variable+0x34,8))==(0,0)
        assert struct.unpack('<ii',uc.mem_read(hdr_variable+0x34,8))==(0,0)
print('PASS: compiled production AArch64 installer/hook/gate runs both native bools and zero/nonzero CVar pairs, validates both before any setter and applies SDR before original continuation; caller SP/LR/all integer callee-saved registers preserved; late/RHI+HDR signature/write failures leave native entry intact, repeat install is a no-op',flush=True)
