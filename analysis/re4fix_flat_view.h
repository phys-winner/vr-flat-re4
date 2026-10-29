// Flat-screen view helpers. Include after world_touch / interact / menu_visual.
//  * Authored world dialogs face the phone viewport; the complete Lobby console uses a bounded camera zoom.
//  * Body holster icons/indicators (VR "reach here" ammo/weapon glyphs on the belt) are hidden on phones.
#ifndef RE4FIX_FLAT_VIEW_H
#define RE4FIX_FLAT_VIEW_H

static volatile float g_menu_yaw_off, g_menu_pitch;   // radians, used by head_pose() for in-world menus
static volatile int g_menu_view_active;
static void* (*fv_ui_manager)(void);
static void* (*fv_peek_screen)(void*);
static void* (*fv_screen_class)(void);
static void* (*fv_button_class)(void);
static int (*fv_ui_enabled)(const void*);
static void (*fv_set_int)(void*, unsigned long long, int);
static unsigned fv_logs;
static long fv_holster_ms;
#include "re4fix_ui_frame.h"
static unsigned (*fv_widget_planes)(void*, struct fv_plane*, unsigned);
static int (*fv_control_plane)(void*,struct fv_plane*);
static void (*fv_screen_moved)(void*,void*);
static long fv_lobby_zoom_ms;
static struct { struct cinema_ui_weak screen, root; struct reload_tf original; } fv_framed = {{-1,0},{-1,0}};
static int fv_frame_owned(void* root, void* screen) {
    return root && screen && pv_alive(root) && pv_alive(screen) && mv_scene_class
        && menu_visual_layout(root, mv_scene_class(), 0x1f0)
        && *(void* const*)((const char*)root + 0xa0) == screen
        && *(void* const*)((const char*)screen + 0x130) == root;
}
static void fv_frame_restore(void) {
    if (!cu_weak_get || !rl_world_transform) return;
    void* root = cu_weak_get(&fv_framed.root), *screen = cu_weak_get(&fv_framed.screen);
    if (fv_frame_owned(root, screen)) {
        rl_world_transform(root, &fv_framed.original, 0, 0, 0);
        if (fv_screen_moved) fv_screen_moved(screen,root);
    }
    fv_framed.screen = (struct cinema_ui_weak){-1,0};
    fv_framed.root = (struct cinema_ui_weak){-1,0};
}
static int fv_descends_from(void* part, const void* root) {
    for (unsigned depth = 0; part && depth < 24; depth++) {
        if (part == root) return 1;
        if (!pv_alive(part) || !menu_visual_layout(part, mv_scene_class(), 0x1f0)) return 0;
        void* parent = *(void* const*)((const char*)part + 0xc0);
        if (parent == part) return 0;
        part = parent;
    }
    return 0;
}
static int fv_component_plane(void* part,struct fv_plane* plane) {
    if (fv_control_plane) return fv_control_plane(part,plane);
    if (!part || !pv_alive(part) || !menu_visual_layout(part,fv_button_class(),0x320) || !fv_ui_enabled(part)) return 0;
    memcpy(&plane->transform,(const char*)part+0x1c0,sizeof plane->transform);
    memcpy(&plane->width,(const char*)part+0x318,8);
    return 1;
}
static unsigned fv_screen_planes(void* screen,void* root,const struct reload_tf* current,
                                 struct fv_plane* planes,unsigned capacity,int minimum_controls) {
    struct { void** data; int count, max; } list;
    memcpy(&list, (const char*)screen + 0x238, sizeof list);
    if (!capacity || capacity>64 || list.count < minimum_controls || list.count > 512 || list.max < list.count
        || (list.count && !list.data)) return capacity+1;
    unsigned count = 0;
    for (int i = 0; i < list.count; i++) {
        void* part = list.data[i];
        struct fv_plane plane;
        if (!fv_component_plane(part,&plane) || *(void* const*)((const char*)part + 0x308) != screen) continue;
        if (!fv_descends_from(part, root) || count == capacity) return capacity+1;
        float dx=plane.transform.p[0]-current->p[0],dy=plane.transform.p[1]-current->p[1],dz=plane.transform.p[2]-current->p[2];
        // Some pooled tutorial controls stay enabled at remote placeholder locations.
        if (dx*dx+dy*dy+dz*dz>400.0f*400.0f) continue;
        planes[count++]=plane;
    }
    if (fv_widget_planes && count < capacity) {
        unsigned widgets = fv_widget_planes(root, planes + count, capacity - count);
        if (widgets > capacity - count) return capacity+1;
        count += widgets;
    }
    return count;
}
static int fv_frame_screen(void* screen, const struct reload_tf* camera) {
    if (!fv_button_class || !fv_ui_enabled || !cu_weak_assign || !cu_weak_get || !rl_world_transform) return 0;
    void* root = *(void* const*)((const char*)screen + 0x130);
    if (!fv_frame_owned(root, screen)) return 0;
    struct reload_tf current, framed;
    memcpy(&current, (const char*)root + 0x1c0, sizeof current);
    if (!reload_marker_valid(&current)) return 0;
    struct fv_plane planes[64];unsigned count=fv_screen_planes(screen,root,&current,planes,64,1);
    if (count>64) return 0;
    float aspect = g_comp_h ? (float)g_comp_w / (float)g_comp_h : 2.2f;
    if (!fv_frame_transform(camera, &current, planes, count, aspect, g_fov_h_deg, &framed)) return 0;
    if (cu_weak_get(&fv_framed.screen) != screen || cu_weak_get(&fv_framed.root) != root) {
        fv_frame_restore();
        fv_framed.original = current;
        cu_weak_assign(&fv_framed.screen, screen); cu_weak_assign(&fv_framed.root, root);
        if (fv_logs < 40) { char name[80]; obj_name(screen, name, sizeof name); fv_logs++; LOGF("UIFRAME '%s' screen=%p root=%p controls=%u", name, screen, root, count); }
    }
    rl_world_transform(root, &framed, 0, 0, 0);
    if (fv_screen_moved) fv_screen_moved(screen,root);
    return 1;
}

