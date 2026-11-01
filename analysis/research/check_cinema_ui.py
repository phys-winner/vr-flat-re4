"""Original subtitle viewport/weak-pointer ABI proof and actual helper C tests."""
import os
from pathlib import Path
import hashlib,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
header=(root/'analysis/re4fix_cinema_ui.h').read_text()
for lo,hi,digest in (
    (0x69defcc,0x69df01c,'5263854ad7474485db542937af9e1a65927596cca4b4633190c64b57614c9f67'),
    (0x69df1cc,0x69df24c,'2144371488f7df4eaa292697317803db88ca4ffa31e731786a20f9ecd6f85344'),
    (0x7dd5c58,0x7dd5c6c,'dc561575093e6cc9d54dbba119785c333747fecbc993a170ca0c24e22758d3c3'),
    (0x7dd70f4,0x7dd743c,'b137241a03510ec9efebe54c1b613fa72fbf39269852d65df57583aea9b9accf'),
    (0x7dd7468,0x7dd7494,'e40504e95936574988f394e441bbaa723e6018c1d23e22a8f82029e715684461'),
    (0x7db1a28,0x7db1a30,'3306a9fc3bb6052ef5dbee5d5a3407f4e4f737c6ed4025ce1d03224d0c1e3f86'),
    (0x7e37bec,0x7e37cac,'172e88cd88646ef0101627ef377d4e9821544ebadcec13d3ef9c918c0683f9de'),
    (0x7da30fc,0x7da3104,'0c212c7473b7fd0a59731f2321adfe362e7b43ceb76319298f3c41842a018c77'),
    (0x7da3274,0x7da34b4,'ac8a2b121a58d920fa740a334391dbb6cd9fbc3ce333c34c3e1e4a358b119193'),
    (0x7dd762c,0x7dd7678,'90e234cae491208f09aebc35a64a8bc585284d6f8fe12f2bdb1fcbf568851ab9'),
    (0x7dd7678,0x7dd7688,'6ab7d97918af0206b07933d7771ad64a87d851dd4b69d0f9c2f63efd5c1c1e09'),
    (0x7dd7688,0x7dd769c,'c5c88e575e61d7f4d64f736ef37afae43492f6da73b0e6f7d460c30c6582fc80'),
    (0x7dd769c,0x7dd76a8,'b3adb66bf13ffe95efdc43483d5f947f906191f43dbc1cd1fc3773053eea87bd'),
    (0x7db1fb4,0x7db1fc8,'fc78490c9c96bb78cf558c17d0c79c0bad7231aef87825192bd416026e09001c'),
    (0x7de6f14,0x7de70b8,'6ea8ba9ebbbc6402fe16180e0627d8ee25b361b85e48a5f64180ab013bd196d3'),
    (0x7e296b4,0x7e29770,'a2d7c6e5320ee8ead524ba4d7752a49bb9150562501bb736507725093dd22069'),
    (0x7e52bf4,0x7e52ca8,'22d6e23a5e1794a5306076ae80627bf514b04d0e07812aa5e1126146529ce687'),
    (0x7dd5818,0x7dd5828,'dd4ad9287fddf5429c92c11fe247d9ffecedba3c175b0884243593be1be82da1'),
    (0x7da4524,0x7da46d4,'f163ed4c06558806a3a8b5d2d8b0257c1bf21cbc131798dd168d464a9c89d13a'),
    (0x7d89928,0x7d899f0,'11799691411b0640c39c54155f5c6df1302e40925b5de41bc10c2d92c1e7f733'),
    (0x7d899f0,0x7d89ab0,'725ed42b90a36168dc20bb7e2c800c71eae47a680a87a4030ff21a6ad7aaab2a'),
    (0x6a48428,0x6a48460,'1fce40d2adab2e19471a945b4087cc9ce5460dfb9e2e09112f7cef4e97a213ed'),
    (0x6a96f34,0x6a96f54,'26ad33fd064f20d8824ff3720ff4ff75d070998beae982c1ecea3fc3214a3edd'),
    (0x7cc126c,0x7cc1284,'77bac5f8884b2ddbd82f1fec4de91af732fb2e81b69e5bcf298827b871cee7bd'),
    (0x64f6a14,0x64f6aa4,'e4ebaba0580e753caf8af610600dc99bed29c491021d76a5cdec93072d45a524'),
    (0x6548a24,0x6548a40,'9910365216df1595d3f3dcb3810ee6f8868f77f40565d930008dc097d93096cc'),
    (0x838db28,0x838db70,'8424dfeec7aa2ee9462fa36dea9db6358082680d60c0b226d8067be1d78ca246'),
    (0x8f5f8f0,0x8f5f900,'4e981b29900a19dde2a6d3f660b21b232eb89a1c0fc6220cd728efe63a825d07'),
    (0x8f5f4c8,0x8f5f4f0,'33df81c4577c0e37a3e44453940a9a0f0a53089fe709ed55b9bea4fed64cb18d'),
    (0x7db2bf0,0x7db2d30,'23ea2790feab814700c0384661d6b430f8ac084988cc5a91288606831b6bb1f9'),
    (0x8a80460,0x8a80478,'410b20a0dbb7c7ac5832b953d84c1ea816e0619fa8d87c01e9874251e5911aee'),
    (0x8a807d0,0x8a807ec,'68b2504e4825f797f0e5b12e96aa4e1fc34862ef56ab31ab5aa4dea5d22fc15d'),
    (0x78d5dc0,0x78d615c,'a4ebca47a6f01c24a54063efba307683224a644a0225dc813c4158c6aabcdc5d'),
    (0x8479d0c,0x8479d18,'02636407787103e166ab0312bee94b41ff5b231b82464240ad10a77faa2b07cd'),
    (0x7d89f04,0x7d89fc8,'6a67554f858a45fbb699eab8ec339eaa5d0614d2d08a9883dd0483dcea7bb236'),
    (0x7e2a0bc,0x7e2a0cc,'20e0bf5508afed6e2827fe4613e141f97811a7328ab716f0830697c05386aa8b'),
    (0x7d8c40c,0x7d8c46c,'6e6a572431d9ef8b61ff396f22d8a7067f5b92d892d63fe32bb044695d833e56'),
    (0x6e07d50,0x6e07f0c,'fb62ad0fd569db04022e5c693f9b06442cde12c36c130efe62ece42efb7b9d15'),
    (0x6d85198,0x6d859bc,'7b1d668f4b065a0957e9031eee05de1dc6aa54c6c1018a7dd578b5e47a1f3805'),
    (0x6ace4c4,0x6acec4c,'384b3a9795929b5f1853a96a391e5fa6288b6eee8e31f9801eebd9206efb0057'),
    (0x6df9a2c,0x6df9b60,'0d5a414d7b1f8c3bac0aafb5d7bbba5512e178eb635b5b8c7497ddf7596cafd8'),
    (0x6dfad00,0x6dfada4,'86f81d04d3cd71a81d97661a0fa71896424ccbbe688b267cdebec07d1902ed0d'),
    (0x7d6e744,0x7d6e758,'d7578c6610ce85abcb571d8d61a6ec8bdb7e15a1f765c354354b710971f43894'),
    (0x7d8284c,0x7d82938,'cd9d5953e0bc27022ffd2d36f936c7abf7b7de7484d75523c765f6a1c3e36b52'),
    (0x6d848c4,0x6d848e8,'2bbb71d4e335add9f4cfb8ceddf5ffbb6434a0307dbf6b2c2dd25eb1a9aea9c9'),
    (0x7e13474,0x7e1352c,'0ea830f5314572c5b51be2451fea356d48776933bbfe4a2b57d472c096411042'),
    (0x7e202c4,0x7e2037c,'ee97da9775d2df22b1665339e76de8ce9bee54a6cb767900ee3e051d4b0c4be2'),
    (0x7e12a14,0x7e12acc,'9cfc2deeb92d57a033c813371249070eb7f60e98d08966470253d06aa7170fcf'),
    (0x7e1f864,0x7e1f91c,'4f6cd1880dacd9d51451791d1cad0439e04eb83aded8eb6e9c3ab9e8c653fceb'),
):
    assert hashlib.sha256(native[lo:hi]).hexdigest()==digest,hex(lo)
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',header)
assert len(checks)==39
for address,first,second in checks:
    assert struct.unpack_from('<II',native,int(address,16))==(int(first,16),int(second,16)),address
