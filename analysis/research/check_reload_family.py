"""Check original native family seams; compile and execute the actual C adapter."""
import os
from pathlib import Path
import hashlib,json,mmap,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_reload_family.h';source=header.read_text(encoding='utf8')
reload_source=(root/'analysis/re4fix_reload.h').read_text(encoding='utf8')
shotgun=root/'analysis/research/shotgun_ui_originals'
package=json.loads((shotgun/'VR4GamePlayerAmmo_Wep07_BP.package.json').read_text())
body=(shotgun/'VR4GamePlayerAmmo_Wep07_BP.uexp').read_bytes()
assert hashlib.sha256(body).hexdigest()==package['body_sha256']
default=next(item for item in package['exports'] if item['name']=='Default__VR4GamePlayerAmmo_Wep07_BP_C')
data=body[default['uexp_offset']:default['uexp_offset']+default['serial_size']]
for name,seconds in (('ReloadDuration',.24),('ReloadFullyBlendedTime',.02),('PostReloadLingerDuration',.13)):
 marker=struct.pack('<II',package['names'].index(name),0);at=data.index(marker)
 assert data.count(marker)==1 and package['names'][struct.unpack_from('<I',data,at+8)[0]]=='FloatProperty'
 assert struct.unpack_from('<III',data,at+12)==(0,4,0) and data[at+24]==0
 assert abs(struct.unpack_from('<f',data,at+25)[0]-seconds)<1e-6
