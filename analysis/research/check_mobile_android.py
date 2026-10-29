"""Validate native JNI ownership/ABI and execute the actual settings request helper."""
import os
from pathlib import Path
import hashlib,json,mmap,re,struct,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
NDK=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64'))
LLVM=NDK/'bin';VC=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231');SDK=Path(r'C:\Program Files (x86)\Windows Kits\10')
symbols=subprocess.check_output([str(LLVM/'llvm-nm.exe'),'-D','-S','-C',str(ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so')],text=True)
for name,address in (('FJavaWrapper::GameActivityThis',0xa42ad50),('FJavaWrapper::GameActivityClassID',0xa42aa90)):
 match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+B\s+'+re.escape(name)+r'\s*$',symbols,re.M);assert match and int(match[1],16)==address and int(match[2],16)==8
production=(ROOT/'analysis/re4fix.c').read_text(encoding='utf-8')
assert not re.search(r'\b(?:gate_fq|hook_fq|g_tgt_fq)\b',production), 'ForceQuit must retain its original native body, without the relocated diagnostic gate'
assert '0x54b4254' not in production.lower(), 'The original ForceQuit entry must never be patched'
assert '0x9442dbb5' not in production.lower(), 'The native ForceQuit PC-relative BL cannot be replayed in another library'
for name,address,size in (('AndroidThunkCpp_ForceQuit()',0x54b4254,0x3c),('AndroidJavaEnv::GetJavaEnv(bool)',0x656b134,0xf8)):
 match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+T\s+'+re.escape(name)+r'\s*$',symbols,re.M);assert match and (int(match[1],16),int(match[2],16))==(address,size)
with (ROOT/'apk_decoded/lib/arm64-v8a/libUE4.so').open('rb') as file,mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as data:
 phoff=struct.unpack_from('<Q',data,32)[0];phsize,count=struct.unpack_from('<HH',data,54);segments=[struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize) for i in range(count)]
 def read(address,size):
  s=next(s for s in segments if s[0]==1 and s[3]<=address and address+size<=s[3]+s[5]);offset=s[2]+address-s[3];return data[offset:offset+size]
 pairs={0x656b134:(0xd100c3ff,0xa9014ff4,0xa9027bfd,0x910083fd),0x54b0004:(0xf81e0ff3,0xa9017bfd,0x910043fd,0xaa0003f3),0x54b0828:(0xd10483ff,0xa9104ffc,0xa9117bfd,0x910443fd),0x663dbd4:(0xa9be4ff4,0xa9017bfd,0x910043fd,0xd001e9a8),0x661956c:(0x9100a000,0xd65f03c0)}
 for address,words in pairs.items():assert read(address,len(words)*4)==struct.pack('<'+'I'*len(words),*words)
 forcequit=read(0x54b4254,0x3c)
 assert hashlib.sha256(forcequit).hexdigest()=='f708eeffb922c855ff74e4257ff9cfeb4d9fb1ee6e7310c03c112963c797c185'
 assert struct.unpack_from('<4I',forcequit)==(0xa9bf7bfd,0x910003fd,0x52800020,0x9442dbb5)
 op=struct.unpack_from('<I',forcequit,12)[0];assert op&0xfc000000==0x94000000
 displacement=op&0x3ffffff
 if displacement&0x2000000:displacement-=0x4000000
 assert 0x54b4260+4*displacement==0x656b134
 assert hashlib.sha256(read(0x656b134,0xf8)).hexdigest()=='818bce899343d9e425730aa866b0aa5095fc6e87754d892319cd353b18634931'
 assert struct.unpack('<I',read(0x895f44c,4))[0]==0xa942cc00
 startup_body=read(0x626dce8,88);startup_fixup=read(0x63579fc,0x4c)
 assert hashlib.sha256(startup_body).hexdigest()=='afd4adcd9081ef63eec54386f0814c486afa989d54fef001dbfba7db5a7e4e32'
 assert struct.unpack('<I',read(0x9e056b0+0x24,4))[0]==0x180
 assert struct.unpack('<I',read(0x626dd24,4))[0]==0x940f41b8 and 0x626dd24+4*0xf41b8==0x663e404
 proof={f'{address:x}':{'bytes':size,'sha256':hashlib.sha256(read(address,size)).hexdigest()} for address,size in ((0x656b134,0xf8),(0x54b0004,0x54),(0x54b0828,0xa0),(0x663dbd4,0x15c),(0x661956c,8))}
 (ROOT/'analysis/research/mobile_android_native.json').write_text(json.dumps(proof,indent=2)+'\n')
with tempfile.TemporaryDirectory(prefix='mobile-jni-check-') as temporary:
 work=Path(temporary);source=work/'native.c';source.write_text('#include <string.h>\n#include <unistd.h>\n#include <sys/mman.h>\nstatic int protect_mem(void* p,size_t n,int flags){return 0;}\nstatic int g_touch_gt_tid,g_touch_context;\nstatic long ptr_now_ms(void){return 1000;}\nstatic int touch_mobile_back(void){return 0;}\n#define LOGF(...) ((void)0)\n#define LOGI(...) ((void)0)\n#include "'+(ROOT/'analysis/re4fix_mobile_android.h').as_posix()+'"\nvoid install(unsigned long b){install_mobile_android(b);touch_mobile_android_tick();}\n')
 subprocess.run([str(LLVM/'clang.exe'),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(source)],check=True)
 obj,exe=work/'check.obj',work/'check.exe'
 fixture=work/'startup_fixture.h'
 fixture.write_text('''
__declspec(dllimport) void* __stdcall VirtualAlloc(void*,size_t,unsigned long,unsigned long);
__declspec(dllimport) int __stdcall VirtualFree(void*,size_t,unsigned long);
static void verify_startup_language_patch(void){
 unsigned char* image=VirtualAlloc((void*)0x20000000,0x6270000,0x3000,4);assert(image==(void*)0x20000000);
 static const unsigned original[]={'''+','.join(hex(word) for word in struct.unpack('<22I',startup_body))+'''};
 unsigned char* body=image+0x626dce8;unsigned long base=(unsigned long)(size_t)image;
 for(unsigned i=0;i<22;i++){
  memcpy(body,original,sizeof original);body[i*4]^=1;unsigned char before[sizeof original];memcpy(before,body,sizeof before);
  protect_calls=0;assert(!mobile_android_keep_startup_language(base)&&!protect_calls&&!memcmp(body,before,sizeof before));
 }
 memcpy(body,original,sizeof original);protect_calls=0;protect_fail=1;
 assert(!mobile_android_keep_startup_language(base)&&protect_calls==1&&!memcmp(body,original,sizeof original));
 protect_fail=0;protect_calls=0;assert(mobile_android_keep_startup_language(base)&&protect_calls==1&&protected_site==image+0x626dd24);
 for(unsigned i=0;i<22;i++)assert(((unsigned*)body)[i]==(i==15?0xd503201f:original[i]));
 assert(VirtualFree(image,0,0x8000));
 puts("PASS: actual startup patch checks all22 original words, declines every corruption/protection failure and changes only the saved-language BL to NOP");
}
''',encoding='utf8')
 subprocess.run([str(LLVM/'clang.exe'),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-Wno-int-to-pointer-cast','-Wno-int-to-void-pointer-cast','-DMOBILE_STARTUP_FIXTURE="'+fixture.as_posix()+'"','-isystem',str(VC/'include'),'-isystem',str(SDK/'Include/10.0.26100.0/ucrt'),'-idirafter',str(NDK/'sysroot/usr/include'),'-c',str(ROOT/'analysis/research/test_mobile_android.c'),'-o',str(obj)],check=True)
 subprocess.run([str(VC/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{VC/"lib/x64"}',f'/LIBPATH:{SDK/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{SDK/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)

# Execute the original save-load body and its real Fixup, replacing only the guarded BL.
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm64_const as arm
registers=[getattr(arm,'UC_ARM64_REG_X'+str(i)) for i in range(31)]
def startup_run(loaded,has_save,saved_language,initial_language,patched):
 vm=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
 for page in (0x626d000,0x6357000,0x640b000,0x663d000,0x663e000):vm.mem_map(page,4096)
 body=bytearray(startup_body)
 if patched:struct.pack_into('<I',body,0x626dd24-0x626dce8,0xd503201f)
 vm.mem_write(0x626dce8,bytes(body));vm.mem_write(0x63579fc,startup_fixup)
 vm.mem_map(0x20000000,0x10000);vm.mem_map(0x30000000,0x4000);vm.mem_map(0x70000000,4096)
 instance,save,culture,characters=0x20000100,0x20001000,0x20002000,0x20003000
 vm.mem_write(save,bytes((i*37+11)&255 for i in range(0x600)))
 encoded=(saved_language+'\0').encode('utf-16-le');vm.mem_write(characters,encoded)
 vm.mem_write(instance+0x1a0,struct.pack('<Q',save if has_save else 0))
 vm.mem_write(save+0x180,struct.pack('<Qii',characters,len(encoded)//2,len(encoded)//2))
 vm.mem_write(save+0x590,struct.pack('<I',0))
 state={'language':initial_language,'calls':[]}
 def external(uc,address,size,user):
  if address not in (0x640bd64,0x663dbd4,0x663e404,0x63579fc):return
  state['calls'].append(address)
  if address==0x63579fc:
   assert uc.reg_read(arm.UC_ARM64_REG_X0)==save
   return
  if address==0x640bd64:
   assert uc.reg_read(arm.UC_ARM64_REG_X0)==instance;result=loaded
  elif address==0x663dbd4:result=culture
  else:
   assert uc.reg_read(arm.UC_ARM64_REG_X0)==culture and uc.reg_read(arm.UC_ARM64_REG_X1)==save+0x180
   pointer,count,capacity=struct.unpack('<Qii',uc.mem_read(save+0x180,16))
   state['language']=bytes(uc.mem_read(pointer,(count-1)*2)).decode('utf-16-le');result=1
  uc.reg_write(arm.UC_ARM64_REG_X0,result);uc.reg_write(arm.UC_ARM64_REG_PC,uc.reg_read(arm.UC_ARM64_REG_LR))
 vm.hook_add(UC_HOOK_CODE,external)
 for i,register in enumerate(registers):vm.reg_write(register,0x11110000+i*16)
 vm.reg_write(arm.UC_ARM64_REG_X0,instance);vm.reg_write(arm.UC_ARM64_REG_SP,0x30003000);vm.reg_write(arm.UC_ARM64_REG_LR,0x70000000)
 vm.reg_write(arm.UC_ARM64_REG_NZCV,0xa0000000)
 vm.emu_start(0x626dce8,0x70000000,count=300)
 assert vm.reg_read(arm.UC_ARM64_REG_PC)==0x70000000 and vm.reg_read(arm.UC_ARM64_REG_X0)==(loaded&1)
 snapshot=[vm.reg_read(register) for register in registers]+[vm.reg_read(arm.UC_ARM64_REG_SP),vm.reg_read(arm.UC_ARM64_REG_NZCV)]
 return state,snapshot,bytes(vm.mem_read(0x20000000,0x4000))

startup_cases=((0,True,'fr-FR','ru'),(1,False,'fr-FR','ru'),(1,True,'','ru'),
 (1,True,'fr-FR','ru'),(1,True,'ru','ru'),(1,True,'fr-FR','es-419'),(1,True,'ru','ja-JP'))
for arguments in startup_cases:
 before,old_registers,old_memory=startup_run(*arguments,False)
 after,new_registers,new_memory=startup_run(*arguments,True)
 loaded,has_save,saved_language,initial_language=arguments
 override=bool(loaded and has_save and saved_language)
 assert before['language']==(saved_language if override else initial_language)
 assert after['language']==initial_language
 assert before['calls'].count(0x663e404)==int(override) and 0x663e404 not in after['calls']
 assert [call for call in before['calls'] if call!=0x663e404]==after['calls']
 assert before['calls'].count(0x63579fc)==int(bool(loaded and has_save))
 assert old_registers==new_registers and old_memory==new_memory,arguments
print('PASS: seven original ARM64 save-load/Fixup cases; legacy French overrides Russian only before patch, Android locale retained after patch; failure/null/empty paths, every register/flags/SP, all saved object bytes and other calls identical')
print('PASS: original native GetJavaEnv automatic attach/TLS detach, optional FindMethod clears missing-method exception, zero/three-arg CallVoidMethodV ABI and two8-byte global slots; Android23 Werror')
print('PASS: ForceQuit remains unpatched with no legacy diagnostic gate; exact original60-byte body/prologue and native BL target the verified248-byte GetJavaEnv')
print('PASS: original current-language shared pointer+28 and FCulture const FString getter, once-only recorded-GT read-only culture diagnostic with32 UTF16 cap/bounds/terminator/ASCII-name guards')
