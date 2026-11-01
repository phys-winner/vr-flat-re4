"""Original attachment/signature evidence plus actual QA-only C watch routing. No ADB."""
from pathlib import Path
import hashlib
import json
import os
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
assert hashlib.sha256(native).hexdigest() == "6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1"
assert hashlib.sha256(native[0x62c80d4:0x62c80d4 + 696]).hexdigest() == "4cd60f068fa43984f55be3342a314d97f25d11fcb2008ee590cf2bb7de68f824"
assert struct.unpack_from("<I", native, 0xa235b80 + 36)[0] == 0xc8
assert struct.unpack_from("<I", native, 0xa235ba8 + 36)[0] == 0xc0
# The original VM reads a 12-byte FScriptName and constructs its ordinary FName from words0/2.
for at, expected in ((0x6994518, 0xb9400101), (0x699451c, 0xb9400909),
                     (0x6994520, 0x91003108), (0x6994528, 0xb3607d21)):
    assert struct.unpack_from("<I", native, at)[0] == expected
directory = root / "analysis/research/wrist_ui_originals"
original = json.loads((directory / "UI_WristWatch_BP.UpdateArm.script.json").read_text())
body = (directory / "UI_WristWatch_BP.uexp").read_bytes()
item = original["export"]
payload = body[item["uexp_offset"]:item["uexp_offset"] + item["serial_size"]]
assert hashlib.sha256(payload).hexdigest() == "a029cd028d580f9742e00264efb58e5777ceb433252c72f50e1c446c6a00b008"
assert original["runtime_bytes"] == 37 and original["script_bytes"] == 49
assert original["tree"][0]["expression"]["call"] == "AttachToArm"
assert original["tree"][1]["expression"]["let_bool"][1] == {"literal_token": 39}
assert struct.unpack_from("<Q", payload, 0x34)[0] == 0x94  # input EnumProperty NewHand
assert struct.unpack_from("<Q", payload, 0x9a)[0] == 0x180  # OUT BoolProperty callback
assert struct.unpack_from("<I", payload, original["script_offset"] + original["script_bytes"])[0] == 0x0c420000
assert json.loads((directory / "UI_WristWatch_BP.AttachToArm.script.json").read_text())["tree"][0]["expression"]["let"][0]["field"] == ["ActiveArm"]
print("PASS: original most-derived37-byte UpdateArm calls AttachToArm then acknowledges true; input/out flags, native FScriptName conversion and actual attach parent/socket offsets pinned", flush=True)

sdk_root = Path(os.environ.get("ANDROID_SDK_ROOT", str(Path.home() / "AppData/Local/Android/Sdk")))
llvm = sdk_root / "ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin"
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
source = root / "analysis/research/test_watch_binding.c"
with tempfile.TemporaryDirectory(prefix="re4-watch-binding-") as temporary:
    directory = Path(temporary)
    obj, exe = directory / "test.obj", directory / "test.exe"
    subprocess.run([str(llvm / "clang.exe"), "--target=aarch64-linux-android23", "-std=c11", "-O2", "-Werror", "-fsyntax-only", str(source)], check=True)
    subprocess.run([str(llvm / "clang.exe"), "--target=x86_64-pc-windows-msvc", "-std=c11", "-O2", "-Werror",
        "-isystem", str(vc / "include"), "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"), "-c", "-o", str(obj), str(source)], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj),
        "libcmt.lib", "libucrt.lib", "libvcruntime.lib", "oldnames.lib", "kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
    excluded = directory / "excluded.c"
    excluded.write_text('#include "' + (root / "analysis/research/qa_watch_binding.h").as_posix()
                        + '"\nint main(void){return 0;}\n', encoding="utf8")
    subprocess.run([str(llvm / "clang.exe"), "--target=aarch64-linux-android23", "-std=c11", "-Werror", "-fsyntax-only", str(excluded)], check=True)
print("PASS: actual QA watch binding C compiles for Android, executes on host, and excludes its code/dependencies without QA define")