assert struct.unpack_from('<I',native,0x9f77968+36)[0]==0x498
assert struct.unpack_from('<I',native,0x9f77e20+36)[0]==0x429
for address,value in ((0x9f701c0+36,0x90),(0x9f70190+36,0xac),(0x9f7bd18+36,8),(0x9f7bd98+32,28)):
    assert struct.unpack_from('<I',native,address)[0]==value,hex(address)
for address,value in ((0x9f6b718+36,0x1a8),(0x9f64bd0+36,0x188),(0x9f64c30+36,0x150),
    (0x9f64b70+36,0x260),(0x9f64b40+36,0x268),(0x9e51138+36,0),(0x9e51068+36,0x10),
    (0x9e51020+32,0x50),(0x9e50f98+36,8),(0x9e50f68+36,0x10),(0x9e51098+36,8),
    (0x9e50f40+36,0x40),(0x9e50f18+36,0x48),(0x9e503b8+32,0x28),(0x9e50348+36,0x10)):
    assert struct.unpack_from('<I',native,address)[0]==value,hex(address)
for address,value in ((0x9f70308+36,0x28),(0x9f5a388+36,0x28),(0x9f5a358+36,0x30),
    (0x9f7bf38+36,0x28),(0x9f5a260+36,0x50),(0x9f5fb10+36,0x58)):
    assert struct.unpack_from('<I',native,address)[0]==value,hex(address)
