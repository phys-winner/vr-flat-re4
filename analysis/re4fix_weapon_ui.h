// Include after flat_view; hide held-weapon VR markers and preserve the native wrist display.
#ifndef RE4FIX_WEAPON_UI_H
#define RE4FIX_WEAPON_UI_H
static void touch_weapon_ui_tick(void* hand) {
    if(!cinema_on_game_thread() || !mv_scene_class || !mv_static_class || !cu_component_class || !mv_visible)return;
    void* gun=touch_equipped_gun(hand);
    if(!gun || !pv_alive(gun))return;
    void* root=*(void* const*)((const char*)gun+0x130);
    if(!pv_component(root,gun,0x1f0))return;
    void* queue[96]={root};unsigned count=1;
    for(unsigned i=0;i<count;i++) {
        void* component=queue[i];
        if(!pv_alive(component) || !menu_visual_layout(component,mv_scene_class(),0x1f0))continue;
        void* owner=*(void* const*)((const char*)component+0x20);
        if(pv_component(component,gun,0x5a0)
            && cinema_ui_named(component,"VR4GamePlayerGripIndicator",0x5a0)
            && fv_descends_from(component,root)
            && (*(const unsigned short*)((const char*)component+0x14c)&0x20))mv_visible(component,0,0);
        if(pv_alive(owner) && cinema_ui_named(owner,"UI_AmmoCounter_Pistol_C",0x220)) {
            void* leaves[2]={fv_object_property(owner,"Widget"),fv_object_property(owner,"Cube")};
            for(unsigned leaf=0;leaf<2;leaf++) {
                void* part=leaves[leaf];
                if(!pv_component(part,owner,leaf?0x448:0x498)
                    || !menu_visual_layout(part,leaf?mv_static_class():cu_component_class(),leaf?0x448:0x498)
                    || !fv_descends_from(part,root))continue;
                if(*(const unsigned short*)((const char*)part+0x14c)&0x20)mv_visible(part,0,0);
            }
        }
        struct {void** data;int count,max;} children;
        memcpy(&children,(const char*)component+0xd0,sizeof children);
        if(children.count<0 || children.count>64 || children.max<children.count || children.max>256
            || (children.count&&!children.data))continue;
        for(int j=0;j<children.count && count<96;j++) {
            void* child=children.data[j];
            if(!pv_alive(child) || !menu_visual_layout(child,mv_scene_class(),0x1f0)
                || *(void* const*)((const char*)child+0xc0)!=component)continue;
            int duplicate=0;for(unsigned k=0;k<count;k++)if(queue[k]==child)duplicate=1;
            if(!duplicate)queue[count++]=child;
        }
    }
}
#endif