// ---- UI screen diagnostics / scripted clicks (dev console: uiscreen, uiclick <name-part>) ----
static volatile int g_ui_dump_request;
static char g_ui_click_pat[48];
static volatile int g_ui_click_request;
static void (*fv_ui_click)(void*, const void*, const void*);
struct fv_ui_object { void* actor; void* component; void* screen; };
struct fv_ui_click_state { unsigned char action, pad[3]; int count; float press[3], release[3]; float press_time, release_time; void* hand; };
_Static_assert(sizeof(struct fv_ui_click_state) == 0x30, "FVR4UIClickState ABI");
static void fv_dump_scene(const char* label, void* part) {
    if (!mv_scene_class || !part || !pv_alive(part) || !menu_visual_layout(part, mv_scene_class(), 0x1f0)) return;
    struct reload_tf tf; memcpy(&tf, (const char*)part + 0x1c0, sizeof tf);
    char name[64]; obj_name(part, name, sizeof name);
    LOGF("UISCREEN %s=%p '%s' owner=%p parent=%p q=(%.4f,%.4f,%.4f,%.4f) p=(%.1f,%.1f,%.1f) s=(%.4f,%.4f,%.4f)",
         label, part, name, *(void* const*)((const char*)part + 0xa0), *(void* const*)((const char*)part + 0xc0),
         tf.q.x, tf.q.y, tf.q.z, tf.q.w, tf.p[0], tf.p[1], tf.p[2], tf.s[0], tf.s[1], tf.s[2]);
}
static int fv_click_component(void* component, void* screen) {
    if (!fv_ui_click || !component || !pv_alive(component)) return 0;
    struct fv_ui_object object = {*(void**)((char*)component + 0x20), component, screen};
    struct fv_ui_click_state state;
    memset(&state, 0, sizeof state);
    state.action = 0; state.count = 1;     // EVR4UIAction::Click
    fv_ui_click(component, &object, &state);
    return 1;
}
// Native widget rectangles include headings/body text; enabled button bounds alone omit that context.
static struct cinema_ui_vec2 (*fv_current_draw_size)(const void*);
static unsigned fv_collect_widgets(void* root, struct fv_plane* out, unsigned capacity) {
    if (!cu_component_class || !cu_draw_size) return 0;
    void* queue[256] = {root}; unsigned queued = 1, count = 0;
    for (unsigned i = 0; i < queued; i++) {
        void* part = queue[i];
        if (!part || !pv_alive(part) || !menu_visual_layout(part, mv_scene_class(), 0x1f0)) continue;
        unsigned short flags = *(const unsigned short*)((const char*)part + 0x14c);
        if ((flags & 0x20) && !(flags & 0x400) && menu_visual_layout(part, cu_component_class(), 0x519)
            && !*(const unsigned char*)((const char*)part + 0x429)
            && !*(const unsigned char*)((const char*)part + 0x518)) {
            // DrawAtDesiredSize changes the rendered quad without updating the requested DrawSize.
            struct cinema_ui_vec2 size = fv_current_draw_size ? fv_current_draw_size(part) : cu_draw_size(part);
            const float* pivot = (const float*)((const char*)part + 0x45c);
            if (size.x > 0 && size.x <= 16384 && size.y > 0 && size.y <= 16384
                && isfinite(pivot[0]) && isfinite(pivot[1]) && pivot[0] >= 0 && pivot[0] <= 1 && pivot[1] >= 0 && pivot[1] <= 1) {
                if (count == capacity) return capacity + 1;
                struct fv_plane* p = out + count++;
                memcpy(&p->transform, (const char*)part + 0x1c0, sizeof p->transform);
                p->width = size.x; p->height = size.y;
                float local[3] = {0, (pivot[0] - .5f) * size.x * p->transform.s[1],
                                     (pivot[1] - .5f) * size.y * p->transform.s[2]}, shift[3];
                q_rot(p->transform.q, local, shift);
                for (unsigned k = 0; k < 3; k++) p->transform.p[k] += shift[k];
            }
        }
        struct { void** data; int count, max; } children;
        memcpy(&children, (const char*)part + 0xd0, sizeof children);
        if (children.count < 0 || children.count > 256 || children.max < children.count || (children.count && !children.data)) return capacity + 1;
        for (int k = 0; k < children.count; k++) {
            void* child = children.data[k]; int seen = 0;
            for (unsigned j = 0; j < queued; j++) if (queue[j] == child) seen = 1;
            if (seen) continue;
            if (queued == 256) return capacity + 1;
            queue[queued++] = child;
        }
    }
    return count;
}
#include "re4fix_ui_footer.h"
#include "re4fix_ui_console.h"
#include "re4fix_ui_start.h"
#include "re4fix_ui_actions.h"
#include "re4fix_ui_health.h"
#include "re4fix_ui_description.h"
#include "re4fix_ui_manual.h"
#include "re4fix_ui_material.h"
static unsigned fv_native_widgets(void* root,void** widgets,unsigned capacity) {
    void* queue[256]={root};unsigned queued=1,count=0;
    if (!root || !cu_component_class || !mv_scene_class) return 0;
    for(unsigned i=0;i<queued;i++) {
        void* part=queue[i];
        if (!part || !pv_alive(part) || !menu_visual_layout(part,mv_scene_class(),0x1f0)) continue;
        unsigned flags=*(const unsigned short*)((const char*)part+0x14c);
        if ((flags&0x20) && !(flags&0x400) && menu_visual_layout(part,cu_component_class(),0x519)) {
            if (count==capacity) return count;
            widgets[count++]=part;
        }
        struct {void** data;int count,max;} children;memcpy(&children,(const char*)part+0xd0,sizeof children);
        if (children.count<0 || children.count>256 || children.max<children.count || (children.count&&!children.data)) continue;
        for(int k=0;k<children.count && queued<256;k++) {
            void* child=children.data[k];int seen=0;
            for(unsigned j=0;j<queued;j++)if(queue[j]==child)seen=1;
            if (!seen)queue[queued++]=child;
        }
    }
    return count;
}
static void fv_menu_material_tick(void* screen,int lobby,const struct reload_tf* camera) {
    ui_material_begin();
    void* footer=cu_weak_get ? cu_weak_get(&fv_footer.footer) : 0;
    void* scroll=cu_weak_get ? cu_weak_get(&fv_footer.scroll) : 0;
    if (screen && cu_weak_get && cu_weak_get(&fv_footer.screen)==screen && footer && scroll) {
        void* row=cu_weak_get(&fv_footer.row);void* source[16],*parts[64];
        void* row_root=row && pv_alive(row) ? *(void* const*)((const char*)row+0x130) : 0;
        void* root=cu_weak_get(&fv_footer.root);
        unsigned sources=fv_native_widgets(row_root,source,16);
        if (sources) {
            unsigned count=fv_native_widgets(root,parts,64);int changed=0;
            for(unsigned i=0;i<count;i++)for(unsigned j=0;j<sources;j++)
                if (ui_material_clip(parts[i],source[j])) {changed=1;break;}
            if (changed && fv_scroll_update)fv_scroll_update(scroll);
        }
    } else if (screen && !lobby && phone_type && fv_world_popup_type(phone_type()) && pv_alive(screen)
               && !cinema_ui_named(screen,"OptionsUIScreen_BP_C",0x2d0)) {
        void* root=*(void* const*)((const char*)screen+0x130);void* parts[64];
        if (fv_frame_owned(root,screen)) {
            unsigned count=fv_native_widgets(root,parts,64);
            struct reload_tf widgets[64];int priorities[64];
            for(unsigned i=0;i<count;i++)memcpy(widgets+i,(const char*)parts[i]+0x1c0,sizeof *widgets);
            if (fv_front_priorities(camera,widgets,count,priorities))
                for(unsigned i=0;i<count;i++)ui_material_front(parts[i],priorities[i]);
        }
    }
    ui_material_end();
}
static void touch_ui_diag_tick(void) {
    int dump = __atomic_exchange_n(&g_ui_dump_request, 0, __ATOMIC_ACQ_REL);
    int click = __atomic_exchange_n(&g_ui_click_request, 0, __ATOMIC_ACQ_REL);
    if ((!dump && !click) || !fv_ui_manager) return;
    void* manager = fv_ui_manager();
    void* screen = manager ? fv_peek_screen(manager) : 0;
    if (!screen || !pv_alive(screen)) { LOGF("UISCREEN none"); return; }
    char name[96], cls[96];
    obj_name(screen, name, sizeof name); obj_name(*(void* const*)((const char*)screen + 0x10), cls, sizeof cls);
    if (dump) {
        LOGF("UISCREEN top=%p '%s' class='%s'", screen, name, cls);
        void* outer = manager ? *(void* const*)((const char*)manager + 0x20) : 0;
        char outer_name[80] = "", outer_class[80] = "";
        if (outer && pv_alive(outer)) {
            obj_name(outer, outer_name, sizeof outer_name);
            obj_name(*(void* const*)((const char*)outer + 0x10), outer_class, sizeof outer_class);
        }
        LOGF("UISCREEN manager=%p outer=%p '%s' class='%s'", manager, outer, outer_name, outer_class);
        fv_dump_scene("root", *(void* const*)((const char*)screen + 0x130));
        if (phone_active && phone_active()) fv_dump_scene("camera", *(void* const*)((const char*)phone_active() + 0x2d8));
    }
    struct { void** data; int count, max; } list;
    memcpy(&list, (const char*)screen + 0x238, sizeof list);
    if (list.count < 0 || list.count > 512 || list.max < list.count || (list.count && !list.data)) return;
    for (int i = 0; i < list.count; i++) {
        void* component = list.data[i];
        if (!component || !pv_alive(component)) continue;
        char cname[96], ccls[64], owner[96] = "";
        obj_name(component, cname, sizeof cname); obj_name(*(void* const*)((const char*)component + 0x10), ccls, sizeof ccls);
        void* outer = *(void* const*)((const char*)component + 0x20);
        if (outer) obj_name(outer, owner, sizeof owner);
        float at[3], sx = -1, sy = -1, depth = 0;
        if (dump && aim_head && phone_active && phone_active() && wt_component_location(component, at)) {
            struct reload_tf camera = aim_head(phone_active());
            camera.q = q_mul(camera.q, pose_camera_delta_ue());
            if (!reload_tf_valid(&camera) || !wt_project(&camera, at, &sx, &sy, &depth)) sx = sy = -1;
        }
        if (dump) LOGF("UISCREEN  [%d] %p '%s' class='%s' owner='%s' enabled=%u state=%u screen=%.0f,%.0f d=%.0f", i, component, cname, ccls, owner,
                       *(const unsigned char*)((const char*)component + 0x264), *(const unsigned char*)((const char*)component + 0x260), sx, sy, depth);
        if (dump && i < 4) {
            void* part = component;
            for (unsigned depth = 0; part && depth < 8; depth++) {
                if (!menu_visual_layout(part, mv_scene_class(), 0x1f0)) break;
                fv_dump_scene("chain", part);
                void* parent = *(void* const*)((const char*)part + 0xc0);
                if (parent == part) break;
                part = parent;
            }
        }
        if (click && g_ui_click_pat[0] && (strstr(cname, g_ui_click_pat) || strstr(owner, g_ui_click_pat))) {
            click = 0;
            LOGF("UISCREEN click %p '%s' owner='%s' -> %d", component, cname, owner, fv_click_component(component, screen));
        }
    }
}
// New game: select the wizard's original Finish page and invoke its own Next once.
static void* fv_wizard_screen;
static int fv_wizard_is;
static long fv_wizard_click_ms;
static struct cinema_ui_weak fv_wizard_finished = {-1,0};
static int fv_finish_wizard(void* screen) {
    if (!screen || !fv_set_int || !fv_ui_click || !fv_button_class || !cu_weak_get || !cu_weak_assign
        || !cinema_ui_named(screen, "ComfortCustomizeCoverScreen_BP_C", 0x250)) return 0;
    if (cu_weak_get(&fv_wizard_finished) == screen) return 1;
    const char* field = cinema_field_property(screen, "currentScreen", "IntProperty");
    const char* cls = *(const char* const*)((const char*)screen + 0x10);
    int offset = field ? *(const int*)(field + 0x4c) : -1;
    if (!field || offset < 0x250 || offset > *(const int*)(cls + 0x58) - 4
        || *(const int*)(field + 0x34) != 1 || *(const int*)(field + 0x38) != 4) return 0;
    struct { void** data; int count, max; } list;
    memcpy(&list, (const char*)screen + 0x238, sizeof list);
    if (list.count <= 0 || list.count > 64 || list.max < list.count || !list.data) return 0;
    for (int i = 0; i < list.count; i++) {
        void* next = list.data[i]; char name[64];
        if (!next || !pv_alive(next) || !menu_visual_layout(next, fv_button_class(), 0x320)
            || *(void* const*)((const char*)next + 0x308) != screen
            || *(void* const*)((const char*)next + 0xa0) != screen
            || !*(const unsigned char*)((const char*)next + 0x264)) continue;
        obj_name(next, name, sizeof name);
        if (strcmp(name, "VR4UIButton_Next")) continue;
        unsigned long long field_name; memcpy(&field_name, field + 0x28, sizeof field_name);
        fv_set_int(screen, field_name, 4);
        if (*(const int*)((const char*)screen + offset) != 4) return 0;
        cu_weak_assign(&fv_wizard_finished, screen);
        fv_click_component(next, screen);
        LOGF("WIZARD finished through original final-page Next; VR setup pages omitted");
        return 1;
    }
    return 0;
}
// GameThread (menu hand tick): publish tap targets of in-world screens during gameplay; run a queued direct click.
static void* fv_options_header_back(void* screen) {
    if (!cinema_on_game_thread() || !fv_button_class || !fv_ui_enabled || !pv_alive(screen)
        || !cinema_ui_named(screen,"OptionsUIScreen_BP_C",0x2f0)) return 0;
    void* menu=fv_object_property(screen,"CurrentMenu");
    if (!pv_alive(menu) || !cinema_ui_named(menu,"OptionsScreen_Menu_C",0x308)
        || fv_object_property(menu,"OptionsScreen")!=screen) return 0;
    void* screen_root=*(void* const*)((const char*)screen+0x130), *root=*(void* const*)((const char*)menu+0x130);
    void* back=fv_object_property(menu,"BackButton");
    if (!fv_frame_owned(screen_root,screen) || !fv_frame_owned(root,menu) || !fv_descends_from(root,screen_root)
        || !pv_alive(back) || !menu_visual_layout(back,fv_button_class(),0x321)
        || *(void* const*)((const char*)back+0x20)!=menu || *(void* const*)((const char*)back+0xa0)!=menu
        || *(void* const*)((const char*)back+0x308)!=screen || *(void* const*)((const char*)back+0x310)
        || *(const unsigned char*)((const char*)back+0x320)!=0 || !fv_descends_from(back,root)
        || !fv_ui_enabled(back)) return 0;
    return back;
}
static void* fv_options_cover_inventory(void* screen,void* pause) {
    if (!pv_child_class || !fv_button_class || !fv_ui_enabled || !pv_alive(screen)
        || !pv_alive(pause) || !cinema_ui_named(pause,"UI_PauseMenu_BP_C",0x258)) return 0;
    void* content=fv_object_property(pause,"Ref_ContentActor"), *header=fv_object_property(pause,"Ref_Header_InGame");
    void* content_child=fv_object_property(pause,"Content_Actor");
    // The authored content is a ChildActor: its AActor::Owner is legitimately null.
    // Prove the actual owning component rather than substituting the actor Owner field.
    if (!pv_alive(content) || !cinema_ui_named(content,"UI_PauseMenu_Content_GameOptions_BP_C",0x3c0)
        || !pv_alive(content_child) || !menu_visual_layout(content_child,pv_child_class(),0x220)
        || *(void* const*)((const char*)content_child+0x20)!=pause || *(void* const*)((const char*)content_child+0xa0)!=pause
        || *(void* const*)((const char*)content_child+0x1f8)!=content || fv_object_property(content,"CentralCoverScreen")!=screen
        || *(void* const*)((const char*)screen+0xe0)!=content || !pv_alive(header)
        || !cinema_ui_named(header,"UI_PauseMenu_Header_BP_C",0x220) || fv_object_property(header,"Ref_Parent_PauseMenu")!=pause) return 0;
    void* pause_root=*(void* const*)((const char*)pause+0x130), *root=*(void* const*)((const char*)header+0x130);
    void* child=fv_object_property(header,"Button_Inventory");
    if (!fv_frame_owned(pause_root,pause) || !fv_frame_owned(root,header) || !fv_descends_from(root,pause_root)
        || !pv_alive(child) || !menu_visual_layout(child,pv_child_class(),0x220)
        || *(void* const*)((const char*)child+0x20)!=header || *(void* const*)((const char*)child+0xa0)!=header) return 0;
    void* actor=*(void* const*)((const char*)child+0x1f8);
    if (!pv_alive(actor) || !cinema_ui_named(actor,"UI_PauseMenu_Header_Button_C",0x220)
        || fv_object_property(actor,"Ref_Parent_Header")!=header) return 0;
    void* button=fv_object_property(actor,"Button");
    return pv_alive(button) && menu_visual_layout(button,fv_button_class(),0x321)
        && *(void* const*)((const char*)button+0x20)==actor && *(void* const*)((const char*)button+0xa0)==actor
        && *(void* const*)((const char*)button+0x308)==pause && !*(void* const*)((const char*)button+0x310)
        && !*(const unsigned char*)((const char*)button+0x320) && fv_descends_from(button,root) && fv_ui_enabled(button) ? button:0;
}
static int fv_options_back_action(void* pause) {
    if (!cinema_on_game_thread() || !phone_type || phone_type()!=2 || !fv_ui_manager || !fv_peek_screen) return 0;
    void* manager=fv_ui_manager(), *screen=manager ? fv_peek_screen(manager):0;
    if (!pv_alive(screen)) return 0;
    void* back=0,*owner=screen;
    if (cinema_ui_named(screen,"OptionsUIScreen_BP_C",0x2f0)) back=fv_options_header_back(screen);
    else if (cinema_ui_named(screen,"OptionsCoverScreen_BP_C",0x248)) {
        // The original Inventory tab updates the remembered pause tab through SwapMenuFn/CanSwapMenu.
        // A direct resume here would reopen this Options cover on the next Inventory request.
        back=fv_options_cover_inventory(screen,pause);owner=pause;
    } else return 0;
    // The authored delegate clears LastPauseMenuSubScreen and closes the panels before returning.
    // Never resume behind an Options screen whose native Back is temporarily unavailable.
    return back && fv_ui_click && fv_click_component(back,owner) ? 1:-1;
}
static int fv_load_cover_button(void* screen,void* button) {
    if(!cinema_on_game_thread() || !fv_button_class || !fv_ui_enabled || !pv_alive(screen)
        || !cinema_ui_named(screen,"LoadGameCoverScreen_BP_C",0x248)
        || !pv_alive(button) || !menu_visual_layout(button,fv_button_class(),0x321)
        || *(void* const*)((const char*)button+0x20)!=screen
        || *(void* const*)((const char*)button+0xa0)!=screen
        || *(void* const*)((const char*)button+0x308)!=screen
        || *(const unsigned char*)((const char*)button+0x320)!=0 || !fv_ui_enabled(button))return 0;
    if(button!=fv_object_property(screen,"VR4UIButton_continue")
        && button!=fv_object_property(screen,"VR4UIButton_loadSave"))return 0;
    void* root=*(void* const*)((const char*)screen+0x130);
    return fv_frame_owned(root,screen) && fv_descends_from(button,root);
}
static void touch_ui_targets_tick(long now) {
    static long last_ns;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long ns = ts.tv_sec * 1000000000L + ts.tv_nsec;
    if (ns - last_ns < 6000000L && ns >= last_ns) return;   // pooled menu hands: once per frame
    last_ns = ns;
    void* click = __atomic_exchange_n(&g_ui_direct_click, 0, __ATOMIC_ACQ_REL);
    struct ui_target targets[16]; int count = 0;
    void* screen = 0;
    if (phone_type && fv_ui_manager && aim_head && phone_active && phone_active()) {
        void* manager = fv_ui_manager();
        screen = manager ? fv_peek_screen(manager) : 0;
        if (screen && (!pv_alive(screen) || !menu_visual_layout(screen, fv_screen_class(), 0x248))) screen = 0;
    }
    if (screen) {
        struct reload_tf camera = aim_head(phone_active());
        camera.q = q_mul(camera.q, pose_camera_delta_ue());
        void* footer=cu_weak_get ? cu_weak_get(&fv_footer.footer) : 0;
        void* scroll=cu_weak_get ? cu_weak_get(&fv_footer.scroll) : 0;
        if (!cu_weak_get || cu_weak_get(&fv_footer.screen)!=screen)footer=0;
        unsigned type=phone_type();
        void* header=(type==2 || type==4 || type==7) ? fv_options_header_back(screen):0;
        int load_cover=type==7 && cinema_ui_named(screen,"LoadGameCoverScreen_BP_C",0x248);
        // Menu actors are pooled at the same position. Only CurrentMenu owns the visible header;
        // retain its original Click delegate instead of selecting another menu's Back by position.
        // The current Load cover's two original Click controls also need direct phone tap targets.
        void* screens[3]={header ? screen:0,type==1 || load_cover ? screen:0,footer};
        for(unsigned s=0;s<3;s++) {
            void* owner=screens[s];if (!owner || !pv_alive(owner))continue;
            struct { void** data; int count, max; } list;
            memcpy(&list, (const char*)owner + 0x238, sizeof list);
            if (reload_tf_valid(&camera) && list.count > 0 && list.count <= 512 && list.max >= list.count && list.data)
            for (int i = 0; i < list.count && count < 16; i++) {
                void* component = list.data[i];
                float at[3], sx, sy, depth;
                if ((!s && component!=header) || !component || !pv_alive(component) || !fv_ui_enabled(component)
                    || !menu_visual_layout(component,fv_button_class(),0x320)
                    || *(void* const*)((const char*)component+0x308)!=owner
                    || (s==1 && load_cover && !fv_load_cover_button(screen,component))
                    || (s==2 && !fv_footer_control_visible(component,scroll))
                    || !wt_component_location(component, at) || !wt_project(&camera, at, &sx, &sy, &depth) || depth > 600.0f) continue;
                if (sx<0 || sx>g_comp_w || sy<0 || sy>g_comp_h)continue;
                if (component == click) {
                    click = 0;
                    char name[64];obj_name(component,name,sizeof name);
                    int accepted=fv_click_component(component,owner);
                    LOGF("UITAP click %p '%s' owner=%p -> %d",component,name,owner,accepted);
                }
                targets[count].x = sx; targets[count].y = sy; targets[count].component = component; count++;
            }
        }
    }
    pthread_mutex_lock(&g_ui_target_mx);
    memcpy(g_ui_targets, targets, sizeof targets); g_ui_target_count = count; g_ui_targets_ms = now;
    pthread_mutex_unlock(&g_ui_target_mx);
}
static long fv_hint_since;
// First-time hint popups (map, files, treasure, ...): a tap anywhere dismisses them through their own OK button.
// Taps that already hit OK close the popup natively first; only a tap that left it open triggers the click.
static void fv_hint_popup_tick(void* screen, long now) {
    long tap = __atomic_load_n(&g_menu_tap_ms, __ATOMIC_ACQUIRE);
    if (tap <= fv_hint_since || now - tap < 250 || now - tap > 1500) return;
    struct { void** data; int count, max; } list;
    memcpy(&list, (const char*)screen + 0x238, sizeof list);
    if (list.count <= 0 || list.count > 16 || list.max < list.count || !list.data) return;
    for (int i = 0; i < list.count; i++) {
        void* component = list.data[i];
        if (!component || !pv_alive(component) || !*(const unsigned char*)((const char*)component + 0x264)) continue;
        fv_hint_since = now;
        fv_click_component(component, screen);
        if (fv_logs < 40) { fv_logs++; LOGF("HINT popup dismissed by tap"); }
        return;
    }
}
static void touch_comfort_wizard_tick(long now) {
    if (!fv_ui_manager || !fv_ui_click) return;
    void* manager = fv_ui_manager();
    void* screen = manager ? fv_peek_screen(manager) : 0;
    if (!screen || !pv_alive(screen)) { fv_wizard_screen = 0; return; }
    if (screen != fv_wizard_screen) {
        char cls[96] = "";
        obj_name(*(void* const*)((const char*)screen + 0x10), cls, sizeof cls);
        fv_wizard_screen = screen;
        fv_wizard_is = !strcmp(cls, "ComfortCustomizeCoverScreen_BP_C") ? 1
            : !strcmp(cls, "PauseScreen_FirstTime_C") || !strcmp(cls, "PauseScreen_Intro_C") ? 2 : 0;
        if (fv_wizard_is == 2) fv_hint_since = now;
    }
    if (fv_wizard_is == 2) { fv_hint_popup_tick(screen, now); return; }
    if (!fv_wizard_is) return;
    if (fv_finish_wizard(screen)) return;
    if (now - fv_wizard_click_ms < 180) return;
    struct { void** data; int count, max; } list;
    memcpy(&list, (const char*)screen + 0x238, sizeof list);
    if (list.count < 0 || list.count > 64 || list.max < list.count || (list.count && !list.data)) return;
    void* next = 0, *current = 0;
    for (int i = 0; i < list.count; i++) {
        void* component = list.data[i];
        if (!component || !pv_alive(component) || !*(const unsigned char*)((const char*)component + 0x264)) continue;
        char name[64]; obj_name(component, name, sizeof name);
        if (!strcmp(name, "VR4UIButton_Next")) next = component;
        else if (!strcmp(name, "VR4UIButton_currentSettings")) current = component;
    }
    void* target = next ? next : current;
    if (!target) return;
    fv_wizard_click_ms = now;
    fv_click_component(target, screen);
    if (fv_logs < 20) { fv_logs++; LOGF("WIZARD skip: clicked %s", target == next ? "Next" : "Current settings"); }
}
static int fv_actor_location(const void* actor, float out[3]) {
    if (!actor) return 0;
    void* root = *(void* const*)((const char*)actor + 0x130);
    return root && wt_component_location(root, out);
}
// GameThread (menu hand tick).
static void touch_menu_view_tick(long now) {
    fv_start_confirm_tick(now);
    unsigned type = phone_type ? phone_type() : 0;
    fv_title_intro_tick(now,type);
    int lobby = now - __atomic_load_n(&g_lobby_menu_ms, __ATOMIC_ACQUIRE) < 1000;
    // Runs from the active menu-hand Tick only: type 1 here is an in-world overlay menu.
    if ((type != 1 && type != 2 && type != 4 && type != 6 && type != 7 && type != 8)
        || now - g_menu_tick_ms >= 300 || !fv_ui_manager || !aim_head || !phone_active) {
        g_menu_view_active = 0; g_menu_yaw_off = 0; g_menu_pitch = 0;
        fv_camera_menu.used=0;
        fv_lobby_zoom_ms = 0;
        fv_manual_restore();
        fv_description_tick(0);
        fv_menu_material_tick(0,0,0);
        fv_footer_restore();
        fv_frame_restore();
        fv_console_restore();
        fv_actions_publish(0,now);
        return;
    }
    // Several pooled menu hands tick per frame: correct once per frame, or the loop gain multiplies and the
    // view swings around the menu.
    static long last_frame_ns;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long ns = ts.tv_sec * 1000000000L + ts.tv_nsec;
    if (ns - last_frame_ns < 6000000L && ns >= last_frame_ns) return;
    last_frame_ns = ns;
    fv_camera_menu.used=0;
    void* pawn = phone_active();
    void* manager = fv_ui_manager();
    void* screen = manager ? fv_peek_screen(manager) : 0;
    fv_actions_publish(screen,now);
    float target[3];
    if (!pawn || !screen || !pv_alive(screen) || !menu_visual_layout(screen, fv_screen_class(), 0x248)
        || !fv_actor_location(screen, target)) {
        g_menu_view_active=0;g_menu_yaw_off=g_menu_pitch=0;
        fv_manual_restore();fv_description_tick(0);fv_menu_material_tick(0,0,0);fv_footer_restore(); return;
    }
    struct reload_tf camera = aim_head(pawn);
    if (!reload_tf_valid(&camera)) {g_menu_view_active=0;g_menu_yaw_off=g_menu_pitch=0;return;}
    camera.q = q_mul(camera.q, pose_camera_delta_ue());   // this frame's view, not last frame's
    fv_footer_tick(screen);
    fv_description_tick(screen);
    if (lobby) {
        fv_console_restore();
        fv_manual_restore();
        fv_frame_restore();
        fv_menu_material_tick(screen,1,&camera);
        g_menu_view_active = 0; g_menu_yaw_off = 0; g_menu_pitch = 0;
        fv_lobby_zoom_ms = now;
        return;
    }
    fv_lobby_zoom_ms = 0;
    if (fv_console_tick(screen,&camera)) {
        fv_menu_material_tick(screen,0,&camera);
        g_menu_view_active=0;g_menu_yaw_off=0;g_menu_pitch=0;
        return;
    }
    if (fv_manual_tick(screen,&camera)) {
        fv_menu_material_tick(screen,0,&camera);
        g_menu_view_active=0;g_menu_yaw_off=0;g_menu_pitch=0;
        return;
    }
    if(fv_camera_menu_tick(screen,&camera,now)) {
        fv_frame_restore();
        fv_menu_material_tick(screen,0,&camera);
        return;
    }
    if (fv_frame_screen(screen, &camera)) {
        fv_menu_material_tick(screen,0,&camera);
        g_menu_view_active = 0; g_menu_yaw_off = 0; g_menu_pitch = 0;
        return;
    }
    fv_menu_material_tick(screen,0,&camera);
    g_menu_yaw_off=g_menu_pitch=0;g_menu_view_active=0;
}
static volatile int g_project_diag_request;
// Diagnostic: screen projection of every registered grip's owner and its scene components.
static void* volatile g_inspect_ptr;
static volatile int g_inspect_owner;
// GameThread (gameplay right hand and menu hand ticks): dev reflection dump of an object or a component's owner.
static void touch_inspect_tick(void) {
    void* inspect = __atomic_exchange_n(&g_inspect_ptr, 0, __ATOMIC_ACQ_REL);
    if (inspect && pv_alive(inspect)) {
        if (g_inspect_owner) inspect = *(void* const*)((const char*)inspect + 0x20);
        if (inspect && pv_alive(inspect)) phone_inspect_objects(inspect, 0, 0);
    }
}
static void touch_project_diag(void* pawn) {
    touch_inspect_tick();
    if (!__atomic_exchange_n(&g_project_diag_request, 0, __ATOMIC_ACQ_REL) || !pawn || !aim_head) return;
    struct reload_tf camera = aim_head(pawn);
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips; i++) {
        void* owner = *(void* const*)((const char*)g_grips[i].g + 0x20);
        float world[3], sx = -1, sy = -1, depth = 0;
        int ok = owner && fv_actor_location(owner, world) && wt_project(&camera, world, &sx, &sy, &depth);
        float gw[3], gx = -1, gy = -1, gd = 0;
        int gok = wt_component_location(g_grips[i].g, gw) && wt_project(&camera, gw, &gx, &gy, &gd);
        if (ok || gok) LOGF("PROJECT grip=%p '%s'.'%s' owner_screen=%.0f,%.0f d=%.0f grip_screen=%.0f,%.0f d=%.0f",
                            g_grips[i].g, g_grips[i].owner, g_grips[i].comp, sx, sy, depth, gx, gy, gd);
    }
    pthread_mutex_unlock(&g_gripmx);
}
// GameThread (right game hand tick, gameplay): hide VR holster icons and grip indicators. Seated holster BPs keep
// their icon widget (UI_SeatedIcon_*) and the black ArcMesh outside IconRoot, so those are resolved by reflection
// (cached per class). SetHiddenInGame wins over the Blueprint's own visibility toggles.
static struct { const void* cls; int widget, arc; } fv_holster_cache[8];
static void fv_holster_offsets(void* holster, int* widget, int* arc) {
    const void* cls = *(const void* const*)((const char*)holster + 0x10);
    for (int i = 0; i < 8; i++) if (fv_holster_cache[i].cls == cls) { *widget = fv_holster_cache[i].widget; *arc = fv_holster_cache[i].arc; return; }
    *widget = cinema_field_offset(holster, "IconWidget", "ObjectProperty");
    *arc = cinema_field_offset(holster, "ArcMesh", "ObjectProperty");
    const char* c = (const char*)cls; int size = c ? *(const int*)(c + 0x58) : 0;
    if (*widget < 0x240 || *widget > size - 8) *widget = -1;
    if (*arc < 0x240 || *arc > size - 8) *arc = -1;
    for (int i = 0; i < 8; i++) if (!fv_holster_cache[i].cls) {
        fv_holster_cache[i].cls = cls; fv_holster_cache[i].widget = *widget; fv_holster_cache[i].arc = *arc; break;
    }
}
// Hide a holster part and its own sub-components without native propagation: in seated mode the holstered gun
// hangs under the seated IconWidget, and a propagated hide left the drawn pistol invisible in the hand.
// USceneComponent AttachChildren: data +0xd0, count +0xd8 (SetHiddenInGame 0x83adc18 walks the same array).
static int fv_hide_owned(void* part, const void* holster, int depth) {
    int hid = 0;
    if (!(*(const unsigned short*)((const char*)part + 0x14c) & 0x400)) { mv_hidden(part, 1, 0); hid = 1; }
    if (depth >= 6) return hid;
    void* const* children = *(void* const* const*)((const char*)part + 0xd0);
    int count = *(const int*)((const char*)part + 0xd8);
    if (!children || count <= 0 || count > 64) return hid;
    for (int i = 0; i < count; i++) {
        void* child = children[i];
        if (!child || !pv_alive(child) || *(void* const*)((const char*)child + 0xa0) != holster) continue;
        hid |= fv_hide_owned(child, holster, depth + 1);
    }
    return hid;
}
static void touch_holster_visual_tick(long now) {
    if (now - fv_holster_ms < 250 || !mv_hidden || !ti_holster_class) return;
    fv_holster_ms = now;
    void* holster_class = ti_holster_class();
    pthread_mutex_lock(&g_gripmx);
    for (int i = 0; i < g_ngrips; i++) {
        if (strcmp(g_grips[i].comp, "HolsterGrip") || strncmp(g_grips[i].owner, "PropHolster_", 12)) continue;
        void* holster = *(void* const*)((const char*)g_grips[i].g + 0x20);
        if (!holster || !interact_is_a(holster, holster_class)) continue;
        int widget = -1, arc = -1;
        fv_holster_offsets(holster, &widget, &arc);
        void* parts[4] = {*(void* const*)((const char*)holster + 0x278),     // IconRoot
                          *(void* const*)((const char*)holster + 0x258),     // HolsterGripIndicator
                          widget > 0 ? *(void* const*)((const char*)holster + widget) : 0,   // IconWidget
                          arc > 0 ? *(void* const*)((const char*)holster + arc) : 0};        // ArcMesh
        for (int k = 0; k < 4; k++) {
            void* part = parts[k];
            if (!part || !menu_visual_layout(part, mv_scene_class(), 0x150)
                || *(void* const*)((const char*)part + 0xa0) != holster) continue;
            if (fv_hide_owned(part, holster, 0) && fv_logs < 40) {
                fv_logs++; LOGF("FLATVIEW hid holster part=%p holster='%s' slot=%d", part, g_grips[i].owner, k);
            }
        }
    }
    pthread_mutex_unlock(&g_gripmx);
}
static void install_flat_view(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x6373060, {0xf001ff88, 0xf9449d08, 0xf9400100, 0xd65f03c0}},   // UVR4UIManager::GetInstance
        {0x6373328, {0xf001ff88, 0xf9449d08, 0xf9400108, 0xb40002e8}},   // UVR4UIManager::PeekUIScreen
        {0x64db798, {0xd10183ff, 0xf90023f3, 0xa9057bfd, 0x910143fd}},   // AVR4UIScreen::GetPrivateStaticClass
        {0x64d8eec, {0xd10183ff, 0xf90023f3, 0xa9057bfd, 0x910143fd}},   // UVR4UIButtonComponent::GetPrivateStaticClass
        {0x6371fdc, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0x39499008}},   // IsEnabled: includes native visibility
        {0x637139c, {0xf9418400, 0xd65f03c0, 0xf9018401, 0xd65f03c0}},   // Get/SetRegisteredScreen
        {0x83a926c, {0xd10303ff, 0xa9085ff8, 0xa90957f6, 0xa90a4ff4}},   // USceneComponent::SetWorldTransform
        {0x897628c, {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},   // UKismetSystemLibrary::SetIntPropertyByName
        {0x63784c4, {0x6db733ed, 0x6d012beb, 0x6d0223e9, 0xa9036ffc}},   // Scroll.AttachActor
        {0x637a830, {0xd10503ff, 0xf90073fc, 0xa90f67fa, 0xa9105ff8}},   // Scroll.UnattachActor: clear clip/UI parent
        {0x6378330, {0xd101c3ff, 0xa9016ffc, 0xa90267fa, 0xa9035ff8}},   // Scroll.FindAttachedActor
        {0x63795c0, {0xd10343ff, 0x6d082beb, 0x6d0923e9, 0xf90053f5}},   // Scroll.GetContentToWorldTransform
        {0x83a96f4, {0x6dba33ed, 0x6d012beb, 0x6d0223e9, 0xa90357f6}},   // Scene.GetRelativeTransform
        {0x83a8b84, {0xd100c3ff, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},   // Scene.SetRelativeTransform
        {0x64dee50, {0xd10183ff, 0xf90023f3, 0xa9057bfd, 0x910143fd}},   // Scroll class size430
        {0x637b594, {0xd10783ff, 0x6d143bef, 0x6d1533ed, 0x6d162beb}},   // Scroll.UpdateAttachedActors
        {0x7db1fc8, {0xbd445000, 0xbd445401, 0x5e21d800, 0x5e21d821}},   // Widget.GetCurrentDrawSize: integer pixels -> FVector2D
    };
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++)
        if (memcmp((const void*)(base + check[i].addr), check[i].first, 16)) {
            LOGF("FLATVIEW disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    fv_ui_manager = (void* (*)(void))(base + 0x6373060);
    fv_peek_screen = (void* (*)(void*))(base + 0x6373328);
    fv_screen_class = (void* (*)(void))(base + 0x64db798);
    fv_set_int = (void (*)(void*, unsigned long long, int))(base + 0x897628c);
    install_ui_material(base);
    static const struct { unsigned long address; unsigned offset; } fields[] = {
        {0x9e0fbb0, 0x318}, {0x9e0fb88, 0x31c}, {0x9e10ed8, 0x308}, {0x9e10ea8, 0x310},
        {0xa0b7530, 0x130}, {0xa235ba8, 0xc0}, {0x9e11c48, 0x238},
        {0x9e12a08, 0xa0}, {0x9f77c28, 0x45c}, {0x9f77e20, 0x429}, {0x9f77698, 0x518},
        {0x9e14028, 0x318}, {0x9e14000, 0x31c}, {0x9e0fb28, 0x320}, {0xa1087b8, 0x1f8},
    };
    int framing = 1;
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; i++)
        if (*(const unsigned*)(base + fields[i].address + 0x24) != fields[i].offset) framing = 0;
    if (framing && rl_world_transform == (void (*)(void*, const struct reload_tf*, int, void*, int))(base + 0x83a926c)) {
        fv_button_class = (void* (*)(void))(base + 0x64d8eec);
        fv_ui_enabled = (int (*)(const void*))(base + 0x6371fdc);
        fv_widget_planes = fv_collect_widgets;
        fv_current_draw_size = (struct cinema_ui_vec2 (*)(const void*))(base + 0x7db1fc8);
        cinema_menu_view = fv_menu_camera_view;
        fv_scroll_class = (void* (*)(void))(base + 0x64dee50);
        fv_scroll_attach = (void (*)(void*,void*,const struct reload_tf*))(base + 0x63784c4);
        fv_scroll_unattach = (void (*)(void*,void*))(base + 0x637a830);
        fv_scroll_find = (int (*)(void*,int*,void*))(base + 0x6378330);
        fv_scroll_world = (struct reload_tf (*)(const void*))(base + 0x63795c0);
        fv_relative_transform = (struct reload_tf (*)(const void*))(base + 0x83a96f4);
        fv_set_relative = (void (*)(void*,const struct reload_tf*,int,void*,int))(base + 0x83a8b84);
        fv_scroll_update = (void (*)(void*))(base + 0x637b594);
        fv_control_plane = fv_scroll_plane;
        fv_screen_moved = fv_update_scrollers;
        unpause_ui_back = fv_options_back_action;
    }
    if (!memcmp((const void*)(base + 0x6371bcc), (const unsigned[]){0xd101c3ff, 0xa90457f6, 0xa9054ff4, 0xa9067bfd}, 16))
        fv_ui_click = (void (*)(void*, const void*, const void*))(base + 0x6371bcc);
    install_ui_start(base);
    install_ui_actions(base);
    install_ui_health(base);
    LOGF("FLATVIEW ready: owned native UI framing=%d, hidden VR holster icons", framing && fv_ui_enabled != 0);
}
#endif