# Native ABI/storage, including position's authoritative DPI conversion.
assert struct.unpack_from('<II',native,0x7da30fc)==(0x2d130400,0x17fffeb4)
assert struct.unpack_from('<II',native,0x7dd7654)==(0x1e201929,0x1e201908)
assert struct.unpack_from('<II',native,0x7dd765c)==(0xbd01f269,0xbd01f668)
assert struct.unpack_from('<II',native,0x7dd7664)==(0xa91e7e7f,0xa9427bfd)
# Exact original weak Get rejects serial changes and PendingKill/Unreachable before loading object.
assert struct.unpack_from('<II',native,0x69df220)==(0x6b08019f,0x540000c1)
assert struct.unpack_from('<II',native,0x69df234)==(0x721c051f,0x54000060)
# Exact script fields come from the saved original runtime reflection, not guessed offsets.
runtime=(root/'analysis/research/p252_cinema_probe_runtime.log').read_text(encoding='utf-8-sig')
for name,offset in (('TheatreBox','1198'),('WidgetSubtitles','2b0'),('SubtitleWidget','338')):
    assert re.search(r"name='"+name+r"' type='ObjectProperty' offset="+offset+r" element=8 dim=1",runtime)
assert "class='CutsceneTheatreBox_C' size=528" in runtime
assert "'Subtitles_C_" in runtime and "class='Subtitles_C'" in runtime
main=(root/'analysis/re4fix_cinema.h').read_text()
assert main.index('gate_cinema_view(camera,dt,view);')<main.index('touch_cinema_ui_tick();')<main.index('cinema_flat_view(camera,view);')
print('PASS: 49 native ranges and 39 preimages; exact owned TextBlock/WidgetTree, original AutoWrap and typed slot Fill APIs, slot Parent/Content/Outer and native RootWidget ABI; original font80/color40/opacity/outline/shadow, viewport/scale/pivot HFA and DPI; Widget498/getter, weak serial/GC refusal, runtime BP fields and original-first integration',flush=True)