published=re.search(r'rl_interactivity\s*=.*?base\s*\+\s*(0x[0-9a-f]+)',reload_source)
assert published and int(published[1],16)==0x628cf08
assert '(uintptr_t)rl_interactivity!=base+0x628cf08' in source
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
expected=[["0x6438a30","0xd10183ff","0xf90023f3"],["0x6435f44","0xd10183ff","0xf90023f3"],["0x6427294","0xd10183ff","0xf90023f3"],["0x6434e34","0xd10183ff","0xf90023f3"],["0x643a7c8","0xd10183ff","0xf90023f3"],["0x643bd4c","0xd10183ff","0xf90023f3"],["0x6438ab8","0x52822204","0x52800205"],["0x6435fcc","0x52824204","0x52800205"],["0x642731c","0x52822a04","0x52800205"],["0x6434ebc","0x5281ca04","0x52800205"],["0x643a850","0x52823404","0x52800205"],["0x643bdd4","0x52821a04","0x52800205"],["0x8f4e5bc","0xd10183ff","0xf90023f3"],["0x8f4e644","0x5280f404","0x52800205"],["0x8f82500","0xd10183ff","0xf90023f3"],["0x62a8e50","0xf81e0ff3","0xa9017bfd"],["0x62c7790","0x52821e08","0x38686808"],["0x62c40ec","0x52823908","0x38686808"],["0x62a1dc4","0x52822388","0x38686808"],["0x62c23a4","0x39790008","0x34000068"],["0x62c9c78","0x52822c28","0x38686808"],["0x62cbf98","0x52821988","0x38686808"],["0x6291744","0xa9be4ff4","0xa9017bfd"],["0x629182c","0xf81e0ff3","0xa9017bfd"],["0x62918d4","0xf81d0ff5","0xa9014ff4"],["0x6280060","0xd10143ff","0xf90013f5"],["0x628e55c","0x39710008","0x34000068"],["0x62bb880","0xf81e0ff3","0xa9017bfd"],["0x62bb584","0xf81e0ff3","0xa9017bfd"],["0x628db4c","0xfc1e0fe8","0xf90007f3"],["0x62c763c","0xf9486400","0x17fef900"],["0x62c3588","0xf9486400","0x17fefc09"],["0x62a1ca4","0xf9486400","0x17ff8242"],["0x62825b0","0xf9417c08","0xb4000268"],["0x83c6310","0xd10643ff","0xfd0083ee"],["0x6281040","0xf9411809","0xb40002c9"],["0x6299ee8","0xa9be4ff4","0xa9017bfd"],["0x62c7e04","0xa9be4ff4","0xa9017bfd"],["0x62c7614","0x52800028","0xaa1303e0"],["0x62c7620","0x97ff1061","0x1e2703e0"],["0x62c3170","0x97ff218d","0x52824088"],["0x62c3dbc","0xaa1303e0","0x9405c6c7"],["0x62a1b18","0x390022e8","0x97ffa722"],["0x62c1fc8","0x1e202168","0x1e200981"],["0x62c2004","0x94000004","0x52800035"],["0x62a1678","0xfd000140","0xbd000941"],["0x62a1694","0xd63f0120","0xf9400be8"],["0x83a926c","0xd10303ff","0xa9085ff8"],["0x629a7cc","0xf81a0ffb","0xa90167fa"],["0x633c54c","0x52800028","0x390cf408"],["0x633c558","0x390cf41f","0x1405d0a2"]]
fields=[["0x9da58e0","0xd60"],["0x9db2d78","0xd38"],["0x9da8918","0x10c8"],["0x9db2b30","0xda0"],["0x9db3ac0","0x10c8"],["0x9db4e50","0x10c8"],["0x9da88c8","0x10d8"],["0x9da88a0","0x10e0"],["0x9da8878","0x10e8"],["0x9da7038","0x460"]]
ranges=[["0x6438a30","0x6438af0","4d2a9901bd8f34b3fe6eae9570b99284bbff1bb67c5199f1bab16c96be285f0a"],["0x6435f44","0x6436004","35f27a4b47534b9bea316ddd70ef1fadb45f812f7b4d747071b0815ff0eb3439"],["0x6427294","0x6427354","f72160c21a21b1adc6a07bda01e6de94a9762a3d539153b6c22304dedb2c92f9"],["0x6434e34","0x6434ef4","7d64366a97edf7821d1ff5f7ecad2f446c5eee9fea81af924db71045bbed58c5"],["0x643a7c8","0x643a888","c50ed972016f69549c5bbf2ca423a08bdf6415e98325e91def2c8bfafe4c2857"],["0x643bd4c","0x643be0c","681f9198414ac21dd4d0940011cf89f65813f772b174f49bdc3a528d024f9e78"],["0x8f4e5bc","0x8f4e67c","272f733aa0b1197c486842a2845723d9483373387ae819950b358eaf652ef10f"],["0x62c7198","0x62c763c","c0bb8d72f56c2bc510df107fe9901447e353085af0827fb917f0aa1c3e90facc"],["0x62c2f7c","0x62c3494","501816b4db1a92a3fbb00d9002a6db9321d8b48e4272834dddfb8cd844319797"],["0x62a16cc","0x62a1ca4","502f37b3504fa38c74781e76ca6c75f0adccf427daffde8c1477923bca80f641"],["0x62c1a9c","0x62c2014","338edad5e24ccb64836f37d6dee21dcdbb92415c1e8b83162e70ed7e6cc13fa5"],["0x62c35e0","0x62c408c","f245087a758f077d349b878c00b0c448f51a2765b10473cadd1bb11eba0fe2bc"],["0x62a1534","0x62a16cc","b6ae9deabc5310397ef2b7587202405c24b5f135ea4c340219798300f069c648"],["0x62c7790","0x62c77a8","52b5eae1b7df935b6f9cd4faaf8be2fa99be26634ce8e37bdcd738a5a283387c"],["0x62c40ec","0x62c4104","8d3b5da8fac316a16e74157a5d8c05f36c13a9e9a9d530d52f8122e6a8bdb7e8"],["0x62a1dc4","0x62a1ddc","1139c8c84561db2e6f24b284c9bb9c342b81939a76033b19fdce90cc8acb509c"],["0x62c23a4","0x62c23c4","43675d242dfb5d56c8754005db8d45fee47e7f0553ea986a445dd6e927d41826"],["0x62c9c78","0x62c9ca0","9f3808b81b7baccf0bf683b37d26825ef6709936314ace33d7ca35fa1989b473"],["0x62cbf98","0x62cbfb0","8cd42dd7b0e5ab93f895b0c74b4fc3e15b1ebbb5ae2f0d37a1f0c974435bb859"],["0x62ca018","0x62ca0e0","9b11b3a20bb5e1d7e63a715005c21c9d98c5faa7e5220736d29ba63776749efc"],["0x62cbfb0","0x62cc010","189ddb04e961556010224e0a85b4de86c85f844c964f8b61fa26ed3410c1618f"],["0x62a1e04","0x62a1e38","303b205176011f08107be35165391f915f3d62c5bc4d17dedf830507726199d6"],["0x62c2494","0x62c24e8","2f2e05383d1fdbc2273d87e6a50916bf0491c94ece4d0c2e4f5c565ffdbf0bc2"],["0x62c4708","0x62c47b0","9b249bafc07aa5b556083ec12a3cbd4c9314a16bed8f6d04be8afbdd65b041d9"],["0x62c77a8","0x62c77dc","d95e2c895005f2627f565f7da3316825ed555ef1f7f325d426beebb842ec07d4"]]
expected += [["0x62ca018","0xa9be4ff4","0xa9017bfd"],["0x62ca020","0x910043fd","0x52822608"],
 ["0x62cbfb0","0xf81e0ff3","0xa9017bfd"],["0x62cbfb8","0x910043fd","0x528218a8"],
 ["0x643a7a8","0x52822608","0x52800029"],["0x643a7b0","0x38286809","0xd65f03c0"],
 ["0x643bd3c","0x528218a8","0x52800029"],["0x643bd44","0x38286809","0xd65f03c0"]]
