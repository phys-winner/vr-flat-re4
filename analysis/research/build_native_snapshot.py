"""Build a signed checkpoint from an immutable copy of its actual native includes."""
import os
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import zipfile

root=Path(__file__).resolve().parents[2]
if len(sys.argv)!=3 or not re.fullmatch(r'p[0-9]+',sys.argv[1]):
    raise SystemExit('usage: build_native_snapshot.py pNNN base.apk')
tag=sys.argv[1];base=Path(sys.argv[2]).resolve()
assert base.is_file() and base.suffix=='.apk'
snapshot=root/'analysis/build'/(tag+'_source')
assert not snapshot.exists(),'checkpoint snapshot already exists'
pending=[root/'analysis/re4fix.c',root/'analysis/re4fix_overlay.c'];inputs={}
while pending:
    path=pending.pop().resolve()
    assert path.is_relative_to(root/'analysis')
    if path in inputs:continue
    data=path.read_bytes();inputs[path]=data
    for include in re.findall(rb'^\s*#include\s+"([^"]+)"',data,re.M):
        pending.append(path.parent/include.decode('ascii'))
snapshot.mkdir()
for path,data in inputs.items():
    target=snapshot/path.relative_to(root/'analysis');target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(data)
assert all(path.read_bytes()==data for path,data in inputs.items()),'native source changed while copying'
digest=lambda b:hashlib.sha256(b).hexdigest()
manifest={'candidate':tag,'base':{'path':str(base.relative_to(root)),'sha256':digest(base.read_bytes())},
    'inputs':[{'path':str(path.relative_to(root)).replace('\\','/'),'bytes':len(data),'sha256':digest(data)} for path,data in sorted(inputs.items())]}
sdk=Path(os.environ['ANDROID_SDK_ROOT'])
clang=sdk/'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
library=snapshot/'libre4fix.so';unsigned=root/('re4_'+tag+'i.apk');signed=root/('re4_'+tag+'a.apk')
assert not unsigned.exists() and not signed.exists(),'checkpoint APK already exists'
subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-fPIC','-shared','-fno-stack-protector',
    '-Wl,--no-undefined','-Wl,-z,max-page-size=16384','-o',str(library),str(snapshot/'re4fix.c'),str(snapshot/'re4fix_overlay.c'),'-llog','-ldl','-lm'],check=True)
subprocess.run([sys.executable,'-I',str(root/'analysis/repack.py'),str(base),str(unsigned),
    'lib/arm64-v8a/libre4fix.so='+str(library)],check=True)
tools=sdk/'build-tools/36.0.0';signer=Path(os.environ['RE4_TOOLS_DIR'])
subprocess.run([str(tools/'zipalign.exe'),'-f','4',str(unsigned),str(signed)],check=True)
subprocess.run(['java','-jar',str(signer/'apksigner.jar'),'sign','--key',str(signer/'testkey.pk8'),
    '--cert',str(signer/'testkey.x509.pem'),str(signed)],check=True)
subprocess.run([str(tools/'zipalign.exe'),'-c','4',str(signed)],check=True)
subprocess.run(['java','-jar',str(signer/'apksigner.jar'),'verify','--verbose','--print-certs',str(signed)],check=True)
with zipfile.ZipFile(signed) as apk:assert apk.read('lib/arm64-v8a/libre4fix.so')==library.read_bytes()
assert digest(base.read_bytes())==manifest['base']['sha256'],'Android base changed during build'
manifest['library']={'bytes':library.stat().st_size,'sha256':digest(library.read_bytes())}
manifest['apk']={'path':signed.name,'bytes':signed.stat().st_size,'sha256':digest(signed.read_bytes())}
(root/'analysis/research'/(tag+'_source_build_manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
print(json.dumps({'sources':len(inputs),'library':manifest['library'],'apk':manifest['apk']},indent=2))
