// Include after cutscene_weapon.h; reuse its original-first avatar writer and exact leaf bindings.
#ifndef RE4FIX_PHONE_VIEW_H
#define RE4FIX_PHONE_VIEW_H
#include <math.h>
static void* (*pv_pawn_class)(void);
static int phone_view_ready;
static unsigned pv_avatar_logs;
struct __attribute__((aligned(16))) pv_scene_tf { float q[4],p[3],s[3],padding[2]; };
struct pv_mesh_bounds { float origin[3],extent[3],radius; };
static struct pv_scene_tf (*pv_head_transform)(const void*);
static struct pv_mesh_bounds (*pv_mesh_bounds)(const void*);
static void* (*pv_static_class)(void), *(*pv_asset_class)(void);
static void (*pv_name)(const void*,void*), (*pv_free)(void*);
_Static_assert(sizeof(struct pv_scene_tf)==48 && sizeof(struct pv_mesh_bounds)==28,"native scalar probe return ABI");

static void pv_name_text(const void* name,char* out,int capacity) {
    out[0]=0;
    if (!pv_name || !pv_free || !name) return;
    struct { unsigned short* data;int count,capacity; } text={0,0,0};
    pv_name(name,&text);
    if (text.data && text.count>0 && text.count<32768) {
        int i=0;for (;i<capacity-1 && i<text.count && text.data[i];i++) out[i]=text.data[i]<128 ? (char)text.data[i] : '?';
        out[i]=0;
    }
    if (text.data) pv_free(text.data);
}
static void* pv_object_property(void* object,const char* wanted) {
    const char* cls=object ? *(const char* const*)((const char*)object+0x10) : 0;
    int size=cls ? *(const int*)(cls+0x58) : 0;
    if (size<0x138 || size>0x10000) return 0;
    const char* field=*(const char* const*)(cls+0x70);
    for (unsigned i=0;field && i<256;i++) {
        char name[64],kind[48];pv_name_text(field+0x28,name,sizeof name);
        if (!strcmp(name,wanted)) {
            pv_name_text(*(const void* const*)(field+0x8),kind,sizeof kind);
            int offset=*(const int*)(field+0x4c);
            if (strcmp(kind,"ObjectProperty") || *(const int*)(field+0x38)!=8
                || *(const int*)(field+0x34)!=1 || offset<0 || offset>size-8) return 0;
            LOGF("PHONEVIEWFIELD name=%s offset=%x",wanted,offset);
            return *(void* const*)((const char*)object+offset);
        }
        const char* next=*(const char* const*)(field+0x58);if (next==field) break;field=next;
    }
    return 0;
}
static int pv_tf_finite(const struct pv_scene_tf* tf) {
    float norm=0;for (unsigned i=0;i<4;i++) norm+=tf->q[i]*tf->q[i];
    if (!isfinite(norm) || fabsf(norm-1)>0.01f) return 0;
    for (unsigned i=0;i<3;i++) if (!isfinite(tf->p[i]) || !isfinite(tf->s[i]) || tf->s[i]<=0) return 0;
    return 1;
}
static int pv_probe_component(const void* component,const void* device) {
    return cutscene_weapon_layout(component,cw_scene_class(),0x1f0)
        && *(void* const*)((const char*)component+0x20)==device;
}
static void pv_probe_scene(const char* label,void* component,void* device) {
    if (!pv_probe_component(component,device)) { LOGF("PHONEVIEWTF label=%s rejected owned Scene",label);return; }
    struct pv_scene_tf tf;memcpy(&tf,(const char*)component+0x1c0,sizeof tf);
    void* parent=*(void* const*)((const char*)component+0xc0);
    LOGF("PHONEVIEWTF label=%s object=%p parent=%p q=(%.6f,%.6f,%.6f,%.6f) p=(%.6f,%.6f,%.6f) s=(%.6f,%.6f,%.6f) finite=%d",
         label,component,parent,tf.q[0],tf.q[1],tf.q[2],tf.q[3],tf.p[0],tf.p[1],tf.p[2],tf.s[0],tf.s[1],tf.s[2],pv_tf_finite(&tf));
    void* node=component;
    for (unsigned i=0;node && i<8;i++) {
        if (!pv_probe_component(node,device)) break;
        char name[64];pv_name_text((const char*)node+0x18,name,sizeof name);
        void* next=*(void* const*)((const char*)node+0xc0);
        LOGF("PHONEVIEWCHAIN label=%s depth=%u component=%p name=%s parent=%p actorRoot=%d",
             label,i,node,name,next,node==*(void* const*)((const char*)device+0x130));
        if (next==node) break;node=next;
    }
    if (!cutscene_weapon_layout(component,pv_static_class(),0x4a0)) return;
    void* asset=*(void* const*)((const char*)component+0x440);
    if (!cutscene_weapon_layout(asset,pv_asset_class(),0x138)) return;
    char asset_name[96];pv_name_text((const char*)asset+0x18,asset_name,sizeof asset_name);
    struct pv_mesh_bounds bounds=pv_mesh_bounds(asset);
    LOGF("PHONEVIEWBOUNDS label=%s asset=%p name=%s origin=(%.6f,%.6f,%.6f) extent=(%.6f,%.6f,%.6f) radius=%.6f",
         label,asset,asset_name,bounds.origin[0],bounds.origin[1],bounds.origin[2],bounds.extent[0],bounds.extent[1],bounds.extent[2],bounds.radius);
}
static void phone_view_inspect(void* pawn,void* device) {
    if (!__atomic_load_n(&phone_view_ready,__ATOMIC_ACQUIRE) || !pv_head_transform || !pv_mesh_bounds
        || !pv_static_class || !pv_asset_class || !pv_name || !pv_free || !pawn || !device
        || !cutscene_weapon_layout(pawn,pv_pawn_class(),0x10d0)
        || phone_type()!=5 || phone_active()!=pawn) return;
    const char* cls=*(const char* const*)((const char*)device+0x10);
    int size=cls ? *(const int*)(cls+0x58) : 0;char name[64];
    if (size<0x368 || size>0x10000) return;
    pv_name_text(cls+0x18,name,sizeof name);if (strcmp(name,"CommunicatorDevice_C")) return;
    void* head=*(void* const*)((const char*)pawn+0x2d8);
    if (pv_probe_component(head,pawn)) {
        struct pv_scene_tf tf=pv_head_transform(pawn);
        LOGF("PHONEVIEWHEAD pawn=%p component=%p q=(%.6f,%.6f,%.6f,%.6f) p=(%.6f,%.6f,%.6f) s=(%.6f,%.6f,%.6f) finite=%d",
             pawn,head,tf.q[0],tf.q[1],tf.q[2],tf.q[3],tf.p[0],tf.p[1],tf.p[2],tf.s[0],tf.s[1],tf.s[2],pv_tf_finite(&tf));
    }
    void* root=pv_object_property(device,"Root");void* actor_root=*(void* const*)((const char*)device+0x130);
    LOGF("PHONEVIEWROOT device=%p propertyRoot=%p actorRoot=%p equal=%d",device,root,actor_root,root==actor_root);
    pv_probe_scene("root",root,device);if (root!=actor_root) pv_probe_scene("actorRoot",actor_root,device);
    pv_probe_scene("incoming",pv_object_property(device,"ScreenIncoming"),device);
    pv_probe_scene("outgoing",pv_object_property(device,"ScreenOutgoing"),device);
}

