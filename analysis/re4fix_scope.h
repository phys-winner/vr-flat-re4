// Included inside aim.h after its scalar rigid-transform helpers. Scope.UpdateScope stays native.
#ifndef RE4FIX_SCOPE_H
#define RE4FIX_SCOPE_H
static void* (*as_two_class)(void),*(*as_scope_class)(void),*(*as_scene_class)(void),*(*as_box_class)(void),*(*as_curve_class)(void);
static void* (*as_scope)(const void*);
static struct reload_tf (*as_lerp)(const struct reload_tf*,const struct reload_tf*,float,int);
static float (*as_pitch)(const float*),(*as_curve)(const void*,float);
static int as_ready;
static struct {void* objects[7];unsigned long long names[7];long neutral_at;unsigned equip_generation;struct reload_tf box[2],root;} as_cache;
static unsigned as_logs;
static int as_typed(const void* o,const void* c,int minimum){return c && reload_layout(o,minimum) && reload_is_a(o,c);}
static int as_scene_owned(const void* o,const void* owner){
    return as_typed(o,as_scene_class(),0x1f0) && *(void* const*)((const char*)o+0x20)==owner;
}
static int as_attached(void* root,void* gun_root,void* scope,void* gun){
    void* node=root;
    for(int i=0;i<16;i++){
        if(node==gun_root)return as_scene_owned(node,gun);
        if(!as_typed(node,as_scene_class(),0x1f0))return 0;
        void* owner=*(void* const*)((const char*)node+0x20);
        if(owner!=scope && owner!=gun)return 0;
        void* next=*(void* const*)((const char*)node+0xc0);if(!next || next==node)return 0;node=next;
    }
    return 0;
}
static int as_box_angles(struct reload_tf box,struct reload_tf head,const float* threshold){
    for(int i=0;i<4;i++)if(!isfinite(threshold[i]) || threshold[i]<=0 || (i && threshold[i]>180))return 0;
    float world[3],local[3];q_rot(head.q,(float[3]){1,0,0},world);
    q_rot((qf_t){-box.q.x,-box.q.y,-box.q.z,box.q.w},world,local);
    float yaw=atan2f(local[1],local[0])*57.29577951308232f;
    float pitch=atan2f(local[2],sqrtf(local[0]*local[0]+local[1]*local[1]))*57.29577951308232f;
    return fabsf(yaw)<threshold[1] && pitch<threshold[2] && pitch>-threshold[3];
}
static int aim_scope_adjust(void* gun,void* pawn,void* hand,struct reload_tf current,struct reload_tf head,
                            long neutral_at,float blend,struct reload_tf* pose){
    if(!as_ready || !qs_gt() || g_touch_context!=1 || g_touch_cancel || g_reload_phase
        || !gun || !pawn || !hand || !pose || blend<=0 || blend>1
        || !reload_tf_valid(&current) || !reload_tf_valid(&head) || !reload_tf_valid(pose)
        || !as_typed(gun,as_two_class(),0x1050)
        || *(void* const*)((const char*)gun+0x3c0)!=pawn || body_active()!=pawn
        || rl_primary(gun)!=hand || f_GetCurrentGrip(hand)!=*(void* const*)((const char*)gun+0x270))return 0;
    void* scope=as_scope(gun);if(!as_typed(scope,as_scope_class(),0x2b1))return 0;
    void* root=*(void* const*)((const char*)scope+0x220),*gun_root=*(void* const*)((const char*)gun+0x130);
    void* boxes[2]={*(void* const*)((const char*)scope+0x228),*(void* const*)((const char*)scope+0x238)};
    if(root!=*(void* const*)((const char*)scope+0x130) || !as_scene_owned(root,scope)
        || !as_attached(root,gun_root,scope,gun))return 0;
    for(int i=0;i<2;i++)if(!as_typed(boxes[i],as_box_class(),0x42c) || !as_scene_owned(boxes[i],scope)
        || *(void* const*)((const char*)boxes[i]+0xc0)!=root)return 0;
    void* objects[7]={pawn,hand,gun,scope,root,boxes[0],boxes[1]};unsigned long long names[7];int same=as_cache.neutral_at==neutral_at
        && as_cache.equip_generation==touch_quickselect_pose_generation();
    for(int i=0;i<7;i++){
        memcpy(&names[i],(const char*)objects[i]+0x18,8);
        if(objects[i]!=as_cache.objects[i] || names[i]!=as_cache.names[i])same=0;
    }
    if(!same){
        if(!aim_blending || !aim_flourishing || !aim_recoiling || aim_blending(gun)
            || aim_flourishing(gun) || aim_recoiling(gun))return 0;
        struct reload_tf world[3];memcpy(&world[0],(const char*)boxes[0]+0x1c0,48);
        memcpy(&world[1],(const char*)boxes[1]+0x1c0,48);memcpy(&world[2],(const char*)root+0x1c0,48);
        for(int i=0;i<3;i++)if(!reload_marker_valid(&world[i]))return 0;
        memcpy(as_cache.objects,objects,sizeof objects);memcpy(as_cache.names,names,sizeof names);as_cache.neutral_at=neutral_at;
        as_cache.equip_generation=touch_quickselect_pose_generation();
        for(int i=0;i<2;i++)as_cache.box[i]=aim_relative_marker(current,world[i]);
        as_cache.root=aim_relative_marker(current,world[2]);
    }
    struct reload_tf posed_root=aim_world_marker(*pose,as_cache.root),posed[2];
    for(int i=0;i<2;i++)posed[i]=aim_world_marker(*pose,as_cache.box[i]);
    float alpha=0;void* curve=*(void* const*)((const char*)scope+0x248);
    if(curve){
        if(!as_typed(curve,as_curve_class(),0x98))return 0;
        float forward[3];q_rot(posed_root.q,(float[3]){1,0,0},forward);
        alpha=as_curve(curve,as_pitch(forward));if(!isfinite(alpha) || alpha<0 || alpha>1)return 0;
    }
    // Match the native normal/downwards eye-box interpolation, without activating or scaling the scope.
    struct reload_tf box=as_lerp(&posed[0],&posed[1],alpha,0);
    if(!reload_marker_valid(&box) || !as_box_angles(box,head,(const float*)((const char*)scope+0x250)))return 0;
    struct reload_tf result=*pose;
    for(int i=0;i<3;i++)result.p[i]+=(head.p[i]-box.p[i])*blend;
    float dx=result.p[0]-current.p[0],dy=result.p[1]-current.p[1],dz=result.p[2]-current.p[2];
    if(!reload_tf_valid(&result) || dx*dx+dy*dy+dz*dz>150.0f*150.0f)return 0;
    *pose=result;
    if(as_logs<8){as_logs++;LOGF("FPSSCOPE gun=%p scope=%p ADS=%.3f authored_eye_box_alpha=%.3f",gun,scope,blend,alpha);}
    return 1;
}
static void install_touch_scope(unsigned long base){
    static const struct{unsigned long addr;unsigned first[2];}check[]={
        {0x62936e4,{0xf9482408,0xb4000268}},{0x6437b40,{0xd10183ff,0xf90023f3}},
        {0x6422a44,{0xd10183ff,0xf90023f3}},{0x8ce93b8,{0xd10183ff,0xf90023f3}},
        {0x8f82500,{0xd10183ff,0xf90023f3}},{0x61c4ab0,{0xd101c3ff,0xa90457f6}},
        {0x61ca820,{0x2d400801,0xbd400800}},{0x83faeb8,{0x9100c000,0x1e2703e1}},
        {0x8d1bc2c,{0xd10183ff,0xf90023f3}},{0x62c5cb8,{0xd10403ff,0xfd0063e8}},
        {0x62c61b0,{0xd10203ff,0x6d022beb}},
    };
    static const struct{unsigned long addr;unsigned offset;}fields[]={
        {0x9db4828,0x220},{0x9db47f8,0x228},{0x9db4798,0x238},{0x9db4708,0x250},{0x9db4738,0x248},
    };
    as_ready=0;memset(&as_cache,0,sizeof as_cache);if(!base)return;
    for(unsigned i=0;i<sizeof check/sizeof *check;i++)if(memcmp((const void*)(base+check[i].addr),check[i].first,8))return;
    for(unsigned i=0;i<sizeof fields/sizeof *fields;i++)if(*(const unsigned*)(base+fields[i].addr+36)!=fields[i].offset)return;
    as_scope=(void* (*)(const void*))(base+0x62936e4);as_scope_class=(void* (*)(void))(base+0x6437b40);
    as_two_class=(void* (*)(void))(base+0x6422a44);as_scene_class=(void* (*)(void))(base+0x8f82500);
    as_box_class=(void* (*)(void))(base+0x8ce93b8);as_curve_class=(void* (*)(void))(base+0x8d1bc2c);
    as_lerp=(struct reload_tf (*)(const struct reload_tf*,const struct reload_tf*,float,int))(base+0x61c4ab0);
    as_pitch=(float (*)(const float*))(base+0x61ca820);as_curve=(float (*)(const void*,float))(base+0x83faeb8);
    __atomic_store_n(&as_ready,1,__ATOMIC_RELEASE);LOGF("FPSSCOPE ready: original attached eye boxes; native eligibility/transitions unchanged");
}
#endif
