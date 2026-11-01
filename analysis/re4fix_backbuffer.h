// Establish the SDR window format before RHI creates any backbuffer or Slate pipeline.
#ifndef RE4FIX_BACKBUFFER_H
#define RE4FIX_BACKBUFFER_H
#include <stdint.h>
#include <string.h>

static uintptr_t backbuffer_base;
static int backbuffer_installed,backbuffer_prepared;
static unsigned backbuffer_logs;
static void* (*backbuffer_find)(void*,const unsigned short*,int);
static int (*backbuffer_get)(const void*);
static void (*backbuffer_set)(void*,const unsigned short*,unsigned);

static int backbuffer_before_rhi(uintptr_t caller,int argument,unsigned thread) {
    uintptr_t base=backbuffer_base;
    if (!base || backbuffer_prepared || caller!=base+0x54c4c2cul || (unsigned)argument>1
        || *(void* const*)(base+0xa74e6f8)
        || !*(const unsigned char*)(base+0xa6eb068)
        || *(const unsigned*)(base+0xa6eb058)!=thread) {
        if (base && !backbuffer_prepared && backbuffer_logs++<4)
            LOGF("BACKBUFFER startup context declined caller=%p bool=%d thread=%u",(void*)caller,argument,thread);
        return 0;
    }
    void* manager=*(void* const*)(base+0xa6b1ac0);
    if (!manager || !backbuffer_find || !backbuffer_get || !backbuffer_set) {
        if (backbuffer_logs++<4) LOGF("BACKBUFFER startup native console unavailable");return 0;
    }
    static const unsigned short names[2][31]={
        {'r','.','D','e','f','a','u','l','t','B','a','c','k','B','u','f','f','e','r','P','i','x','e','l','F','o','r','m','a','t',0},
        {'r','.','H','D','R','.','E','n','a','b','l','e','H','D','R','O','u','t','p','u','t',0},
    };
    void* variables[2];unsigned flags[2];int before[2],after[2];
    // Validate BOTH native variables before either setter can change their values or priority.
    for (unsigned i=0;i<2;i++) {
        variables[i]=backbuffer_find(manager,names[i],0);
        if (!variables[i] || *(const uintptr_t*)variables[i]!=base+0x9e20d30ul) {
            if (backbuffer_logs++<4) LOGF("BACKBUFFER startup native CVar class unavailable field=%u",i);return 0;
        }
        flags[i]=*(const unsigned*)((const char*)variables[i]+0x18);
        before[i]=backbuffer_get(variables[i]);
        // Default is READONLY4; HDR is RenderThreadSafe32, without READONLY/Unregistered.
        if ((i ? (flags[i]&0x2cu)!=0x20u : (flags[i]&12u)!=4u)
            || (before[i] && (flags[i]&0xff000000u)>0x08000000u)
            || (unsigned)before[i]>(i ? 1u : 4u)) {
            if (backbuffer_logs++<4) LOGF("BACKBUFFER startup refused field=%u value=%d flags=%08x",i,before[i],flags[i]);
            return 0;
        }
    }
    for (unsigned i=0;i<2;i++) {
        // Promote even an existing0 so later lower-priority game/config settings cannot select HDR/10-bit.
        if (before[i] || (flags[i]&0xff000000u)<0x08000000u) {
            static const unsigned short zero[]={'0',0};
            backbuffer_set(variables[i],zero,0x08000000u);
        }
        after[i]=backbuffer_get(variables[i]);
    }
    backbuffer_prepared=after[0]==0 && after[1]==0;
    if (backbuffer_logs++<4) LOGF("BACKBUFFER before native RHIInit(bool=%d) default=%d->%d flags=%08x->%08x HDR=%d->%d flags=%08x->%08x accepted=%d requestedNativePF=2 SDR",
        argument,before[0],after[0],flags[0],*(const unsigned*)((const char*)variables[0]+0x18),
        before[1],after[1],flags[1],*(const unsigned*)((const char*)variables[1]+0x18),backbuffer_prepared);
    return backbuffer_prepared;
}

