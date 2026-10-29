"""Verify original grenade contracts and execute the actual C helper."""
import os
from pathlib import Path
import hashlib,mmap,re,struct,subprocess,tempfile,json
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_grenade.h';source=header.read_text(encoding='utf8')
proof=json.loads(r'''{"pairs":[["0x6414d98","0xd10183ff","0xf90023f3"],["0x6414e20","0x5280c804","0x52800205"],["0x627c69c","0xa9bf7bfd","0x910003fd"],["0x627ea7c","0xa9be4ff4","0xa9017bfd"],["0x627d4ac","0xa9bf7bfd","0x910003fd"],["0x627cae8","0xf81e0ff3","0xa9017bfd"],["0x627d97c","0x6dbe23e9","0xa9017bfd"],["0x627c6dc","0xf9427400","0x140017b4"],["0x627b44c","0xd10303ff","0xfd002bec"],["0x627b538","0x940306c4","0x1e2703ea"],["0x627b544","0xbd460260","0x1e28280a"],["0x627b54c","0xbd454a6b","0xaa1403e0"],["0x627b55c","0x1e2b2140","0x1a9fd7f9"],["0x627b564","0x9402fe51","0x2d0707e0"],["0x627b5ac","0x4ea81d00","0xaa1603e1"],["0x627b918","0x3957c668","0x34000348"],["0x627b920","0x3957be68","0x340001e8"],["0x627b928","0x3957ba68","0x7100091f"],["0x627b954","0x9401312c","0x39410008"],["0x627b96c","0xbd454660","0xbd45fe61"],["0x627b980","0x94000198","0x3917c679"],["0x627c3c8","0x940000c7","0xf9431a60"],["0x61ce740","0xf81e0ff3","0xa9017bfd"],["0x61ce778","0x2d420001","0x1e212040"],["0x61ce7bc","0xbd400a61","0x1e212000"],["0x633d048","0x394cf400","0xd65f03c0"],["0x633c558","0x390cf41f","0x1405d0a2"],["0x627c090","0xbd454e69","0xaa1403e0"]],"fields":[["0x9d9ae38","0x548"],["0x9d9ae10","0x54c"],["0x9d9ade8","0x550"],["0x9d9acf8","0x580"],["0x9d50ee0","0x0"],["0x9d50eb8","0x4"],["0x9d50e90","0x8"],["0x9d50e68","0xc"],["0x9d50e40","0x10"],["0x9d9af80","0x520"],["0x9d9af58","0x524"],["0x9db5890","0x40"]],"ranges":[["0x6414d98","0x6414e58","0efa62fc0abfafe31a9413d5dcc364121ee87eb843cf989c8d8e56161ec2109f"],["0x627ab4c","0x627adb8","23935b72743abdce45217da1f9d6654beb121d18857b8adb341d8b8570984bba"],["0x627b44c","0x627bc74","4f5fcbd3341f6ee47dcf99051f0cefba7b1e7446dd48240a6aa09f902d5ed0fa"],["0x627bfe0","0x627c69c","fa0567dc72f233bc64d58c92e610d69b9835af28bc0ad305e23636c03844515d"],["0x627c69c","0x627c6dc","2cba8b5b32e4ccb0c8f78cc686f6ca722e67e58e2104a3d84015ff76eae5917b"],["0x61ce740","0x61ce800","b70bf7ed7c8a8ff30ccba65bf960ded356f4e567be89b2402bfd6cb9734a647f"],["0x61c833c","0x61c83b4","93a8b7d12c1f9862d69988176dbfbf68cdfce00b731dc241a198661643f7150b"],["0x627d97c","0x627da14","69012d6062fa7203fea2c8f7dad3e4b7e162d35e81f44cddddbf8c10afce607d"],["0x627ea7c","0x627eb1c","d3bf2c82921e46b9d2bd44b76b40e459ce9b3f975622f6341dbb8b50389f2233"]],"enums":[["0x9d5e808",0,"EGrenadeStyle::Default"],["0x9d5e818",1,"EGrenadeStyle::Manual"],["0x9d5e828",2,"EGrenadeStyle::Count"],["0x9d998e0",0,"EGrenadeState::NoAmmo"],["0x9d998f0",1,"EGrenadeState::Assigned"],["0x9d99900",2,"EGrenadeState::Ready"],["0x9d99910",3,"EGrenadeState::Cooking"]],"deps":[["tw_hand_class","0x6424b5c"],["tw_pawn_class","0x642c718"],["tw_grip_class","0x64192f4"],["phone_player","0x633d030"],["phone_active","0x63380dc"],["phone_type","0x6338aec"],["rl_primary","0x62bbc30"],["rl_get_pawn","0x62bbd24"],["rl_left","0x62a8db4"],["rl_right","0x62a8e50"],["rl_paused","0x61d0934"],["rl_player_valid","0x61d401c"],["f_GetCurrentGrip","0x629a774"],["ti_parent","0x6280170"],["ti_down","0x633d048"],["ti_release","0x633c558"],["qs_prop","0x62a7ad4"],["qs_hand","0x62a7b34"],["qs_settings","0x62c7e04"],["qs_context","0x62bc710"],["qs_copy","0x5fe1978"],["qs_set","0x62c80d4"],["qs_destroy","0x62c7fd0"]]}''')
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source);assert [list(v) for v in checks]==proof['pairs']
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
 with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as n:
  assert hashlib.sha256(n).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
  for a,b,c in checks:assert struct.unpack_from('<II',n,int(a,16))==(int(b,16),int(c,16)),a
  for a,b in proof['fields']:assert struct.unpack_from('<I',n,int(a,16)+36)[0]==int(b,16),a
  for lo,hi,digest in proof['ranges']:assert hashlib.sha256(n[int(lo,16):int(hi,16)]).hexdigest()==digest,lo
  targets={int(a,16):(value,name) for a,value,name in proof['enums']};found={}
  sh=struct.unpack_from('<Q',n,40)[0];step,count=struct.unpack_from('<HH',n,58)
  for i in range(count):
   sec=struct.unpack_from('<IIQQQQIIQQ',n,sh+i*step)
   if sec[1]!=4:continue
   for off in range(sec[4],sec[4]+sec[5],sec[9]):
    addr,info,value=struct.unpack_from('<QQq',n,off)
    if addr in targets:
     assert info==0x403;name=n[value:n.find(b'\0',value)].decode();found[addr]=(struct.unpack_from('<q',n,addr+8)[0],name)
  assert found==targets
