"""Execute the original sync-slot selector and Vulkan pacing guards offline. No ADB."""
from pathlib import Path
import hashlib
import os
import re
import struct
import subprocess
import tempfile

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_SP, UC_ARM64_REG_W0, UC_ARM64_REG_W21, UC_ARM64_REG_X0, UC_ARM64_REG_X19

root = Path(__file__).resolve().parents[2]
native = (root / "apk_decoded/lib/arm64-v8a/libUE4.so").read_bytes()
assert hashlib.sha256(native).hexdigest() == "6902538e4a8266be0cfbbd9ef42829ae44fa1b9c10839809def450f45fd555a1"
for address, size, expected in (
    (0x6544438, 968, "bd4804fa5f5d1224f55107f131b78e8a0c8a99f94563b7264c31d1e82317e625"),
    (0x6544250, 488, "cc40736c2a7a0a76a74f5a274467d7ff3012f616438d57e198063f11a19126b8"),
    (0x6f1a13c, 100, "7d8af84369612091547920497002851bc22e3077af2657658b7a1f79a3a68886"),
):
    assert hashlib.sha256(native[address:address + size]).hexdigest() == expected

got = {
    0xa372558: 0xa6eb020,  # GStartTime
    0xa372560: 0xa40d418,  # extension pacer
    0xa372568: 0xa40d410,  # render-thread CPU pacer
    0xa372570: 0xa40d414,  # RHI CPU pacer
    0xa372588: 0xa5e7510,  # force pacing without vsync
    0xa37d1f8: 0xa75e600,  # TAutoConsoleVariable<int> CVarRHISyncInterval
    0xa352000: 0xa6eb068,  # game-thread ID initialized
    0xa352008: 0xa6eb058,  # game-thread ID
}
shoff = struct.unpack_from("<Q", native, 40)[0]
stride, count = struct.unpack_from("<HH", native, 58)
sections = [struct.unpack_from("<IIQQQQIIQQ", native, shoff + i * stride) for i in range(count)]
found = {}
for section in sections:
    if section[1] != 4:
        continue
    for at in range(section[4], section[4] + section[5], section[9]):
        address, info, addend = struct.unpack_from("<QQq", native, at)
        if address in got:
            assert info == 1027 and addend == got[address]
            found[address] = addend
assert found == got

machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
for page in (0x6544000, 0x6f1a000, 0x9bf2000, 0xa372000, 0xa37d000, 0xa352000,
             0xa40d000, 0xa5e7000, 0xa6eb000, 0xa75e000, 0x50000000, 0x50010000,
             0x50020000, 0x50021000, 0x60000000, 0x70000000):
    machine.mem_map(page, 4096)
for page in (0x6544000, 0x6f1a000):
    machine.mem_write(page, native[page:page + 4096])
for address, value in got.items():
    machine.mem_write(address, struct.pack("<Q", value))
machine.mem_write(0xa75e610, struct.pack("<Q", 0x50000000))
machine.mem_write(0x50010008, struct.pack("<Q", 0x50020000))
machine.mem_write(0xa6eb058, struct.pack("<I", 11))
machine.mem_write(0x9bf2ee0, struct.pack("<I", 0xd65f03c0))
destinations = set()
stopped = []
caller_thread = [7]

def stop_at_destination(uc, address, size, opaque):
    if address in destinations:
        stopped.append(address)
        uc.emu_stop()
    elif address == 0x9bf2ee0:  # imported gettid; all selector and branch instructions remain original
        uc.reg_write(UC_ARM64_REG_W0, caller_thread[0])
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

machine.hook_add(UC_HOOK_CODE, stop_at_destination)

def put_int(address, value):
    machine.mem_write(address, struct.pack("<i", value))

def run(entry, stops, tid=7):
    destinations.clear()
    destinations.update(stops)
    stopped.clear()
    caller_thread[0] = tid
    machine.reg_write(UC_ARM64_REG_SP, 0x60000f00)
    machine.reg_write(UC_ARM64_REG_LR, 0x70000000)
    machine.reg_write(UC_ARM64_REG_X0, 0x50010000)
    machine.reg_write(UC_ARM64_REG_X19, 0x50010000)
    machine.emu_start(entry, 0x70000004, count=200)
    assert len(stopped) == 1 and machine.reg_read(UC_ARM64_REG_PC) == stopped[0]
    return stopped[0]