#if defined(__aarch64__)
__attribute__((used)) static uintptr_t backbuffer_native_tail;
__attribute__((naked)) static void backbuffer_native_init(int argument) {
    __asm__ volatile(
        ".inst 0xd10203ff\n" ".inst 0xa90557f6\n" ".inst 0xa9064ff4\n" ".inst 0xa9077bfd\n"
        "adrp x16, backbuffer_native_tail\n" "ldr x16, [x16, :lo12:backbuffer_native_tail]\n" "br x16\n");
}
static void backbuffer_rhi_init(int argument) {
    backbuffer_before_rhi((uintptr_t)__builtin_return_address(0),argument,(unsigned)gettid());
    backbuffer_native_init(argument);
}
#endif

static int install_backbuffer(uintptr_t base) {
    if (!base || backbuffer_installed) return backbuffer_installed;
    static const struct {unsigned long address;unsigned words[2];} checks[]={
        {0x6ed4148,{0xd10203ff,0xa90557f6}},{0x6ed4150,{0xa9064ff4,0xa9077bfd}},
        {0x54c4c24,{0x2a1f03e0,0x94683d48}},
        {0x65e9ba0,{0xf81e0ff3,0xa9017bfd}},
        {0x65e7940,{0xf81e0ff3,0xa9017bfd}},
        {0x65ee9f0,{0xf81d0ff5,0xa9014ff4}},
        {0x75581cc,{0x52800404,0x2a1f03e2}},
    };
    for (unsigned i=0;i<sizeof checks/sizeof *checks;i++)
        if (memcmp((const void*)(base+checks[i].address),checks[i].words,sizeof checks[i].words)) {
            LOGF("BACKBUFFER disabled: native mismatch +%lx",checks[i].address);return 0;
        }
    if (*(void* const*)(base+0xa74e6f8)) {LOGF("BACKBUFFER startup hook too late: native RHI exists");return 0;}
    backbuffer_base=base;
    backbuffer_find=(void* (*)(void*,const unsigned short*,int))(base+0x65e9ba0);
    backbuffer_get=(int (*)(const void*))(base+0x65e7940);
    backbuffer_set=(void (*)(void*,const unsigned short*,unsigned))(base+0x65ee9f0);
#if defined(__aarch64__)
    backbuffer_native_tail=base+0x6ed4158;
    if (patch_code((void*)(base+0x6ed4148),(void*)backbuffer_rhi_init)) {
        backbuffer_base=0;LOGF("BACKBUFFER disabled: RHIInit hook write failed");return 0;
    }
    backbuffer_installed=1;
    LOGF("BACKBUFFER ready: original default-window/HDR CVars before original RHIInit");
#endif
    return backbuffer_installed;
}

