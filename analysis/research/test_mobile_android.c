#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
static int g_touch_gt_tid=71,g_touch_context=2,current_tid=71;
static long confirm_now=1000;
static long ptr_now_ms(void){return confirm_now;}
static int gettid(void){return current_tid;}
static int patch_code(void* a,void* b){(void)a;(void)b;return 0;}
static int protect_calls,protect_fail;
static void* protected_site;
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
static int protect_mem(void* site,size_t size,int flags){assert(size==4&&flags==7);protected_site=site;protect_calls++;return protect_fail;}
static int back_result=1,back_calls;
static int touch_mobile_back(void){++back_calls;return back_result;}
#define LOGF(...) ((void)0)
#define LOGI(...) ((void)0)
#include "../re4fix_mobile_android.h"
#ifdef MOBILE_STARTUP_FIXTURE
#include MOBILE_STARTUP_FIXTURE
#endif
static struct JNINativeInterface functions;
static JNIEnv table=&functions;
static jobject activity=(jobject)1;
static jclass klass=(jclass)2;
static int pending,pins,deletes,clears,calls,lookups,env_calls;
static int confirm_calls,language_calls,last_token,last_kind,last_russian,input_locked;
static int fail_pin,missing_method,exception_after_call,change_pair,change_after_method,wrong_instance,null_env;
static unsigned char culture[64],internationalization[64];
static unsigned short locale_text[33]={'j','a','-','J','P',0};
static struct {const unsigned short* text;int count,capacity;} locale_string={locale_text,6,6};
static void* locale_get(void){return internationalization;}
static const void* locale_name(const void* given){assert(given==culture);return &locale_string;}
static jboolean exception_check(JNIEnv* env){assert(env==&table);return pending?JNI_TRUE:JNI_FALSE;}
static void exception_clear(JNIEnv* env){assert(env==&table&&pending);pending=0;++clears;}
static jobject new_local(JNIEnv* env,jobject source){assert(env==&table&&!pending);++pins;if(pins==fail_pin){pending=1;return 0;}if(source==(jobject)1)return(jobject)10;assert(source==(jobject)2);if(change_pair)activity=(jobject)3;return(jobject)20;}
static void delete_local(JNIEnv* env,jobject ref){assert(env==&table&&!pending&&(ref==(jobject)10||ref==(jobject)20));++deletes;}
static jboolean instance(JNIEnv* env,jobject ref,jclass type){assert(env==&table&&!pending&&ref==(jobject)10&&type==(jclass)20);return wrong_instance?JNI_FALSE:JNI_TRUE;}
static JNIEnv* java_env(_Bool attach){assert(attach && !input_locked);++env_calls;return null_env?0:&table;}
static jmethodID method(JNIEnv* env,jclass type,const char* name,const char* signature,_Bool optional){assert(env==&table&&!pending&&type==(jclass)20&&optional);int loading=!strcmp(name,"AndroidThunkJava_MobileLoading");int confirm=!strcmp(name,"AndroidThunkJava_ConfirmMobileAction");int language=!strcmp(name,"AndroidThunkJava_ShowMobileLanguage");assert((loading&&!strcmp(signature,"(I)V"))||(confirm&&!strcmp(signature,"(IIZ)V"))||((language||!strcmp(name,"AndroidThunkJava_ShowMobileFiles"))&&!strcmp(signature,"()V")));++lookups;if(change_after_method)klass=(jclass)4;return missing_method?0:(jmethodID)(loading?7:confirm?6:language?8:5);}
static void call(JNIEnv* env,jobject ref,jmethodID method,...){assert(env==&table&&!pending&&ref==(jobject)10&&(method==(jmethodID)5||method==(jmethodID)6||method==(jmethodID)7||method==(jmethodID)8)&&!input_locked);++calls;if(method==(jmethodID)8)++language_calls;if(method==(jmethodID)7){va_list args;va_start(args,method);last_kind=va_arg(args,int);va_end(args);assert(last_kind>=0&&last_kind<=4);}if(method==(jmethodID)6){va_list args;va_start(args,method);last_token=va_arg(args,int);last_kind=va_arg(args,int);last_russian=va_arg(args,int);va_end(args);++confirm_calls;assert(last_token>0&&last_kind>=1&&last_kind<=7&&(last_russian==0||last_russian==1));}if(exception_after_call)pending=1;}
static int show_files(void){return mobile_android_call_thunk("AndroidThunkJava_ShowMobileFiles",0,0,0);}
static void reset(void){pending=pins=deletes=clears=calls=lookups=env_calls=fail_pin=missing_method=exception_after_call=change_pair=change_after_method=wrong_instance=null_env=0;confirm_calls=language_calls=last_token=last_kind=last_russian=input_locked=0;confirm_now=1000;mobile_android_confirm_cancel();activity=(jobject)1;klass=(jclass)2;g_touch_context=2;current_tid=71;g_mobile_files_request=0;}
int main(void){
#ifdef MOBILE_STARTUP_FIXTURE
 verify_startup_language_patch();
#endif
 assert(offsetof(struct JNINativeInterface,GetMethodID)==0x108&&offsetof(struct JNINativeInterface,ExceptionClear)==0x88&&offsetof(struct JNINativeInterface,CallVoidMethodV)==0x1f0);
 functions.ExceptionCheck=exception_check;functions.ExceptionClear=exception_clear;functions.NewLocalRef=new_local;functions.DeleteLocalRef=delete_local;functions.IsInstanceOf=instance;
 assert(!Java_com_epicgames_ue4_MobileAndroid_nativeBack(0,0)&&!back_calls);
 pending=1;assert(!Java_com_epicgames_ue4_MobileAndroid_nativeBack(&table,0)&&!back_calls&&pending);pending=0;
 assert(Java_com_epicgames_ue4_MobileAndroid_nativeBack(&table,0)==JNI_TRUE&&back_calls==1);
 back_result=0;assert(Java_com_epicgames_ue4_MobileAndroid_nativeBack(&table,0)==JNI_FALSE&&back_calls==2);
 f_mobile_java_env=java_env;f_mobile_java_method=method;f_mobile_java_void=call;g_mobile_java_activity=&activity;g_mobile_java_class=&klass;
 reset();assert(show_files()==1&&calls==1&&pins==2&&deletes==2&&clears==0);
 reset();pending=1;assert(!show_files()&&pending==1&&clears==0&&pins==0&&calls==0);
 reset();null_env=1;assert(!show_files()&&pins==0&&calls==0);
 reset();activity=0;assert(!show_files()&&pins==0&&calls==0);
 for(int failure=1;failure<=2;failure++){reset();fail_pin=failure;assert(!show_files()&&!pending&&clears==1&&deletes==failure-1&&calls==0);}
 reset();missing_method=1;assert(!show_files()&&deletes==2&&calls==0);
 reset();wrong_instance=1;assert(!show_files()&&deletes==2&&lookups==0);
 reset();change_pair=1;assert(!show_files()&&deletes==2&&lookups==0);
 for(int stage=0;stage<=4;stage++){reset();assert(mobile_android_call_thunk("AndroidThunkJava_MobileLoading",-1,stage,0)&&last_kind==stage&&calls==1&&deletes==2);}
 reset();change_after_method=1;assert(!show_files()&&deletes==2&&calls==0);
 reset();exception_after_call=1;assert(!show_files()&&calls==1&&deletes==2&&clears==1&&!pending);
 reset();g_mobile_files_request=1;current_tid=72;touch_mobile_android_tick();assert(g_mobile_files_request==1&&env_calls==0);
 current_tid=71;g_touch_context=1;touch_mobile_android_tick();assert(g_mobile_files_request==1&&env_calls==0);
 for(int context=2;context<=7;context++){if(context!=2&&context!=4&&context!=7)continue;reset();g_touch_context=context;g_mobile_files_request=1;touch_mobile_android_tick();assert(!g_mobile_files_request&&calls==1&&deletes==2);touch_mobile_android_tick();assert(calls==1);}
 for(int context=2;context<=7;context++){if(context!=2&&context!=4&&context!=7)continue;reset();g_touch_context=context;
  input_locked=1;mobile_android_request_language();mobile_android_request_files();mobile_android_request_language();
  assert(g_mobile_files_request==2&&!calls&&!env_calls);input_locked=0;current_tid=72;touch_mobile_android_tick();assert(g_mobile_files_request==2&&!calls);
  current_tid=71;g_touch_context=1;touch_mobile_android_tick();assert(g_mobile_files_request==2&&!calls);
  g_touch_context=context;touch_mobile_android_tick();assert(!g_mobile_files_request&&calls==1&&language_calls==1&&deletes==2);
  touch_mobile_android_tick();assert(calls==1);mobile_android_request_files();mobile_android_request_language();assert(g_mobile_files_request==1);
  touch_mobile_android_tick();assert(calls==2&&language_calls==1&&!g_mobile_files_request);
 }
 reset();input_locked=1;assert(!mobile_android_confirm_request(0,0,1000)&&!mobile_android_confirm_request(8,0,1000)&&!mobile_android_confirm_request(1,0,0));
 assert(mobile_android_confirm_request(1,1,1000)&&!mobile_android_confirm_request(2,0,1000));assert(!env_calls&&!calls&&!mobile_android_confirm_take(1,1001));input_locked=0;
 current_tid=72;touch_mobile_android_tick();assert(!calls);current_tid=71;touch_mobile_android_tick();assert(confirm_calls==1&&last_kind==1&&last_russian==1);touch_mobile_android_tick();assert(confirm_calls==1);
 input_locked=1;Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token+1,JNI_TRUE);assert(!mobile_android_confirm_take(1,1001));
 Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_FALSE);
 assert(!mobile_android_confirm_take(2,1001)&&mobile_android_confirm_take(1,1001)==1&&!mobile_android_confirm_take(1,1001)&&calls==1);input_locked=0;
 for(int kind=1;kind<=7;kind++){reset();assert(mobile_android_confirm_request(kind,0,1000));g_touch_context=kind==7?1:kind==6?6:4;touch_mobile_android_tick();assert(confirm_calls==1&&last_kind==kind&&!last_russian);Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_FALSE);assert(mobile_android_confirm_take(kind,1001)==-1&&!mobile_android_confirm_take(kind,1001));}
 reset();assert(mobile_android_confirm_request(1,0,1000));unsigned queued_token=(unsigned)g_mobile_confirm>>8;
 Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,queued_token,JNI_TRUE);assert(!mobile_android_confirm_take(1,1001));touch_mobile_android_tick();int stale=last_token;mobile_android_confirm_cancel();
 assert(mobile_android_confirm_request(1,0,1001));confirm_now=1001;touch_mobile_android_tick();assert(last_token!=stale);Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,stale,JNI_TRUE);assert(!mobile_android_confirm_take(1,1002));Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);assert(mobile_android_confirm_take(1,1002)==1);
 reset();assert(mobile_android_confirm_request(1,0,1000));g_touch_context=1;touch_mobile_android_tick();assert(!calls&&mobile_android_confirm_take(1,1001)==-1);
 reset();assert(mobile_android_confirm_request(1,0,1000));g_touch_context=6;touch_mobile_android_tick();assert(!calls&&mobile_android_confirm_take(1,1001)==-1);
 reset();assert(mobile_android_confirm_request(6,0,1000));missing_method=1;touch_mobile_android_tick();assert(!calls&&mobile_android_confirm_take(6,1001)==-1);
 reset();assert(mobile_android_confirm_request(6,0,1000));exception_after_call=1;touch_mobile_android_tick();assert(calls==1&&mobile_android_confirm_take(6,1001)==-1);
 reset();assert(mobile_android_confirm_request(6,0,1000));confirm_now=121001;touch_mobile_android_tick();assert(!calls&&mobile_android_confirm_take(6,121001)==-1);
 reset();assert(mobile_android_confirm_request(6,0,1000));touch_mobile_android_tick();Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);assert(mobile_android_confirm_take(6,121001)==-1);
 reset();assert(mobile_android_confirm_request(6,0,1000));touch_mobile_android_tick();Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);assert(mobile_android_confirm_take(6,121000)==1);
 reset();assert(mobile_android_confirm_request(6,0,1000));assert(mobile_android_confirm_take(6,999)==-1);
 reset();assert(!mobile_android_confirm_active(7));assert(mobile_android_confirm_request(7,0,1000)&&mobile_android_confirm_active(7)&&!mobile_android_confirm_active(6));
 mobile_android_confirm_cancel_kind(6);assert(mobile_android_confirm_active(7));mobile_android_confirm_cancel_kind(7);assert(!mobile_android_confirm_active(7)&&!mobile_android_confirm_take(7,1001));
 reset();assert(mobile_android_confirm_request(6,0,1000));touch_mobile_android_tick();Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);assert(mobile_android_confirm_active(6));mobile_android_confirm_cancel_kind(7);assert(mobile_android_confirm_take(6,1001)==1);
 reset();g_mobile_confirm_token=0xffffffu;assert(mobile_android_confirm_request(6,0,1000));touch_mobile_android_tick();assert(last_token==1);mobile_android_confirm_cancel();
 reset();assert(mobile_android_confirm_request(6,0,1000));g_mobile_confirm=(g_mobile_confirm&0xffffffffu)|((uint64_t)0xfffffff0u<<32);confirm_now=16;touch_mobile_android_tick();Java_com_epicgames_ue4_MobileAndroid_nativeConfirm(0,0,last_token,JNI_TRUE);assert(mobile_android_confirm_take(6,16)==1);
 f_mobile_locale_get=locale_get;f_mobile_locale_name=locale_name;void* culture_pointer=culture;memcpy(internationalization+0x28,&culture_pointer,sizeof culture_pointer);
 unsigned char before[64];memcpy(before,internationalization,64);reset();g_mobile_locale_logged=0;assert(mobile_android_log_locale()&&g_mobile_locale_logged);assert(!memcmp(before,internationalization,64));assert(!mobile_android_log_locale());
 g_mobile_locale_logged=0;locale_string.count=33;assert(!mobile_android_log_locale()&&!g_mobile_locale_logged);locale_string.count=6;
 locale_string.capacity=5;assert(!mobile_android_log_locale());locale_string.capacity=6;
 locale_text[5]='x';assert(!mobile_android_log_locale());locale_text[5]=0;
 locale_text[0]=0x400;assert(!mobile_android_log_locale());locale_text[0]='j';
 reset();g_mobile_locale_logged=0;current_tid=72;touch_mobile_android_tick();assert(!g_mobile_locale_logged);current_tid=71;g_touch_context=1;touch_mobile_android_tick();assert(!g_mobile_locale_logged);g_touch_context=4;touch_mobile_android_tick();assert(g_mobile_locale_logged);
 puts("PASS: actual native files/confirmation JNI helpers; atomic-only queue/reply/take, one dialog, seven actions, token/duplicate/cancel/deadline/context/wrap rejection, GT dispatch and exact varargs; paired engine globals/ref/exception ownership");return 0;
}
