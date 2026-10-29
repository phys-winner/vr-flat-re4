// Whole stored OBB assets use the engine's native descriptor-backed bounded file handle.
typedef struct { const unsigned short* data; int count, capacity; } MobileObbString;
static void* (*f_mobile_obb_manager)(void);
static void* (*f_mobile_obb_open)(void*, const char*, int);
static long long (*f_mobile_obb_length)(void*);
static int (*f_mobile_obb_descriptor)(void*, long long*, long long*);
static void (*f_mobile_obb_close)(void*);
static void* g_mobile_obb_android_library;
static void* (*f_mobile_obb_allocate)(unsigned long);
static void (*f_mobile_obb_construct)(void*, const MobileObbString*, void*);
static void (*f_mobile_obb_construct_fd)(void*, const MobileObbString*, int);
unsigned long g_tgt_mobile_obb_exists, g_tgt_mobile_obb_read;
static volatile int g_mobile_obb_ready;
static int mobile_zip_descriptor(int index, long long* offset);

#ifndef MOBILE_OBB_TEST
#include <jni.h>
#include <pthread.h>
#include <fcntl.h>
static pthread_mutex_t g_mobile_zip_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_mobile_zip_fd = -1;
static long long g_mobile_zip_offsets[2];
static int mobile_zip_descriptor(int index, long long* offset) {
    pthread_mutex_lock(&g_mobile_zip_mutex);
    int fd = g_mobile_zip_fd < 0 ? -1 : fcntl(g_mobile_zip_fd, F_DUPFD_CLOEXEC, 0);
    if (fd >= 0) *offset = g_mobile_zip_offsets[index];
    pthread_mutex_unlock(&g_mobile_zip_mutex);
    return fd;
}
__attribute__((visibility("default")))
jboolean Java_com_epicgames_ue4_MobileCache_nativeZip(JNIEnv* env, jclass type, jint descriptor,
        jlong main_offset, jlong main_length, jlong patch_offset, jlong patch_length) {
    (void)env; (void)type;
    if (descriptor < 0 || main_length != 4278831240LL || patch_length != 4237413580LL
        || main_offset < 0 || patch_offset < 0) return JNI_FALSE;
    // A provider must expose a seekable descriptor; pipes cannot back the native pread64 reader.
    long long length = lseek64(descriptor, 0, SEEK_END);
    if (length < 0 || main_offset > length - main_length || patch_offset > length - patch_length
        || (main_offset < patch_offset + patch_length && patch_offset < main_offset + main_length)) return JNI_FALSE;
    int fd = fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
    if (fd < 0) return JNI_FALSE;
    pthread_mutex_lock(&g_mobile_zip_mutex);
    int previous = g_mobile_zip_fd;
    g_mobile_zip_offsets[0] = main_offset; g_mobile_zip_offsets[1] = patch_offset;
    g_mobile_zip_fd = fd;
    pthread_mutex_unlock(&g_mobile_zip_mutex);
    if (previous >= 0) close(previous);
    LOGF("mobile cache ZIP mounted: main=%lld+%lld patch=%lld+%lld", (long long)main_offset,
        (long long)main_length, (long long)patch_offset, (long long)patch_length);
    return JNI_TRUE;
}
#endif

static int mobile_obb_resolve_asset_api(void) {
    if (g_mobile_obb_android_library) return 1;
    // A separate loader group need not expose libUE4's libandroid dependency through RTLD_DEFAULT.
    void* library = dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    if (!library) return 0;
    void* open_asset = dlsym(library,"AAssetManager_open");
    void* length = dlsym(library,"AAsset_getLength64");
    void* descriptor = dlsym(library,"AAsset_openFileDescriptor64");
    void* close_asset = dlsym(library,"AAsset_close");
    if (!open_asset || !length || !descriptor || !close_asset) { dlclose(library); return 0; }
    f_mobile_obb_open = (void*(*)(void*,const char*,int))open_asset;
    f_mobile_obb_length = (long long(*)(void*))length;
    f_mobile_obb_descriptor = (int(*)(void*,long long*,long long*))descriptor;
    f_mobile_obb_close = (void(*)(void*))close_asset;
    g_mobile_obb_android_library = library; // Keep the function-pointer owner loaded for all hook calls.
    return 1;
}