#ifdef RE4_QA_USER_TRANSITIONS
static unsigned backbuffer_debug_pending,backbuffer_debug_count;
static struct {
    uintptr_t variable,variable_class,hdr_data,hdr_class,rhi;
    unsigned flags,hdr_flags,thread,installed,prepared,hdr_format;
    int values[2],hdr_values[2],variable_valid,hdr_valid;
    unsigned char hdr_enabled,hdr_supported;
} backbuffer_debug_last;
static void backbuffer_debug_request(void) {
    if (__atomic_load_n(&backbuffer_debug_count,__ATOMIC_ACQUIRE)<8)
        __atomic_store_n(&backbuffer_debug_pending,1,__ATOMIC_RELEASE);
}
static void backbuffer_debug_tick(void) {
    uintptr_t base=backbuffer_base;
    if (!base || !*(const unsigned char*)(base+0xa6eb068)
        || *(const unsigned*)(base+0xa6eb058)!=(unsigned)gettid()) return;
    if (!__atomic_exchange_n(&backbuffer_debug_pending,0,__ATOMIC_ACQ_REL)
        || __atomic_load_n(&backbuffer_debug_count,__ATOMIC_RELAXED)>=8) return;
    memset(&backbuffer_debug_last,0,sizeof backbuffer_debug_last);
    backbuffer_debug_last.variable=*(const uintptr_t*)(base+0xa7dd318);
    uintptr_t variable=backbuffer_debug_last.variable;
    if (variable>=0x10000 && variable<=UINTPTR_MAX-0x40 && !(variable&7)) {
        backbuffer_debug_last.variable_class=*(const uintptr_t*)variable;
        if (backbuffer_debug_last.variable_class==base+0x9e20d30ul) {
            backbuffer_debug_last.variable_valid=1;
            backbuffer_debug_last.flags=*(const unsigned*)(variable+0x18);
            memcpy(backbuffer_debug_last.values,(const void*)(variable+0x34),8);
        }
    }
    backbuffer_debug_last.hdr_data=*(const uintptr_t*)(base+0xa76dca8);
    uintptr_t hdr=backbuffer_debug_last.hdr_data;
    // Native FConsoleVariable<int>::AsVariableInt returns its embedded data at+0x34.
    if (hdr>=0x10034 && hdr<=UINTPTR_MAX-8 && !((hdr-0x34)&7)) {
        backbuffer_debug_last.hdr_class=*(const uintptr_t*)(hdr-0x34);
        if (backbuffer_debug_last.hdr_class==base+0x9e20d30ul) {
            backbuffer_debug_last.hdr_valid=1;
            backbuffer_debug_last.hdr_flags=*(const unsigned*)(hdr-0x34+0x18);
            memcpy(backbuffer_debug_last.hdr_values,(const void*)hdr,8);
        }
    }
    backbuffer_debug_last.rhi=*(const uintptr_t*)(base+0xa74e6f8);
    backbuffer_debug_last.thread=*(const unsigned*)(base+0xa6eb058);
    backbuffer_debug_last.installed=backbuffer_installed;backbuffer_debug_last.prepared=backbuffer_prepared;
    backbuffer_debug_last.hdr_format=*(const unsigned*)(base+0xa41322c);
    backbuffer_debug_last.hdr_enabled=*(const unsigned char*)(base+0xa75079b);
    backbuffer_debug_last.hdr_supported=*(const unsigned char*)(base+0xa75079c);
    unsigned count=__atomic_add_fetch(&backbuffer_debug_count,1,__ATOMIC_RELEASE);
    LOGF("BACKBUFFER QA snapshot=%u/8 default=%p class=%p valid=%d flags=%08x GT=%d RT=%d HDRdata=%p HDRclass=%p HDRvalid=%d HDRflags=%08x HDR_GT=%d HDR_RT=%d",
         count,(void*)variable,(void*)backbuffer_debug_last.variable_class,backbuffer_debug_last.variable_valid,
         backbuffer_debug_last.flags,backbuffer_debug_last.values[0],backbuffer_debug_last.values[1],(void*)hdr,
         (void*)backbuffer_debug_last.hdr_class,backbuffer_debug_last.hdr_valid,backbuffer_debug_last.hdr_flags,
         backbuffer_debug_last.hdr_values[0],backbuffer_debug_last.hdr_values[1]);
    LOGF("BACKBUFFER QA native rhi=%p preRHI=%d installed=%u prepared=%u GTid=%u HDRenabled=%u HDRsupported=%u HDRformat=%u",
         (void*)backbuffer_debug_last.rhi,!backbuffer_debug_last.rhi,backbuffer_debug_last.installed,
         backbuffer_debug_last.prepared,backbuffer_debug_last.thread,backbuffer_debug_last.hdr_enabled,
         backbuffer_debug_last.hdr_supported,backbuffer_debug_last.hdr_format);
}
#endif
#endif
