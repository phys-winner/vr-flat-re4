"""Verify the original Ashley receiver and execute the actual adapter with shared guards."""
import os
from pathlib import Path
import hashlib,mmap,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=root/'analysis/re4fix_ashley.h'
source=header.read_text(encoding='utf8')
checks=re.findall(r'\{(0x[0-9a-f]+),\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}\}',source)
assert len(checks)==16
spans=(
 (0x621a6d4,0x621a788,'54b4d0ea58c363b85f84e826591566d5da6b097c43ce7f2ec4622d16d65549e3'),
 (0x621a968,0x621a9c8,'e6ade95e157d3abd822a236fa1bdc60109378f36695a8bac06331a2424bad99c'),
 (0x621a9c8,0x621aa1c,'ec8ae58099bb0818938b0763b28d1fa1cdb06e03dba484551d98a61f643ebbd0'),
 (0x621ee50,0x621eeac,'7727dca0ca22cef0ab5633649ba4a64459f4724ca6ff377c0e745b71ad5cd8fd'),
 (0x621eeec,0x621ef18,'c9696413863482dfe3abdc40ff986a867609f76127a53c8c64142efef170da16'),
 (0x621ef18,0x621ef50,'9628c8b48f1b34df362124f2b43be3ead25cbab0c7321b5f2b4e1b0bd2ca3536'),
 (0x621ef50,0x621f08c,'ca828517de2d4cca0d08917d623b75fb512001d97824b00d2b03c5a7d0e995e1'),
 (0x63c6b34,0x63c6b48,'ad096ca05866da304bf2b5849c418b27b5903ad2155d2f2dfd4c8921c0c7e1c1'),
 (0x6216efc,0x6216f2c,'2c68852cce55403813dd70b928e4ef3d78a9c3f07c89b38c068a91e458a8e789'),
 (0x5fb1770,0x5fb17e0,'3a40a926f1cd5f11c0f5b8f6d5d17e3d04ec27922575b6a16f3621f914607fee'),
 (0x6241018,0x6241054,'e82240229086a6676307c6b3cb0b5a2f3117a7aa013ed59194f4aa8a85f4dfc9'),
 (0x62dd310,0x62dd344,'a008606cc612373ff2d7f65b86d22e1962d7e8a985f3a07781e9ccc57e9ab045'),
 (0x621984c,0x62198b0,'e58cd6917c5cb46a4bd9e4301a8385d4aa79466d3a7c3e4b3b3020a243d68388'),
)
with (root/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as stream:
 with mmap.mmap(stream.fileno(),0,access=mmap.ACCESS_READ) as n:
  assert hashlib.sha256(n).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
  for a,w0,w1 in checks:assert struct.unpack_from('<II',n,int(a,16))==(int(w0,16),int(w1,16)),a
  for a,b,digest in spans:assert hashlib.sha256(n[a:b]).hexdigest()==digest,hex(a)
  assert struct.unpack_from('<I',n,0x9d703f0+40)[0]==2
  sh=struct.unpack_from('<Q',n,40)[0];step,count=struct.unpack_from('<HH',n,58)
  targets={0xa361e48:0xa4e9b48,0xa361d78:0xa4e9b40,0x9d70410:0x9d703d0,
           0x9d703d0:0x42486ee,0x9d703e0:0x42a98c9};found={}
  for i in range(count):
   sec=struct.unpack_from('<IIQQQQIIQQ',n,sh+i*step)
   if sec[1]!=4:continue
   for off in range(sec[4],sec[4]+sec[5],sec[9]):
    a,info,value=struct.unpack_from('<QQq',n,off)
    if a in targets:assert info==0x403;found[a]=value
  assert found==targets
  for i,name in enumerate(('EAshleyCommand::Follow','EAshleyCommand::Stay')):
   entry=0x9d703d0+16*i;address=targets[entry]
   assert n[address:n.find(b'\0',address)].decode()==name and struct.unpack_from('<q',n,entry+8)[0]==i
  def branch(a):
   word=struct.unpack_from('<I',n,a)[0];assert word&0x7c000000==0x14000000
   d=word&0x3ffffff;return a+(d-0x4000000 if d&0x2000000 else d)*4
  for a,b in ((0x63c6b44,0x621ef50),(0x63c6b68,0x621ef18),(0x63c6b9c,0x621eeec),
              (0x621f00c,0x5fda87c),(0x621f048,0x5fda87c),(0x621c55c,0x5fb067c)):
   assert branch(a)==b
assert source.count('ash_toggle(pawn)')==1 and 'hand!=rl_right(pawn)' in source
assert 'qs_allowed(pawn)' in source and 'ash_still(pawn,name,sub,player,active,next)' in source
# No write through the cached legacy globals; comparisons (!=, ==) are reads.
assert not re.search(r'\*[^;\n]*(?:ash_sub|ash_player)[^;\n]*(?<![!<>=])=(?!=)',source)
assert not re.search(r'base\s*\+\s*0x(?:5fda87c|6397874)',source)
print('PASS:16 original preimages/13 complete bodies/six callers/five enum+GOT relocations; original Toggle only mutation',flush=True)
def extract(text,prefix):
 start=text.index(prefix);return text[start:text.index('\n}',start)+2]
weapon=(root/'analysis/re4fix_weapon.h').read_text(encoding='utf8')
qs=(root/'analysis/re4fix_quickselect.h').read_text(encoding='utf8')
test=(root/'analysis/research/test_ashley.c').read_text(encoding='utf8')
test=test.replace('/* SHARED_TYPED */',extract(weapon,'static int tw_typed('))
test=test.replace('/* SHARED_QS_GT */',extract(qs,'static int qs_gt(')).replace('/* SHARED_QS_NAME */',extract(qs,'static unsigned long long qs_name('))
test=test.replace('ASHLEY_HEADER',header.as_posix())
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231');sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
deps=(('tw_hand_class',0x6424b5c),('phone_player',0x633d030),('phone_active',0x63380dc),('phone_type',0x6338aec),
      ('rl_paused',0x61d0934),('rl_player_valid',0x61d401c),('rl_right',0x62a8e50),('qs_allowed',0x63c767c))
with tempfile.TemporaryDirectory(prefix='re4-ashley-') as tmp:
 d=Path(tmp);c=d/'test.c';c.write_text(test,encoding='utf8');fixture=d/'native_fixture.h'
 fixture.write_text(r'''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_ashley_install_guard(void){
 unsigned char* image=VirtualAlloc((void*)0x20000000,0xab00000,0x3000,4);assert(image);
 static const struct{unsigned a,w[2];} code[]={
'''+''.join('{%s,{%s,%s}},\n'%row for row in checks)+r'''};
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)memcpy(image+code[i].a,code[i].w,8);
 *(void**)(image+0xa361e48)=image+0xa4e9b48;*(void**)(image+0xa361d78)=image+0xa4e9b40;
 *(void**)(image+0x9d703f0+32)=image+0x9d703d0;*(unsigned*)(image+0x9d703f0+40)=2;
 const char* names[]={"EAshleyCommand::Follow","EAshleyCommand::Stay"};
 for(unsigned i=0;i<2;i++){*(const char**)(image+0x9d703d0+16*i)=names[i];*(long long*)(image+0x9d703d8+16*i)=i;}
 static const struct{void* slot;unsigned a;} deps[]={
'''+''.join('{&%s,0x%x},\n'%row for row in deps)+r'''};
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){void* p=image+deps[i].a;memcpy(deps[i].slot,&p,8);}
 qs_ready=1;unsigned long base=(unsigned long)(uintptr_t)image;install_touch_ashley(base);assert(ash_ready);
 for(unsigned i=0;i<sizeof code/sizeof *code;i++)for(unsigned j=0;j<2;j++){
  *(unsigned*)(image+code[i].a+4*j)^=1;install_touch_ashley(base);assert(!ash_ready);*(unsigned*)(image+code[i].a+4*j)^=1;
 }
 unsigned pointers[]={0xa361e48,0xa361d78,0x9d703f0+32};
 for(unsigned i=0;i<3;i++){void* old=*(void**)(image+pointers[i]);*(void**)(image+pointers[i])=0;install_touch_ashley(base);assert(!ash_ready);*(void**)(image+pointers[i])=old;}
 *(unsigned*)(image+0x9d703f0+40)=3;install_touch_ashley(base);assert(!ash_ready);*(unsigned*)(image+0x9d703f0+40)=2;
 for(unsigned i=0;i<2;i++){
  *(long long*)(image+0x9d703d8+16*i)=9;install_touch_ashley(base);assert(!ash_ready);*(long long*)(image+0x9d703d8+16*i)=i;
  *(const char**)(image+0x9d703d0+16*i)=0;install_touch_ashley(base);assert(!ash_ready);
  *(const char**)(image+0x9d703d0+16*i)="EAshleyCommand::Bad";install_touch_ashley(base);assert(!ash_ready);*(const char**)(image+0x9d703d0+16*i)=names[i];
 }
 for(unsigned i=0;i<sizeof deps/sizeof *deps;i++){
  void* zero=0;memcpy(deps[i].slot,&zero,8);install_touch_ashley(base);assert(!ash_ready);
  void* p=image+deps[i].a;memcpy(deps[i].slot,&p,8);
 }
 qs_ready=0;install_touch_ashley(base);assert(!ash_ready);qs_ready=1;install_touch_ashley(base);assert(ash_ready);
 assert(VirtualFree(image,0,0x8000));puts("PASS: actual installer30 instruction-word refusals/three GOT+enum pointers/enumcount/six enum mutations/eight missing dependencies/QS unavailable");
}
''',encoding='utf8')
 subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(c)],check=True)
 obj,exe=d/'test.obj',d/'test.exe'
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast',
  '-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-DASHLEY_NATIVE_FIXTURE="'+fixture.as_posix()+'"','-c','-o',str(obj),str(c)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
  f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: Android aarch64 Werror + executed actual Ashley adapter/shared typed ancestry, GT and full FName guards',flush=True)