fields += [["0x9db6fa8","0x1134"],["0x9db7968","0x10c8"]]
ranges.append(["0x6272068","0x6272940","6747ba2d8f28c103a8736ab34b01d5fe3d274e4f086850aaa0c6fde6f4012c51"])
ranges.append(["0x62c8bac","0x62c8d10","65d66fe064b65bc1ea6dfc769725775612b35a5c93067d27817fc22e0413b1a2"])
ranges.append(["0x62cbc88","0x62cbce0","ce4886321f4a3cc45222b36626e0c295676957f84713490c3023dceebbb996fb"])
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source);assert [list(v) for v in checks]==expected
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
 with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as n:
  assert hashlib.sha256(n).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
  assert struct.unpack_from('<II',n,0x628cf08)==(0xf81e0ff3,0xa9017bfd)
  assert struct.unpack_from('<I',n,0x628cf18)[0]==0x9400b9ad
  for a,b,c in checks:assert struct.unpack_from('<II',n,int(a,16))==(int(b,16),int(c,16)),a
  for a,b in fields:assert struct.unpack_from('<I',n,int(a,16)+36)[0]==int(b,16),a
  for lo,hi,digest in ranges:assert hashlib.sha256(n[int(lo,16):int(hi,16)]).hexdigest()==digest,lo
  syms=(root/'analysis/ue4_syms.txt').read_text(encoding='utf8')
  names=['SemiAutoRifle','Rifle','MachineGun','Revolver','Shotgun','Striker']
  methods=[0x62c7790,0x62c40ec,0x62a1dc4,0x62c23a4,0x62c9c78,0x62cbf98]
  vtables={int(re.search(r'^([0-9a-f]+) D vtable for AVR4GamePlayer'+name+r'$',syms,re.M)[1],16)+16+0x778:method for name,method in zip(names,methods)}
  found={};sh=struct.unpack_from('<Q',n,40)[0];step,count=struct.unpack_from('<HH',n,58)
  for i in range(count):
   sec=struct.unpack_from('<IIQQQQIIQQ',n,sh+i*step)
   if sec[1]!=4:continue
   for off in range(sec[4],sec[4]+sec[5],sec[9]):
    addr,info,value=struct.unpack_from('<QQq',n,off)
    if addr in vtables:assert info==0x403;found[addr]=value
  assert found==vtables
assert not re.search(r'base\s*\+\s*0x(?:628dc70|629aec4|6281ce4|62a1cac|62c7644|62c3494|62c2014|62c9da4|62c9f74)',source)
assert 'rf_post.kind==3' in source and 'rf_machine_pose(now,0)' in source
assert 'rf_post.kind==4' in source and 'rf_revolver_pose(now)' in source
assert 'rf_post.trigger_bit=rf_post.side?26:28' in source
assert 'unsigned side=*(const unsigned char*)((const char*)rf_post.binding.object[0]' not in source
main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
for name,mask in [('RT','04000000'),('RG','08000000'),('LT','10000000'),('LG','20000000')]:
 assert re.search(r'\{"'+name+r'",\s*0x'+mask+r'u\}',main)