#ifndef MOBILE_OBB_TEST
__attribute__((naked)) static int gate_mobile_obb_exists(void* self, const unsigned short* path, int local) {
    asm volatile(".inst 0xd10643ff\n" ".inst 0xf900a3fc\n" ".inst 0xa9155ff8\n" ".inst 0xa91657f6\n"
                 "adrp x16, :got:g_tgt_mobile_obb_exists\n" "ldr x16, [x16, :got_lo12:g_tgt_mobile_obb_exists]\n"
                 "ldr x16, [x16]\n" "br x16\n");
}

__attribute__((naked)) static void* gate_mobile_obb_read(void* self, const unsigned short* path, int allow_local, int allow_write) {
    asm volatile(".inst 0xd10483ff\n" ".inst 0xf9006bfc\n" ".inst 0xa90e5ff8\n" ".inst 0xa90f57f6\n"
                 "adrp x16, :got:g_tgt_mobile_obb_read\n" "ldr x16, [x16, :got_lo12:g_tgt_mobile_obb_read]\n"
                 "ldr x16, [x16]\n" "br x16\n");
}
#else
static int gate_mobile_obb_exists(void* self, const unsigned short* path, int local);
static void* gate_mobile_obb_read(void* self, const unsigned short* path, int write, int mapped);
#endif

static int mobile_obb_name(const unsigned short* path, const char** name, long long* size, int* count) {
    if (!path) return 0;
    const unsigned short* leaf = path;
    int length = 0;
    for (; length < 1024 && path[length]; ++length) {
        if (path[length] == '/') leaf = path + length + 1;
    }
    if (length == 1024) return 0;
    const char* names[] = {"main.201.com.Armature.VR4.obb", "patch.201.com.Armature.VR4.obb"};
    const long long sizes[] = {4278831240LL, 4237413580LL};
    for (int index = 0; index < 2; ++index) {
        int character = 0;
        while (names[index][character] && leaf[character] == (unsigned char)names[index][character]) ++character;
        if (!names[index][character] && !leaf[character]) {
            *name = names[index]; *size = sizes[index]; *count = length + 1;
            return 1;
        }
    }
    return 0;
}

static void* mobile_obb_asset(const unsigned short* path, int* count) {
    const char* name;
    long long size;
    if (!__atomic_load_n(&g_mobile_obb_ready,__ATOMIC_ACQUIRE) || !f_mobile_obb_manager || !mobile_obb_name(path, &name, &size, count)) return 0;
    void* manager = f_mobile_obb_manager();
    if (!manager) return 0;
    void* asset = f_mobile_obb_open(manager, name, 1);
    if (!asset) return 0;
    long long start = -1, length = -1;
    int fd = f_mobile_obb_descriptor(asset, &start, &length);
    int valid = fd >= 0 && start >= 0 && length == size && f_mobile_obb_length(asset) == size;
    if (fd >= 0) close(fd);
    if (!valid) { f_mobile_obb_close(asset); return 0; }
    return asset;
}

int hook_mobile_obb_exists(void* self, const unsigned short* path, int local) {
    int count;
    void* asset = mobile_obb_asset(path, &count);
    if (asset) { f_mobile_obb_close(asset); return 1; }
    const char* name; long long size, offset;
    if (__atomic_load_n(&g_mobile_obb_ready,__ATOMIC_ACQUIRE) && mobile_obb_name(path,&name,&size,&count)) {
        int fd = mobile_zip_descriptor(name[0] == 'm' ? 0 : 1,&offset);
        if (fd >= 0) { close(fd); return 1; }
    }
    return gate_mobile_obb_exists(self, path, local);
}

