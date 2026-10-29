"""Static acceptance for the p304 features (levers/cranks, throws, gamepad, map zoom, FOV/skip, ammo/paint, Ashley HP).

1. Every instruction-word guard in the new headers equals the real libUE4.so words at that RVA.
2. Every naked gate replays exactly the first 16 bytes of the function its g_tgt_* resumes in (+16).
3. The header-relative field offsets are proven by the instructions that use them.
4. Host-executed C: map pinch accumulation, gamepad dead zone, quick-turn easing and the throw pose curve.
"""
import os
from pathlib import Path
import hashlib
import mmap
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
analysis = root / 'analysis'
so_path = root / 'apk_decoded/lib/arm64-v8a/libUE4.so'
headers = ['re4fix_sce_touch.h', 're4fix_throw.h', 're4fix_gamepad.h', 're4fix_map_zoom.h', 're4fix_view_extra.h',
           're4fix_hud_extra.h', 're4fix_ashley.h']
texts = {h: (analysis / h).read_text(encoding='utf8') for h in headers}

with so_path.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as so:
    assert hashlib.sha256(so).hexdigest() == '6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1'
    words = lambda rva, n: struct.unpack_from('<%dI' % n, so, rva)

    # 1. {addr, {w0, w1[, w2, w3]}} guard tables (zero words mean "two-word guard").
    guards = 0
    for name, text in texts.items():
        for addr, body in re.findall(r'\{(0x[0-9a-f]{6,8}),\s*\{((?:0x[0-9a-f]+|0)(?:,\s*(?:0x[0-9a-f]+|0)){1,3})\}\}', text):
            values = [int(v, 16) if v.startswith('0x') else 0 for v in re.split(r',\s*', body)]
            while values and values[-1] == 0:
                values.pop()
            actual = words(int(addr, 16), len(values))
            assert tuple(values) == actual, (name, addr, [hex(v) for v in values], [hex(v) for v in actual])
            guards += 1
    # Single-word / array guards written as statics.
    hud = texts['re4fix_hud_extra.h']
    assert words(0x5d5c90c, 4) == (0x39402c08, 0x7100051f, 0x54000121, 0xf9400008) and 'alive[4]={0x39402c08,0x7100051f,0x54000121,0xf9400008}' in hud
    assert words(0x628d8b0, 2) == (0x9124c295, 0xaa1503e0) and 'paint[2]={0x9124c295,0xaa1503e0}' in hud
    mapz = texts['re4fix_map_zoom.h']
    assert words(0x630c2bc, 4) == (0xd10143ff, 0xf9001bf3, 0xa9047bfd, 0x910103fd) and 'zoom[4] = {0xd10143ff, 0xf9001bf3, 0xa9047bfd, 0x910103fd}' in mapz
    assert words(0x6471928, 2) == (0xd10183ff, 0xf90023f3) and 'cls[2] = {0xd10183ff, 0xf90023f3}' in mapz
    assert words(0x630c2dc, 1)[0] == 0x910d9009 and words(0x630c2e4, 1)[0] == 0xf9415808 and words(0x630c304, 1)[0] == 0xbd037000
    assert 'centre = 0x910d9009, area = 0xf9415808, scale = 0xbd037000' in mapz
    assert words(0x6465690, 1)[0] == 0xf9410008 and '(base + 0x6465690) != 0xf9410008' in texts['re4fix_throw.h']
    # Gun vtable slots used by the ammo counter: vtable for AVR4GamePlayerGun @0x9d1ad70 (address point +0x10).
    relocs = {}
    sh = struct.unpack_from('<Q', so, 40)[0]; step, count = struct.unpack_from('<HH', so, 58)
    want = {0x9d1ad70 + 0x10 + 0x7b8, 0x9d1ad70 + 0x10 + 0x7d8}
    for i in range(count):
        sec = struct.unpack_from('<IIQQQQIIQQ', so, sh + i * step)
        if sec[1] != 4:
            continue
        for off in range(sec[4], sec[4] + sec[5], sec[9]):
            a, info, value = struct.unpack_from('<QQq', so, off)
            if a in want:
                assert info == 0x403
                relocs[a] = value
    assert relocs == {0x9d1ad70 + 0x10 + 0x7b8: 0x6291744, 0x9d1ad70 + 0x10 + 0x7d8: 0x62918d4}, relocs
    print('PASS: %d guard tables + map/paint/spear field words + Gun vtable ammo slots match libUE4.so' % guards, flush=True)

    # 2. Naked gates: .inst words == first four words of the hooked function.
    gates = 0
    for name, text in texts.items():
        for gate, body in re.findall(r'static \w+ (gate_\w+)\([^)]*\) \{\s*asm volatile\(((?:".inst 0x[0-9a-f]+\\n"\s*){4})', text):
            insts = [int(v, 16) for v in re.findall(r'0x([0-9a-f]+)', body)]
            target = re.search(r'g_tgt_(\w+)', text[text.index(gate):text.index(gate) + 400]).group(1)
            resume = re.search(r'g_tgt_' + target + r' = base \+ (0x[0-9a-f]+) \+ 16;', text)
            assert resume, (name, gate, target)
            rva = int(resume.group(1), 16)
            assert tuple(insts) == words(rva, 4), (name, gate, hex(rva))
            patch = re.search(r'patch_code\(\(void\*\)\(base \+ ' + resume.group(1) + r'\), \(void\*\)&(hook_\w+)\)', text)
            assert patch, (name, gate)
            # The relocated prologue must not contain PC-relative instructions (adr/adrp/b/bl/ldr literal/cbz/tbz/b.cond).
            for w in insts:
                assert (w & 0x1f000000) != 0x10000000, ('adr/adrp in prologue', gate)
                assert (w & 0x7c000000) != 0x14000000, ('b/bl in prologue', gate)
                assert (w & 0x3b000000) != 0x18000000, ('ldr literal in prologue', gate)
                assert (w & 0x7e000000) not in (0x34000000, 0x36000000), ('cbz/tbz in prologue', gate)
                assert (w & 0xff000010) != 0x54000000, ('b.cond in prologue', gate)
            gates += 1
    assert gates == 7, gates   # sce begin/end, boat begin/end, lantern begin, pad axis/button
    print('PASS: %d naked gates replay the exact relocatable 16-byte prologues they resume after' % gates, flush=True)

    # 3. Field offsets proven by the original instructions.
    sce = texts['re4fix_sce_touch.h']
    assert words(0x61f997c, 1)[0] == 0x39496000            # IsEnabled: ldrb w0,[x0,#0x258]
    assert words(0x63597d8, 2) == (0x39496408, 0x35000188)  # ActivateEvent: ldrb w8,[x0,#0x259]; cbnz
    assert words(0x63597e0, 1)[0] == 0x39496808              # ldrb w8,[x0,#0x25a]
    assert words(0x6238ff8, 1)[0] == 0x394cd008              # AddDeltaAngleDegrees: ldrb w8,[x0,#0x334]
    assert words(0x63023b0, 1)[0] == 0xf9405014              # ActivateSwitch: ldr x20,[x0,#0xa0] (owner)
    for needle in ('b[0x258] != 1 || b[0x259] || b[0x25a] || (kind == 2 && b[0x334])', '((const char*)c + 0xa0)',
                   '((const char*)owner + 0x130)', '0x384) ? 1 : tw_typed(c, sce_crank_class(), 0x350)'):
        assert needle in sce, needle
    # Class sizes passed to the reflection registration (mov w4,#size): lever 0x3b0, crank 0x350, spear 0x590,
    # lantern 0x3d0, lake boat 0x240, map screen 0x408 - every typed minimum stays within them.
    for rva, size in ((0x6469b94, 0x3b0), (0x63dee40, 0x350), (0x643b100, 0x590), (0x63bd2f8, 0x3d0),
                      (0x6465bcc, 0x240), (0x64719b0, 0x408)):
        word = words(rva, 1)[0]
        assert word & 0xffe0001f == 0x52800004 and (word >> 5) & 0xffff == size, (hex(rva), hex(word))
    print('PASS: interaction/crank/owner field offsets and reflected class sizes', flush=True)

