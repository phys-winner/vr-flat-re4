// Existing engine JNI wrappers dispatch files and confirmed actions on the UI thread.
#include <jni.h>
#include <stdint.h>
static int touch_mobile_back(void);
static long ptr_now_ms(void);
__attribute__((visibility("default")))
jboolean Java_com_epicgames_ue4_MobileAndroid_nativeBack(JNIEnv* env,jclass type) {
    (void)type;
    if(!env || (*env)->ExceptionCheck(env))return JNI_FALSE;
    return touch_mobile_back() ? JNI_TRUE:JNI_FALSE;
}
static JNIEnv* (*f_mobile_java_env)(_Bool);
static jmethodID (*f_mobile_java_method)(JNIEnv*,jclass,const char*,const char*,_Bool);
static void (*f_mobile_java_void)(JNIEnv*,jobject,jmethodID,...);
static jobject* g_mobile_java_activity;
static jclass* g_mobile_java_class;
static volatile int g_mobile_files_request;
// One atomic job: timestamp32 / token24 / unused1 / Russian1 / kind3 / phase3.
static uint64_t g_mobile_confirm;
static unsigned g_mobile_confirm_token;
enum { MOBILE_CONFIRM_QUEUED=1,MOBILE_CONFIRM_SHOWN=2,MOBILE_CONFIRM_YES=3,MOBILE_CONFIRM_NO=4 };
static int mobile_android_confirm_request(int kind,int russian,long now) {
    if (kind<1 || kind>7 || now<=0) return 0;
    unsigned token=__atomic_add_fetch(&g_mobile_confirm_token,1,__ATOMIC_RELAXED)&0xffffffu;
    if (!token) token=__atomic_add_fetch(&g_mobile_confirm_token,1,__ATOMIC_RELAXED)&0xffffffu;
    uint64_t empty=0,job=((uint64_t)(unsigned)now<<32)|((uint64_t)token<<8)
        |((uint64_t)!!russian<<6)|((unsigned)kind<<3)|MOBILE_CONFIRM_QUEUED;
    return __atomic_compare_exchange_n(&g_mobile_confirm,&empty,job,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static int mobile_android_confirm_take(int kind,long now) {
    uint64_t job=__atomic_load_n(&g_mobile_confirm,__ATOMIC_ACQUIRE);
    if (!job || kind<1 || kind>7 || (((unsigned)job>>3)&7)!=(unsigned)kind) return 0;
    unsigned phase=(unsigned)job&7;
    int expired=now<=0 || (unsigned)((unsigned)now-(unsigned)(job>>32))>120000u;
    if (!expired && phase!=MOBILE_CONFIRM_YES && phase!=MOBILE_CONFIRM_NO) return 0;
    if (!__atomic_compare_exchange_n(&g_mobile_confirm,&job,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return 0;
    return !expired && phase==MOBILE_CONFIRM_YES ? 1:-1;
}
static void mobile_android_confirm_cancel(void) { __atomic_store_n(&g_mobile_confirm,0,__ATOMIC_RELEASE); }
static int mobile_android_confirm_active(int kind) {
    uint64_t job=__atomic_load_n(&g_mobile_confirm,__ATOMIC_ACQUIRE);
    return job && kind>=1 && kind<=7 && (((unsigned)job>>3)&7)==(unsigned)kind;
}
static void mobile_android_confirm_cancel_kind(int kind) {
    uint64_t job=__atomic_load_n(&g_mobile_confirm,__ATOMIC_ACQUIRE);
    unsigned token=(unsigned)job>>8;
    while (job && kind>=1 && kind<=7 && (((unsigned)job>>3)&7)==(unsigned)kind && ((unsigned)job>>8)==token)
        if (__atomic_compare_exchange_n(&g_mobile_confirm,&job,0,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return;
}
__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(JNIEnv* env,jclass type,jint token,jboolean yes) {
    (void)env;(void)type;
    uint64_t job=__atomic_load_n(&g_mobile_confirm,__ATOMIC_ACQUIRE);
    if (token<=0 || ((unsigned)job>>8)!=(unsigned)token || ((unsigned)job&7)!=MOBILE_CONFIRM_SHOWN) return;
    uint64_t reply=(job&~(uint64_t)7)|(yes==JNI_TRUE ? MOBILE_CONFIRM_YES:MOBILE_CONFIRM_NO);
    __atomic_compare_exchange_n(&g_mobile_confirm,&job,reply,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void* (*f_mobile_locale_get)(void);
static const void* (*f_mobile_locale_name)(const void*);
static int g_mobile_locale_logged;
static char g_mobile_language[32];   // native FInternationalization culture, e.g. "en", "ru"
static const char* mobile_android_language(void) { return g_mobile_language; }

static int mobile_android_log_locale(void) {
    if (g_mobile_locale_logged || !f_mobile_locale_get || !f_mobile_locale_name) return 0;
    void* internationalization=f_mobile_locale_get();
    if (!internationalization) return 0;
    void* language=*(void**)((char*)internationalization+0x28);
    if (!language) return 0;
    const void* string=f_mobile_locale_name(language);
    if (!string) return 0;
    const unsigned short* data=*(const unsigned short* const*)string;
    int count=*(const int*)((const char*)string+8),capacity=*(const int*)((const char*)string+12);
    if (!data || count<2 || count>32 || capacity<count || data[count-1]) return 0;
    char text[32];
    for (int index=0;index<count-1;++index) {
        unsigned value=data[index];
        if (!((value>='a' && value<='z') || (value>='A' && value<='Z') || (value>='0' && value<='9') || value=='-' || value=='_')) return 0;
        text[index]=(char)value;
    }
    text[count-1]=0;
    if (language!=*(void**)((char*)internationalization+0x28)) return 0;
    g_mobile_locale_logged=1;
    memcpy(g_mobile_language,text,sizeof text<sizeof g_mobile_language ? sizeof text : sizeof g_mobile_language);
    g_mobile_language[sizeof g_mobile_language-1]=0;
    LOGF("MOBILELOCALE actual native current language='%s'",text);
    return 1;
}

static int mobile_android_call_thunk(const char* thunk,int token,int kind,int russian) {
    if (!f_mobile_java_env || !f_mobile_java_method || !f_mobile_java_void || !g_mobile_java_activity || !g_mobile_java_class) return 0;
    JNIEnv* env=f_mobile_java_env(1);
    if (!env || (*env)->ExceptionCheck(env)) return 0;
    jobject activity=__atomic_load_n(g_mobile_java_activity,__ATOMIC_ACQUIRE);
    jclass klass=__atomic_load_n(g_mobile_java_class,__ATOMIC_ACQUIRE);
    if (!activity || !klass) return 0;
    jobject pinned_activity=(*env)->NewLocalRef(env,activity);
    jclass pinned_class=0;
    int result=0;
    if (!pinned_activity || (*env)->ExceptionCheck(env)) goto done;
    pinned_class=(jclass)(*env)->NewLocalRef(env,(jobject)klass);
    if (!pinned_class || (*env)->ExceptionCheck(env)) goto done;
    if (activity!=__atomic_load_n(g_mobile_java_activity,__ATOMIC_ACQUIRE) || klass!=__atomic_load_n(g_mobile_java_class,__ATOMIC_ACQUIRE) ||
        !(*env)->IsInstanceOf(env,pinned_activity,pinned_class)) goto done;
    // A negative token selects the one-int loading notification; positive tokens are confirmations.
    jmethodID method=f_mobile_java_method(env,pinned_class,thunk,token<0 ? "(I)V":token ? "(IIZ)V":"()V",1);
    if (!method || (*env)->ExceptionCheck(env)) goto done;
    if (activity!=__atomic_load_n(g_mobile_java_activity,__ATOMIC_ACQUIRE) || klass!=__atomic_load_n(g_mobile_java_class,__ATOMIC_ACQUIRE)) goto done;
    if (token<0) f_mobile_java_void(env,pinned_activity,method,(jint)kind);
    else if (token) f_mobile_java_void(env,pinned_activity,method,(jint)token,(jint)kind,(jboolean)!!russian);
    else f_mobile_java_void(env,pinned_activity,method);
    result=!(*env)->ExceptionCheck(env);
done:
    // No unrelated exception was pending on entry; release only these two local references.
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    if (pinned_class) (*env)->DeleteLocalRef(env,(jobject)pinned_class);
    if (pinned_activity) (*env)->DeleteLocalRef(env,pinned_activity);
    LOGF("mobile Java %s request accepted=%d",thunk,result);
    return result;
}
// Native menu button: the original GameActivity opens the save-files/language Activity on its UI thread.
static void mobile_android_request_files(void) {
    int idle=0; __atomic_compare_exchange_n(&g_mobile_files_request,&idle,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static void mobile_android_request_language(void) {
    int idle=0; __atomic_compare_exchange_n(&g_mobile_files_request,&idle,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}

static void touch_mobile_android_tick(void) {
    if (!g_touch_gt_tid || gettid()!=g_touch_gt_tid) return;
    int context=__atomic_load_n(&g_touch_context,__ATOMIC_ACQUIRE);
    uint64_t job=__atomic_load_n(&g_mobile_confirm,__ATOMIC_ACQUIRE);
    if (job) {
        unsigned kind=((unsigned)job>>3)&7,phase=(unsigned)job&7;
        long now=ptr_now_ms();
        int menu=context==2 || context==4 || context==7 || (kind==6 && context==6) || (kind==7 && context==1);
        if (!menu || now<=0 || (unsigned)((unsigned)now-(unsigned)(job>>32))>120000u) {
            uint64_t cancelled=(job&~(uint64_t)7)|MOBILE_CONFIRM_NO;
            __atomic_compare_exchange_n(&g_mobile_confirm,&job,cancelled,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        } else if (phase==MOBILE_CONFIRM_QUEUED) {
            uint64_t shown=(job&~(uint64_t)7)|MOBILE_CONFIRM_SHOWN;
            if (__atomic_compare_exchange_n(&g_mobile_confirm,&job,shown,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)
                && !mobile_android_call_thunk("AndroidThunkJava_ConfirmMobileAction",(unsigned)shown>>8,kind,((unsigned)shown>>6)&1)) {
                uint64_t cancelled=(shown&~(uint64_t)7)|MOBILE_CONFIRM_NO;
                __atomic_compare_exchange_n(&g_mobile_confirm,&shown,cancelled,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
            }
        }
        return;
    }
    if (context!=2 && context!=4 && context!=7) return;
    mobile_android_log_locale();
    if (!f_mobile_java_env || !__atomic_load_n(&g_mobile_files_request,__ATOMIC_ACQUIRE)) return;
    int request=__atomic_exchange_n(&g_mobile_files_request,0,__ATOMIC_ACQ_REL);
    if (request==1 || request==2) mobile_android_call_thunk(request==2 ? "AndroidThunkJava_ShowMobileLanguage":"AndroidThunkJava_ShowMobileFiles",0,0,0);
}

static int mobile_android_keep_startup_language(unsigned long base) {
    static const unsigned original[] = {
        0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xaa0003f4,
        0x9406781b, 0x2a0003f3, 0x36000180, 0xf940d280,
        0xb4000140, 0xb9418808, 0x7100091f, 0x540000cb,
        0x940f3faf, 0xf940d288, 0x91060101, 0x940f41b8,
        0xf940d280, 0x9403a734, 0xa9417bfd, 0x12000260,
        0xa8c24ff4, 0xd65f03c0,
    };
    if (!base || memcmp((const void*)(base+0x626dce8), original, sizeof original)) {
        LOGI("mobile startup language disabled: save-load preimage mismatch"); return 0;
    }
    unsigned* site = (unsigned*)(base+0x626dd24);
    if (protect_mem(site, sizeof *site, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        LOGI("mobile startup language disabled: save-load protection failed"); return 0;
    }
    // System.sav's legacy Language must not override the Android locale already chosen at native startup.
    *site = 0xd503201f;
    __builtin___clear_cache((char*)site, (char*)site+sizeof *site);
    LOGI("mobile startup language ready: Android locale retained across original save load");
    return 1;
}

static void install_mobile_android(unsigned long base) {
    const unsigned int env[4]={0xd100c3ff,0xa9014ff4,0xa9027bfd,0x910083fd};
    const unsigned int method[4]={0xf81e0ff3,0xa9017bfd,0x910043fd,0xaa0003f3};
    const unsigned int call[4]={0xd10483ff,0xa9104ffc,0xa9117bfd,0x910443fd};
    const unsigned int internationalization[4]={0xa9be4ff4,0xa9017bfd,0x910043fd,0xd001e9a8};
    const unsigned int culture_name[2]={0x9100a000,0xd65f03c0};
    const unsigned int language_layout=0xa942cc00;
    mobile_android_keep_startup_language(base);
    f_mobile_locale_get=0;f_mobile_locale_name=0;
    if (!memcmp((void*)(base+0x663dbd4),internationalization,sizeof internationalization) &&
        !memcmp((void*)(base+0x661956c),culture_name,sizeof culture_name) &&
        !memcmp((void*)(base+0x895f44c),&language_layout,sizeof language_layout)) {
        f_mobile_locale_get=(void*(*)(void))(base+0x663dbd4);
        f_mobile_locale_name=(const void*(*)(const void*))(base+0x661956c);
    } else LOGI("readonly locale diagnostic disabled: original getter preimage");
    f_mobile_java_env=0;
    if (memcmp((void*)(base+0x656b134),env,sizeof env) || memcmp((void*)(base+0x54b0004),method,sizeof method) ||
        memcmp((void*)(base+0x54b0828),call,sizeof call)) {LOGI("mobile settings JNI disabled: original wrapper preimage");return;}
    g_mobile_java_activity=(jobject*)(base+0xa42ad50);
    g_mobile_java_class=(jclass*)(base+0xa42aa90);
    f_mobile_java_method=(jmethodID(*)(JNIEnv*,jclass,const char*,const char*,_Bool))(base+0x54b0004);
    f_mobile_java_void=(void(*)(JNIEnv*,jobject,jmethodID,...))(base+0x54b0828);
    f_mobile_java_env=(JNIEnv*(*)(_Bool))(base+0x656b134);
    LOGI("mobile settings JNI installed: original attach/TLS/optional method/void wrappers; activity references remain engine-owned");
}
