// Exact original UMG button-texture leaves; native text, QTE arrows and progress remain authored.
#ifndef RE4FIX_PROMPT_GLYPH_H
#define RE4FIX_PROMPT_GLYPH_H
#include <math.h>
static int pv_glyph_ready;
static void*(*pv_image_class)(void),*(*pv_panel_class)(void);
static float(*pv_glyph_opacity)(const void*);
static void(*pv_glyph_set_opacity)(void*,float);
static const char* const pv_glyph_names[]={
    "Image_166","Image_A","Image_A_1","Image_A_2","Image_A_3","Image_A_4","Image_A_5","Image_A_6","Image_A_7","Image_A_8",
    "Image_B","Image_B_1","Image_B_2","Image_B_3","Image_Invalid","Image_X","Image_X_1","Image_X_2","Image_X_3",
    "Image_Y","Image_Y_1","Image_Y_2","Image_Y_3"
};
static struct {
    struct cinema_ui_weak widget,tree,image[23];
    uint64_t name[23];float opacity[23];unsigned char masked[23];unsigned count;
} pv_glyph_mask[6];
static int pv_glyph_kind(const void* widget) {
    if(!pv_alive(widget) || !cu_user_class || !cinema_object(widget,cu_user_class(),0x230))return 0;
    return cinema_ui_named(widget,"InfoRing_C",0x4a3) ? 1:cinema_ui_named(widget,"NoteInstruction_C",0x2b2) ? 2:0;
}
static int pv_glyph_name(const char* name,int kind) {
    if(kind==2)return !strcmp(name,"Image_A_3") || !strcmp(name,"Image_A_6") || !strcmp(name,"Image_B_2")
        || !strcmp(name,"Image_X_2") || !strcmp(name,"Image_Y_2");
    if(kind==1)for(unsigned i=0;i<23;i++)if(!strcmp(name,pv_glyph_names[i]))return 1;
    return 0;
}
static int pv_glyph_owned(const void* image,const void* tree,uint64_t name) {
    uint64_t current=0;if(!pv_alive(image) || !pv_alive(tree) || !pv_image_class
        || !cinema_object(image,pv_image_class(),0xc8) || *(void* const*)((const char*)image+0x20)!=tree)return 0;
    memcpy(&current,(const char*)image+0x18,8);return current==name;
}
static void pv_glyph_restore(unsigned slot) {
    if(slot>=6)return;
    void* widget=cu_weak_get(&pv_glyph_mask[slot].widget),*tree=cu_weak_get(&pv_glyph_mask[slot].tree);
    if(pv_glyph_kind(widget) && pv_alive(tree) && *(void* const*)((const char*)tree+0x20)==widget
        && *(void* const*)((const char*)widget+0x1a8)==tree) {
        for(unsigned i=0;i<pv_glyph_mask[slot].count;i++) {
            void* image=cu_weak_get(&pv_glyph_mask[slot].image[i]);
            if(pv_glyph_mask[slot].masked[i] && pv_glyph_owned(image,tree,pv_glyph_mask[slot].name[i])
                && pv_glyph_opacity(image)==0)
                pv_glyph_set_opacity(image,pv_glyph_mask[slot].opacity[i]);
        }
    }
    memset(&pv_glyph_mask[slot],0,sizeof pv_glyph_mask[slot]);
}
static int pv_glyph_bind(unsigned slot,void* widget,int kind) {
    void* tree=*(void* const*)((const char*)widget+0x1a8);
    if(!pv_alive(tree) || !cinema_ui_named(tree,"WidgetTree",0x30)
        || *(void* const*)((const char*)tree+0x20)!=widget)return 0;
    void* queue[256]={*(void* const*)((const char*)tree+0x28)};unsigned count=1,found=0;
    for(unsigned i=0;i<count;i++) {
        void* node=queue[i];
        if(!pv_alive(node) || *(void* const*)((const char*)node+0x20)!=tree)continue;
        if(cinema_object(node,pv_image_class(),0xc8)) {
            char name[96];phone_inspect_fname((const char*)node+0x18,name,sizeof name);
            if(pv_glyph_name(name,kind)) {
                if(found==23)return 0;
                cu_weak_assign(&pv_glyph_mask[slot].image[found],node);
                memcpy(&pv_glyph_mask[slot].name[found],(const char*)node+0x18,8);
                pv_glyph_mask[slot].opacity[found]=pv_glyph_opacity(node);
                if(!isfinite(pv_glyph_mask[slot].opacity[found]) || pv_glyph_mask[slot].opacity[found]<0
                    || pv_glyph_mask[slot].opacity[found]>1)return 0;
                found++;
            }
        } else if(cinema_object(node,pv_panel_class(),0x118)) {
            struct {void**data;int count,max;} children;memcpy(&children,(const char*)node+0x108,sizeof children);
            if(children.count<0 || children.count>64 || children.max<children.count || children.max>256
                || (children.count && !children.data))return 0;
            for(int j=0;j<children.count;j++) {
                void* child=children.data[j];
                if(!pv_alive(child) || *(void* const*)((const char*)child+0x20)!=node)continue;
                child=*(void* const*)((const char*)child+0x30);int duplicate=0;
                for(unsigned k=0;k<count;k++)if(child==queue[k])duplicate=1;
                if(duplicate || !child)continue;
                if(count==256)return 0;queue[count++]=child;
            }
        }
    }
    if(!found)return 0;
    cu_weak_assign(&pv_glyph_mask[slot].widget,widget);cu_weak_assign(&pv_glyph_mask[slot].tree,tree);
    pv_glyph_mask[slot].count=found;return 1;
}
static void pv_glyph_tick(void* floater,void*const* controllers,unsigned controller_count,int hide) {
    if(!pv_glyph_ready)return;
    static const char* const fields[]={"Widget","WidgetNote"};static const unsigned offsets[]={0x2d8,0x280};
    for(unsigned slot=0;slot<6;slot++) {
        void* owner=slot<2?floater:controllers && slot-2<controller_count?controllers[slot-2]:0;
        if(!hide || !owner){pv_glyph_restore(slot);continue;}
        void* component=slot<2?cinema_ui_property(owner,"FloatingUI_BP_C",fields[slot],offsets[slot])
            :cinema_ui_property(owner,"ControllerIcon3d_C","Widget1",0x270);
        void* widget=pv_component(component,owner,0x4a0) && cinema_object(component,cu_component_class(),0x4a0)
            ? cu_get_widget(component):0;
        int kind=pv_glyph_kind(widget);
        if(!kind || (slot>=2 && kind!=1)){pv_glyph_restore(slot);continue;}
        void* tree=*(void* const*)((const char*)widget+0x1a8);
        if(cu_weak_get(&pv_glyph_mask[slot].widget)!=widget || cu_weak_get(&pv_glyph_mask[slot].tree)!=tree) {
            pv_glyph_restore(slot);
            if(!pv_glyph_bind(slot,widget,kind)){memset(&pv_glyph_mask[slot],0,sizeof pv_glyph_mask[slot]);continue;}
        }
        for(unsigned i=0;i<pv_glyph_mask[slot].count;i++) {
            void* image=cu_weak_get(&pv_glyph_mask[slot].image[i]);
            if(!pv_glyph_owned(image,tree,pv_glyph_mask[slot].name[i]))continue;
            float opacity=pv_glyph_opacity(image);
            // Collapsed/Hidden changes native visibility queries and shared caption layout. A transparent
            // image keeps its exact authored slot, desired size, visibility and the adjacent command text.
            if(isfinite(opacity) && opacity>0 && opacity<=1) {
                pv_glyph_mask[slot].opacity[i]=opacity;pv_glyph_mask[slot].masked[i]=1;
                pv_glyph_set_opacity(image,0);
            }
        }
    }
}
static void install_touch_prompt_glyph(uintptr_t base) {
    static const unsigned getter[]={0xd10183ff,0xf90023f3,0xa9057bfd,0x910143fd};
    static const unsigned opacity_getter[]={0xfc1c0fe8,0xa90157f6,0xa9024ff4,0xa9037bfd};
    static const unsigned setter[]={0xfc1d0fe8,0xf90007f5,0xa9014ff4,0xa9027bfd};
    pv_glyph_ready=0;
    if(!base || !cu_ready || !cu_get_widget || !cu_component_class || !cu_user_class
        || memcmp((const void*)(base+0x7e01294),getter,16) || memcmp((const void*)(base+0x7e1495c),getter,16)
        || memcmp((const void*)(base+0x7da4524),opacity_getter,16) || memcmp((const void*)(base+0x7da46d4),setter,16)
        || *(const unsigned*)(base+0x9f6b718+36)!=0x1a8 || *(const unsigned*)(base+0x9f5abd8+36)!=0x108
        || *(const unsigned*)(base+0x9f5a358+36)!=0x30 || *(const unsigned*)(base+0x9f7bf38+36)!=0x28
        || *(const unsigned*)(base+0x9f6ff30+36)!=0xc4)return;
    pv_image_class=(void*(*)(void))(base+0x7e01294);pv_panel_class=(void*(*)(void))(base+0x7e1495c);
    pv_glyph_opacity=(float(*)(const void*))(base+0x7da4524);
    pv_glyph_set_opacity=(void(*)(void*,float))(base+0x7da46d4);pv_glyph_ready=1;
}
#endif