# Both values are genuine native int slots. The selector uses the initialized flag and actual thread ID.
put_int(0x50000000, 2)
put_int(0x50000004, 3)
for initialized, tid, expected in ((0, 7, 2), (0, 11, 2), (1, 11, 2), (1, 7, 3)):
    machine.mem_write(0xa6eb068, bytes([initialized]))
    run(0x6f1a13c, {0x70000000}, tid)
    assert machine.reg_read(UC_ARM64_REG_W0) == expected
print("PASS: original RHIGetSyncInterval selects GT/non-GT slots from initialized flag and real caller thread ID")

machine.mem_write(0xa6eb068, b"\1")
put_int(0xa40d418, 0)
guard_cases = 0
for interval in (-1, 0, 1, 2, 3):
    put_int(0x50000004, interval)
    for lock in (0, 1):
        machine.mem_write(0x50010068, bytes([lock]))
        for force in (0, 1):
            put_int(0xa5e7510, force)
            for enabled in (0, 1):
                eligible = bool((lock or force) and interval >= 1 and enabled)
                put_int(0xa40d414, enabled)
                reached = run(0x65444c4, {0x6544574, 0x654468c})
                assert (reached == 0x6544574) == eligible
                expected_interval = interval if lock or force else 0
                assert machine.reg_read(UC_ARM64_REG_W21) == (expected_interval & 0xffffffff)
                put_int(0xa40d410, enabled)
                reached = run(0x6544280, {0x65442b4, 0x6544404})
                assert (reached == 0x65442b4) == eligible
                guard_cases += 2
print(f"PASS: {guard_cases} actual render-thread/RHI native guard cases; no clock, sleep, or Vulkan driver call executed")

# ScheduleNextFrame can be entered even for syncInterval 0. That is eligibility, not proof of a timed pNext.
for enabled in (0, 1):
    put_int(0xa40d418, enabled)
    for feature in (0, 0x40):
        machine.mem_write(0x50021a80, bytes([feature]))
        reached = run(0x65444c4, {0x6544500, 0x6544574, 0x654468c})
        assert (reached == 0x6544500) == bool(enabled and feature)
print("PASS: original extension scheduling guard requires both live enable and device feature bit; sync duration remains separate")

# Compile and execute the actual QA header. No function is replaced inside the snapshot implementation.
header_path = root / "analysis/research/qa_frame_pacing.h"
header = header_path.read_text(encoding="utf8")
arrays = {}
for name, body in re.findall(r"static const uint32_t (qa_fp_\w+)\[\]=\{(.*?)\};", header, re.S):
    arrays[name] = bytes().join(struct.pack("<I", int(word, 16)) for word in re.findall(r"0x[0-9a-f]+", body))
checks = re.findall(r"\{(0x[0-9a-f]+),(qa_fp_\w+),sizeof \2\}", header)
assert len(checks) == 10
for offset, name in checks:
    address = int(offset, 16)
    assert native[address:address + len(arrays[name])] == arrays[name]
