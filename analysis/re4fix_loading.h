// Keep the Android loading bar responsive while the original engine prepares every pipeline.
static unsigned long long g_loading_state = (2ULL<<32) | 0xffffffffu;
static int g_loading_startup = 1, g_loading_present_wait;
static void (*loading_batch_mode)(int), (*loading_resume)(void);
static void loading_state(int stage,int remaining) {
    __atomic_store_n(&g_loading_state,((unsigned long long)(unsigned)stage<<32)|(unsigned)remaining,__ATOMIC_RELEASE);
}
__attribute__((visibility("default")))
jlong Java_com_epicgames_ue4_MobileLoading_nativeState(JNIEnv* env,jclass type) {
    (void)env;(void)type;
    return (jlong)__atomic_load_n(&g_loading_state,__ATOMIC_ACQUIRE);
}
unsigned long g_tgt_loading_remaining;
__attribute__((naked)) static int gate_loading_remaining(void) {
    asm volatile(".inst 0xfc1b0fe8\n" ".inst 0xa9015ff8\n" ".inst 0xa90257f6\n" ".inst 0xa9034ff4\n"
        "adrp x16, :got:g_tgt_loading_remaining\n" "ldr x16, [x16, :got_lo12:g_tgt_loading_remaining]\n" "ldr x16, [x16]\n" "br x16\n");
}
static int hook_loading_remaining(void) {
    int remaining=gate_loading_remaining();
    if (__atomic_load_n(&g_loading_startup,__ATOMIC_ACQUIRE)) loading_state(4,remaining);
    return remaining;
}
static void hook_loading_fast(void) {
    // Android draws the bar separately, so startup can use the original uncapped precompile mode.
    int mode=__atomic_load_n(&g_loading_startup,__ATOMIC_ACQUIRE) ? 2:1;
    loading_batch_mode(mode);
    loading_resume();
    LOGF("LOADING shader batch mode=%d (original completion gate retained)",mode);
}
static void loading_map_begin(const char* name) {
    __atomic_store_n(&g_loading_present_wait,0,__ATOMIC_RELEASE);
    int stage=!strcmp(name,"/Game/Levels/SplashScreenMap") ? 2:3;
    loading_state(stage,-1);
    mobile_android_call_thunk("AndroidThunkJava_MobileLoading",-1,stage,0);
}
static void loading_map_end(const char* name,int success) {
    if (success && strcmp(name,"/Game/Levels/SplashScreenMap")) {
        __atomic_store_n(&g_loading_startup,0,__ATOMIC_RELEASE);
        // Lobby's logo movie runs before Start is enabled; reveal it with the loaded map.
        // Drain the previously queued swapchain images before revealing the new map.
        __atomic_store_n(&g_loading_present_wait,6,__ATOMIC_RELEASE);
    }
}
static void loading_present(int result) {
    int pending=__atomic_load_n(&g_loading_present_wait,__ATOMIC_ACQUIRE);
    if (result==0 && pending>0 && __atomic_compare_exchange_n(&g_loading_present_wait,&pending,pending-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)
        && pending==1) { loading_state(0,0);LOGF("LOADING map presented; bar complete"); }
}
static void install_loading(unsigned long base) {
    const unsigned remaining[4]={0xfc1b0fe8,0xa9015ff8,0xa90257f6,0xa9034ff4};
    const unsigned fast[6]={0xa9bf7bfd,0x910003fd,0x52800020,0x9430bfda,0xa8c17bfd,0x1430c020};
    if (memcmp((void*)(base+0x6fb2d64),remaining,sizeof remaining) || memcmp((void*)(base+0x6381ef0),fast,sizeof fast)) {
        LOGF("LOADING shader integration disabled: original preimage mismatch");return;
    }
    loading_batch_mode=(void(*)(int))(base+0x6fb1e64);
    loading_resume=(void(*)(void))(base+0x6fb1f84);
    g_tgt_loading_remaining=base+0x6fb2d64+16;
    patch_code((void*)(base+0x6fb2d64),(void*)&hook_loading_remaining);
    patch_code((void*)(base+0x6381ef0),(void*)&hook_loading_fast);
    LOGF("LOADING real pipeline counter and original startup precompile mode installed");
}