static void install_phone_view_inspect(unsigned long base) {
    static const struct { unsigned long address;unsigned words[2]; } check[]={
        {0x6341aec,{0xf9416c09,0xad4e8122}}, {0x8b9091c,{0x3ccfc000,0x3dc03c01}},
        {0x8fdb46c,{0xd10183ff,0xf90023f3}}, {0x8fd8478,{0xd10183ff,0xf90023f3}},
        {0x67c885c,{0xa9bd57f6,0xa9014ff4}}, {0x6615fd0,{0xa9be4ff4,0xa9017bfd}},
        {0x693cf00,{0x2946aa69,0xb9004e68}}, {0x6960d30,{0xf9400020,0xd65f03c0}},
    };
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].address),check[i].words,sizeof check[i].words)) {
            LOGF("PHONEVIEW inspect disabled: opcode mismatch +%lx",check[i].address);return;
        }
    if (*(const unsigned*)(base+0xa269120+0x24)!=0x440
        || *(const unsigned*)(base+0xa2668d8+0x24)!=0xf0) return;
    pv_head_transform=(struct pv_scene_tf (*)(const void*))(base+0x6341aec);
    pv_mesh_bounds=(struct pv_mesh_bounds (*)(const void*))(base+0x8b9091c);
    pv_static_class=(void* (*)(void))(base+0x8fdb46c);pv_asset_class=(void* (*)(void))(base+0x8fd8478);
    pv_name=(void (*)(const void*,void*))(base+0x67c885c);pv_free=(void (*)(void*))(base+0x6615fd0);
}