assert 'int bit=side?27:29' in source and 'angle>180.0f' in source
assert 'rf_post.stage=0' in source and 'rf_post.stage=3' in source and 'rf_post.stage=4' in source
print(f'PASS:{len(checks)} original opcode pairs/{len(fields)} uint32 metadata/{len(ranges)} complete native bodies/six actual ready vtables and original ELF',flush=True)
test=root/'analysis/research/test_reload_family.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231');sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
with tempfile.TemporaryDirectory(prefix='re4-family-check-') as temporary:
 fixture=Path(temporary)/'native_fixture.h'
 fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_reload_family_install_guard(void){
 unsigned char* image=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);assert(image);
 static const struct{unsigned addr,words[2];} code[]={
'''+''.join(' {%s,{%s,%s}},\n'%row for row in checks)+r'''};
 static const struct{unsigned addr,offset;} fields[]={
'''+''.join(' {%s,%s},\n'%tuple(row) for row in fields)+r'''};
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].addr,code[i].words,8);
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)*(unsigned*)(image+fields[i].addr+36)=fields[i].offset;
 static const struct{void* slot;unsigned rva;} deps[]={
 {&rl_hand_class,0x6424b5c},{&rl_pawn_class,0x642c718},{&rl_grip_class,0x64192f4},{&rl_ammo_class,0x640dea8},
 {&rl_player,0x633d030},{&rl_active,0x63380dc},{&rl_left,0x62a8db4},{&rl_primary,0x62bbc30},
 {&rl_get_pawn,0x62bbd24},{&rl_paused,0x61d0934},{&rl_player_valid,0x61d401c},{&rl_interactivity,0x628cf08},
 {&rl_reloading,0x628c410},{&rl_world_transform,0x83a926c},{&rl_base_ready,0x628d268},{&rl_trigger,0x628d978},
 {&rl_held_ammo_ready,0x628e090},{&f_GetCurrentGrip,0x629a774},{&f_SetGripForcedHover,0x629a7cc},{&ti_parent,0x6280170},
 {&ti_free,0x627feb8},{&ti_best,0x629a930},{&ti_press,0x633c54c},{&ti_release,0x633c558},{&ti_down,0x633d048}
 };
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){void* p=image+deps[i].rva;memcpy(deps[i].slot,&p,sizeof p);}
 ti_ready=1;unsigned long base=(unsigned long)(uintptr_t)image;install_touch_reload_family(base);for(int i=0;i<6;i++)assert(rf_classes[i]&&rf_ready[i]);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned word=0;word<2;word++){
  *(unsigned*)(image+code[i].addr+4*word)^=1;install_touch_reload_family(base);for(int j=0;j<6;j++)assert(!rf_classes[j]&&!rf_ready[j]);*(unsigned*)(image+code[i].addr+4*word)^=1;
 }
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
  *(unsigned*)(image+fields[i].addr+36)^=1;install_touch_reload_family(base);assert(!rf_classes[0]);*(unsigned*)(image+fields[i].addr+36)^=1;
 }
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){
  void* zero=0;memcpy(deps[i].slot,&zero,sizeof zero);install_touch_reload_family(base);assert(!rf_classes[0]);
  void* p=image+deps[i].rva;memcpy(deps[i].slot,&p,sizeof p);
 }
 rl_interactivity=(int (*)(const void*))(image+0x62bb5cc);install_touch_reload_family(base);assert(!rf_classes[0]);
 rl_interactivity=(int (*)(const void*))(image+0x628cf08);install_touch_reload_family(base);assert(rf_classes[0]);
 ti_ready=0;install_touch_reload_family(base);assert(!rf_classes[0]);ti_ready=1;install_touch_reload_family(base);assert(rf_classes[0]);
 assert(VirtualFree(image,0,0x8000));puts("PASS: actual family installer rejects118 instruction/12 field/25 dependency corruptions, publishes all six only after guards");
}
''',encoding='utf8')
 obj,exe=Path(temporary)/'family.obj',Path(temporary)/'family.exe'
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
  '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-DFAMILY_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-c','-o',str(obj),str(test)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
  f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: actual header and fixture Android/host -Werror',flush=True)