snapshot_code = header[header.index("static void qa_frame_pacing_tick("):header.index("static unsigned qa_frame_pacing_cap_logs")]
assert "mobile_fps_set" not in snapshot_code and "patch_code" not in header
assert "touch_settings_value" not in header and "mobile_fps_applied" not in header
source = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
static unsigned current_thread=123,logs,finds,gets,sets,last_priority;
static int clock_failure,set_failure,priority_failure,find_failure;
static char last_log[1200];
static unsigned char* image;
static uint64_t sync_storage[8],fps_storage[8];
static unsigned char* sync_variable=(unsigned char*)sync_storage;
static unsigned char* fps_variable=(unsigned char*)fps_storage;
static uintptr_t mobile_fps_base;
static void* (*mobile_fps_find)(void*,const unsigned short*,int);
static float (*mobile_fps_get)(const void*);
static void (*mobile_fps_set)(void*,const unsigned short*,unsigned);
static uint64_t touch_settings_value=0x123456789abcULL;
static int mobile_fps_applied=60000;
struct touch_mobile_settings {float fps;};
static float requested_fps=60,native_fps=120;
static struct touch_mobile_settings touch_mobile_settings_snapshot(void){return (struct touch_mobile_settings){requested_fps};}
static int gettid(void){return (int)current_thread;}
static int qa_test_clock(int id,struct timespec* ts){assert(id==1);ts->tv_sec=123;ts->tv_nsec=456789;return clock_failure;}
static void log_snapshot(const char* format,...){va_list ap;va_start(ap,format);vsnprintf(last_log,sizeof last_log,format,ap);va_end(ap);logs++;assert(logs<=8);}
#undef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#define clock_gettime qa_test_clock
#define LOGF(...) log_snapshot(__VA_ARGS__)
#define RE4_QA_USER_TRANSITIONS 1
#include "HEADER_PATH"
enum {IMAGE_SIZE=0xaabf000};
static void* find_fps(void* manager,const unsigned short* name,int tracking){
    const char expected[]="t.MaxFPS";assert(manager==image+0x100 && tracking==0 && current_thread==123);
    for(unsigned i=0;i<sizeof expected;i++)assert(name[i]==(unsigned char)expected[i]);
    finds++;return find_failure?0:fps_variable;
}
static float get_fps(const void* object){assert(object==fps_variable && current_thread==123);gets++;return native_fps;}
static void set_fps(void* object,const unsigned short* value,unsigned flags){
    assert(object==fps_variable && current_thread==123 && flags==(*(unsigned*)(fps_variable+0x18)&0xff000000u));
    int target=value[0]=='0'&&value[1]==0?0:value[0]=='6'&&value[1]=='0'&&value[2]==0?60:-1;assert(target>=0);
    sets++;last_priority=flags;
    if(!set_failure){native_fps=(float)target;memcpy(fps_variable+0x34,&native_fps,4);}
    if(priority_failure)*(unsigned*)(fps_variable+0x18)=0x01000000u;
}
static void ready(void){
    uintptr_t base=(uintptr_t)image;
    memcpy(image+0x6f1a13c,qa_fp_selector,sizeof qa_fp_selector);
    memcpy(image+0x6544280,qa_fp_render_guard,sizeof qa_fp_render_guard);
    memcpy(image+0x65444c4,qa_fp_present_guard,sizeof qa_fp_present_guard);
    memcpy(image+0x65e7a50,qa_fp_int_data,sizeof qa_fp_int_data);
    memcpy(image+0x65e9ba0,qa_fp_find,sizeof qa_fp_find);
    memcpy(image+0x65e7b0c,qa_fp_float_get,sizeof qa_fp_float_get);
    memcpy(image+0x65eec38,qa_fp_float_set,sizeof qa_fp_float_set);
    memcpy(image+0x8d66dac,qa_fp_engine_class,sizeof qa_fp_engine_class);
    memcpy(image+0x8d46710,qa_fp_engine_flags,sizeof qa_fp_engine_flags);
    memcpy(image+0x8c0d3c0,qa_fp_fps_wrapper,sizeof qa_fp_fps_wrapper);
    const uintptr_t refs[][2]={{0xa372560,0xa40d418},{0xa372568,0xa40d410},{0xa372570,0xa40d414},
        {0xa372588,0xa5e7510},{0xa37d1f8,0xa75e600},{0xa352000,0xa6eb068},{0xa352008,0xa6eb058}};
    for(unsigned i=0;i<sizeof refs/sizeof *refs;i++)*(uintptr_t*)(image+refs[i][0])=base+refs[i][1];
    *(unsigned*)(image+0xa6eb058)=123;image[0xa6eb068]=1;
    *(int*)(image+0xa40d410)=1;*(int*)(image+0xa40d414)=0;*(int*)(image+0xa40d418)=1;*(int*)(image+0xa5e7510)=0;
    memset(sync_storage,0xa5,sizeof sync_storage);memset(fps_storage,0x5a,sizeof fps_storage);
    *(uintptr_t*)sync_variable=base+0x9e20d30;*(int*)(sync_variable+0x34)=2;*(int*)(sync_variable+0x38)=1;
    *(uintptr_t*)fps_variable=base+0x9e20df8;
    *(unsigned*)(fps_variable+0x18)=0x08000080u;
    *(float*)(fps_variable+0x34)=*(float*)(fps_variable+0x38)=120;
    *(uintptr_t*)(image+0xa75e610)=(uintptr_t)sync_variable+0x34;
    *(uintptr_t*)(image+0xa6b1ac0)=base+0x100;
    *(uintptr_t*)(image+0xaa76538)=(uintptr_t)fps_variable+0x34;
    *(uintptr_t*)(image+0xaa75db0)=base+0x1000;*(uintptr_t*)(image+0xaabe080)=base+0x4000;
    *(uintptr_t*)(image+0x1010)=base+0x3000;
    *(int*)(image+0x4058)=0xe00;*(int*)(image+0x3058)=0x1000;
    *(int*)(image+0x4038)=2;*(int*)(image+0x3038)=3;
    *(uintptr_t*)(image+0x3030)=base+0x5000;*(uintptr_t*)(image+0x5010)=base+0x4030;
    image[0x17b8]=0x60;*(float*)(image+0x17bc)=60;
    image[0x17c0]=0;image[0x17c8]=1;*(float*)(image+0x17c4)=22;*(float*)(image+0x17cc)=62;
    mobile_fps_base=base;mobile_fps_find=find_fps;mobile_fps_get=get_fps;mobile_fps_set=set_fps;
    qa_frame_pacing_base=0;qa_frame_pacing_requests=0;memset(&qa_frame_pacing_last,0,sizeof qa_frame_pacing_last);
    qa_frame_pacing_cap_logs=0;memset(&qa_frame_pacing_cap_last,0,sizeof qa_frame_pacing_cap_last);
    current_thread=123;clock_failure=set_failure=priority_failure=find_failure=0;requested_fps=60;native_fps=120;
    logs=finds=gets=sets=last_priority=0;last_log[0]=0;
    assert(qa_frame_pacing_install(base));
}
int main(void){
    image=calloc(1,IMAGE_SIZE);assert(image);
    assert(!qa_frame_pacing_install(0) && !qa_frame_pacing_request());
    ready();assert(qa_frame_pacing_install((uintptr_t)image));assert(!qa_frame_pacing_install((uintptr_t)image+8));
    unsigned char* before=malloc(IMAGE_SIZE);assert(before);memcpy(before,image,IMAGE_SIZE);
    unsigned char sync_before[sizeof sync_storage],fps_before[sizeof fps_storage];
    memcpy(sync_before,sync_storage,sizeof sync_before);memcpy(fps_before,fps_storage,sizeof fps_before);
    assert(qa_frame_pacing_request());for(int i=0;i<20;i++)assert(!qa_frame_pacing_request());
    current_thread=124;qa_frame_pacing_tick();assert((qa_frame_pacing_requests&1) && !logs && !finds && !gets);
    current_thread=123;image[0xa6eb068]=0;qa_frame_pacing_tick();assert((qa_frame_pacing_requests&1) && !logs);
    image[0xa6eb068]=2;qa_frame_pacing_tick();assert((qa_frame_pacing_requests&1) && !logs);
    image[0xa6eb068]=1;qa_frame_pacing_tick();assert(logs==1 && finds==1 && gets==1 && !(qa_frame_pacing_requests&1));
    assert(qa_frame_pacing_last.ordinal==1 && qa_frame_pacing_last.thread==123 && qa_frame_pacing_last.game_thread==123);
    assert(qa_frame_pacing_last.clock_valid && qa_frame_pacing_last.monotonic_ns==123000456789LL);
    assert(qa_frame_pacing_last.render_cpu==1 && !qa_frame_pacing_last.rhi_cpu && qa_frame_pacing_last.extension==1 && !qa_frame_pacing_last.force);
    assert(qa_frame_pacing_last.sync_valid && qa_frame_pacing_last.sync_values[0]==2 && qa_frame_pacing_last.sync_values[1]==1);
    assert(qa_frame_pacing_last.requested_valid && qa_frame_pacing_last.requested_fps==60 && qa_frame_pacing_last.native_fps_valid && qa_frame_pacing_last.native_fps==120);
    assert(qa_frame_pacing_last.fps_slots_valid && qa_frame_pacing_last.fps_slots[0]==120 && qa_frame_pacing_last.fps_slots[1]==120 && qa_frame_pacing_last.fps_wrapper_matches);
    assert(qa_frame_pacing_last.engine_valid && qa_frame_pacing_last.engine==(uintptr_t)image+0x1000 && qa_frame_pacing_last.engine_flags==0x60 && qa_frame_pacing_last.fixed_fps==60);
    assert(qa_frame_pacing_last.smooth_min_kind==0 && qa_frame_pacing_last.smooth_max_kind==1 && qa_frame_pacing_last.smooth_min_fps==22 && qa_frame_pacing_last.smooth_max_fps==62);
    assert(strstr(last_log,"syncGT=2 syncNonGT=1") && strstr(last_log,"requestedFPS=60.000 nativeFPS=120.000"));
    assert(strstr(last_log,"fpsGT=120.000 fpsNonGT=120.000") && strstr(last_log,"wrapperMatches=1") && strstr(last_log,"fixed=1 fixedFPS=60.000 smooth=1 smoothMin=0/22.000 smoothMax=1/62.000"));
    for(int i=0;i<20;i++){qa_frame_pacing_request();qa_frame_pacing_tick();}
    assert(logs==8 && finds==8 && gets==8 && qa_frame_pacing_requests==16 && !qa_frame_pacing_request());
    for(int i=0;i<20;i++)qa_frame_pacing_tick();assert(logs==8);
    assert(!memcmp(before,image,IMAGE_SIZE) && !memcmp(sync_before,sync_storage,sizeof sync_before) && !memcmp(fps_before,fps_storage,sizeof fps_before));
    for(unsigned malformed=0;malformed<5;malformed++){
        ready();mobile_fps_base=0;
        if(malformed==0)*(uintptr_t*)(image+0xa75e610)=0;
        if(malformed==1)*(uintptr_t*)(image+0xa75e610)=1;
        if(malformed==2)*(uintptr_t*)(image+0xa75e610)=(uintptr_t)sync_variable+0x35;
        if(malformed==3)*(uintptr_t*)(image+0xa75e610)=UINTPTR_MAX;
        if(malformed==4)*(uintptr_t*)sync_variable=0;
        memcpy(before,image,IMAGE_SIZE);memcpy(sync_before,sync_storage,sizeof sync_before);memcpy(fps_before,fps_storage,sizeof fps_before);
        assert(qa_frame_pacing_request());qa_frame_pacing_tick();
        assert(logs==1 && !finds && !gets && !qa_frame_pacing_last.sync_valid && !qa_frame_pacing_last.native_fps_valid);
        assert(strstr(last_log,"syncGT=unknown syncNonGT=unknown") && strstr(last_log,"nativeFPS=unknown"));
        assert(!memcmp(before,image,IMAGE_SIZE) && !memcmp(sync_before,sync_storage,sizeof sync_before) && !memcmp(fps_before,fps_storage,sizeof fps_before));
    }
    for(unsigned unavailable=0;unavailable<5;unavailable++){
        ready();
        if(unavailable==0)mobile_fps_find=0;
        if(unavailable==1)mobile_fps_get=0;
        if(unavailable==2)*(uintptr_t*)(image+0xa6b1ac0)=1;
        if(unavailable==3)*(uintptr_t*)fps_variable=0;
        if(unavailable==4)native_fps=NAN;
        assert(qa_frame_pacing_request());qa_frame_pacing_tick();
        assert(logs==1 && qa_frame_pacing_last.sync_valid && !qa_frame_pacing_last.native_fps_valid && strstr(last_log,"nativeFPS=unknown"));
    }
    ready();clock_failure=-1;assert(qa_frame_pacing_request());qa_frame_pacing_tick();assert(!qa_frame_pacing_last.clock_valid && strstr(last_log,"mono_ns=unknown"));
    ready();*(float*)(fps_variable+0x38)=60;*(uintptr_t*)(image+0xaa76538)=0;
    assert(qa_frame_pacing_request());qa_frame_pacing_tick();
    assert(qa_frame_pacing_last.fps_slots_valid && qa_frame_pacing_last.fps_slots[0]==120 && qa_frame_pacing_last.fps_slots[1]==60 && !qa_frame_pacing_last.fps_wrapper_matches);
    ready();*(float*)(fps_variable+0x38)=NAN;assert(qa_frame_pacing_request());qa_frame_pacing_tick();
    assert(!qa_frame_pacing_last.fps_slots_valid && strstr(last_log,"fpsGT=unknown fpsNonGT=unknown"));
    for(unsigned malformed=0;malformed<11;malformed++){
        ready();
        if(malformed==0)*(uintptr_t*)(image+0xaa75db0)=0;
        if(malformed==1)*(uintptr_t*)(image+0xaa75db0)=1;
        if(malformed==2)*(uintptr_t*)(image+0xaabe080)=0;
        if(malformed==3)*(uintptr_t*)(image+0x1010)=0;
        if(malformed==4)*(int*)(image+0x4058)=0x10000+1;
        if(malformed==5)*(int*)(image+0x3058)=0x7d0-1;
        if(malformed==6)*(int*)(image+0x4038)=-1;
        if(malformed==7)*(int*)(image+0x4038)=129;
        if(malformed==8)*(int*)(image+0x3038)=1;
        if(malformed==9)*(uintptr_t*)(image+0x3030)=0;
        if(malformed==10)*(uintptr_t*)(image+0x5010)=0;
        memcpy(before,image,IMAGE_SIZE);assert(qa_frame_pacing_request());qa_frame_pacing_tick();
        assert(!qa_frame_pacing_last.engine_valid && !memcmp(before,image,IMAGE_SIZE));
    }
    const uintptr_t signature_addresses[]={0x6f1a13c,0x6544280,0x65444c4,0x65e7a50,0x65e9ba0,0x65e7b0c,0x65eec38,0x8d66dac,0x8d46710,0x8c0d3c0};
    for(unsigned i=0;i<sizeof signature_addresses/sizeof *signature_addresses;i++){
        ready();qa_frame_pacing_base=0;image[signature_addresses[i]]^=1;
        unsigned char corrupted=image[signature_addresses[i]];
        assert(!qa_frame_pacing_install((uintptr_t)image) && !qa_frame_pacing_request() && image[signature_addresses[i]]==corrupted);
    }
    const uintptr_t got_addresses[]={0xa372560,0xa372568,0xa372570,0xa372588,0xa37d1f8,0xa352000,0xa352008};
    for(unsigned i=0;i<sizeof got_addresses/sizeof *got_addresses;i++){
        ready();qa_frame_pacing_base=0;*(uintptr_t*)(image+got_addresses[i])=0;
        assert(!qa_frame_pacing_install((uintptr_t)image) && !qa_frame_pacing_request());
    }
    puts("PASS: actual QA C snapshot: coalescing/eight lifetime cap, matching GT, both sync/FPS slots/native wrapper, checked engine fixed/smooth fields, eleven class/owner refusals, ten preimage/seven GOT refusals, entire native image/CVar bytes unchanged");
    const unsigned priorities[]={0,0x03000080u,0x08000080u,0x09000080u};
    for(unsigned i=0;i<sizeof priorities/sizeof *priorities;i++){
        ready();native_fps=60;*(float*)(fps_variable+0x34)=60;*(unsigned*)(fps_variable+0x18)=priorities[i];
        memcpy(before,image,IMAGE_SIZE);memcpy(fps_before,fps_storage,sizeof fps_before);
        assert(qa_frame_pacing_test_cap(0)&&sets==1&&native_fps==0&&last_priority==(priorities[i]&0xff000000u));
        assert(qa_frame_pacing_cap_last.before==60&&qa_frame_pacing_cap_last.after==0);
        assert(qa_frame_pacing_test_cap(60)&&sets==2&&native_fps==60&&last_priority==(priorities[i]&0xff000000u));
        assert(*(unsigned*)(fps_variable+0x18)==priorities[i]&&!memcmp(before,image,IMAGE_SIZE)&&!memcmp(fps_before,fps_storage,sizeof fps_before));
        assert(touch_settings_value==0x123456789abcULL&&mobile_fps_applied==60000&&requested_fps==60);
        assert(qa_frame_pacing_test_cap(60)&&sets==2); // Already correct: no redundant native setter.
    }
    for(unsigned refusal=0;refusal<17;refusal++){
        ready();int wanted=0;
        if(refusal==0)wanted=-1;if(refusal==1)wanted=120;if(refusal==2)qa_frame_pacing_base=0;
        if(refusal==3)mobile_fps_base=0;if(refusal==4)mobile_fps_find=0;if(refusal==5)mobile_fps_get=0;if(refusal==6)mobile_fps_set=0;
        if(refusal==7)current_thread=124;if(refusal==8)image[0xa6eb068]=0;
        if(refusal==9)*(uintptr_t*)(image+0xa6b1ac0)=0;if(refusal==10)*(uintptr_t*)(image+0xa6b1ac0)=1;
        if(refusal==11)*(uintptr_t*)fps_variable=0;if(refusal==12)*(unsigned*)(fps_variable+0x18)|=4;
        if(refusal==13)*(unsigned*)(fps_variable+0x18)|=8;if(refusal==14)*(unsigned*)(fps_variable+0x18)=0x0a000000u;
        if(refusal==15)native_fps=NAN;if(refusal==16)find_failure=1;
        memcpy(before,image,IMAGE_SIZE);memcpy(fps_before,fps_storage,sizeof fps_before);
        assert(!qa_frame_pacing_test_cap(wanted)&&!sets&&!memcmp(before,image,IMAGE_SIZE)&&!memcmp(fps_before,fps_storage,sizeof fps_before));
        assert(touch_settings_value==0x123456789abcULL&&mobile_fps_applied==60000&&requested_fps==60);
    }
    ready();set_failure=1;assert(!qa_frame_pacing_test_cap(0)&&sets==1&&native_fps==120&&qa_frame_pacing_cap_last.after==120);
    ready();priority_failure=1;assert(!qa_frame_pacing_test_cap(0)&&sets==1&&qa_frame_pacing_cap_last.after==0&&!qa_frame_pacing_cap_last.accepted);
    free(before);free(image);
    puts("PASS: actual QA C cap0/60 roundtrip for4 current priorities, original setter/readback only,17 pre-set refusals plus failed readback/priority detection; packed/requested/applied FPS and all unrelated native bytes unchanged");
    return 0;
}
'''.replace("HEADER_PATH", header_path.as_posix())
sdk_root = Path(os.environ.get("ANDROID_SDK_ROOT", str(Path.home() / "AppData/Local/Android/Sdk")))
llvm = sdk_root / "ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin"
vc = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231")
sdk = Path(r"C:\Program Files (x86)\Windows Kits\10")
with tempfile.TemporaryDirectory(prefix="re4-frame-pacing-qa-") as temporary:
    directory = Path(temporary)
    test = directory / "test.c"
    test.write_text(source, encoding="utf8")
    subprocess.run([str(llvm / "clang.exe"), "--target=aarch64-linux-android23", "-std=c11", "-O2", "-Werror", "-fsyntax-only", str(test)], check=True)
    obj, exe = directory / "test.obj", directory / "test.exe"
    subprocess.run([str(llvm / "clang.exe"), "--target=x86_64-pc-windows-msvc", "-std=c11", "-O2", "-Werror",
        "-isystem", str(vc / "include"), "-isystem", str(sdk / "Include/10.0.26100.0/ucrt"), "-c", "-o", str(obj), str(test)], check=True)
    subprocess.run([str(vc / "bin/Hostx64/x64/link.exe"), "/NOLOGO", f"/OUT:{exe}", str(obj),
        "libcmt.lib", "libucrt.lib", "libvcruntime.lib", "oldnames.lib", "kernel32.lib",
        f"/LIBPATH:{vc / 'lib/x64'}", f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/ucrt/x64'}",
        f"/LIBPATH:{sdk / 'Lib/10.0.26100.0/um/x64'}"], check=True)
    subprocess.run([str(exe)], check=True)
    excluded = directory / "excluded.c"
    excluded.write_text('#include "' + header_path.as_posix() + '"\nint main(void){return 0;}\n', encoding="utf8")
    subprocess.run([str(llvm / "clang.exe"), "--target=aarch64-linux-android23", "-std=c11", "-O2", "-Werror", "-fsyntax-only", str(excluded)], check=True)
print("PASS: all QA preimages match immutable native bytes; actual C header executes on host, compiles for Android, and excludes its dependencies/code without the QA define")
