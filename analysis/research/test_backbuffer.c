#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static int debug_thread=123;
int gettid(void){return debug_thread;}
static int patch_code(void* target,void* hook){(void)target;(void)hook;return 0;}
static unsigned logs;
#define LOGF(...) ((void)logs++)
#define RE4_QA_USER_TRANSITIONS 1
#include "../re4fix_backbuffer.h"
enum{IMAGE_SIZE=0xa7de000};
static unsigned char* image;static unsigned char variable[0x40],hdr_variable[0x40];
static unsigned finds,gets,sets,field_sets[2];static unsigned find_ok,set_ok;
static void* find(void* manager,const unsigned short* name,int tracking){
    const char* expected[]={"r.DefaultBackBufferPixelFormat","r.HDR.EnableHDROutput"};
    assert(manager==image+0x100&&tracking==0);unsigned field=name[2]=='D'?0:1;
    for(unsigned i=0;i<=strlen(expected[field]);i++)assert(name[i]==(unsigned char)expected[field][i]);
    finds++;return (find_ok&(1u<<field)) ? (field?hdr_variable:variable) : 0;
}
static int get(const void* object){assert(object==variable||object==hdr_variable);gets++;return *(const int*)((const char*)object+0x34);}
static void set(void* object,const unsigned short* value,unsigned flags){
    assert((object==variable||object==hdr_variable)&&value[0]=='0'&&value[1]==0&&flags==0x08000000);
    assert(finds==2&&gets>=2);unsigned field=object==hdr_variable;sets++;field_sets[field]++;
    char* data=object;
    if(set_ok&(1u<<field)){*(int*)(data+0x34)=*(int*)(data+0x38)=0;*(unsigned*)(data+0x18)=(*(unsigned*)(data+0x18)&0xffff)|flags;}
}
static void ready(int value,unsigned flags){
    memset(variable,0xa5,sizeof variable);*(uintptr_t*)variable=(uintptr_t)image+0x9e20d30;
    *(unsigned*)(variable+0x18)=flags;*(int*)(variable+0x34)=*(int*)(variable+0x38)=value;
    memset(hdr_variable,0xa5,sizeof hdr_variable);*(uintptr_t*)hdr_variable=(uintptr_t)image+0x9e20d30;
    *(unsigned*)(hdr_variable+0x18)=0x20;*(int*)(hdr_variable+0x34)=*(int*)(hdr_variable+0x38)=0;
    *(void**)(image+0xa74e6f8)=0;*(void**)(image+0xa6b1ac0)=image+0x100;
    *(unsigned*)(image+0xa6eb058)=123;image[0xa6eb068]=1;
    backbuffer_base=(uintptr_t)image;backbuffer_prepared=backbuffer_installed=0;backbuffer_logs=0;
    backbuffer_find=find;backbuffer_get=get;backbuffer_set=set;
    finds=gets=sets=logs=0;field_sets[0]=field_sets[1]=0;find_ok=set_ok=3;
}
static int prepare(uintptr_t caller,int value,unsigned thread){return backbuffer_before_rhi(caller,value,thread);}
static void debug_ready(void){
    ready(4,0x03000004);debug_thread=123;backbuffer_installed=backbuffer_prepared=1;
    memset(hdr_variable,0,sizeof hdr_variable);*(uintptr_t*)hdr_variable=(uintptr_t)image+0x9e20d30;
    *(unsigned*)(hdr_variable+0x18)=0x02000020;
    *(int*)(variable+0x38)=0;*(int*)(hdr_variable+0x34)=1;*(int*)(hdr_variable+0x38)=0;
    *(void**)(image+0xa7dd318)=variable;*(void**)(image+0xa76dca8)=hdr_variable+0x34;
    *(void**)(image+0xa74e6f8)=image+0x200;
    *(unsigned*)(image+0xa41322c)=18;image[0xa75079b]=1;image[0xa75079c]=1;
    backbuffer_debug_pending=backbuffer_debug_count=0;memset(&backbuffer_debug_last,0,sizeof backbuffer_debug_last);
}
static void test_debug(void){
    debug_ready();unsigned char before[sizeof variable],hdr_before[sizeof hdr_variable];
    unsigned char* native_before=malloc(IMAGE_SIZE);assert(native_before);memcpy(native_before,image,IMAGE_SIZE);
    memcpy(before,variable,sizeof before);memcpy(hdr_before,hdr_variable,sizeof hdr_before);
    for(unsigned i=0;i<20;i++)backbuffer_debug_request();
    assert(backbuffer_debug_pending==1);backbuffer_debug_tick();
    assert(backbuffer_debug_count==1&&!backbuffer_debug_pending&&logs==2&&!finds&&!gets&&!sets);
    assert(backbuffer_debug_last.variable==(uintptr_t)variable&&backbuffer_debug_last.variable_valid);
    assert(backbuffer_debug_last.flags==0x03000004&&backbuffer_debug_last.values[0]==4&&backbuffer_debug_last.values[1]==0);
    assert(backbuffer_debug_last.hdr_data==(uintptr_t)hdr_variable+0x34&&backbuffer_debug_last.hdr_valid);
    assert(backbuffer_debug_last.hdr_values[0]==1&&backbuffer_debug_last.hdr_values[1]==0);
    assert(backbuffer_debug_last.hdr_flags==0x02000020);
    assert(backbuffer_debug_last.rhi==(uintptr_t)image+0x200&&backbuffer_debug_last.thread==123);
    assert(backbuffer_debug_last.hdr_format==18&&backbuffer_debug_last.hdr_enabled==1&&backbuffer_debug_last.hdr_supported==1);
    assert(backbuffer_debug_last.installed==1&&backbuffer_debug_last.prepared==1);
    for(unsigned i=0;i<20;i++)backbuffer_debug_tick();assert(backbuffer_debug_count==1&&logs==2);
    for(unsigned i=0;i<20;i++){backbuffer_debug_request();backbuffer_debug_tick();}
    assert(backbuffer_debug_count==8&&!backbuffer_debug_pending&&logs==16&&!finds&&!gets&&!sets);
    assert(!memcmp(native_before,image,IMAGE_SIZE)&&!memcmp(before,variable,sizeof before)&&!memcmp(hdr_before,hdr_variable,sizeof hdr_before));
    for(unsigned refusal=0;refusal<3;refusal++){
        debug_ready();backbuffer_debug_request();
        if(refusal==0)backbuffer_base=0;if(refusal==1)image[0xa6eb068]=0;if(refusal==2)debug_thread=124;
        backbuffer_debug_tick();assert(backbuffer_debug_pending==1&&!backbuffer_debug_count&&!logs&&!finds&&!gets&&!sets);
        backbuffer_base=(uintptr_t)image;image[0xa6eb068]=1;debug_thread=123;
        backbuffer_debug_tick();assert(backbuffer_debug_count==1&&!backbuffer_debug_pending);
    }
    for(unsigned malformed=0;malformed<10;malformed++){
        debug_ready();
        if(malformed==0)*(void**)(image+0xa7dd318)=0;
        if(malformed==1)*(uintptr_t*)(image+0xa7dd318)=1;
        if(malformed==2)*(void**)(image+0xa7dd318)=variable+1;
        if(malformed==3)*(uintptr_t*)(image+0xa7dd318)=UINTPTR_MAX;
        if(malformed==4)*(uintptr_t*)variable=0;
        if(malformed==5)*(void**)(image+0xa76dca8)=0;
        if(malformed==6)*(uintptr_t*)(image+0xa76dca8)=1;
        if(malformed==7)*(void**)(image+0xa76dca8)=hdr_variable+0x35;
        if(malformed==8)*(uintptr_t*)(image+0xa76dca8)=UINTPTR_MAX;
        if(malformed==9)*(uintptr_t*)hdr_variable=0;
        memcpy(before,variable,sizeof before);memcpy(hdr_before,hdr_variable,sizeof hdr_before);
        backbuffer_debug_request();backbuffer_debug_tick();assert(backbuffer_debug_count==1&&!finds&&!gets&&!sets);
        assert(!memcmp(before,variable,sizeof before)&&!memcmp(hdr_before,hdr_variable,sizeof hdr_before));
        assert(malformed<5?!backbuffer_debug_last.variable_valid:!backbuffer_debug_last.hdr_valid);
    }
    debug_ready();*(void**)(image+0xa74e6f8)=0;backbuffer_installed=backbuffer_prepared=0;
    backbuffer_debug_request();backbuffer_debug_tick();assert(!backbuffer_debug_last.rhi&&!backbuffer_debug_last.installed&&!backbuffer_debug_last.prepared);
    free(native_before);
    puts("PASS: actual QA-only backbuffer request/GT snapshot: coalesced requests,8-snapshot hard cap, exact default/HDR GT+RT/class/flags/RHI fields,3 context deferrals and10 malformed-pointer/class refusals; zero engine calls and full native image/CVar bytes unchanged");
}
int main(void){
    image=calloc(1,IMAGE_SIZE);assert(image);uintptr_t caller=(uintptr_t)image+0x54c4c2c;
    for(int value=0;value<=4;value++)for(unsigned hdr=0;hdr<2;hdr++)for(unsigned argument=0;argument<2;argument++){
        ready(value,0x03000004);*(int*)(hdr_variable+0x34)=*(int*)(hdr_variable+0x38)=(int)hdr;
        unsigned char before[sizeof variable],hdr_before[sizeof hdr_variable];memcpy(before,variable,sizeof before);memcpy(hdr_before,hdr_variable,sizeof hdr_before);
        assert(prepare(caller,(int)argument,123)&&backbuffer_prepared&&finds==2&&gets==4&&sets==2);
        assert(*(int*)(variable+0x34)==0&&*(int*)(variable+0x38)==0);
        assert(*(int*)(hdr_variable+0x34)==0&&*(int*)(hdr_variable+0x38)==0&&field_sets[0]==1&&field_sets[1]==1);
        struct {unsigned char* before;unsigned char* after;unsigned flags;} fields[]={{before,variable,0x08000004},{hdr_before,hdr_variable,0x08000020}};
        for(unsigned i=0;i<2;i++){*(unsigned*)(fields[i].before+0x18)=fields[i].flags;memset(fields[i].before+0x34,0,8);assert(!memcmp(fields[i].before,fields[i].after,0x40));}
        assert(!prepare(caller,(int)argument,123)&&finds==2&&gets==4&&sets==2);
    }
    // Existing0 is promoted only below Code; compatible Code/Console priorities are preserved.
    for(unsigned default_priority=0;default_priority<10;default_priority++)for(unsigned hdr_priority=0;hdr_priority<10;hdr_priority++){
        ready(0,(default_priority<<24)|4);*(unsigned*)(hdr_variable+0x18)=(hdr_priority<<24)|0x20;
        assert(prepare(caller,0,123));assert(field_sets[0]==(default_priority<8)&&field_sets[1]==(hdr_priority<8));
        assert(*(unsigned*)(variable+0x18)==((default_priority<8?8:default_priority)<<24|4));
        assert(*(unsigned*)(hdr_variable+0x18)==((hdr_priority<8?8:hdr_priority)<<24|0x20));
    }
    for(unsigned refusal=0;refusal<28;refusal++){
        ready(4,4);uintptr_t actual=caller;int argument=0;unsigned thread=123;
        if(refusal==0)backbuffer_base=0;
        if(refusal==1)backbuffer_prepared=1;
        if(refusal==2)actual++;
        if(refusal==3)argument=2;
        if(refusal==4)*(void**)(image+0xa74e6f8)=image+0x200;
        if(refusal==5)image[0xa6eb068]=0;
        if(refusal==6)thread=124;
        if(refusal==7)*(void**)(image+0xa6b1ac0)=0;
        if(refusal==8)backbuffer_find=0;
        if(refusal==9)backbuffer_get=0;
        if(refusal==10)backbuffer_set=0;
        if(refusal==11)find_ok=2;
        if(refusal==12)(*(uintptr_t*)variable)++;
        if(refusal==13)*(unsigned*)(variable+0x18)=12;
        if(refusal==14)*(unsigned*)(variable+0x18)=0;
        if(refusal==15)*(unsigned*)(variable+0x18)=0x09000004;
        if(refusal==16)*(int*)(variable+0x34)=5;
        if(refusal==17)*(int*)(variable+0x34)=-1;
        if(refusal==18)find_ok=1;
        if(refusal==19)(*(uintptr_t*)hdr_variable)++;
        if(refusal==20)*(unsigned*)(hdr_variable+0x18)=0x24;
        if(refusal==21)*(unsigned*)(hdr_variable+0x18)=0x28;
        if(refusal==22)*(unsigned*)(hdr_variable+0x18)=0;
        if(refusal==23){*(unsigned*)(hdr_variable+0x18)=0x09000020;*(int*)(hdr_variable+0x34)=1;}
        if(refusal==24)*(int*)(hdr_variable+0x34)=2;
        if(refusal==25)*(int*)(hdr_variable+0x34)=-1;
        if(refusal==26){set_ok=2;*(unsigned*)(hdr_variable+0x18)=0x08000020;}
        if(refusal==27){set_ok=1;*(int*)(variable+0x34)=*(int*)(variable+0x38)=0;*(unsigned*)(variable+0x18)=0x08000004;*(int*)(hdr_variable+0x34)=1;}
        unsigned char before[sizeof variable],hdr_before[sizeof hdr_variable];memcpy(before,variable,sizeof before);memcpy(hdr_before,hdr_variable,sizeof hdr_before);
        assert(!prepare(actual,argument,thread)&&sets==(refusal>=26)&&!memcmp(before,variable,sizeof before)&&!memcmp(hdr_before,hdr_variable,sizeof hdr_before));
    }
    ready(4,4);for(unsigned i=0;i<20;i++)assert(!prepare(caller,0,123)||i==0);
    assert(logs==1&&sets==2);
    ready(4,0x09000004);for(unsigned i=0;i<20;i++)assert(!prepare(caller,0,123));assert(logs==4&&!sets);
    assert(!install_backbuffer(0));
    ready(4,4);assert(!install_backbuffer((uintptr_t)image)&&!backbuffer_installed); // No native signatures.
    test_debug();
    free(image);
    puts("PASS: actual pre-RHI SDR helper: all5 default formats, both HDR/bool values, both native CVars prevalidated before either write; existing0 promoted through native Code setter once,100 priority pairs and28 context/type/flags/range/stronger/failure refusals preserve native state; correct Code/stronger0 is untouched; bounded diagnostics");
    return 0;
}