# Execute the original leaf constructors with the original RELATIVE relocation for FLinearColor::Black.
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_X30
shoff=struct.unpack_from('<Q',native,40)[0];shsize,shcount=struct.unpack_from('<HH',native,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',native,shoff+i*shsize) for i in range(shcount)]
relocations=[struct.unpack_from('<QQq',native,at) for sec in sections if sec[1]==4
    for at in range(sec[4],sec[4]+sec[5],sec[9]) if struct.unpack_from('<Q',native,at)[0]==0xa35e178]
assert relocations==[(0xa35e178,1027,0x430fd7c)]
u=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
for page in (0x6a48000,0x6a96000,0xa35e000,0x430f000):
    u.mem_map(page,4096);u.mem_write(page,native[page:page+4096])
target,ret=0x10000000,0x70000000
u.mem_map(target,4096);u.mem_map(ret,4096);u.mem_write(0xa35e178,struct.pack('<Q',0x430fd7c))
for function,bytes_ in ((0x6a96f34,32),(0x6a48428,80)):
    u.mem_write(target,bytes([0xa5])*bytes_);u.reg_write(UC_ARM64_REG_X0,target);u.reg_write(UC_ARM64_REG_X30,ret)
    u.emu_start(function,ret,count=100);value=bytes(u.mem_read(target,bytes_))
    offset=0 if bytes_==32 else 16
    assert struct.unpack_from('<I',value,offset)[0]==0 and value[offset+4:offset+6]==bytes(2)
    assert struct.unpack_from('<Q',value,offset+8)[0]==0 and struct.unpack_from('<4f',value,offset+16)==(0,0,0,1)
    if bytes_==80:
        assert value[:16]==bytes(16) and struct.unpack_from('<I',value,72)[0]==24 and value[76]==3
print('PASS: original ARM64 FontOutlineSettings/SlateFontInfo constructors execute: outline0, separateFill0, materialsNULL, black opaque outline; native grayscale atlas PF_A8 uses R8_UNORM and alpha=R mapping',flush=True)

# Execute the original main-pass setter and its actual Widget3D/renderer consumers.
# The material itself can remain masked: no colour/depth pass receives the suppressed quad.
from unicorn import UC_HOOK_CODE
from unicorn import arm64_const as a64
q=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
for page in sorted({address&~4095 for address in (0x838db28,0x8327b3c,0x8f5f8f0,0x8f5f4c8,
    0x8a80460,0x8a807d0,0x7db2bf0,0x8479d0c,0x8a831c4,0x8a83250,0x78d5dc0,0x78d6000)}):
    q.mem_map(page,4096);q.mem_write(page,native[page:page+4096])
memory,stop=0x11000000,0x71000000
q.mem_map(memory,0x10000);q.mem_map(stop,4096)
component,proxy,view,family,tls,stack,relevance,batch,mask=[memory+n*4096 for n in range(9)]
q.reg_write(a64.UC_ARM64_REG_TPIDR_EL0,tls)
q.mem_write(view,struct.pack('<Q',family));q.mem_write(family+0x28,struct.pack('<Q',1<<56))
dirty_calls=[]
def quad_native_callback(uc,address,size,user):
    if address==0x8327b3c:
        dirty_calls.append(uc.reg_read(a64.UC_ARM64_REG_X0));uc.reg_write(a64.UC_ARM64_REG_PC,uc.reg_read(a64.UC_ARM64_REG_X30))
    elif address in (0x8a831c4,0x8a83250):
        # Only visibility/shadow context is supplied; all relevance calculation runs original code.
        uc.reg_write(a64.UC_ARM64_REG_X0,int(address==0x8a831c4))
        uc.reg_write(a64.UC_ARM64_REG_PC,uc.reg_read(a64.UC_ARM64_REG_X30))
q.hook_add(UC_HOOK_CODE,quad_native_callback)
for original in (bytes(7),bytes.fromhex('fffdffff5519a1'),bytes.fromhex('cdab8967452381')):
    q.mem_write(component,bytes(0x550));q.mem_write(component+0x208,original)
    for value in (0,0,1,1,0,1):
        before=bytes(q.mem_read(component,0x550));previous=(int.from_bytes(before[0x208:0x20c],'little')>>17)&1
        count=len(dirty_calls)
        q.reg_write(a64.UC_ARM64_REG_X0,component);q.reg_write(a64.UC_ARM64_REG_X1,value);q.reg_write(a64.UC_ARM64_REG_X30,stop)
        q.emu_start(0x838db28,stop,count=100)
        expected=bytearray(before);flags=struct.unpack_from('<I',expected,0x208)[0]
        struct.pack_into('<I',expected,0x208,(flags&~0x20000)|(value<<17))
        assert bytes(q.mem_read(component,0x550))==expected
        assert len(dirty_calls)==count+int(previous!=value)
        assert not dirty_calls or dirty_calls[-1]==component
# Reflection SetBit consumers independently establish both bit locations.
for function,bit in ((0x8f5f8f0,17),(0x8f5f4c8,52)):
    q.mem_write(component,bytes(0x550));q.reg_write(a64.UC_ARM64_REG_X0,component);q.reg_write(a64.UC_ARM64_REG_X30,stop)
    q.emu_start(function,stop,count=30)
    assert int.from_bytes(q.mem_read(component+0x208,7),'little')==1<<bit
for main in (0,1):
    for custom in (0,1):
        q.mem_write(component+0x208,struct.pack('<Q',(main<<17)|(custom<<52)))
        q.mem_write(proxy,bytes(0x200))
        # Exact constructor blocks copy native component bits to their scene-proxy flags.
        for begin,end in ((0x8a80460,0x8a80478),(0x8a807d0,0x8a807ec)):
            flags=int.from_bytes(q.mem_read(proxy+0x1f,6),'little')
            for register,value in ((a64.UC_ARM64_REG_X27,component+(0x14c if begin==0x8a80460 else 0x208)),
                (a64.UC_ARM64_REG_X26,proxy+0x1f),(a64.UC_ARM64_REG_X8,proxy+0x1f),
                (a64.UC_ARM64_REG_X9,flags),(a64.UC_ARM64_REG_X10,flags)):
                q.reg_write(register,value)
            q.emu_start(begin,end,count=30)
        assert ((int.from_bytes(q.mem_read(proxy+0x1f,6),'little')>>7)&1)==main
        assert ((int.from_bytes(q.mem_read(proxy+0x1f,6),'little')>>46)&1)==custom
        for material in (0,1,0xffffffffffffffff):
            q.mem_write(proxy+0x170,struct.pack('<Q',material))
            for register,value in ((a64.UC_ARM64_REG_X0,proxy),(a64.UC_ARM64_REG_X1,view),
                (a64.UC_ARM64_REG_SP,stack+4096),(a64.UC_ARM64_REG_X30,stop)):
                q.reg_write(register,value)
            q.emu_start(0x7db2bf0,stop,count=200)
            result=struct.pack('<QQ',q.reg_read(a64.UC_ARM64_REG_X0),q.reg_read(a64.UC_ARM64_REG_X1))
            # RenderCustomDepth=bit5, RenderInDepthPass=bit6, RenderInMainPass=bit7.
            assert struct.unpack_from('<H',result,8)[0]&0xe0==(main<<7)|(custom<<5)
            if not main and not custom and material in (0,1):
                q.mem_write(relevance,result);q.mem_write(batch,bytes(0x200))
                q.mem_write(batch,struct.pack('<Q',batch+0x100));q.mem_write(batch+0x170,struct.pack('<I',1))
                q.mem_write(mask,bytes(4))
                for register,value in ((a64.UC_ARM64_REG_X0,0),(a64.UC_ARM64_REG_X1,0),
                    (a64.UC_ARM64_REG_X2,relevance),(a64.UC_ARM64_REG_X3,batch),
                    (a64.UC_ARM64_REG_X4,view),(a64.UC_ARM64_REG_X5,mask),
                    (a64.UC_ARM64_REG_SP,stack+4096),(a64.UC_ARM64_REG_X30,stop)):
                    q.reg_write(register,value)
                q.emu_start(0x78d5dc0,stop,count=300)
                assert bytes(q.mem_read(mask,4))==bytes(4)
print('PASS: original ARM64 main-pass setter preserves all other component bytes and marks render state only on change; native reflection/proxy/Widget3D consumers execute all main/custom-depth combinations, with separate depth relevance always0; main0/custom0 produces native dynamic mesh-pass mask0',flush=True)

# The real UMG API updates the live Slate attribute and invalidates layout, rather than only
# editing a UObject flag. Then original arranged geometry and text-layout consumers prove
# that both native slot alignments must fill the allotted phone width.
wrap_vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
for page,size in ((0x7d8c000,4096),(0x7d89000,4096),(0x6e07000,4096),(0x7da2000,4096),
    (0x6ad2000,4096),(0x6d85000,4096),(0x6ace000,4096),(0x6a5b000,4096),
    (0x6df9000,8192),(0x6cc0000,4096),(0xa3ac000,4096),(0xa356000,4096),
    (0xa378000,4096),(0x431a000,4096),(0xa731000,4096),
    (0x7d6e000,4096),(0x7d82000,4096),(0x6d84000,4096)):
    wrap_vm.mem_map(page,size)
    if page<len(native):wrap_vm.mem_write(page,native[page:page+size])
wrap_vm.mem_map(0x12000000,0x14000);wrap_stop=0x72000000;wrap_vm.mem_map(wrap_stop,4096)
wrap_widget,wrap_slate,wrap_tls,wrap_stack,wrap_box,wrap_overlay,wrap_text,wrap_slot,wrap_slots,wrap_geom,wrap_result,wrap_array,wrap_cache,wrap_args,wrap_attr=range(0x12000000,0x1200f000,4096)
wrap_vm.reg_write(a64.UC_ARM64_REG_TPIDR_EL0,wrap_tls)
wrap_vm.mem_write(0xa3ac798,bytes(8)) # Constructor's vtable is not called by this fixture.
for pointer,target in ((0xa3566f8,0x431a59c),(0xa3785d8,0xa731f80)):
    assert [(addr,kind,addend) for sec in sections if sec[1]==4
        for at in range(sec[4],sec[4]+sec[5],sec[9])
        for addr,kind,addend in [struct.unpack_from('<QQq',native,at)] if addr==pointer]==[(pointer,1027,target)]
    wrap_vm.mem_write(pointer,struct.pack('<Q',target))
wrap_umg_overlay,wrap_umg_box,wrap_ref=0x12010000,0x12011000,0x12012000
wrap_invalidations=[];wrap_arranged=[];wrap_desired={}
def wrap_float_bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def wrap_float(at,value):wrap_vm.mem_write(at,struct.pack('<f',value))
def wrap_int(at,value):wrap_vm.mem_write(at,struct.pack('<I',value))
def wrap_pointer(at,value):wrap_vm.mem_write(at,struct.pack('<Q',value))
def wrap_native_callback(uc,address,size,user):
    if address in (0x7da2ad0,0x6ad2fa8,0x6ad2e48,0x6a5b9bc,0x6cc07b8):
        if address==0x6ad2fa8:wrap_invalidations.append((uc.reg_read(a64.UC_ARM64_REG_X0),uc.reg_read(a64.UC_ARM64_REG_W1)))
        if address==0x6ad2e48:
            # Supply measured desired sizes only; all arranging is original engine code.
            desired=wrap_desired.get(uc.reg_read(a64.UC_ARM64_REG_X0),(10000,104))
            uc.reg_write(a64.UC_ARM64_REG_S0,wrap_float_bits(desired[0]));uc.reg_write(a64.UC_ARM64_REG_S1,wrap_float_bits(desired[1]))
        if address==0x6a5b9bc:wrap_arranged.append(bytes(uc.mem_read(uc.reg_read(a64.UC_ARM64_REG_X1),72)))
        uc.reg_write(a64.UC_ARM64_REG_PC,uc.reg_read(a64.UC_ARM64_REG_X30))
wrap_vm.hook_add(UC_HOOK_CODE,wrap_native_callback)
def wrap_run(pc,x0,x1=0,x2=0,end=wrap_stop):
    for reg,value in ((a64.UC_ARM64_REG_X0,x0),(a64.UC_ARM64_REG_X1,x1),(a64.UC_ARM64_REG_X2,x2),
        (a64.UC_ARM64_REG_SP,wrap_stack+4000),(a64.UC_ARM64_REG_X30,wrap_stop)):
        wrap_vm.reg_write(reg,value)
    wrap_vm.emu_start(pc,end,count=10000)
def wrap_s0():return struct.unpack('<f',struct.pack('<I',wrap_vm.reg_read(a64.UC_ARM64_REG_S0)))[0]
wrap_vm.mem_write(wrap_widget,bytes([0xa5])*0x2a0);wrap_run(0x7d8c40c,wrap_widget)
assert wrap_vm.mem_read(wrap_widget+0x10d,1)==b'\xa4' and wrap_vm.mem_read(wrap_widget+0x110,4)==bytes(4)
for live in (False,True):
    for initial in (0xa4,0xa5):
        for enabled in (0,1):
            wrap_vm.mem_write(wrap_widget+0x10d,bytes([initial]));wrap_pointer(wrap_widget+0x290,wrap_slate if live else 0)
            wrap_vm.mem_write(wrap_slate,bytes(0x800));wrap_vm.mem_write(wrap_slate+0x6e8,b'\0\1')
            wrap_vm.mem_write(wrap_slate+0x790,b'\1');wrap_vm.mem_write(wrap_slate+0x794,b'\1')
            before=bytes(wrap_vm.mem_read(wrap_widget,0x2a0));count=len(wrap_invalidations)
            wrap_run(0x7d89f04,wrap_widget,enabled)
            expected=bytearray(before);expected[0x10d]=(initial&0xfe)|enabled
            assert bytes(wrap_vm.mem_read(wrap_widget,0x2a0))==expected
            assert len(wrap_invalidations)==count+int(live and enabled)
            if live:
                assert bytes(wrap_vm.mem_read(wrap_slate+0x6e8,2))==bytes([enabled,1])
                assert wrap_vm.mem_read(wrap_slate+0x790,1)==bytes([int(not enabled)])
                if enabled:assert wrap_invalidations[-1]==(wrap_slate,5)
wrap_pointer(wrap_widget+0x290,wrap_slate);wrap_run(0x7d89f04,wrap_widget,1)
wrap_pointer(wrap_box+0x2c8,wrap_overlay);wrap_int(wrap_box+0x2d8,2);wrap_int(wrap_box+0x2dc,1)
wrap_float(wrap_box+0x398,-1);wrap_float(wrap_box+0x3b0,-1)
wrap_vm.mem_write(wrap_overlay+0x1a8,b'\1');wrap_vm.mem_write(wrap_result,b'\1')
wrap_pointer(wrap_overlay+0x2b8,wrap_slots);wrap_int(wrap_overlay+0x2c0,1)
wrap_pointer(wrap_slots,wrap_slot);wrap_pointer(wrap_slot+0x10,wrap_text)
wrap_vm.mem_write(wrap_slot+0x24,b'\2\1');wrap_vm.mem_write(wrap_text+0x1a8,b'\1')
for offset in (0x28,0x2c,0x30,0x34):wrap_float(wrap_slot+offset,4)
# Execute both complete original UMG slot setters, including the SizeBox weak-Slate pin.
for live in (False,True):
    for function,widget,size,field,pointer,slate_field in (
        (0x7d6e744,wrap_umg_overlay,0x58,0x50,0x38,0x24),
        (0x7d8284c,wrap_umg_box,0x60,0x58,0x48,0x2d8)):
        slate=wrap_slot if widget==wrap_umg_overlay else wrap_box
        wrap_vm.mem_write(widget,bytes([0xa5])*size)
        wrap_pointer(widget+pointer,slate if live else 0)
        if widget==wrap_umg_box:wrap_pointer(widget+0x50,wrap_ref);wrap_int(wrap_ref+8,1)
        for value in (0,0,2,3,1,2):
            before=bytes(wrap_vm.mem_read(widget,size));previous=int.from_bytes(wrap_vm.mem_read(slate+slate_field,1),'little')
            count=len(wrap_invalidations);wrap_run(function,widget,value)
            expected=bytearray(before);expected[field]=value
            assert bytes(wrap_vm.mem_read(widget,size))==expected
            if live:
                assert wrap_vm.mem_read(slate+slate_field,1)==bytes([value])
                if widget==wrap_umg_box:
                    assert len(wrap_invalidations)==count+int(previous!=value)
                    if previous!=value:assert wrap_invalidations[-1]==(wrap_box,1)
                    assert int.from_bytes(wrap_vm.mem_read(wrap_ref+8,4),'little')==1
# A short wrapped desired size is the missing control in the older overflowing-only fixture.
# The real p435 car50 cache and geometry both measured127.067352DIP, with828.408325DIP canvas.
for width,scale,desired in ((828.4083251953125,.4958000183105469,127.06735229492188),
    (822.272,1,127.06735229492188),(384,1.5,127.06735229492188),
    (1056,2,127.06735229492188),(828.4083251953125,.4958000183105469,10000)):
    wrap_desired={wrap_overlay:(desired+8,104),wrap_text:(desired,104)}
    for box_align,overlay_align in ((2,2),(0,2),(2,0),(0,0)):
        wrap_run(0x7d8284c,wrap_umg_box,box_align);wrap_run(0x7d6e744,wrap_umg_overlay,overlay_align)
        wrap_vm.mem_write(wrap_geom,bytes(56))
        for offset,value in ((0,width),(4,104),(8,scale),(28,scale),(40,scale)):wrap_float(wrap_geom+offset,value)
        wrap_arranged.clear();wrap_run(0x6d85198,wrap_box,wrap_geom,wrap_result)
        assert len(wrap_arranged)==1
        wrap_vm.mem_write(wrap_geom,wrap_arranged[0][:56])
        wrap_pointer(wrap_result+0x128,wrap_array);wrap_int(wrap_result+0x130,0);wrap_int(wrap_result+0x134,1)
        wrap_run(0x6ace4c4,wrap_overlay,wrap_geom,wrap_result)
        child=bytes(wrap_vm.mem_read(wrap_array,56));cw,ch,cs=struct.unpack_from('<3f',child)
        expected=width-8 if (box_align,overlay_align)==(0,0) else min(desired,width-8)
        assert abs(cw-expected)<.001 and abs(cs-scale)<.00001 and ch==96,(width,box_align,overlay_align,cw)
        if desired<width-8 and (box_align,overlay_align)!=(0,0):assert abs(cw-127.06735229492188)<.001
        wrap_vm.mem_write(wrap_geom,child);wrap_pointer(wrap_cache,wrap_slate)
        wrap_pointer(wrap_args+0x10,wrap_attr);wrap_pointer(wrap_args+0x18,wrap_slate+0x6e8)
        wrap_run(0x7d89f04,wrap_widget,1);wrap_float(wrap_cache+0x30,cw)
        wrap_run(0x6df9a2c,wrap_cache,wrap_args,end=0x6df9b5c);assert abs(wrap_s0()-cw)<.001
        wrap_run(0x6dfad00,wrap_cache,x2=wrap_geom,end=0x6dfada0);assert abs(wrap_s0()-cw)<.001
wrap_desired={}
wrap_run(0x7d8284c,wrap_umg_box,2);wrap_run(0x7d6e744,wrap_umg_overlay,2)
for width in (822.272,384.0,1056.0):
    wrap_vm.mem_write(wrap_geom,bytes(56))
    for offset,value in ((0,width),(4,104),(8,1),(28,1),(40,1)):wrap_float(wrap_geom+offset,value)
    wrap_arranged.clear();wrap_run(0x6d85198,wrap_box,wrap_geom,wrap_result)
    assert len(wrap_arranged)==1 and abs(struct.unpack_from('<f',wrap_arranged[0])[0]-width)<0.001
    wrap_vm.mem_write(wrap_geom,wrap_arranged[0][:56])
    wrap_pointer(wrap_result+0x128,wrap_array);wrap_int(wrap_result+0x130,0);wrap_int(wrap_result+0x134,1)
    wrap_run(0x6ace4c4,wrap_overlay,wrap_geom,wrap_result)
    child=bytes(wrap_vm.mem_read(wrap_array,56));cw,ch,cs,cx,cy=struct.unpack_from('<5f',child)
    assert abs(cw-(width-8))<0.001 and (ch,cs,cx,cy)==(96,1,4,4)
    wrap_vm.mem_write(wrap_geom,child);wrap_pointer(wrap_cache,wrap_slate)
    wrap_pointer(wrap_args+0x10,wrap_attr);wrap_pointer(wrap_args+0x18,wrap_slate+0x6e8)
    for enabled in (0,1):
        wrap_run(0x7d89f04,wrap_widget,enabled);wrap_float(wrap_cache+0x30,cw)
        wrap_run(0x6df9a2c,wrap_cache,wrap_args,end=0x6df9b5c)
        assert abs(wrap_s0()-(cw if enabled else 0))<0.001
        wrap_run(0x6dfad00,wrap_cache,x2=wrap_geom,end=0x6dfada0)
        assert abs(wrap_s0()-(cw if enabled else 0))<0.001
assert not re.search(r'\*\([^;\n]*text\+0x10d[^;\n]*\)\s*[|&^]?=',header)
print('PASS: original ARM64 AutoWrap default/setter and both complete UMG slot setters; other UObject bytes preserved, original weak-Slate pin released, native layout invalidated; actual car50 desired127.067 reproduces narrow geometry with either centered ancestor, while both Fill produce820.408 at DPI0.4958; original SBox/Overlay/size/paint consumers cover full/half canvases and overflowing/short desired sizes',flush=True)

# Exact authored subtitle families share ordinary runtime text with a black (2,2) shadow.
from inspect_ui_package import package
originals=root/'analysis/research/subtitle_originals'
for stem,asset_hash,body_hash in (
    ('Subtitles','225de12f49a33bd8b8b0205aaa2417ede08136e2292375b2564d926bb3824085','f9c30887a989cf7c9d7d521e17e888d15c23474aa85383f63c1002e453ec453e'),
    ('CommSubtitles','21999e56c8acaa23e160eed3ab2dd17715ea546c4c8eb5eb0569e90becbe9ec6','3a98f66a99a3ef21456222fbae8906d3133f66a12b7e7191d0ad36f9a689847d'),
):
    asset=(originals/(stem+'.uasset')).read_bytes();body=(originals/(stem+'.uexp')).read_bytes()
    assert hashlib.sha256(asset).hexdigest()==asset_hash and hashlib.sha256(body).hexdigest()==body_hash
    meta=package(asset,body);names=meta['names']
    if stem=='Subtitles':
        assert 'AutoWrapText' not in names and 'WrapTextAt' not in names
        boxes=[e for e in meta['exports'] if e['class_path']=='/Script/UMG.SizeBox']
        assert len(boxes)==2
        for export in boxes:
            data=body[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
            for name,value in (('WidthOverride',1000),('HeightOverride',104)):
                marker=struct.pack('<6I',names.index(name),0,names.index('FloatProperty'),0,4,0)+b'\0'+struct.pack('<f',value)
                assert data.count(marker)==1
            for name in ('bOverride_WidthOverride','bOverride_HeightOverride'):
                marker=struct.pack('<6I',names.index(name),0,names.index('BoolProperty'),0,0,0)+b'\1\0'
                assert data.count(marker)==1
        trees=[e for e in meta['exports'] if e['class_path']=='/Script/UMG.WidgetTree']
        assert len(trees)==2
        for export,box in zip(trees,boxes):
            data=body[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
            marker=struct.pack('<6I',names.index('RootWidget'),0,names.index('ObjectProperty'),0,4,0)+b'\0'+struct.pack('<i',box['index'])
            assert data.count(marker)==1
        def original_data(export):return body[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
        def object_ref(export,name):
            data=original_data(export)
            marker=struct.pack('<6I',names.index(name),0,names.index('ObjectProperty'),0,4,0)+b'\0'
            assert data.count(marker)==1
            return struct.unpack_from('<i',data,data.index(marker)+len(marker))[0]
        def original_enum(export,name,kind,value):
            marker=struct.pack('<6I',names.index(name),0,names.index('ByteProperty'),0,8,0)
            marker+=struct.pack('<II',names.index(kind),0)+b'\0'+struct.pack('<II',names.index(value),0)
            assert original_data(export).count(marker)==1
        exports={e['index']:e for e in meta['exports']}
        for text in (e for e in meta['exports'] if e['class_path']=='/Script/UMG.TextBlock'):
            overlay_slot=exports[object_ref(text,'Slot')];overlay=exports[object_ref(overlay_slot,'Parent')]
            box_slot=exports[object_ref(overlay,'Slot')];box=exports[object_ref(box_slot,'Parent')]
            assert (overlay_slot['class_path'],overlay['class_path'],box_slot['class_path'],box['class_path'])==(
                '/Script/UMG.OverlaySlot','/Script/UMG.Overlay','/Script/UMG.SizeBoxSlot','/Script/UMG.SizeBox')
            assert object_ref(overlay_slot,'Content')==text['index'] and overlay_slot['outer']==overlay['index']
            assert object_ref(box_slot,'Content')==overlay['index'] and box_slot['outer']==box['index']
            assert text['outer']==overlay['outer']==box['outer']
            tree=exports[text['outer']];assert object_ref(tree,'RootWidget')==box['index']
            for slot in (overlay_slot,box_slot):original_enum(slot,'HorizontalAlignment','EHorizontalAlignment','HAlign_Center')
            original_enum(text,'Justification','ETextJustify','ETextJustify::Center')
    for export in meta['exports']:
        if export['class_path']!='/Script/UMG.TextBlock':continue
        data=body[export['uexp_offset']:export['uexp_offset']+export['serial_size']]
        def property_value(name,size,structure):
            marker=struct.pack('<II',names.index(name),0);assert data.count(marker)==1;at=data.index(marker)
            assert struct.unpack_from('<II',data,at+8)==(names.index('StructProperty'),0)
            assert struct.unpack_from('<II',data,at+16)==(size,0)
            assert struct.unpack_from('<II',data,at+24)==(names.index(structure),0)
            assert data[at+32:at+49]==bytes(17)
            return data[at+49:at+49+size]
        font=property_value('Font',37,'SlateFontInfo')
        assert font==struct.pack('<IIIIIIbiII',names.index('Size'),0,names.index('IntProperty'),0,4,0,0,20,names.index('None'),0)
        assert struct.unpack('<4f',property_value('ShadowColorAndOpacity',16,'LinearColor'))==(0,0,0,1)
        assert struct.unpack('<2f',property_value('ShadowOffset',8,'Vector2D'))==(2,2)
font_bytes=(originals/'RobotoBold.ufont').read_bytes()
assert hashlib.sha256(font_bytes).hexdigest()=='24571503140760240924dcd1238f77e7cd0454c0d8b0793990cefa2fad71471f'
from PIL import ImageFont
raster=ImageFont.truetype(str(originals/'RobotoBold.ufont'),50)
for text in ('\u041d\u0435 \u0445\u043e\u0447\u0443 \u043f\u043e\u043b\u0443\u0447\u0438\u0442\u044c \u0448\u0442\u0440\u0430\u0444 \u0437\u0430 \u043f\u0430\u0440\u043a\u043e\u0432\u043a\u0443.','I do not want a parking ticket.'):
    coverage=list(raster.getmask(text));assert max(coverage)==255 and sum(v==255 for v in coverage)>5000
print('PASS: exact original cinema/communicator TextBlocks override font size20 only, shadow black opaque at(2,2); original RobotoBold produces solid Cyrillic/Latin interiors. Asset/constructor checks do not establish the runtime draw/atlas failure.',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
source=root/'analysis/research/test_cinema_ui.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-cinema-ui-') as td:
    obj,exe=Path(td)/'ui.obj',Path(td)/'ui.exe'
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror',
        '-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
        '-c','-o',str(obj),str(source)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc / "lib/x64"}',f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}',
        f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: Android ARM64 -Werror and actual C host execution',flush=True)
