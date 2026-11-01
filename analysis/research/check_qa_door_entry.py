"""Execute the actual QA door guards and original authored room-entry dispatch.

Reads two bounded original DAS blocks per room in memory; writes no game assets.
"""
from pathlib import Path
import hashlib
import os
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
for start,end,digest in (
    (0x61563c8,0x615646c,'681c8ed0be6f8b0c7cc8cebc9ed9b794c7f3071c5d4f54950e80e4fa0c9819df'),
    (0x61532c4,0x61534b4,'729f805fec7a41cb488fbe305563821ebee3415e8f2676c840163cc510614a13'),
    (0x6382924,0x6382b98,'d8659942ac0de6924499af134e2de9801f822aefce7b53edd3010fb46c9ba375'),
):assert hashlib.sha256(native[start:end]).hexdigest()==digest
assert struct.unpack_from('<II',native,0x6155664)==(0xb0021f68,0xf9402d08)
assert struct.unpack_from('<II',native,0x61532c4)==(0xf81d0ff5,0xa9014ff4)
assert struct.unpack_from('<II',native,0x6153ec0)==(0xf0021f68,0xf9402d08)
clang=Path(os.environ['ANDROID_SDK_ROOT'])/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
test=ROOT/'analysis/research/test_qa_door_entry.c'
with tempfile.TemporaryDirectory(prefix='re4-qa-door-') as directory:
    obj=Path(directory)/'door.obj';exe=Path(directory)/'door.exe'
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-Werror','-fsyntax-only',str(test)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-Werror','-O2',
        '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c',str(test),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)

# Original PT_LOAD/RELA loading and decoder. C++ local-static guards have their
# ordinary single-threaded acquire/release semantics; no native game call is replaced.
loader=ROOT/'analysis/research/host_oodle_decode.py'
prefix=loader.read_text().split("fixture = ROOT / 'analysis/research/flat_ui_graph'",1)[0]
mark='u.hook_add(UC_HOOK_CODE, external, begin=stubbase, end=stubbase+0xfffff)'
assert prefix.count(mark)==1
environment={'__file__':str(loader),'__name__':'original_door_loader'}
exec(compile(prefix.replace(mark,'original_external_hook='+mark),str(loader),'exec'),environment)
u,buffers,call,base=(environment[name] for name in ('u','buffers','call','base'))
u.hook_del(environment['original_external_hook'])
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_PC,UC_ARM64_REG_LR
def external(uc,address,size,unused):
    name=environment['names'][address];a,b,c=(uc.reg_read(UC_ARM64_REG_X0+i) for i in range(3));result=a
    if name in ('memcpy','memmove'):
        assert 0<=c<0x1000000;uc.mem_write(a,bytes(uc.mem_read(b,c)))
    elif name=='memset':
        assert 0<=c<0x1000000;uc.mem_write(a,bytes([b&255])*c)
    elif name=='__cxa_guard_acquire':result=int(not uc.mem_read(a,1)[0])
    elif name=='__cxa_guard_release':uc.mem_write(a,b'\x01');result=0
    else:raise AssertionError(('unexpected external',name,hex(uc.reg_read(UC_ARM64_REG_LR))))
    uc.reg_write(UC_ARM64_REG_X0,result);uc.reg_write(UC_ARM64_REG_PC,uc.reg_read(UC_ARM64_REG_LR))