void* hook_mobile_obb_read(void* self, const unsigned short* path, int allow_local, int allow_write) {
    int count;
    // MountOBB calls OpenRead(path,AllowLocal=true,AllowWrite=false). Both paths are reads.
    void* asset = mobile_obb_asset(path, &count);
    if (!asset) {
        const char* name; long long size, offset;
        if (__atomic_load_n(&g_mobile_obb_ready,__ATOMIC_ACQUIRE) && f_mobile_obb_construct_fd && mobile_obb_name(path,&name,&size,&count)) {
            int fd = mobile_zip_descriptor(name[0] == 'm' ? 0 : 1,&offset);
            if (fd >= 0) {
                void* handle = f_mobile_obb_allocate(48);
                if (!handle) { close(fd); return 0; }
                MobileObbString string = {path,count,count};
                f_mobile_obb_construct_fd(handle,&string,fd);
                // The existing Android handle owns the duplicate and uses absolute pread64 with this slice.
                *(long long*)((char*)handle+0x18) = offset;
                *(long long*)((char*)handle+0x20) = size;
                *(long long*)((char*)handle+0x28) = offset;
                LOGF("mobile OBB direct ZIP opened: %p length=%lld offset=%lld",handle,size,offset);
                return handle;
            }
        }
        return gate_mobile_obb_read(self, path, allow_local, allow_write);
    }
    void* handle = f_mobile_obb_allocate(48);
    if (!handle) { f_mobile_obb_close(asset); return 0; }
    MobileObbString string = {path, count, count};
    f_mobile_obb_construct(handle, &string, asset);
    // Native constructor owns both AAsset and its descriptor; the destructor closes them.
    LOGF("mobile OBB direct asset opened: %p length=%lld offset=%lld", handle,
         *(long long*)((char*)handle + 0x20), *(long long*)((char*)handle + 0x18));
    return handle;
}

static void install_packaged_obb(unsigned long base) {
    __atomic_store_n(&g_mobile_obb_ready,0,__ATOMIC_RELEASE);
    const unsigned int exists[4] = {0xd10643ff,0xf900a3fc,0xa9155ff8,0xa91657f6};
    const unsigned int read[4] = {0xd10483ff,0xf9006bfc,0xa90e5ff8,0xa90f57f6};
    const unsigned int manager[4] = {0xa9bd57f6,0xa9014ff4,0xa9027bfd,0x910083fd};
    const unsigned int construct[4] = {0xd10183ff,0xa9025ff8,0xa90357f6,0xa9044ff4};
    const unsigned int allocate[2] = {0x2a1f03e1,0x140a330e};
    const unsigned int construct_fd[4] = {0xf81c0ff7,0xa90157f6,0xa9024ff4,0xa9037bfd};
    if (memcmp((void*)(base+0x6572d40),exists,sizeof exists) || memcmp((void*)(base+0x6575508),read,sizeof read) ||
        memcmp((void*)(base+0x54b4110),manager,sizeof manager) || memcmp((void*)(base+0x657598c),construct,sizeof construct) ||
        memcmp((void*)(base+0x63892b4),allocate,sizeof allocate) || memcmp((void*)(base+0x657201c),construct_fd,sizeof construct_fd)) {
        LOGI("packaged OBB disabled: native preimage mismatch"); return;
    }
    if (!mobile_obb_resolve_asset_api()) {
        LOGI("packaged OBB disabled: asset API unavailable"); return;
    }
    f_mobile_obb_allocate = (void*(*)(unsigned long))(base+0x63892b4);
    f_mobile_obb_construct = (void(*)(void*,const MobileObbString*,void*))(base+0x657598c);
    f_mobile_obb_construct_fd = (void(*)(void*,const MobileObbString*,int))(base+0x657201c);
    f_mobile_obb_manager = (void*(*)(void))(base+0x54b4110);
    g_tgt_mobile_obb_exists = base+0x6572d50;
    g_tgt_mobile_obb_read = base+0x6575518;
    if (patch_code((void*)(base+0x6572d40),(void*)&hook_mobile_obb_exists) ||
        patch_code((void*)(base+0x6575508),(void*)&hook_mobile_obb_read)) {
        LOGI("packaged OBB hook failed; external OBB fallback retained"); return;
    }
    __atomic_store_n(&g_mobile_obb_ready,1,__ATOMIC_RELEASE);
    LOGI("whole packaged OBB installed: stored assets from base/splits, external cache fallback, no extraction");
}
