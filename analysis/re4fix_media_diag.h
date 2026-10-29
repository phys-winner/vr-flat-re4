// Android media player data sources (backstory/FMV movies): routes OBB-split movies through AssetManager.openFd.
#ifndef RE4FIX_MEDIA_DIAG_H
#define RE4FIX_MEDIA_DIAG_H
unsigned long g_tgt_media_open, g_tgt_media_src_path, g_tgt_media_src_archive, g_tgt_media_src_file, g_tgt_media_src_asset;
static unsigned g_media_logs;
__attribute__((naked)) static int gate_media_open(void* self, const void* url, const void* options) {
    asm volatile(".inst 0xd103c3ff\n" ".inst 0xf9004bfb\n" ".inst 0xa90a67fa\n" ".inst 0xa90b5ff8\n"
        "adrp x16, :got:g_tgt_media_open\n" "ldr x16, [x16, :got_lo12:g_tgt_media_open]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_media_src_path(void* self, const void* url) {
    asm volatile(".inst 0xd10143ff\n" ".inst 0xa9034ff4\n" ".inst 0xa9047bfd\n" ".inst 0x910103fd\n"
        "adrp x16, :got:g_tgt_media_src_path\n" "ldr x16, [x16, :got_lo12:g_tgt_media_src_path]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_media_src_archive(void* self, const void* archive) {
    asm volatile(".inst 0xd10203ff\n" ".inst 0xa90367fa\n" ".inst 0xa9045ff8\n" ".inst 0xa90557f6\n"
        "adrp x16, :got:g_tgt_media_src_archive\n" "ldr x16, [x16, :got_lo12:g_tgt_media_src_archive]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_media_src_file(void* self, const void* path, long long offset, long long size) {
    asm volatile(".inst 0xd10183ff\n" ".inst 0xa90357f6\n" ".inst 0xa9044ff4\n" ".inst 0xa9057bfd\n"
        "adrp x16, :got:g_tgt_media_src_file\n" "ldr x16, [x16, :got_lo12:g_tgt_media_src_file]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_media_src_asset(void* self, void* assets, const void* path, long long offset, long long size) {
    asm volatile(".inst 0xd101c3ff\n" ".inst 0xf9001bf7\n" ".inst 0xa90457f6\n" ".inst 0xa9054ff4\n"
        "adrp x16, :got:g_tgt_media_src_asset\n" "ldr x16, [x16, :got_lo12:g_tgt_media_src_asset]\n" "ldr x16, [x16]\n" "br x16\n");
}
static void media_fstring(const void* fs, char* out, int n) {
    out[0] = 0;
    if (!fs) return;
    const unsigned short* data = *(const unsigned short* const*)fs;
    int num = *(const int*)((const char*)fs + 8);
    if (!data || num <= 0 || num > 1024) return;
    u16_to_ascii(data, out, n);
}
int hook_media_open(void* self, const void* url, const void* options) {
    char path[256]; media_fstring(url, path, sizeof path);
    int r = gate_media_open(self, url, options);
    if (g_media_logs < 40) { g_media_logs++; LOGF("MEDIA Open('%s') -> %d", path, r); }
    return r;
}
int hook_media_src_path(void* self, const void* url) {
    char path[256]; media_fstring(url, path, sizeof path);
    int r = gate_media_src_path(self, url);
    if (g_media_logs < 40) { g_media_logs++; LOGF("MEDIA SetDataSource(url='%s') -> %d", path, r); }
    return r;
}
int hook_media_src_archive(void* self, const void* archive) {
    int r = gate_media_src_archive(self, archive);
    if (g_media_logs < 40) { g_media_logs++; LOGF("MEDIA SetDataSource(archive=%p) -> %d", archive, r); }
    return r;
}
int hook_media_src_file(void* self, const void* path, long long offset, long long size) {
    char name[256]; media_fstring(path, name, sizeof name);
    int r = gate_media_src_file(self, path, offset, size);
    if (g_media_logs < 40) { g_media_logs++; LOGF("MEDIA SetDataSource(file='%s' offset=%lld size=%lld) -> %d", name, offset, size, r); }
    return r;
}
// Movies stored in the whole-OBB split APK: FileRootPath() names the external OBB file, while the offset is
// already relative to the split APK asset ("main.201....obb" stored uncompressed). AssetManager.openFd needs
// the asset name, so pass the leaf; offset/size stay the engine's own values.
int hook_media_src_asset(void* self, void* assets, const void* path, long long offset, long long size) {
    char name[256]; media_fstring(path, name, sizeof name);
    struct { const unsigned short* data; int num, max; } leaf_string;
    const unsigned short* data = path ? *(const unsigned short* const*)path : 0;
    int num = path ? *(const int*)((const char*)path + 8) : 0;
    if (data && num > 1 && num < 1024 && data[0] == '/') {
        int slash = -1;
        for (int i = 0; i < num - 1 && data[i]; i++) if (data[i] == '/') slash = i;
        char leaf[96]; u16_to_ascii(data + slash + 1, leaf, sizeof leaf);
        if (slash >= 0 && (!strcmp(leaf, "main.201.com.Armature.VR4.obb") || !strcmp(leaf, "patch.201.com.Armature.VR4.obb"))) {
            leaf_string.data = data + slash + 1; leaf_string.num = num - slash - 1; leaf_string.max = leaf_string.num;
            path = &leaf_string;
        }
    }
    int r = gate_media_src_asset(self, assets, path, offset, size);
    if (g_media_logs < 40) { g_media_logs++; LOGF("MEDIA SetDataSource(asset='%s' offset=%lld size=%lld) -> %d", name, offset, size, r); }
    return r;
}
static void install_media_diag(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; void* hook; unsigned long* target; } hooks[] = {
        {0x5bd2888, {0xd103c3ff, 0xf9004bfb, 0xa90a67fa, 0xa90b5ff8}, (void*)&hook_media_open, &g_tgt_media_open},
        {0x6a193cc, {0xd10143ff, 0xa9034ff4, 0xa9047bfd, 0x910103fd}, (void*)&hook_media_src_path, &g_tgt_media_src_path},
        {0x6a19490, {0xd10203ff, 0xa90367fa, 0xa9045ff8, 0xa90557f6}, (void*)&hook_media_src_archive, &g_tgt_media_src_archive},
        {0x6a197f4, {0xd10183ff, 0xa90357f6, 0xa9044ff4, 0xa9057bfd}, (void*)&hook_media_src_file, &g_tgt_media_src_file},
        {0x6a198d0, {0xd101c3ff, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}, (void*)&hook_media_src_asset, &g_tgt_media_src_asset},
    };
    for (unsigned i = 0; i < sizeof hooks / sizeof *hooks; i++)
        if (memcmp((const void*)(base + hooks[i].addr), hooks[i].first, 16)) { LOGF("MEDIA diag disabled +%lx", hooks[i].addr); return; }
    for (unsigned i = 0; i < sizeof hooks / sizeof *hooks; i++) {
        *hooks[i].target = base + hooks[i].addr + 16;
        patch_code((void*)(base + hooks[i].addr), hooks[i].hook);
    }
    LOGF("MEDIA diag ready");
}
#endif
