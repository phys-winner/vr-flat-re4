"""Read-only proof that stock internal scaling stays inactive in the current flat mobile port."""
from pathlib import Path
import hashlib
import re
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
native = (ROOT / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
assert hashlib.sha256(native).hexdigest() == "6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1"
RANGES = (
    ("SupportsScreenPercentage", 0x8ac5a58, 0x58, "a8593f01a0c6503910c36befc929da6b207c34ee625e8a15abc109b8e733ff5a"),
    ("IsMobileHDR active branch", 0x8abd2e0, 0x6c, "3d52dc580ac5c86e8c09ca92d22ce7f1aa0c8db939124a1514efe1d06136d9af"),
    ("GetScreenRect", 0x6a1d59c, 0x54c, "1b5f126545544d476b8ba8f725f5bf2cd8774c0ec4335ebb90ebe911b45cb9bb"),
    ("PrepareViewRects LDR branch", 0x78c38bc, 0x30, "d13aa71c7e94aee4fc0f79973a74fa0de5cb8606c3da7a2b8a3ac97742855da2"),
    ("AllocateFoveationTexture", 0x757fe4c, 0x228, "b545e3889f1db953b1cff3cdd2c1d3faa728899c48adb64706f50993fd969837"),
    ("ShouldEnableDynamicResolution", 0x8c0e904, 0x3c, "24557f72a5a6c0c702e2e8a09f2b76fbcc84f5c1eeb920efc38e8c28acb6e48f"),
    ("DynamicRes registration", 0x8c20284, 0x24, "f58ba88d6769a60d4fe3c5d2b7393e904d03c7b38cb6a64236b68996dcb03da8"),
    ("VR4 pixel density and FFR", 0x63825fc, 0x16c, "134d5b6de3e5df2b7ca007b07270c843d00deeb4317770875b4dc534bc590761"),
    ("Oculus dynamic-resolution setup", 0x5c2a3e8, 0xc4, "9b1b0d15293917c627b12ca1be8166a7d17b9e8b90dd1ceb7ca0775a54db2379"),
)
for name, address, length, digest in RANGES:
    assert hashlib.sha256(native[address:address + length]).hexdigest() == digest, name

# Reuse the retained original ELF relocator/Unicorn setup; stop before its fixture decoding/writes.
loader = ROOT / "analysis/research/host_oodle_decode.py"
environment = {"__file__": str(loader), "__name__": "readonly_native_scale_loader"}
exec(compile(loader.read_text(encoding="utf8").split("fixture = ROOT / 'analysis/research/flat_ui_graph'", 1)[0],
             str(loader), "exec"), environment)
u, buffers, call, base, stack, Pak9 = (environment[name] for name in ("u", "buffers", "call", "base", "stack", "Pak9"))
from unicorn import UC_HOOK_CODE
from unicorn.arm64_const import (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X4,
    UC_ARM64_REG_X19, UC_ARM64_REG_X22, UC_ARM64_REG_X25, UC_ARM64_REG_X26, UC_ARM64_REG_SP,
    UC_ARM64_REG_PC, UC_ARM64_REG_LR, UC_ARM64_REG_S0)

def q(address):
    return struct.unpack("<Q", u.mem_read(address, 8))[0]

def putq(address, value):
    u.mem_write(address, struct.pack("<Q", value))

def puti(address, value):
    u.mem_write(address, struct.pack("<I", value))

# Authenticate all cooked config entries through Pak9's original index/data SHA1 checks.
pak = Pak9(ROOT / "com.Armature.VR4/patch.201.com.Armature.VR4.obb", "VR4/Content/Paks/pakchunk0-Android_ETC2.pak")
configs = {name: pak.read_entry(name).decode("utf-8-sig") for name in pak.entries if name.endswith(".ini")}
engine = configs["VR4/Config/DefaultEngine.ini"]
assert "r.MobileHDR=False" in engine and "bSupportsVulkanSM5=False" in engine and "bSupportsVulkan=True" in engine
assert "r.DefaultFeature.AntiAliasing=0" in engine
assignments = []
for name, text in configs.items():
    for line in text.splitlines():
        if re.search(r"(?:^|=)\s*r\.(?:MobileHDR|ScreenPercentage|DynamicRes\.OperationMode)\s*=", line, re.I):
            assignments.append((name, line.strip()))
assert assignments == [("VR4/Config/DefaultEngine.ini", "r.MobileHDR=False")], assignments
assert "FFRLevel=FFR_Off" in engine

# Current production retains the original Oculus-application query and a queryable VR activity category.
android = "{http://schemas.android.com/apk/res/android}"
manifest = ET.parse(ROOT / "apk_decoded/AndroidManifest.xml").getroot()
activity = next(a for a in manifest.find("application").findall("activity")
                if a.get(android + "name") == "com.epicgames.ue4.GameActivity")
assert any(c.get(android + "name") == "com.oculus.intent.category.VR" for c in activity.iter("category"))
smali = (ROOT / "apk_decoded/smali/com/epicgames/ue4/GameActivity.smali").read_text(encoding="utf8")
method = smali.split(".method public AndroidThunkJava_IsOculusMobileApplication()Z", 1)[1].split(".end method", 1)[0]
assert "->PackagedForOculusMobile:Z" in method and "return v0" in method
assert 'const-string v5, "com.oculus.intent.category.VR"' in smali and "->queryIntentActivities(" in smali
assert '"Found Oculus Mobile mode."' in smali and '"No Oculus Mobile mode detected."' in smali

production = "\n".join(p.read_text(encoding="utf8") for p in (ROOT / "analysis").glob("re4fix*.h"))
main = (ROOT / "analysis/re4fix.c").read_text(encoding="utf8")
production += main
for name in ("r.MobileHDR", "r.ScreenPercentage", "r.MobileContentScaleFactor", "r.DynamicRes.OperationMode",
             "r.SecondaryScreenPercentage.GameViewport", "vr.PixelDensity"):
    assert '"' + name + '"' not in production, ("new internal scaling override requires audit", name)
for fragment in ("base + 0x8c0b660", "base + 0x5c16894, base + 0x5c168a0", "0xf9057a7f", "base + 0x8c0a908",
                 "volatile unsigned long* stereo = (volatile unsigned long*)(ge + 0xaf0)", "*stereo = 0"):
    assert fragment in main, fragment
for address, words in ((0x8c0a908, (0xf9057a68, 0xf90017ff)), (0x8c0b660, (0xf81e0ff3, 0xa9017bfd)),
                       (0x5c16894, (0xf9400008, 0xf9414501)), (0x5c168a0, (0xf8538c08, 0xf9414501))):
    assert struct.unpack_from("<2I", native, address) == words
mobile = (ROOT / "analysis/android/com/epicgames/ue4/MobileSettings.java").read_text(encoding="utf8")
surface = mobile.split("public static int[] surfaceSize(", 1)[1].split("private static SurfaceView findSurface", 1)[0]
assert "int percent = nativeRenderPercent()" in surface and "nativeViewportSizes(width, height, w, h)" in surface
assert surface.count("* percent / 100) & ~7") == 2 and "return new int[] {w, h}" in surface
menu = (ROOT / "analysis/re4fix_mobile_menu.h").read_text(encoding="utf8")
assert "mobile_render_scales[] = {50, 67, 75, 85, 100}" in menu
assert "startup_scale = g_mprefs.render_scale" in menu and "return startup_scale" in menu
print("PASS: original config is MobileLDR/ES3_1; current mono/Oculus preconditions and explicit Surface-only percentage route retained", flush=True)

# Fully execute original SupportsScreenPercentage and IsMobileHDR with initialized console data.
family, scene, hdr = buffers, buffers + 0x200, buffers + 0x400
putq(family + 0x20, scene)
u.mem_write(base + 0xaa58138, b"\1"); putq(base + 0xaa58130, hdr)
u.mem_write(q(base + 0xa352000), b"\0")
for feature, hdr_value, expected in ((1, 0, 0), (1, 1, 1), (3, 0, 1)):
    puti(scene + 8, feature); puti(hdr, hdr_value)
    assert call(0x8ac5a58, [family]) == expected

# The actual LDR branch copies every input ViewRect without consuming secondary resolution fraction.
renderer, views = buffers + 0x1000, buffers + 0x2000
puti(renderer + 0xb0, 2); putq(renderer + 0xa8, views)
u.mem_write(renderer + 0x94, struct.pack("<f", .25))
rects = ((0, 0, 2400, 1080), (8, 16, 1608, 736))
for i, rect in enumerate(rects):
    u.mem_write(views + i * 0x51d0 + 0x258, struct.pack("<4i", *rect))
u.reg_write(UC_ARM64_REG_X19, renderer)
u.emu_start(base + 0x78c38bc, base + 0x78c38ec, count=50)
for i, rect in enumerate(rects):
    assert struct.unpack("<4i", u.mem_read(views + i * 0x51d0 + 0x15a0, 16)) == rect
print("PASS: original ES3_1/LDR rejects screen percentage; actual LDR view preparation copies both rectangles unchanged despite secondary0.25", flush=True)
assert struct.unpack_from("<2I", native, 0x8bfaddc) == (0x1e2e1000, 0xd65f03c0)
call(0x8bfaddc, [0])
assert u.reg_read(UC_ARM64_REG_S0) == 0x3f800000

# Original GetScreenRect: real console float getter; only command-line APIs return empty/absent.
cvar, vtable, empty = buffers + 0x10000, buffers + 0x11000, buffers + 0x12000
putq(cvar, vtable); putq(vtable + 0xa0, base + 0x65ef6f4)
putq(base + 0xa72afc0, cvar); u.mem_write(base + 0xa72afc8, b"\1")
u.mem_write(empty, b"\0\0")
def no_command(uc, address, size, user):
    if address == base + 0x6700330:
        uc.reg_write(UC_ARM64_REG_X0, empty)
    elif address == base + 0x67585bc:
        uc.reg_write(UC_ARM64_REG_X0, 0)
    else:
        return
    uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
hook = u.hook_add(UC_HOOK_CODE, no_command)
def screen_rect(width, height, oculus, scale):
    sp = stack + 0x1f0000
    u.mem_write(sp, bytes(0x100)); u.mem_write(sp, struct.pack("<2i", height, width))
    u.mem_write(base + 0xa72af74, bytes([oculus])); u.mem_write(cvar + 0x40, struct.pack("<f", scale))
    u.reg_write(UC_ARM64_REG_SP, sp); u.reg_write(UC_ARM64_REG_X26, base + 0xa72a000)
    u.reg_write(UC_ARM64_REG_X22, buffers + 0x13000); u.reg_write(UC_ARM64_REG_X25, empty)
    u.emu_start(base + 0x6a1d7b8, base + 0x6a1d8f0, count=400)
    h, w = struct.unpack("<2i", u.mem_read(sp, 8))
    return w, h
for width, height in ((2400, 1080), (1200, 536), (1608, 720), (1800, 808), (2040, 912)):
    for scale in (0, .5, .8, 1):
        assert screen_rect(width, height, 1, scale) == (width, height)
assert screen_rect(2400, 1080, 0, 0) == (2400, 1080)
assert screen_rect(2400, 1080, 0, 1) == (1600, 720)
assert screen_rect(2400, 1080, 0, .8) == (1280, 576)
assert screen_rect(2400, 1080, 0, .5) == (800, 360)
u.hook_del(hook)
print("PASS: original Oculus branch bypasses MCSF for all5 Surface choices; non-Oculus1/.8/.5 downscale reproduced and0 bypass proven", flush=True)

# The real registration call passes integer default0 for r.DynamicRes.OperationMode.
manager, engine_object, operation = buffers + 0x14000, buffers + 0x15000, buffers + 0x17000
putq(manager, vtable); u.reg_write(UC_ARM64_REG_X0, manager)
u.emu_start(base + 0x8c20284, base + 0x8c202a4, count=16)
assert u.reg_read(UC_ARM64_REG_X1) == base + 0x40fcf1a
assert u.reg_read(UC_ARM64_REG_X2) == 0 and u.reg_read(UC_ARM64_REG_X4) == 0x20
operation_name = "r.DynamicRes.OperationMode\0".encode("utf-16-le")
assert native[0x40fcf1a:0x40fcf1a + len(operation_name)] == operation_name
assert struct.unpack_from("<2I", native, 0x88c06d8) == (0x1e2703e0, 0x2a1f03e3)
u.reg_write(UC_ARM64_REG_X0, manager)
u.emu_start(base + 0x88c06c0, base + 0x88c06e0, count=16)
assert u.reg_read(UC_ARM64_REG_X1) == base + 0x4056566 and u.reg_read(UC_ARM64_REG_S0) == 0
putq(base + 0xaa764f0, operation)
for paused in (0, 1):
    u.mem_write(engine_object + 0xa68, bytes([paused]))
    for user in (0, 1):
        u.mem_write(engine_object + 0xa69, bytes([user]))
        for mode in (0, 1, 2):
            puti(operation, mode)
            assert call(0x8c0e904, [engine_object]) == int(not paused and (mode == 2 or mode == 1 and user))
print("PASS: actual dynamic-resolution registration default0 and original enable predicate mode0/1/2 x user/paused states; secondary default0 and DPI fraction1", flush=True)

# Current mono engine slot makes the actual native foveation allocator decline and clear its active flag.
targets = buffers + 0x18000
putq(base + 0xaa75db0, engine_object); putq(engine_object + 0xaf0, 0)
putq(targets + 0x1d8, 0); u.mem_write(targets + 0x42a, b"\1")
call(0x757fe4c, [targets, 0])
assert u.mem_read(targets + 0x42a, 1) == b"\0" and q(targets + 0x1d8) == 0
assert struct.unpack_from("<4f", native, 0x3fb6830) == (struct.unpack("<f", struct.pack("<f", 1.2))[0],) * 2 + (struct.unpack("<f", struct.pack("<f", .97))[0],) * 2
density_command = "vr.PixelDensity {0}\0".encode("utf-16-le")
assert density_command == native[0x3fe454e:0x3fe454e + len(density_command)]
print("PASS: original mono foveation allocator remains inactive; stock VR-only density defaults1.2/.97 and native setter contract identified", flush=True)
print("PASS: no production/build/device writes; original instruction execution used only isolated Unicorn process memory")
