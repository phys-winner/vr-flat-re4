"""Verify original native cycle contracts and execute the actual isolated C header."""
import os
from pathlib import Path
import hashlib,mmap,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_weapon_cycle.h'
source=header.read_text(encoding='utf8')
reload_source=(root/'analysis/re4fix_reload.h').read_text(encoding='utf8')
published=re.search(r'rl_interactivity\s*=.*?base\s*\+\s*(0x[0-9a-f]+)',reload_source)
assert published and int(published[1],16)==0x628cf08
assert '(uintptr_t)rl_interactivity!=base+0x628cf08' in source
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
expected=[["0x643a7c8","0xd10183ff","0xf90023f3"],["0x643a850","0x52823404","0x52800205"],["0x8f82500","0xd10183ff","0xf90023f3"],["0x8f82588","0x52803e04","0x52800205"],["0x641baa0","0xd10183ff","0xf90023f3"],["0x641bb28","0x52805004","0x52800205"],["0x62a8e50","0xf81e0ff3","0xa9017bfd"],["0x6292c38","0xf81e0ff3","0xa9017bfd"],["0x628572c","0x9107e000","0xd65f03c0"],["0x6280060","0xd10143ff","0xf90013f5"],["0x628e55c","0x39710008","0x34000068"],["0x6281040","0xf9411809","0xb40002c9"],["0x6299ee8","0xa9be4ff4","0xa9017bfd"],["0x62ca634","0x52822c28","0x38686808"],["0x62ca660","0x52822c28","0x38686808"],["0x62c7e04","0xa9be4ff4","0xa9017bfd"],["0x629a7cc","0xf81a0ffb","0xa90167fa"],["0x633c54c","0x52800028","0x390cf408"],["0x633c558","0x390cf41f","0x1405d0a2"],["0x633d048","0x394cf400","0xd65f03c0"],["0x629a930","0xd10403ff","0x6d082beb"],["0x83a926c","0xd10303ff","0xa9085ff8"],["0x628db4c","0xfc1e0fe8","0xf90007f3"],["0x62bb880","0xf81e0ff3","0xa9017bfd"],["0x62bb584","0xf81e0ff3","0xa9017bfd"],["0x62c8ff0","0x9400022f","0x14000058"],["0x62c8fdc","0x94000521","0xbd4007e0"],["0x62c9370","0x9400001b","0x3940aee9"],["0x62c9354","0x94000443","0xbd4007e0"],["0x62c9cc8","0x39000288","0xf9400268"],["0x62c2938","0x39000289","0x3c804280"],["0x62ca3f4","0xfd004680","0xbd118661"],["0x62ca44c","0x1e203820","0xbd118660"],["0x62ca4fc","0x90020449","0xf943f929"],["0x629ab94","0x9400ca77","0xb4fff8c0"]]
fields=[["0x9db6e10","0x1160"],["0x9db6db0","0x1161"],["0x9da59a0","0xd40"],["0x9da59d0","0xd38"],["0x9db0e60","0x270"],["0x9db0b18","0x3c0"],["0x9d9e298","0x300"],["0x9db70c8","0x10f4"],["0x9db7038","0x1118"],["0x9db7098","0x1100"],["0x9db7008","0x1124"],["0x9da7038","0x460"]]
ranges=[["0x643a7c8","0x643a888","c50ed972016f69549c5bbf2ca423a08bdf6415e98325e91def2c8bfafe4c2857"],["0x641baa0","0x641bb60","2f278c1c327f29577399386bcfec142a7130f010edbb8c44064fe190ef660925"],["0x8f82500","0x8f825c0","3d6a334aee0bc8dc7b91b27c29c94d9390e26d5299f36035255bbd7440b02b07"],["0x6292c38","0x6292c74","1a0bfdefca4b2c80c323a4325c7295deb2dd123a6909bc9218258ad3cc2f01e9"],["0x6280060","0x6280170","b65572d46a9fa503af36da60fe8538c5b3376d5f03a454d1fd26834e89f81619"],["0x6281040","0x62810b8","0800d3b4dd47bdad81048c588573e6ecc4d0f4cc0823ea38e56047ae3cf6b76c"],["0x6299ee8","0x629a054","23b5c32fa6d4b714b7982b86f62e4e09372864a44a070aa110511ec5ba623980"],["0x62ca634","0x62ca660","cd8a7b2c4142e08c66171085938b6ad68e9351bd8394836c19f9bad39575b691"],["0x62ca660","0x62ca68c","b51c30c6913e728602f9b02ed05f3100f0e6f458c5e04a4dc9c536c65554f5fe"],["0x62ca460","0x62ca634","7afd1bf18f95a93aaea95a51e395101df00bd0aa69cc0d72a3755a7ffdc272c3"],["0x62ca2a8","0x62ca460","f70fa28622950ebd659bd9ec55d7af8ec993f1219582dea5df2492f27ce7d3f8"],["0x62c8e44","0x62c93dc","2cee89cb0614c5e6c43f895c882cccfc38bc233cce375781d5ed639e2603cbba"],["0x62c9a8c","0x62c9c78","3e1af4740eced6c1169b5ee12ac5873ef5375da01d69199e6b5ad4d1f713848b"],["0x62c4104","0x62c4300","892947767204c9088568670b03813d2ee47fbaea8c2f27c4590158be18cb4525"],["0x62c35e0","0x62c408c","f245087a758f077d349b878c00b0c448f51a2765b10473cadd1bb11eba0fe2bc"],["0x62c28ec","0x62c2aa0","37ee10a3457cbbe5cbfa98e257ad5959dc4f970d63ee394803c0e294ff62fb0d"],["0x62a8e50","0x62a8eec","8507ac218be413f596e1dbd26295a3a90afd945708fc482a4bdcee1265fe9c55"]]
expected.append(["0x6341aec","0xf9416c09","0xad4e8122"])
expected.append(["0x62ca338","0x52821e88","0x8b080274"])
expected.append(["0x62ca5c8","0xfc41b2f0","0x6e0c04e5"])
expected.append(["0x6296b94","0x94000cd5","0x1e2a3800"])
expected.append(["0x6296d8c","0x2a1f03e2","0x3d801be0"])
expected.extend([
 ["0x62c8e44","0xd10203ff","0x6d0123e9"],
 ["0x62c8e4c","0xf90013fb","0xa90367fa"],
 ["0x62ca460","0xd10203ff","0xf90023f7"],
])
expected.extend([["0x62ca3fc","0x39412288","0x34000068"],["0x643a798","0x52822788","0x52800029"]])
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source)
assert [list(row) for row in checks]==expected
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
 with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as native:
  assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
  assert struct.unpack_from('<II',native,0x628cf08)==(0xf81e0ff3,0xa9017bfd)
  assert struct.unpack_from('<I',native,0x628cf18)[0]==0x9400b9ad
  # Original GameHand Tick queries IsGripDown and can release the current grip
  # before the phone post-Tick callback; this is why a held input is required.
  assert struct.unpack_from('<II',native,0x6298580)==(0x940292b2,0x37000560)
  assert struct.unpack_from('<III',native,0x6298624)==(0x52800021,0xaa1303e0,0x94000bb1)
  assert 0x6298580+4*(0x940292b2&0x3ffffff)==0x633d048
  assert 0x629862c+4*(0x94000bb1&0x3ffffff)==0x629b4f0
  # Native IK GrabTick also releases on actual tracked-point distance even
  # while IsGripDown remains true. Its tracked-point query precedes release.
  assert struct.unpack_from('<I',native,0x62885e4)[0]==0x94004641
  assert struct.unpack_from('<I',native,0x62885f8)[0]==0x97ffe4d1
  assert struct.unpack_from('<I',native,0x62886ac)[0]==0x97ffe617
  assert struct.unpack_from('<II',native,0x62885d4)==(0x3947c688,0x340006c8)
  # Original primary pre-grab invokes the two-hand solver, which reads the
  # other tracked grab point and changes the gun's world ActorRotation.
  for caller,target in ((0x6296a54,0x6296b1c),(0x6296b94,0x6299ee8),(0x6296d94,0x871214c),
                        (0x62bcc64,0x8708888),(0x6213ba4,0x6298468),(0x6213c78,0x6298ec0),
                        (0x62147b8,0x64223c8),(0x62148a0,0x6298ec0),
                        (0x633aab4,0x61c833c),(0x633ab34,0x8712470),
                        (0x62c8e7c,0x628b2d8),(0x628b30c,0x62b9858),
                        (0x628b4d8,0x628b83c),(0x628b4e4,0x628c130)):
   op=struct.unpack_from('<I',native,caller)[0];assert op&0x7c000000==0x14000000,hex(caller)
   displacement=op&0x3ffffff
   if displacement&0x2000000:displacement-=0x4000000
   assert caller+4*displacement==target,hex(caller)
  assert struct.unpack_from('<I',native,0x62b9858)[0]==0xd65f03c0
  # Bio4 hand virtual target/location smoothing precedes SetActorTransform;
  # the pump's native exact endpoint query runs later in ShotgunPreBio4Tick.
  assert struct.unpack_from('<II',native,0x633aa10)==(0xf9431d09,0x910203e8)
  assert struct.unpack_from('<I',native,0x633aa38)[0]==0xf9438908
  assert struct.unpack_from('<4I',native,0x62c8e44)==(0xd10203ff,0x6d0123e9,0xf90013fb,0xa90367fa)
  # Decode the original ARM64 write/read addresses independently of the C
  # fixture and the adapter. FVector is 12 bytes, not padded to 16 bytes.
  grab_base=struct.unpack_from('<I',native,0x62ca338)[0]
  store_xy,store_z=struct.unpack_from('<II',native,0x62ca3f4)
  read_base=struct.unpack_from('<I',native,0x62ca480)[0]
  read_xy=struct.unpack_from('<I',native,0x62ca5c8)[0]
  read_z=struct.unpack_from('<I',native,0x62ca5d0)[0]
  assert (grab_base,store_xy,store_z,read_base,read_xy,read_z)==(0x52821e88,0xfd004680,0xbd118661,0x52822c29,0xfc41b2f0,0xbd518687)
  written=((grab_base>>5)&0xffff)+((store_xy>>10)&0xfff)*8
  displacement=(read_xy>>12)&0x1ff
  if displacement&0x100:displacement-=0x200
  read=((read_base>>5)&0xffff)+displacement
  assert written==read==0x117c and ((store_z>>10)&0xfff)*4==((read_z>>10)&0xfff)*4==0x1184
  for a,b,c in checks:assert struct.unpack_from('<II',native,int(a,16))==(int(b,16),int(c,16)),a
  for a,offset in fields:assert struct.unpack_from('<I',native,int(a,16)+36)[0]==int(offset,16),a
  for lo,hi,digest in ranges:assert hashlib.sha256(native[int(lo,16):int(hi,16)]).hexdigest()==digest,lo
  assert struct.unpack_from('<12f',native,0x4315110)==(0,0,0,1,0,0,0,1,1,1,0,0)
  enum_targets={0x9da2300:(0,'EChamberState::Ready'),0x9da2310:(1,'EChamberState::Spent'),0x9da2320:(2,'EChamberState::Empty'),0x9da4ef0:(0,'EForestockPosition::Front'),0x9da4f00:(1,'EForestockPosition::Back')}
  found={}
  sh=struct.unpack_from('<Q',native,40)[0];step,count=struct.unpack_from('<HH',native,58)
  for i in range(count):
   sec=struct.unpack_from('<IIQQQQIIQQ',native,sh+i*step)
   if sec[1]!=4:continue
   for off in range(sec[4],sec[4]+sec[5],sec[9]):
    addr,info,value=struct.unpack_from('<QQq',native,off)
    if addr not in enum_targets:continue
    end=native.find(b'\0',value);name=bytes(native[value:end]).decode()
    actual=struct.unpack_from('<q',native,addr+8)[0]
    assert info==0x403 and (actual,name)==enum_targets[addr];found[addr]=(actual,name)
  assert found==enum_targets