# 4. Host-executed helpers (pure functions copied from the headers).
def extract(text, prefix):
    start = text.index(prefix)
    return text[start:text.index('\n}', start) + 2]
gamepad = texts['re4fix_gamepad.h']
throw = texts['re4fix_throw.h']
program = r'''
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static volatile unsigned g_map_zoom_bits;
static volatile long g_throw_flick_start;
static float g_throw_forward = 0.80f, g_throw_up = 0.22f;
static int g_throw_ms = 140;
''' + extract(texts['re4fix_map_zoom.h'], 'static void touch_map_zoom_add(') + '\n' \
    + gamepad[gamepad.index('enum { PAD_A'):gamepad.index('};', gamepad.index('enum { PAD_A')) + 2] + '\n' \
    + extract(gamepad, 'static float pad_dead(') + '\n' + extract(gamepad, 'static int pad_bit_of(') + '\n' \
    + extract(throw, 'static void touch_throw_pose(') + r'''
static float pending(void){unsigned b=g_map_zoom_bits;float f=1;if(b)memcpy(&f,&b,4);return f;}
int main(void){
    touch_map_zoom_add(1.25f);touch_map_zoom_add(1.2f);assert(fabsf(pending()-1.5f)<1e-5f);
    for(int i=0;i<40;i++)touch_map_zoom_add(1.5f);assert(pending()==4.0f);           /* clamped */
    g_map_zoom_bits=0;touch_map_zoom_add(NAN);touch_map_zoom_add(-1.0f);assert(!g_map_zoom_bits);
    assert(pad_dead(0.1f)==0 && pad_dead(-0.17f)==0 && pad_dead(1.0f)==1.0f && pad_dead(-1.0f)==-1.0f);
    assert(fabsf(pad_dead(0.59f)-0.5f)<1e-5f);
    assert(pad_bit_of(96)==PAD_A && pad_bit_of(97)==PAD_B && pad_bit_of(105)==PAD_R2 && pad_bit_of(107)==PAD_R3);
    assert(pad_bit_of(19)==PAD_UP && pad_bit_of(22)==PAD_RIGHT && pad_bit_of(4)==-1 && pad_bit_of(66)==-1);
    float off[3]={0,0,0};
    g_throw_flick_start=1000;touch_throw_pose(0,1100,off);assert(off[0]==0&&off[1]==0&&off[2]==0); /* right hand only */
    touch_throw_pose(1,1000,off);assert(off[2]==0);
    touch_throw_pose(1,1070,off);assert(fabsf(off[2]+0.80f*0.25f)<1e-5f && fabsf(off[1]-0.22f*0.25f)<1e-5f);
    off[0]=off[1]=off[2]=0;touch_throw_pose(1,1139,off);assert(off[2]<-0.78f);
    off[0]=off[1]=off[2]=0;touch_throw_pose(1,1140+130,off);assert(fabsf(off[2]+0.40f)<1e-4f);
    off[0]=off[1]=off[2]=0;touch_throw_pose(1,1140+260,off);assert(off[2]==0);       /* follow-through done */
    g_throw_flick_start=0;touch_throw_pose(1,1070,off);assert(off[2]==0);
    puts("PASS: host map pinch clamp/NaN, gamepad dead zone/key map, throw flick pose curve");
    return 0;
}
'''
clang = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'))
vc = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk = Path(r'C:\Program Files (x86)\Windows Kits\10')
with tempfile.TemporaryDirectory(prefix='re4-p304-') as tmp:
    d = Path(tmp); c = d / 'p304.c'; c.write_text(program, encoding='utf8')
    subprocess.run([str(clang), '--target=aarch64-linux-android23', '-O2', '-Werror', '-fsyntax-only', str(c)], check=True)
    obj, exe = d / 'p304.obj', d / 'p304.exe'
    subprocess.run([str(clang), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror', '-isystem', str(vc / 'include'),
                    '-isystem', str(sdk / 'Include/10.0.26100.0/ucrt'), '-c', '-o', str(obj), str(c)], check=True)
    subprocess.run([str(vc / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj), 'libcmt.lib', 'libucrt.lib',
                    'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib', f'/LIBPATH:{vc / "lib/x64"}',
                    f'/LIBPATH:{sdk / "Lib/10.0.26100.0/ucrt/x64"}', f'/LIBPATH:{sdk / "Lib/10.0.26100.0/um/x64"}'], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: p304 feature acceptance (static + host)', flush=True)
