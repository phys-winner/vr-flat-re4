"""Read-only proof of the temporary QA original inventory autopack API; no save/device mutation."""
from pathlib import Path
import hashlib,re,struct
root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
source=(root/'analysis/research/qa_grenade_fixture.h').read_text()
for address,a,b in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\}',source):
 assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16))
rela={a:v for a,t,v in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
assert rela[0xa361da0]==0xa500780
assert native[rela[0x9dc5890]:].startswith(b'EItemId::Grenade\0') and struct.unpack_from('<q',native,0x9dc5898)[0]==1
assert native[rela[0x9dc7770]:].startswith(b'EPickupSpawnType::Crate\0') and struct.unpack_from('<q',native,0x9dc7778)[0]==2
assert hashlib.sha256(native[0x62e97b8:0x62e9b5c]).hexdigest()=='073fcfff4b1d37b8d104eea40b27f23459384896e1bab35de4a54137c8713c9c'
assert hashlib.sha256(native[0x6458828:0x645895c]).hexdigest()=='9035619df9b868d6a70cbe117886ab781c82aa39f9e3f9b51dcabb6b46f7c670'
assert hashlib.sha256(native[0x5ffd1e4:0x5ffd714]).hexdigest()=='30c6d1d80bb0ebd557e2fa2589bfbbd0b392250e4072a7f9de9de375eee572ae'
assert hashlib.sha256(native[0x62e3198:0x62e3200]).hexdigest()=='26ae687eda4da0df57c874444687666c7ae4f284aa3a41a4f0d68be5c7ce3895'
assert hashlib.sha256(native[0x5ffb1e4:0x5ffb2e8]).hexdigest()=='2a72fa1fb51e462e76af30895b9b7e08e1f0d8df07cdae92f6e22f2a223262be'
assert hashlib.sha256(native[0x5f2886c:0x5f28bfc]).hexdigest()=='b2a14f00a7b3ad07f3f64a76f37d2f382c18a402db2eb2757298f38f46d11185'
def branch(address):
 word=struct.unpack_from('<I',native,address)[0];assert word&0xfc000000==0x94000000
 delta=word&0x3ffffff
 if delta&0x2000000:delta-=0x4000000
 return address+4*delta
assert branch(0x62ec888)==0x5f2886c
assert branch(0x5f28bac)==0x5f32408
assert branch(0x5f28bb8)==0x62e1ee4
assert branch(0x5f28bc0)==0x62e2020
assert branch(0x645892c)==0x62e97b8
assert branch(0x5ffd6c0)==0x5f2886c
assert branch(0x61539ac)==0x62e3198 and branch(0x61539bc)==0x5ffd1e4
assert branch(0x6450bf4)==0x62e3198 and struct.unpack_from('<I',native,0x6450bf8)[0]==0xb9000260
assert branch(0x62e31ac)==branch(0x62e31cc)==branch(0x62e31ec)==0x5f29858
assert struct.unpack_from('<I',native,0x62e31a8)[0]==0x52800fe1
assert struct.unpack_from('<I',native,0x62e31c8)[0]==0x52800fc1
assert struct.unpack_from('<I',native,0x62e31e8)[0]==0x52800fa1
assert struct.unpack_from('<I',native,0x63815bc)[0]==0xd65f03c0
print('PASS: temporary QA-only original PutInCase autopack+get+native placement flags, native current-case-size getter and Grenade ID1. SpawnItemPickup ABI audited but its null-SCE receive also uses get-only. Earlier get-only leaves InAttacheCase0; original DebugGive is shipping RET stub')

# Run the exact original getter, its UFunction return store, and its original pickup caller.
# Only cItemMgr::search and the final PutInCase body are dependencies mocked here; the latter's
# complete original native packing/save/get implementation is hash-audited above, not emulated.
from unicorn import Uc,UC_ARCH_ARM64,UC_MODE_ARM,UC_HOOK_CODE
from unicorn import arm64_const as arm
emu=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
for page in (0x62e3000,0x5f29000,0x5ffd000,0x6153000,0x6450000,0xa361000,0x40000000,0x70000000):
 emu.mem_map(page,0x1000)
for lo,hi in ((0x62e3198,0x62e3200),(0x61539ac,0x61539c4),(0x6450bd4,0x6450c08)):
 emu.mem_write(lo,native[lo:hi])
emu.mem_write(0xa361da0,struct.pack('<Q',0xa500780))
emu.mem_write(0x5f29858,b'\xc0\x03\x5f\xd6')
emu.mem_write(0x5ffd1e4,b'\xc0\x03\x5f\xd6')
owned=set();queries=[];received=[]
def dependency(uc,address,size,data):
 if address==0x5f29858:
  assert uc.reg_read(arm.UC_ARM64_REG_X0)==0xa500780
  item=uc.reg_read(arm.UC_ARM64_REG_W1);queries.append(item)
  uc.reg_write(arm.UC_ARM64_REG_X0,0x40000100 if item in owned else 0)
  uc.reg_write(arm.UC_ARM64_REG_PC,uc.reg_read(arm.UC_ARM64_REG_X30))
 elif address==0x5ffd1e4:
  received.append(tuple(uc.reg_read(reg) for reg in (arm.UC_ARM64_REG_W0,arm.UC_ARM64_REG_W1,arm.UC_ARM64_REG_W2)))
  uc.reg_write(arm.UC_ARM64_REG_W0,1)
  uc.reg_write(arm.UC_ARM64_REG_PC,uc.reg_read(arm.UC_ARM64_REG_X30))
emu.hook_add(UC_HOOK_CODE,dependency)
for owned_values,expected,wanted in (((),0,[0x7f,0x7e,0x7d]),((0x7d,),1,[0x7f,0x7e,0x7d]),((0x7e,),2,[0x7f,0x7e]),((0x7f,),3,[0x7f]),((0x7d,0x7e,0x7f),3,[0x7f])):
 owned=set(owned_values)
 def execute(start,end=0x70000000):
  queries.clear();emu.reg_write(arm.UC_ARM64_REG_SP,0x40000f00);emu.reg_write(arm.UC_ARM64_REG_X30,0x70000000)
  emu.emu_start(start,end,count=500);assert queries==wanted
 execute(0x62e3198);assert emu.reg_read(arm.UC_ARM64_REG_W0)==expected
 # Original exec writes precisely one uint32; the following canary proves no FVector2D/HFA return.
 emu.mem_write(0x40000400,bytes(0x28));emu.mem_write(0x40000500,b'\xa5'*8)
 emu.reg_write(arm.UC_ARM64_REG_X1,0x40000400);emu.reg_write(arm.UC_ARM64_REG_X2,0x40000500)
 execute(0x6450bd4);assert bytes(emu.mem_read(0x40000500,8))==struct.pack('<I',expected)+b'\xa5'*4
 for quantity in (1,2,65535):
  received.clear();emu.mem_write(0x400006a8,struct.pack('<H',quantity));emu.reg_write(arm.UC_ARM64_REG_X19,0x40000600);emu.reg_write(arm.UC_ARM64_REG_W21,1)
  execute(0x61539ac,0x61539c4)
  assert received==[(1,quantity,expected)] and emu.reg_read(arm.UC_ARM64_REG_W20)==1
print('PASS: original ARM64 case getter returns scalar W0 sizes0..3; original UFunction stores uint32 with canary intact; actual pickup caller passes Grenade ID1, ushort quantity and returned board size to PutInCase')