u.hook_add(UC_HOOK_CODE,external,begin=environment['stubbase'],end=environment['stubbase']+0xfffff)
from pak9 import Pak9
pak=Pak9(ROOT/'com.Armature.VR4/patch.201.com.Armature.VR4.obb','VR4/Content/Paks/pakchunk0-Android_ETC2.pak')
expected={
    0x100:(0,0x101,40,0x561f40,'2805b156415a4fd1086c01b490b5f7f8eab37b10',
           '6674d063dc2c6176ada5f43b7569f6f4f60db37a5410e823616cb5f717fdfeda',
           (0xc74bb067,0x42b52440,0x46aff88c,0x4007cf34)),
    0x211:(1,0x212,12,0x9120e0,'b81b60195d68bd05eee85d09505ca724cdd2b78c',
           '0c0fe59cd98674dc99e15a8e7bb55230feb4f7656db67ffc45e8fad1e538d10d',
           (0x457dc000,0,0x44f78000,0xbfc90fdc)),
}
def qword(at):return struct.unpack('<Q',u.mem_read(at,8))[0]
def put(at,fmt,*values):u.mem_write(at,struct.pack(fmt,*values))
pg=base+0xa501bd8;source_buffer=buffers+0x1900000;converted_buffer=buffers+0x1910000
manager=base+0xa541b38;position=buffers+0x110000;rotation=buffers+0x111000
assert qword(base+0x9cee030)==base+0x61532c4
partner_slot=qword(base+0xa361e48)
for source,(id,destination,count,offset,compressed_sha1,aev_sha256,pose) in expected.items():
    name=f'VR4/Content/BIO4/St{source>>8}/r{source:03x}.das';entry=pak.entries[name]
    assert entry['method']==1 and not entry['encrypted'] and entry['hash'].hex()==compressed_sha1
    pak.f.seek(entry['off']);local,_=pak.rentry(pak.f.read(entry['header_size']),0)
    assert all(local[k]==entry[k] for k in ('size','unc','method','hash','blocks','encrypted','cbs'))
    raw=pak.f.read(entry['size']);assert hashlib.sha1(raw).digest()==entry['hash'];cache={}
    def block(index):
        if index not in cache:
            start,end=entry['blocks'][index];compressed=raw[start-entry['header_size']:end-entry['header_size']]
            length=min(entry['cbs'],entry['unc']-index*entry['cbs']);trials=[]
            for _ in range(2):
                u.mem_write(buffers,compressed);kind=call(0x96b6f6c,[buffers,len(compressed),0]);needed=call(0x96b4b88,[kind,length])
                decoded=call(0x96b64e0,[buffers,len(compressed),buffers+0x400000,length,1,1,0,0,0,0,0,buffers+0x800000,needed,3])
                assert decoded==length;trials.append(bytes(u.mem_read(buffers+0x400000,decoded)))
            assert trials[0]==trials[1];cache[index]=trials[0]
        return cache[index]
    header=block(0);archive=struct.unpack_from('<I',header,0x2c)[0];chunks=struct.unpack_from('<I',header,archive)[0]
    offsets=list(struct.unpack_from('<'+'I'*chunks,header,archive+16));offsets.append(struct.unpack_from('<I',header,0x24)[0])
    tags=[header[archive+16+chunks*4+i*4:archive+20+chunks*4+i*4] for i in range(chunks)]
    section=tags.index(b'AEV\0');start,end=archive+offsets[section],archive+offsets[section+1];assert start==offset
    decoded=b''.join(block(i) for i in range(start//entry['cbs'],(end-1)//entry['cbs']+1))
    aev=decoded[start%entry['cbs']:start%entry['cbs']+end-start]
    assert hashlib.sha256(aev).hexdigest()==aev_sha256 and aev[:8]==struct.pack('<4sHH',b'AEV\0',0x104,count)
    u.mem_write(pg,bytes(0x9000));put(base+0xa3f5e48,'<Q',pg);put(pg+0x5038,'<H',source)
    u.mem_write(source_buffer,aev);u.mem_write(converted_buffer,bytes(16+count*0x100))
    call(0x5d3a84c,[converted_buffer,16+count*0x100,source_buffer,len(aev)])
    node=converted_buffer+16+id*0x100;authored=bytes(u.mem_read(node,0x100))
    assert authored[0x38:0x3c]==bytes((3,1,id,1)) and authored[0x90:0x94]==bytes((destination>>8,destination&255,0,0))
    assert struct.unpack_from('<4I',authored,0x80)==pose and authored[0xa0:0xa8]==bytes(8)
    # Original getter accepts live mixed AEV/ITA entries, including a tagged skipped node.
    item_node=buffers+0x1920000;u.mem_write(item_node,bytes(0xd0));put(item_node,'<Q',node)
    put(item_node+0x39,'<BB',3,0x80);put(node,'<Q',0xffffffff);put(manager+0xd8,'<Q',item_node)
    put(base+0xa542058,'<Q',manager)
    before_node=bytes(u.mem_read(node,0x100));before_item=bytes(u.mem_read(item_node,0xd0))
    assert call(0x6153ec0,[id])==node and call(0x6153ec0,[0x80])==item_node
    assert call(0x6153ec0,[0x7f])==0
    put(manager+0xd8,'<Q',item_node|1);assert call(0x6153ec0,[id])==node and call(0x6153ec0,[0x80])==0
    assert bytes(u.mem_read(node,0x100))==before_node and bytes(u.mem_read(item_node,0xd0))==before_item
    print(f'PASS: original ARM64 SceAtPtr {source:03x}: mixed ITA/AEV and tagged-node lookups preserve exact entry identity, no entry writes',flush=True)
    # Execute both original public APIs. The proposed wrapper uses SceAtExecute,
    # whose descriptor is the exact converted original entry, not a fabricated door.
    for method in ('SceAtExecRoomJump','SceAtExecute'):
        u.mem_write(pg,bytes(0x9000));put(base+0xa3f5e48,'<Q',pg);put(pg+0x24,'<I',3);put(pg+0x5038,'<HB',source,7)
        put(pg+0x503c,'<HB',0xffff,9);put(pg+0x30,'<4f',-99999,-99999,-99999,-9);put(partner_slot,'<Q',0)
        u.mem_write(manager,bytes(0x100));put(base+0xa542058,'<Q',manager)
        if method=='SceAtExecRoomJump':
            u.mem_write(position,struct.pack('<3I',*pose[:3]));put(rotation,'<III',0,pose[3],0)
            call(0x61563c8,[destination,position,rotation,0,0])
        else:
            u.mem_write(node,authored);put(node,'<Q',0xffffffff);put(manager,'<QQ',converted_buffer,converted_buffer+16);put(manager+0xd8,'<Q',node)
            call(0x6155664,[id])
        assert struct.unpack('<4I',u.mem_read(pg+0x30,16))==pose
        assert struct.unpack('<H',u.mem_read(pg+0x2c,2))[0]==destination and struct.unpack('<I',u.mem_read(pg+0x24,4))[0]==4
        assert struct.unpack('<HB',u.mem_read(pg+0x503c,3))==(source,7)
        print(f'PASS: original ARM64 {method} {source:03x}/AEV{id}->{destination:03x}: exact authored next pose/yaw, previous-room metadata, transition mode4; no substituted native game calls',flush=True)
    # Real native Ashley eligibility succeeds at zero separation, then its native
    # dead-partner guard refuses the doorway before any transition/pose writes.
    partner=buffers+0x120000;player=buffers+0x121000
    u.mem_write(partner,bytes(0x1000));u.mem_write(player,bytes(0x1000));put(partner_slot,'<Q',partner);put(base+0xa4e9b40,'<Q',player)
    u.mem_write(pg,bytes(0x9000));put(pg+0x24,'<I',3);put(pg+0x5038,'<H',source);u.mem_write(node,authored);put(node,'<Q',0xffffffff)
    assert call(0x61531c8,[])==1
    before=bytes(u.mem_read(pg,0x9000));call(0x6155664,[id]);assert bytes(u.mem_read(pg,0x9000))==before
    print(f'PASS: original ARM64 Door {source:03x}/AEV{id} retains native Ashley life guard; no next pose/room or transition-mode modification on refusal',flush=True)
pak.f.close();pak.z.close()
print('LIMIT: proof ends at the original transition request; device room loading, valid collision/visual placement and normal control approach remain separate hardware checks.',flush=True)
