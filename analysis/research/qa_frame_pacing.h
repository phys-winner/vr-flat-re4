// QA only: requested GT snapshots and explicit temporary t.MaxFPS0/60 comparison. No Present hook.
#ifndef RE4_QA_FRAME_PACING_H
#define RE4_QA_FRAME_PACING_H
#ifdef RE4_QA_USER_TRANSITIONS
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static uintptr_t qa_frame_pacing_base;
// bit0: one pending request; upper bits: lifetime accepted count (maximum8).
static unsigned qa_frame_pacing_requests;
static struct {
    long long monotonic_ns;
    unsigned ordinal,thread,game_thread,initialized;
    int clock_valid,render_cpu,rhi_cpu,extension,force;
    uintptr_t sync_data,sync_class;
    int sync_valid,sync_values[2],requested_valid,native_fps_valid;
    float requested_fps,native_fps;
    int fps_slots_valid,fps_wrapper_matches,engine_valid;
    float fps_slots[2],fixed_fps,smooth_min_fps,smooth_max_fps;
    uintptr_t fps_wrapper,engine;
    unsigned engine_flags,smooth_min_kind,smooth_max_kind;
} qa_frame_pacing_last;

static const uint32_t qa_fp_selector[]={
    0xf81e0ff3,0xa9017bfd,0x910043fd,0xf001a308,0x9001a1c9,0xf940fd08,
    0xf9400129,0xf9400913,0x39400129,0x34000169,0x94b3635f,0x9001a1c8,
    0xf9400508,0xb9400108,0x6b08001f,0x1a9f07e8,0xb8687a60,0xa9417bfd,
    0xf84207f3,0xd65f03c0,0xaa1f03e8,0xb8687a60,0xa9417bfd,0xf84207f3,0xd65f03c0
};
static const uint32_t qa_fp_render_guard[]={
    0x3941a008,0x350000a8,0xd001f168,0xf942c508,0xb9400108,0x34000b88,
    0x942757a9,0x7100041f,0x54000b2b,0xd001f168,0xf942b508,0xb9400108,0x34000aa8
};
static const uint32_t qa_fp_present_guard[]={
    0x3941a268,0x35000328,0xd001f168,0xf942c508,0xb9400108,0x350002a8,
    0x2a1f03f5,0xd001f168,0xf942b108,0xb9400108,0x340002c8,0xf9400668,
    0x52835009,0x38696908,0x36300248,0xf9403a60,0xb9407a61,0x2a1503e2,
    0x97fffe66,0xf9403a68,0xf9403509,0x91004108,0xf100013f,0x9a8803e8,
    0xf9000be8,0x14000007,0x94275704,0x2a0003f5,0xd001f168,0xf942b108,
    0xb9400108,0x35fffd88,0xd001f178,0xd001f179,0xf9418b18,0xf9418739,
    0x5288481a,0x72a001fa,0x710006bf,0x5400096b,0xd001f168,0xf942b908,0xb9400108,0x340008e8
};
static const uint32_t qa_fp_int_data[]={0x9100d000,0xd65f03c0};
static const uint32_t qa_fp_find[]={0xf81e0ff3,0xa9017bfd};
static const uint32_t qa_fp_float_get[]={
    0xf81e0ff3,0xa9017bfd,0x910043fd,0xf001eb48,0xf9400108,0xaa0003f3,
    0x39400108,0x34000108,0x94d82ced,0xf001eb48,0xf9400508,0xb9400108,
    0x6b08001f,0x1a9f07e8,0x14000002,0xaa1f03e8,0x8b080a68,0xbd403500,
    0xa9417bfd,0xf84207f3,0xd65f03c0
};
static const uint32_t qa_fp_float_set[]={
    0xf81d0ff5,0xa9014ff4,0xa9027bfd,0x910083fd,0xaa0103f5,0x2a0203e1,
    0x2a0203f3,0xaa0003f4,0x97ffe430,0x36000140,0xaa1503e0,0x97ffd5e5,
    0xbd003680,0xaa1403e0,0x2a1303e1,0xa9427bfd,0xa9414ff4,0xf84307f5,
    0x14000005,0xa9427bfd,0xa9414ff4,0xf84307f5,0xd65f03c0
};
static const uint32_t qa_fp_engine_class[]={0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd,0x9000eac8,0xf9404100};
static const uint32_t qa_fp_engine_flags[]={0x395ee008,0x321a0108,0x391ee008,0xd65f03c0,0x395ee008,0x321b0108,0x391ee008,0xd65f03c0};
static const uint32_t qa_fp_fps_wrapper[]={0xb000f348,0xf9429d15};
static int qa_frame_pacing_install(uintptr_t base) {
    uintptr_t installed=__atomic_load_n(&qa_frame_pacing_base,__ATOMIC_ACQUIRE);
    if(installed)return installed==base;
    if(!base || base>UINTPTR_MAX-0xaabe088ul || (base&7))return 0;
    static const struct {uintptr_t at;const uint32_t* words;size_t bytes;} checks[]={
        {0x6f1a13c,qa_fp_selector,sizeof qa_fp_selector},
        {0x6544280,qa_fp_render_guard,sizeof qa_fp_render_guard},
        {0x65444c4,qa_fp_present_guard,sizeof qa_fp_present_guard},
        {0x65e7a50,qa_fp_int_data,sizeof qa_fp_int_data},
        {0x65e9ba0,qa_fp_find,sizeof qa_fp_find},
        {0x65e7b0c,qa_fp_float_get,sizeof qa_fp_float_get},
        {0x65eec38,qa_fp_float_set,sizeof qa_fp_float_set},
        {0x8d66dac,qa_fp_engine_class,sizeof qa_fp_engine_class},
        {0x8d46710,qa_fp_engine_flags,sizeof qa_fp_engine_flags},
        {0x8c0d3c0,qa_fp_fps_wrapper,sizeof qa_fp_fps_wrapper}
    };
    for(unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if(memcmp((const void*)(base+checks[i].at),checks[i].words,checks[i].bytes))return 0;
    static const struct {uintptr_t got,target;} references[]={
        {0xa372560,0xa40d418},{0xa372568,0xa40d410},{0xa372570,0xa40d414},
        {0xa372588,0xa5e7510},{0xa37d1f8,0xa75e600},
        {0xa352000,0xa6eb068},{0xa352008,0xa6eb058}
    };
    for(unsigned i=0;i<sizeof references/sizeof *references;i++)
        if(*(const uintptr_t*)(base+references[i].got)!=base+references[i].target)return 0;
    __atomic_store_n(&qa_frame_pacing_base,base,__ATOMIC_RELEASE);
    return 1;
}
static int qa_frame_pacing_pointer(uintptr_t value,size_t bytes) {
    return value>=0x10000 && value<=UINTPTR_MAX-bytes && !(value&7);
}
static void qa_frame_pacing_engine(uintptr_t base) {
    uintptr_t engine=*(const uintptr_t*)(base+0xaa75db0),expected=*(const uintptr_t*)(base+0xaabe080);
    if(!qa_frame_pacing_pointer(engine,0x7d0) || !qa_frame_pacing_pointer(expected,0x80))return;
    uintptr_t actual=*(const uintptr_t*)(engine+0x10);
    if(!qa_frame_pacing_pointer(actual,0x80))return;
    int expected_size=*(const int*)(expected+0x58),size=*(const int*)(actual+0x58);
    int depth=*(const int*)(expected+0x38),actual_depth=*(const int*)(actual+0x38);
    uintptr_t chain=*(const uintptr_t*)(actual+0x30);
    if(expected_size<0x7d0 || expected_size>0x10000 || size<expected_size || size>0x10000
        || depth<0 || depth>128 || actual_depth<depth || actual_depth>256
        || !qa_frame_pacing_pointer(chain,(size_t)(depth+1)*8)
        || *(const uintptr_t*)(chain+(size_t)depth*8)!=expected+0x30)return;
    qa_frame_pacing_last.engine=engine;qa_frame_pacing_last.engine_valid=1;
    qa_frame_pacing_last.engine_flags=*(const unsigned char*)(engine+0x7b8);
    memcpy(&qa_frame_pacing_last.fixed_fps,(const void*)(engine+0x7bc),4);
    qa_frame_pacing_last.smooth_min_kind=*(const unsigned char*)(engine+0x7c0);
    qa_frame_pacing_last.smooth_max_kind=*(const unsigned char*)(engine+0x7c8);
    memcpy(&qa_frame_pacing_last.smooth_min_fps,(const void*)(engine+0x7c4),4);
    memcpy(&qa_frame_pacing_last.smooth_max_fps,(const void*)(engine+0x7cc),4);
}
static int qa_frame_pacing_request(void) {
    if(!__atomic_load_n(&qa_frame_pacing_base,__ATOMIC_ACQUIRE))return 0;
    unsigned state=__atomic_load_n(&qa_frame_pacing_requests,__ATOMIC_ACQUIRE);
    do {
        if((state&1u) || (state>>1)>=8)return 0;
    } while(!__atomic_compare_exchange_n(&qa_frame_pacing_requests,&state,((state>>1)+1u)*2u+1u,
                                         0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE));
    return 1;
}
static void qa_frame_pacing_tick(void) {
    uintptr_t base=__atomic_load_n(&qa_frame_pacing_base,__ATOMIC_ACQUIRE);
    if(!base || !(__atomic_load_n(&qa_frame_pacing_requests,__ATOMIC_ACQUIRE)&1u))return;
    unsigned initialized=*(const unsigned char*)(base+0xa6eb068);
    unsigned thread=(unsigned)gettid(),gt=*(const unsigned*)(base+0xa6eb058);
    if(initialized!=1 || thread!=gt)return;
    unsigned state=__atomic_fetch_and(&qa_frame_pacing_requests,~1u,__ATOMIC_ACQ_REL);
    if(!(state&1u) || !(state>>1) || (state>>1)>8)return;
    memset(&qa_frame_pacing_last,0,sizeof qa_frame_pacing_last);
    qa_frame_pacing_last.ordinal=state>>1;
    qa_frame_pacing_last.thread=thread;qa_frame_pacing_last.game_thread=gt;
    qa_frame_pacing_last.initialized=initialized;
    struct timespec ts;
    if(!clock_gettime(CLOCK_MONOTONIC,&ts) && ts.tv_sec>=0 && ts.tv_nsec>=0 && ts.tv_nsec<1000000000L
        && (uint64_t)ts.tv_sec<=(uint64_t)(INT64_MAX-999999999LL)/1000000000u) {
        qa_frame_pacing_last.clock_valid=1;
        qa_frame_pacing_last.monotonic_ns=(long long)ts.tv_sec*1000000000LL+ts.tv_nsec;
    }
    qa_frame_pacing_last.render_cpu=__atomic_load_n((const int*)(base+0xa40d410),__ATOMIC_RELAXED);
    qa_frame_pacing_last.rhi_cpu=__atomic_load_n((const int*)(base+0xa40d414),__ATOMIC_RELAXED);
    qa_frame_pacing_last.extension=__atomic_load_n((const int*)(base+0xa40d418),__ATOMIC_RELAXED);
    qa_frame_pacing_last.force=__atomic_load_n((const int*)(base+0xa5e7510),__ATOMIC_RELAXED);
    uintptr_t data=*(const uintptr_t*)(base+0xa75e610);
    qa_frame_pacing_last.sync_data=data;
    // Original FConsoleVariable<int>::AsVariableInt returns its embedded two-slot data at+0x34.
    if(data>=0x10034 && data<=UINTPTR_MAX-8 && !((data-0x34)&7)) {
        uintptr_t cls=*(const uintptr_t*)(data-0x34);
        qa_frame_pacing_last.sync_class=cls;
        if(cls==base+0x9e20d30ul) {
            qa_frame_pacing_last.sync_valid=1;
            memcpy(qa_frame_pacing_last.sync_values,(const void*)data,8);
        }
    }
    float requested=touch_mobile_settings_snapshot().fps;
    if(isfinite(requested) && requested>=24 && requested<=240) {
        qa_frame_pacing_last.requested_valid=1;qa_frame_pacing_last.requested_fps=requested;
    }
    // The existing FPS helper owns the checked console ABI; never call a setter or manufacture a value.
    uintptr_t manager=*(const uintptr_t*)(base+0xa6b1ac0);
    if(mobile_fps_base==base && mobile_fps_find && mobile_fps_get
        && manager>=0x10000 && manager<=UINTPTR_MAX-0x100 && !(manager&7)) {
        static const unsigned short name[]={'t','.','M','a','x','F','P','S',0};
        uintptr_t variable=(uintptr_t)mobile_fps_find((void*)manager,name,0);
        if(variable>=0x10000 && variable<=UINTPTR_MAX-0x40 && !(variable&7)
            && *(const uintptr_t*)variable==base+0x9e20df8ul) {
            memcpy(qa_frame_pacing_last.fps_slots,(const void*)(variable+0x34),8);
            qa_frame_pacing_last.fps_slots_valid=isfinite(qa_frame_pacing_last.fps_slots[0])
                && isfinite(qa_frame_pacing_last.fps_slots[1]) && qa_frame_pacing_last.fps_slots[0]>=0
                && qa_frame_pacing_last.fps_slots[0]<=1000 && qa_frame_pacing_last.fps_slots[1]>=0
                && qa_frame_pacing_last.fps_slots[1]<=1000;
            qa_frame_pacing_last.fps_wrapper=*(const uintptr_t*)(base+0xaa76538);
            qa_frame_pacing_last.fps_wrapper_matches=qa_frame_pacing_last.fps_wrapper==variable+0x34;
            float native=mobile_fps_get((const void*)variable);
            if(isfinite(native) && native>=0 && native<=1000) {
                qa_frame_pacing_last.native_fps_valid=1;qa_frame_pacing_last.native_fps=native;
            }
        }
    }
    qa_frame_pacing_engine(base);
    char monotonic[24]="unknown",sync_gt[16]="unknown",sync_rt[16]="unknown";
    char fps_requested[24]="unknown",fps_native[24]="unknown";
    if(qa_frame_pacing_last.clock_valid)snprintf(monotonic,sizeof monotonic,"%lld",qa_frame_pacing_last.monotonic_ns);
    if(qa_frame_pacing_last.sync_valid) {
        snprintf(sync_gt,sizeof sync_gt,"%d",qa_frame_pacing_last.sync_values[0]);
        snprintf(sync_rt,sizeof sync_rt,"%d",qa_frame_pacing_last.sync_values[1]);
    }
    if(qa_frame_pacing_last.requested_valid)snprintf(fps_requested,sizeof fps_requested,"%.3f",qa_frame_pacing_last.requested_fps);
    if(qa_frame_pacing_last.native_fps_valid)snprintf(fps_native,sizeof fps_native,"%.3f",qa_frame_pacing_last.native_fps);
    char fps_gt[24]="unknown",fps_rt[24]="unknown";
    if(qa_frame_pacing_last.fps_slots_valid) {
        snprintf(fps_gt,sizeof fps_gt,"%.3f",qa_frame_pacing_last.fps_slots[0]);
        snprintf(fps_rt,sizeof fps_rt,"%.3f",qa_frame_pacing_last.fps_slots[1]);
    }
    LOGF("FRAMEPACING QA snapshot=%u/8 mono_ns=%s tid=%u GTid=%u initialized=%u CPUrender=%d CPUrhi=%d extension=%d force=%d sync_data=%p sync_class=%p syncGT=%s syncNonGT=%s requestedFPS=%s nativeFPS=%s fpsGT=%s fpsNonGT=%s wrapper=%p wrapperMatches=%d engine=%p valid=%d flags=%02x fixed=%d fixedFPS=%.3f smooth=%d smoothMin=%u/%.3f smoothMax=%u/%.3f",
         qa_frame_pacing_last.ordinal,monotonic,thread,gt,initialized,qa_frame_pacing_last.render_cpu,
         qa_frame_pacing_last.rhi_cpu,qa_frame_pacing_last.extension,qa_frame_pacing_last.force,
         (void*)data,(void*)qa_frame_pacing_last.sync_class,sync_gt,sync_rt,fps_requested,fps_native,
         fps_gt,fps_rt,(void*)qa_frame_pacing_last.fps_wrapper,
         qa_frame_pacing_last.fps_wrapper_matches,(void*)qa_frame_pacing_last.engine,qa_frame_pacing_last.engine_valid,
         qa_frame_pacing_last.engine_flags,(qa_frame_pacing_last.engine_flags&0x40)!=0,qa_frame_pacing_last.fixed_fps,
         (qa_frame_pacing_last.engine_flags&0x20)!=0,qa_frame_pacing_last.smooth_min_kind,
         qa_frame_pacing_last.smooth_min_fps,qa_frame_pacing_last.smooth_max_kind,qa_frame_pacing_last.smooth_max_fps);
}
static unsigned qa_frame_pacing_cap_logs;
static struct {
    int requested,before_valid,after_valid,accepted;
    float before,after;
    unsigned flags_before,flags_after;
    const char* reason;
} qa_frame_pacing_cap_last;
static int qa_frame_pacing_cap_finish(const char* reason) {
    qa_frame_pacing_cap_last.reason=reason;
    if(qa_frame_pacing_cap_logs<8) {
        qa_frame_pacing_cap_logs++;
        char before[24]="unknown",after[24]="unknown";
        if(qa_frame_pacing_cap_last.before_valid)snprintf(before,sizeof before,"%.3f",qa_frame_pacing_cap_last.before);
        if(qa_frame_pacing_cap_last.after_valid)snprintf(after,sizeof after,"%.3f",qa_frame_pacing_cap_last.after);
        LOGF("FRAMEPACING QA testCap=%d nativeFPS=%s->%s flags=%08x->%08x accepted=%d reason=%s requestedFPS_unchanged",
             qa_frame_pacing_cap_last.requested,before,after,qa_frame_pacing_cap_last.flags_before,
             qa_frame_pacing_cap_last.flags_after,qa_frame_pacing_cap_last.accepted,reason);
    }
    return qa_frame_pacing_cap_last.accepted;
}
// Explicit diagnostic only: compare engine60 soft cap against native FIFO. No mobile/persistent setting write.
static int qa_frame_pacing_test_cap(int fps) {
    memset(&qa_frame_pacing_cap_last,0,sizeof qa_frame_pacing_cap_last);
    qa_frame_pacing_cap_last.requested=fps;
    uintptr_t base=__atomic_load_n(&qa_frame_pacing_base,__ATOMIC_ACQUIRE);
    if(fps!=0 && fps!=60)return qa_frame_pacing_cap_finish("value");
    if(!base || mobile_fps_base!=base || !mobile_fps_find || !mobile_fps_get || !mobile_fps_set)
        return qa_frame_pacing_cap_finish("base_or_helper");
    if(*(const unsigned char*)(base+0xa6eb068)!=1 || *(const unsigned*)(base+0xa6eb058)!=(unsigned)gettid())
        return qa_frame_pacing_cap_finish("thread");
    uintptr_t manager=*(const uintptr_t*)(base+0xa6b1ac0);
    if(manager<0x10000 || manager>UINTPTR_MAX-0x100 || (manager&7))return qa_frame_pacing_cap_finish("manager");
    static const unsigned short name[]={'t','.','M','a','x','F','P','S',0};
    uintptr_t variable=(uintptr_t)mobile_fps_find((void*)manager,name,0);
    if(variable<0x10000 || variable>UINTPTR_MAX-0x40 || (variable&7)
        || *(const uintptr_t*)variable!=base+0x9e20df8ul)return qa_frame_pacing_cap_finish("class");
    unsigned flags=*(const unsigned*)(variable+0x18),priority=flags&0xff000000u;
    qa_frame_pacing_cap_last.flags_before=qa_frame_pacing_cap_last.flags_after=flags;
    if((flags&12u) || priority>0x09000000u)return qa_frame_pacing_cap_finish("flags");
    float before=mobile_fps_get((const void*)variable);
    if(!isfinite(before) || before<0 || before>1000)return qa_frame_pacing_cap_finish("before");
    qa_frame_pacing_cap_last.before_valid=1;qa_frame_pacing_cap_last.before=before;
    if(fabsf(before-(float)fps)>.0005f) {
        static const unsigned short zero[]={'0',0},sixty[]={'6','0',0};
        mobile_fps_set((void*)variable,fps?sixty:zero,priority);
    }
    float after=mobile_fps_get((const void*)variable);
    qa_frame_pacing_cap_last.flags_after=*(const unsigned*)(variable+0x18);
    if(isfinite(after) && after>=0 && after<=1000) {
        qa_frame_pacing_cap_last.after_valid=1;qa_frame_pacing_cap_last.after=after;
        qa_frame_pacing_cap_last.accepted=fabsf(after-(float)fps)<=.0005f
            && (qa_frame_pacing_cap_last.flags_after&0xff000000u)==priority
            && !(qa_frame_pacing_cap_last.flags_after&12u);
    }
    return qa_frame_pacing_cap_finish(qa_frame_pacing_cap_last.accepted?"readback":"readback_failed");
}
#endif
#endif
