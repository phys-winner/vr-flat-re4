// Read-only runtime reflection, explicit request only. No Blueprint event is invoked.
static void touch_interact_inspect_object(void*);
static void (*phone_inspect_name)(const void*,void*);
static void (*phone_inspect_free)(void*);
static int phone_inspect_ready;
static void phone_inspect_fname(const void* name,char* out,int size) {
    out[0]=0;
    if (!name || !phone_inspect_name) return;
    struct { unsigned short* data; int num,max; } fs={0,0,0};
    phone_inspect_name(name,&fs);
    if (fs.data && fs.num>0 && fs.num<32768) {
        int i=0;
        for (;i<size-1 && i<fs.num && fs.data[i];i++) out[i]=fs.data[i]<128 ? (char)fs.data[i] : '?';
        out[i]=0;
    }
    if (fs.data) phone_inspect_free(fs.data);
}
static int phone_inspect_follow(const char* name,const char* type) {
    return strstr(name,"Device") || strstr(name,"Communicator") || strstr(name,"Phone")
        || strstr(name,"Finger") || strstr(name,"Button") || strstr(name,"Screen")
        || strstr(name,"Theatre") || strstr(name,"Widget") || strstr(name,"Subtitle") || strstr(name,"Capture")
        || !strcmp(name,"ChildActor") || strstr(type,"ChildActorComponent");
}
static void phone_inspect_objects(void* pawn,void* actor,void* hand) {
    if (!phone_inspect_ready) { LOGF("PHONEINSPECT disabled: reflection preimage mismatch"); return; }
    void* queue[16]={pawn,actor,hand};
    unsigned n=3, functions=0,properties=0;
    for (unsigned qi=0;qi<n;qi++) {
        void* obj=queue[qi];
        if (!obj) continue;
        int seen=0;
        for (unsigned k=0;k<qi;k++) if (queue[k]==obj) seen=1;
        if (seen) continue;
        void* cls=*(void**)((char*)obj+0x10);
        char on[96]="",cn[96]="";
        obj_name(obj,on,sizeof on); obj_name(cls,cn,sizeof cn);
        int bytes=cls ? *(int*)((char*)cls+0x58) : 0;
        LOGF("PHONEOBJ id=%u object=%p '%s' class='%s' size=%x",qi,obj,on,cn,bytes);
        if(qi==0)touch_interact_inspect_object(obj);
        void* level=cls;
        for (unsigned depth=0;level && depth<8;depth++) {
            char ln[96]=""; obj_name(level,ln,sizeof ln);
            LOGF("PHONECLASS id=%u depth=%u class=%p '%s'",qi,depth,level,ln);
            if (strstr(ln,"_C")) {
                void* child=*(void**)((char*)level+0x48);
                for (unsigned i=0;child && i<96 && functions<384;i++) {
                    char fn[128]="",kind[64]="";
                    obj_name(child,fn,sizeof fn); obj_name(*(void**)((char*)child+0x10),kind,sizeof kind);
                    if (!strcmp(kind,"Function")) {
                        functions++;
                        LOGF("PHONEFUNC id=%u owner='%s' function=%p '%s' flags=%08x params=%u size=%u return=%u",
                             qi,ln,child,fn,*(unsigned*)((char*)child+0xb0),
                             *(unsigned char*)((char*)child+0xb4),*(unsigned short*)((char*)child+0xb6),
                             *(unsigned short*)((char*)child+0xb8));
                        void* parm=*(void**)((char*)child+0x50);
                        for (unsigned pi=0;parm && pi<12;pi++) {
                            char pn[96]="",pt[48]="";
                            phone_inspect_fname((char*)parm+0x28,pn,sizeof pn);
                            phone_inspect_fname(*(void**)((char*)parm+0x8),pt,sizeof pt);
                            LOGF("PHONEPARAM function='%s' name='%s' type='%s' offset=%x element=%d dim=%d flags=%016llx",
                                 fn,pn,pt,*(int*)((char*)parm+0x4c),*(int*)((char*)parm+0x38),
                                 *(int*)((char*)parm+0x34),*(unsigned long long*)((char*)parm+0x40));
                            void* next=*(void**)((char*)parm+0x20);
                            if (next==parm) break;
                            parm=next;
                        }
                    }
                    void* next=*(void**)((char*)child+0x28);
                    if (next==child) break;
                    child=next;
                }
            }
            void* next=*(void**)((char*)level+0x40);
            if (next==level) break;
            level=next;
        }
        if (!cls || bytes<=0 || bytes>0x10000) continue;
        void* prop=*(void**)((char*)cls+0x70);
        for (unsigned i=0;prop && i<96 && properties<768;i++,properties++) {
            char pn[96]="",pt[48]="",vn[96]="",vt[96]="";
            phone_inspect_fname((char*)prop+0x28,pn,sizeof pn);
            phone_inspect_fname(*(void**)((char*)prop+0x8),pt,sizeof pt);
            int off=*(int*)((char*)prop+0x4c),element=*(int*)((char*)prop+0x38);
            int dim=*(int*)((char*)prop+0x34);
            void* value=0;
            if (off>=0 && element==8 && dim==1 && off<=bytes-8 && !strcmp(pt,"ObjectProperty")) {
                value=*(void**)((char*)obj+off);
                if (value) {
                    obj_name(value,vn,sizeof vn); obj_name(*(void**)((char*)value+0x10),vt,sizeof vt);
                    if (n<sizeof queue/sizeof queue[0] && phone_inspect_follow(pn,vt)) {
                        int duplicate=0;
                        for (unsigned k=0;k<n;k++) if (queue[k]==value) duplicate=1;
                        if (!duplicate) queue[n++]=value;
                    }
                }
            }
            LOGF("PHONEPROP id=%u name='%s' type='%s' offset=%x element=%d dim=%d value=%p '%s' class='%s'",
                 qi,pn,pt,off,element,dim,value,vn,vt);
            if (dim==1 && off>=0 && element>0 && element<=4 && off<=bytes-element) {
                unsigned raw=0;
                memcpy(&raw,(char*)obj+off,(unsigned)element);
                LOGF("PHONEVALUE id=%u name='%s' raw=%08x",qi,pn,raw);
            }
            void* next=*(void**)((char*)prop+0x58);
            if (next==prop) break;
            prop=next;
        }
    }
    LOGF("PHONEINSPECT done objects=%u functions=%u properties=%u",n,functions,properties);
}
static void install_phone_inspect(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[2]; } check[] = {
        {0x67c885c,{0xa9bd57f6,0xa9014ff4}}, {0x6615fd0,{0xa9be4ff4,0xa9017bfd}},
        {0x693e7b4,{0xf9403800,0xb40000c0}}, {0x693e7c8,{0xf9402c00,0xb5ffff80}},
        {0x687eee0,{0xf9402696,0xb4000576}}, {0x687ef14,{0xf94016d6,0xb40003d6}},
        {0x6888094,{0xf94022d8,0xb5000098}}, {0x693cf38,{0xb9004c01,0xd65f03c0}},
        {0x693cf00,{0x2946aa69,0xb9004e68}}, {0x693cf1c,{0xb9405914,0xaa1303e0}},
        {0x68e4054,{0xf94003e8,0xf9000268}}, {0x6888ed0,{0xf9402808,0x52bfffe9}},
        {0x6888f10,{0x3902d009,0xb9404d0a}}, {0x6888f20,{0x79016c0a,0x3941050a}},
        {0x6888f34,{0xf9401108,0xb4000088}}, {0x6960d30,{0xf9400020,0xd65f03c0}},
    };
    for (unsigned i=0;i<sizeof check/sizeof check[0];i++)
        if (memcmp((const void*)(base+check[i].addr),check[i].first,sizeof check[i].first)) {
            LOGF("PHONEINSPECT disabled: opcode mismatch +%lx",check[i].addr); return;
        }
    phone_inspect_name=(void (*)(const void*,void*))(base+0x67c885c);
    phone_inspect_free=(void (*)(void*))(base+0x6615fd0);
    phone_inspect_ready=1;
}