# Only normal grip input and the left Scene setter; no raw state, fire or direct pump/grab calls.
assert not re.search(r'base\s*\+\s*0x(?:62c9f74|62c9da4|62c93dc|62c98ac|629aec4|6281ce4|628dc70|62c3494)',source)
assert 'rl_world_transform(b->object[6],&pose,0,0,0)' in source
assert 'rl_world_transform(b->object[7]' not in source
assert 'fraction=t>=1.0f ? 1.02f:t' in source
assert '+0x117c+4*i' in source and '+0x1180+4*i' not in source
assert 'ti_best(hand)!=binding.object[5]' in source
assert 'wc_current(&binding)) {wc_finish(hand,1);return;}' in source
print('PASS:43 original opcode pairs,12 metadata fields,5 relocated enums,17 complete native functions and scalarTF48; packed FVector117c..1184, native Bio4 hand smoothing/two-hand rotation before exact PreBio endpoint query',flush=True)
test=root/'analysis/research/test_weapon_cycle.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
print('PASS: actual isolated header/test Android aarch64 -Werror syntax',flush=True)
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
with tempfile.TemporaryDirectory(prefix='re4-cycle-check-') as temporary:
 fixture=Path(temporary)/'native_fixture.h'
 fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_cycle_install_guard(void) {
 unsigned char* image=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);assert(image);
 static const struct {unsigned addr,words[2];} code[]={
'''+''.join('  {%s,{%s,%s}},\n'%row for row in checks)+r''' };
 static const struct {unsigned addr,offset;} fields[]={
'''+''.join('  {%s,%s},\n'%tuple(row) for row in fields)+r''' };
 static const struct {unsigned addr;long long value;const char* name;} enums[]={
  {0x9da2300,0,"EChamberState::Ready"},{0x9da2310,1,"EChamberState::Spent"},{0x9da2320,2,"EChamberState::Empty"},
  {0x9da4ef0,0,"EForestockPosition::Front"},{0x9da4f00,1,"EForestockPosition::Back"},
 };
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].addr,code[i].words,8);
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)*(unsigned*)(image+fields[i].addr+36)=fields[i].offset;
 for(unsigned i=0;i<sizeof enums/sizeof *enums;i++){*(const char**)(image+enums[i].addr)=enums[i].name;*(long long*)(image+enums[i].addr+8)=enums[i].value;}
 unsigned long base=(unsigned long)(uintptr_t)image;
 static const struct {void* slot;unsigned rva;} dependencies[]={
  {&tw_hand_class,0x6424b5c},{&tw_pawn_class,0x642c718},{&tw_grip_class,0x64192f4},{&ti_best,0x629a930},
  {&ti_press,0x633c54c},{&ti_release,0x633c558},{&ti_down,0x633d048},{&ti_free,0x627feb8},
  {&ti_parent,0x6280170},{&f_SetGripForcedHover,0x629a7cc},{&f_GetCurrentGrip,0x629a774},{&rl_primary,0x62bbc30},
  {&rl_world_transform,0x83a926c},{&rl_left,0x62a8db4},{&rl_interactivity,0x628cf08},{&rl_reloading,0x628c410},
  {&rl_paused,0x61d0934},{&rl_player_valid,0x61d401c},
 };
 for(unsigned i=0;i<sizeof dependencies/sizeof *dependencies;i++){void* pointer=image+dependencies[i].rva;memcpy(dependencies[i].slot,&pointer,sizeof pointer);}
 ti_ready=1;install_touch_weapon_cycle(base);assert(wc_shotgun_class&&wc_scene_class&&wc_group_class);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned word=0;word<2;word++){
  *(unsigned*)(image+code[i].addr+4*word)^=1;install_touch_weapon_cycle(base);assert(!wc_shotgun_class);*(unsigned*)(image+code[i].addr+4*word)^=1;
 }
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
  *(unsigned*)(image+fields[i].addr+36)^=1;install_touch_weapon_cycle(base);assert(!wc_shotgun_class);*(unsigned*)(image+fields[i].addr+36)^=1;
 }
 for(unsigned i=0;i<sizeof enums/sizeof *enums;i++){
  *(long long*)(image+enums[i].addr+8)^=1;install_touch_weapon_cycle(base);assert(!wc_shotgun_class);*(long long*)(image+enums[i].addr+8)^=1;
  *(const char**)(image+enums[i].addr)=0;install_touch_weapon_cycle(base);assert(!wc_shotgun_class);
  *(const char**)(image+enums[i].addr)="bad";install_touch_weapon_cycle(base);assert(!wc_shotgun_class);*(const char**)(image+enums[i].addr)=enums[i].name;
 }
 for(unsigned i=0;i<sizeof dependencies/sizeof *dependencies;i++){
  void* zero=0;memcpy(dependencies[i].slot,&zero,sizeof zero);install_touch_weapon_cycle(base);assert(!wc_shotgun_class);
  void* pointer=image+dependencies[i].rva;memcpy(dependencies[i].slot,&pointer,sizeof pointer);
 }
 rl_interactivity=(int (*)(const void*))(image+0x62bb5cc);install_touch_weapon_cycle(base);assert(!wc_shotgun_class);
 rl_interactivity=(int (*)(const void*))(image+0x628cf08);install_touch_weapon_cycle(base);assert(wc_shotgun_class);
 ti_ready=0;install_touch_weapon_cycle(base);assert(!wc_shotgun_class);ti_ready=1;install_touch_weapon_cycle(base);assert(wc_shotgun_class);
 fixture_patch_failure=1;install_touch_weapon_cycle(base);assert(!wc_shotgun_class&&!wc_input_plan);fixture_patch_failure=0;
 install_touch_weapon_cycle(base);assert(wc_shotgun_class&&g_tgt_wc_shotgun_pre==base+0x62c8e44+16);
 assert(VirtualFree(image,0,0x8000));
 puts("PASS: actual installer rejects86 corrupted instructions/12 fields/15 enum corruptions/18 missing dependencies and failed native pre-tick hook");
}
''',encoding='utf8')
 obj,exe=Path(temporary)/'cycle.obj',Path(temporary)/'cycle.exe'
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
  '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),
  '-DCYCLE_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-c','-o',str(obj),str(test)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
  f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
 # A post-hand-only placement must fail this same C regression: the native
 # two-hand actor rotation occurs between placement and the exact query.
 for label,old,new in (
  ('old_hand_tick_only','if(wc_shotgun_class)wc_native_pre_pose(gun,ptr_now_ms());','(void)gun;'),
  ('early_endpoint','float fraction=t>=1.0f ? 1.02f:t;','float fraction=1.02f*t;'),
  ('instant_hand_return','float t=(float)(now-start)/220.0f;','float t=1.0f;'),
 ):
  assert source.count(old)==1
  candidate=Path(temporary)/(label+'.h');candidate.write_text(source.replace(old,new),encoding='utf8')
  badtest=Path(temporary)/(label+'.c')
  badtest.write_text(test.read_text(encoding='utf8').replace('#include "../re4fix_weapon_cycle.h"','#include "'+candidate.as_posix()+'"'),encoding='utf8')
  subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
   '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(badtest)],check=True)
  subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
   f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True)
  assert result.returncode!=0 and 'Assertion failed:' in result.stderr+result.stdout,(label,result.stdout,result.stderr)
  print('PASS: unchanged independent native-frame fixture rejects '+label,flush=True)