assert not re.search(r'base\s*\+\s*0x(?:627bfe0|627c6e4|627d358|6281ce4|629aec4)',source)
main=(root/'analysis/re4fix.c').read_text(encoding='utf8')
for name,mask in [('RT','04000000'),('RG','08000000'),('LT','10000000'),('LG','20000000')]:assert re.search(r'\{"'+name+r'",\s*0x'+mask+r'u\}',main)
assert 'gr_run.side?27:29' in source and 'settings[0x40]==0' in source
print('PASS: original grenade28 opcode pairs/12 full uint32 offsets/9 native bodies/7 enum RELAs and original ELF',flush=True)
capture=json.loads((root/'analysis/research/grenade_held_pose_capture.json').read_text())
assert capture['native_sha256']=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
assert capture['surface']==[2400,1080] and capture['horizontal_fov']==90
assert all(abs(a-b)<.00001 for a,b in zip(capture['values']['model_from_hand_ovr_m'],[-.012100123,-.035,-.005001193]))
center=capture['values']['model_from_head_ovr_m']
assert -center[1]/(-center[2]/(2400/1080))>1.7
test=root/'analysis/research/test_grenade.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231');sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
with tempfile.TemporaryDirectory(prefix='re4-grenade-check-') as temporary:
 fixture=Path(temporary)/'native_fixture.h'
 fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_grenade_install_guard(void){
 unsigned char* image=VirtualAlloc((void*)0x20000000,0xa000000,0x3000,4);assert(image);
 static const struct{unsigned addr,words[2];} code[]={
'''+''.join(' {%s,{%s,%s}},\n'%tuple(row) for row in proof['pairs'])+r'''};
 static const struct{unsigned addr,offset;} fields[]={
'''+''.join(' {%s,%s},\n'%tuple(row) for row in proof['fields'])+r'''};
 static const struct{unsigned addr;long long value;const char* name;} enums[]={
'''+''.join(' {%s,%s,"%s"},\n'%tuple(row) for row in proof['enums'])+r'''};
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].addr,code[i].words,8);
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)*(unsigned*)(image+fields[i].addr+36)=fields[i].offset;
 for(unsigned i=0;i<sizeof enums/sizeof *enums;i++){*(const char**)(image+enums[i].addr)=enums[i].name;*(long long*)(image+enums[i].addr+8)=enums[i].value;}
 static const struct{void* slot;unsigned rva;} deps[]={
'''+''.join(' {&%s,%s},\n'%tuple(row) for row in proof['deps'])+r'''};
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){void* p=image+deps[i].rva;memcpy(deps[i].slot,&p,sizeof p);}
 ti_ready=qs_ready=1;unsigned long base=(unsigned long)(uintptr_t)image;install_touch_grenade(base);assert(gr_installed&&gr_class);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned word=0;word<2;word++){
  *(unsigned*)(image+code[i].addr+4*word)^=1;install_touch_grenade(base);assert(!gr_installed&&!gr_class);*(unsigned*)(image+code[i].addr+4*word)^=1;
 }
 for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){
  *(unsigned*)(image+fields[i].addr+36)^=1;install_touch_grenade(base);assert(!gr_installed);*(unsigned*)(image+fields[i].addr+36)^=1;
 }
 for(unsigned i=0;i<sizeof enums/sizeof *enums;i++){
  *(long long*)(image+enums[i].addr+8)^=1;install_touch_grenade(base);assert(!gr_installed);*(long long*)(image+enums[i].addr+8)^=1;
  *(const char**)(image+enums[i].addr)=0;install_touch_grenade(base);assert(!gr_installed);
  *(const char**)(image+enums[i].addr)="wrong";install_touch_grenade(base);assert(!gr_installed);*(const char**)(image+enums[i].addr)=enums[i].name;
 }
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){
  void* zero=0;memcpy(deps[i].slot,&zero,sizeof zero);install_touch_grenade(base);assert(!gr_installed);
  void* p=image+deps[i].rva;memcpy(deps[i].slot,&p,sizeof p);
 }
 ti_ready=0;install_touch_grenade(base);assert(!gr_installed);ti_ready=1;qs_ready=0;install_touch_grenade(base);assert(!gr_installed);qs_ready=1;
 install_touch_grenade(base);assert(gr_installed);assert(VirtualFree(image,0,0x8000));puts("PASS: actual grenade installer refuses56 changed instructions/12 offsets/21 enum mutations/23 missing dependencies");
}
''',encoding='utf8')
 obj,exe=Path(temporary)/'grenade.obj',Path(temporary)/'grenade.exe'
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
  '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-DGRENADE_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-c','-o',str(obj),str(test)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
  f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: actual grenade header fixture Android/host -Werror',flush=True)