static void phone_view_avatar_post(void* pawn) {
    if (!__atomic_load_n(&phone_view_ready,__ATOMIC_ACQUIRE) || !pawn || !pv_pawn_class
        || !phone_type || !phone_active || !cw_meshset_class || !cw_scene_class
        || !cw_skeletal_class || !cw_get_mesh) return;
    void (*visible)(void*,int,int)=__atomic_load_n(&cw_avatar_visible,__ATOMIC_ACQUIRE);
    if (!visible || !cutscene_weapon_layout(pawn,pv_pawn_class(),0x10d0)) return;
    void* meshset=*(void* const*)((const char*)pawn+0xba8);
    if (!cutscene_weapon_layout(meshset,cw_meshset_class(),0x238)) return;
    void* root=*(void* const*)((const char*)meshset+0x130);
    void* pawn_root=*(void* const*)((const char*)pawn+0x130);
    void* primary=*(void* const*)((const char*)meshset+0x228);
    void* secondary=*(void* const*)((const char*)meshset+0x230);
    // GetMesh may read Secondary.SkeletalMesh+430; validate both fields first.
    if (root!=*(void* const*)((const char*)meshset+0x220)
        || !cutscene_weapon_component(root,cw_scene_class(),meshset)
        || !cutscene_weapon_component(pawn_root,cw_scene_class(),pawn)
        || *(void* const*)((const char*)root+0xc0)!=pawn_root
        || !cutscene_avatar_component(primary,meshset)
        || (secondary && !cutscene_avatar_component(secondary,meshset))) return;
    if (phone_type()!=5 || phone_active()!=pawn) return;
    void* meshes[2]={cw_get_mesh(pawn,0),cw_get_mesh(pawn,1)};
    for (unsigned i=0;i<2;i++)
        if ((meshes[i]!=primary && meshes[i]!=secondary)
            || !cutscene_avatar_component(meshes[i],meshset)
            || *(void* const*)((const char*)meshes[i]+0xc0)!=root) return;
    unsigned hidden=0;
    for (unsigned i=0;i<2;i++) {
        if (i && meshes[i]==meshes[0]) continue;
        if (*(const unsigned short*)((const char*)meshes[i]+0x14c)&0x20) visible(meshes[i],0,0);
        hidden++;
    }
    if (pv_avatar_logs<8) {
        pv_avatar_logs++;
        LOGF("PHONE VIEW pawn=%p meshset=%p mesh0=%p mesh1=%p hidden=%u",
             pawn,meshset,meshes[0],meshes[1],hidden);
    }
}

static void install_phone_view(unsigned long base) {
    static const struct { unsigned long address; unsigned words[2]; } check[]={
        {0x642c718,{0xd10183ff,0xf90023f3}}, {0x642c7a0,{0x52821a04,0x52800205}},
        {0x62af678,{0xf945d408,0xb40002a8}},
        {0x83ad964,{0xd104c3ff,0xf90073fc}},
    };
    if (!base || __atomic_load_n(&phone_view_ready,__ATOMIC_ACQUIRE)) return;
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].address),check[i].words,sizeof check[i].words)) {
            LOGF("PHONE VIEW disabled: opcode mismatch +%lx",check[i].address);return;
        }
    // A failed shared avatar-hook install must also disable this post-writer.
    if (phone_type!=(unsigned (*)(void))(base+0x6338aec)
        || phone_active!=(void* (*)(void))(base+0x63380dc)
        || cw_meshset_class!=(void* (*)(void))(base+0x6427ad4)
        || cw_scene_class!=(void* (*)(void))(base+0x8f82500)
        || cw_skeletal_class!=(void* (*)(void))(base+0x8f94650)
        || cw_get_mesh!=(void* (*)(const void*,unsigned char))(base+0x62af678)
        || __atomic_load_n(&cw_avatar_visible,__ATOMIC_ACQUIRE)!=(void (*)(void*,int,int))(base+0x83ad964)
        || g_tgt_cutscene_avatar!=base+0x62a65e0+16) {
        LOGF("PHONE VIEW disabled: shared native avatar dependency mismatch");return;
    }
    pv_pawn_class=(void* (*)(void))(base+0x642c718);
    __atomic_store_n(&phone_view_ready,1,__ATOMIC_RELEASE);
    install_phone_view_inspect(base);
    LOGF("PHONE VIEW ready: active Communicator5 owned avatar leaves only; native visibility recovery");
}
#endif
