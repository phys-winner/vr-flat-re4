"""Prove the isolated QA fixture's item IDs, original inventory ABI, guards and one-shot grant."""
import os
from pathlib import Path
import hashlib,re,runpy,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
native=(root/'apk_decoded/lib/arm64-v8a/libUE4.so').read_bytes()
assert hashlib.sha256(native).hexdigest()=='6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
source=(root/'analysis/research/qa_shotgun_fixture.h').read_text()
main=(root/'analysis/re4fix.c').read_text()
assert not any(text in main for text in ('RE4_QA_SHOTGUN_FIXTURE','qa_shotgun_fixture.h','qashotgunfixture'))
assert source.startswith('// Temporary device QA only;') and '#if defined(RE4_QA_SHOTGUN_FIXTURE)' in source
assert 'put_in_case(44,1,size)' in source and 'put_in_case(24,20,size)' in source
for address,a,b in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)\}',source):
 assert struct.unpack_from('<II',native,int(address,16))==(int(a,16),int(b,16))
rela={a:v for a,t,v in struct.iter_unpack('<QQq',native[0x2e6c028:0x2e6c028+0x1146540]) if t==1027}
for slot,name,item in ((0x9dc5b40,b'EItemId::Shotgun\0',44),(0x9dc5a00,b'EItemId::Bullet_12gg\0',24)):
 assert native[rela[slot]:].startswith(name) and struct.unpack_from('<q',native,slot+8)[0]==item
assert rela[0xa361da0]==0xa500780
# Reuse the complete original PutInCase hash, scalar case getter, placement flags and pickup ABI proof.
proof=runpy.run_path(str(root/'analysis/research/check_qa_grenade_fixture.py'))
emu,arm=proof['emu'],proof['arm']
for item,quantity in ((44,1),(24,20)):
 proof['received'].clear();proof['queries'].clear()
 emu.mem_write(0x400006a8,struct.pack('<H',quantity));emu.reg_write(arm.UC_ARM64_REG_X19,0x40000600)
 emu.reg_write(arm.UC_ARM64_REG_W21,item);emu.reg_write(arm.UC_ARM64_REG_SP,0x40000f00)
 emu.reg_write(arm.UC_ARM64_REG_X30,0x70000000);emu.emu_start(0x61539ac,0x61539c4,count=500)
 assert proof['received']==[(item,quantity,3)]
emu.mem_map(0x5f34000,0x1000);emu.mem_write(0x5f34a8c,native[0x5f34a8c:0x5f34adc])
emu.mem_map(0x4305000,0x1000);emu.mem_write(0x4305108,native[0x4305108:0x4305108+0x3de])
for weapon,expected in ((44,24),(45,24),(148,24),(65535,65535)):
 emu.reg_write(arm.UC_ARM64_REG_W0,weapon);emu.reg_write(arm.UC_ARM64_REG_W1,0)
 emu.reg_write(arm.UC_ARM64_REG_X30,0x70000000);emu.emu_start(0x5f34a8c,0x70000000,count=500)
 assert emu.reg_read(arm.UC_ARM64_REG_W0)==expected
print('PASS: original enum Shotgun44/Bullet_12gg24; original WeaponId2BulletId maps all three shotgun items to24; original pickup ABI receives44/1 and24/20 with scalar case size; canonical source excludes QA fixture',flush=True)
clang=Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc=Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231');sdk=Path(r'C:/Program Files (x86)/Windows Kits/10')
test=root/'analysis/research/test_qa_shotgun_fixture.c'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-fsyntax-only',str(test)],check=True)
with tempfile.TemporaryDirectory(prefix='re4-qa-shotgun-') as directory:
 obj,exe=Path(directory)/'test.obj',Path(directory)/'test.exe'
 subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-isystem',str(vc/'include'),'-isystem',str(sdk/'Include/10.0.26100.0/ucrt'),'-c','-o',str(obj),str(test)],check=True)
 subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{sdk/"Lib/10.0.26100.0/um/x64"}'],check=True)
 subprocess.run([str(exe)],check=True)
with tempfile.TemporaryDirectory(prefix='re4-qa-shotgun-integrated-') as directory:
 qa=main
 edits={
  '#include "re4fix_weapon_ui.h"':'#include "re4fix_weapon_ui.h"\n#define RE4_QA_SHOTGUN_FIXTURE\n#include "qa_shotgun_fixture.h"',
  'static void devcmd_exec(char* line) {':'static void devcmd_exec(char* line) {\n    if(qa_shotgun_fixture_command(line))return;',
  '    touch_weapon_ui_tick(self);':'    touch_weapon_ui_tick(self);\n    qa_shotgun_fixture_tick(self);',
 }
 for before,after in edits.items():
  assert qa.count(before)==1;qa=qa.replace(before,after)
 path=Path(directory)/'re4fix.c';path.write_text(qa,encoding='utf8')
 subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-fsyntax-only','-I',str(root/'analysis'),'-I',str(root/'analysis/research'),str(path)],check=True)
print('PASS: exact three temporary-source injection edits compile integrated for Android aarch64; production source untouched')
