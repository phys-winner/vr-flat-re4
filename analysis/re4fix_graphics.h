// User-selected native runtime graphics; keep the cooked MSAA/HDR and shader permutations.
#ifndef RE4FIX_GRAPHICS_H
#define RE4FIX_GRAPHICS_H
#include <vulkan/vulkan_core.h>

// User choices use only runtime parameters consumed by the existing cooked renderer.
enum { MG_ANISO, MG_MIPBIAS, MG_VIEWDIST, MG_STATICLOD, MG_SKELETALLOD, MG_SHADOWRES, MG_COUNT };
static const struct {
    const char* cvar;
    const char* key;
    int floating, count;
    float lo, hi, choices[7];  // choice zero restores the captured phone-port default.
} mobile_graphics_options[MG_COUNT] = {
    {"r.MaxAnisotropy", "ganiso", 0, 6, 0, 16, {0, 1, 2, 4, 8, 16}},
    {"r.MipMapLODBias", "gmip", 1, 6, -15, 15, {0, -1, -.5f, 0, .5f, 1}},
    {"r.ViewDistanceScale", "gview", 1, 7, .01f, 100, {0, .5f, .75f, 1, 1.25f, 1.5f, 2}},
    {"r.StaticMeshLODDistanceScale", "gstatic", 1, 6, .01f, 100, {0, .5f, .75f, 1, 1.5f, 2}},
    {"r.SkeletalMeshLODBias", "gskel", 0, 4, -4, 16, {0, 0, 1, 2}},
    {"r.Shadow.MaxResolution", "gshadow", 0, 4, 16, 16384, {0, 512, 1024, 2048}},
};
static int mobile_prefs_graphics(int index);
static unsigned long mobile_graphics_base;
static int mobile_graphics_checked;
static int mobile_graphics_aniso_limit;  // zero means the device audit has not supplied a usable limit.
static void* (*mobile_graphics_find)(void*, const unsigned short*, int);
static struct {
    unsigned long vtable, getter, setter;
    int floating;
    int (*get_int)(const void*);
    float (*get_float)(const void*);
    void (*set)(void*, const unsigned short*, unsigned);
} mobile_graphics_classes[] = {
    {0x9e20d30, 0x65e7940, 0x65ee9f0, 0, 0, 0, 0},
    {0x9e20df8, 0x65e7b0c, 0x65eec38, 1, 0, 0, 0},
    {0x9e21050, 0x65ef480, 0x65ef40c, 0, 0, 0, 0},
    {0x9e21118, 0x65ef6f4, 0x65ef67c, 1, 0, 0, 0},
};
static struct {
    void* var;
    int type, requested;
    float original;
} mobile_graphics_state[MG_COUNT];

static int mobile_graphics_init(unsigned long base) {
    if (mobile_graphics_checked) return mobile_graphics_base != 0;
    if (!base || !*(void**)(base + 0xa6b1ac0)) return 0;
    mobile_graphics_checked = 1;
    static const struct { unsigned long address; unsigned words[2]; } checks[] = {
        {0x65e9ba0, {0xf81e0ff3, 0xa9017bfd}},
        {0x65e7940, {0xf81e0ff3, 0xa9017bfd}},
        {0x65ee9f0, {0xf81d0ff5, 0xa9014ff4}},
        {0x65e7b0c, {0xf81e0ff3, 0xa9017bfd}},
        {0x65eec38, {0xf81d0ff5, 0xa9014ff4}},
        {0x65ef480, {0xb9404000, 0xd65f03c0}},
        {0x65ef40c, {0xf81d0ff5, 0xa9014ff4}},
        {0x65ef6f4, {0xbd404000, 0xd65f03c0}},
        {0x65ef67c, {0xf81d0ff5, 0xa9014ff4}},
    };
    for (unsigned i = 0; i < sizeof checks / sizeof *checks; ++i)
        if (memcmp((const void*)(base + checks[i].address), checks[i].words, 8)) {
            LOGF("GRAPHICS options disabled: console signature mismatch +%lx", checks[i].address);
            return 0;
        }
    mobile_graphics_find = (void*)(base + 0x65e9ba0);
    for (unsigned i = 0; i < sizeof mobile_graphics_classes / sizeof *mobile_graphics_classes; ++i) {
        mobile_graphics_classes[i].get_int = (void*)(base + mobile_graphics_classes[i].getter);
        mobile_graphics_classes[i].get_float = (void*)(base + mobile_graphics_classes[i].getter);
        mobile_graphics_classes[i].set = (void*)(base + mobile_graphics_classes[i].setter);
    }
    for (int i = 0; i < MG_COUNT; ++i) mobile_graphics_state[i].requested = -1;
    mobile_graphics_base = base;
    return 1;
}
static int mobile_graphics_type(const void* var, int floating) {
    if (!var) return -1;
    unsigned long vtable = *(const unsigned long*)var;
    for (unsigned i = 0; i < sizeof mobile_graphics_classes / sizeof *mobile_graphics_classes; ++i)
        if (vtable == mobile_graphics_base + mobile_graphics_classes[i].vtable &&
            floating == mobile_graphics_classes[i].floating) return (int)i;
    return -1;
}
static float mobile_graphics_read(const void* var, int type) {
    return mobile_graphics_classes[type].floating ? mobile_graphics_classes[type].get_float(var)
        : (float)mobile_graphics_classes[type].get_int(var);
}
static int mobile_graphics_writable(int index) {
    if (index < 0 || index >= MG_COUNT || !mobile_graphics_base || !mobile_graphics_state[index].var) return 0;
    const void* var = mobile_graphics_state[index].var;
    if (mobile_graphics_type(var, mobile_graphics_options[index].floating) != mobile_graphics_state[index].type) return 0;
    unsigned flags = *(const unsigned*)((const char*)var + 0x18);
    return !(flags & (4u | 8u)) && (flags & 0xff000000u) <= 0x08000000u;
}
static int mobile_graphics_choice_count(int index) {
    if (index < 0 || index >= MG_COUNT) return 0;
    int count = mobile_graphics_options[index].count;
    if (index == MG_ANISO) {
        int limit = __atomic_load_n(&mobile_graphics_aniso_limit, __ATOMIC_ACQUIRE);
        if (limit < 1) return 0;
        while (count > 1 && mobile_graphics_options[index].choices[count - 1] > limit) --count;
    }
    return count;
}
static int mobile_graphics_available(int index) {
    return mobile_graphics_choice_count(index) > 1 && mobile_graphics_writable(index);
}
static int mobile_graphics_write(int index, float wanted) {
    if (!mobile_graphics_writable(index) || !isfinite(wanted) || wanted < mobile_graphics_options[index].lo ||
        wanted > mobile_graphics_options[index].hi) return 0;
    void* var = mobile_graphics_state[index].var;
    int type = mobile_graphics_state[index].type;
    float before = mobile_graphics_read(var, type);
    if (!isfinite(before) || before < mobile_graphics_options[index].lo || before > mobile_graphics_options[index].hi) return 0;
    if (fabsf(before - wanted) > .0001f) {
        char ascii[32]; unsigned short value[32];
        int n;
        if (mobile_graphics_classes[type].floating) {
            int fixed = (int)lroundf(wanted * 10000), magnitude = fixed < 0 ? -fixed : fixed;
            n = snprintf(ascii, sizeof ascii, "%s%d.%04d", fixed < 0 ? "-" : "", magnitude / 10000, magnitude % 10000);
        } else n = snprintf(ascii, sizeof ascii, "%d", (int)lroundf(wanted));
        if (n <= 0 || n >= (int)sizeof value / 2) return 0;
        for (int i = 0; i <= n; ++i) value[i] = (unsigned char)ascii[i];
        mobile_graphics_classes[type].set(var, value, 0x08000000u);  // ECVF_SetByCode; keep stronger console overrides.
    }
    float after = mobile_graphics_read(var, type);
    int accepted = isfinite(after) && fabsf(after - wanted) <= .0001f;
    LOGF("GRAPHICS user %s %.4f -> %.4f wanted=%.4f accepted=%d flags=%08x", mobile_graphics_options[index].cvar,
        before, after, wanted, accepted, *(unsigned*)((char*)var + 0x18));
    return accepted;
}
static int mobile_graphics_select(int index, int choice) {
    int count = mobile_graphics_choice_count(index);
    if (choice < 0 || choice >= count || !mobile_graphics_available(index)) return 0;
    float wanted = choice ? mobile_graphics_options[index].choices[choice] : mobile_graphics_state[index].original;
    if (!mobile_graphics_write(index, wanted)) return 0;
    mobile_graphics_state[index].requested = choice;
    return 1;
}
static int mobile_graphics_current_choice(int index, int preferred) {
    if (!mobile_graphics_writable(index)) return 0;
    float actual = mobile_graphics_read(mobile_graphics_state[index].var, mobile_graphics_state[index].type);
    if (!isfinite(actual)) return 0;
    int count = mobile_graphics_choice_count(index);
    if (preferred >= 0 && preferred < count && fabsf(actual - (preferred ? mobile_graphics_options[index].choices[preferred]
        : mobile_graphics_state[index].original)) <= .0001f) return preferred;
    if (fabsf(actual - mobile_graphics_state[index].original) <= .0001f) return 0;
    for (int choice = 1; choice < count; ++choice)
        if (fabsf(actual - mobile_graphics_options[index].choices[choice]) <= .0001f) return choice;
    return 0;
}
static void apply_touch_graphics(unsigned long base) {
    if (!mobile_graphics_init(base)) return;
    void* manager = *(void**)(base + 0xa6b1ac0);
    if (!manager) return;
    for (int i = 0; i < MG_COUNT; ++i) {
        if (mobile_graphics_state[i].requested == -1) {
            unsigned short name[64]; int n = 0;
            for (; mobile_graphics_options[i].cvar[n] && n < 63; ++n) name[n] = (unsigned char)mobile_graphics_options[i].cvar[n];
            name[n] = 0;
            void* var = mobile_graphics_find(manager, name, 0);
            int type = mobile_graphics_type(var, mobile_graphics_options[i].floating);
            mobile_graphics_state[i].requested = -2;
            if (type < 0) { LOGF("GRAPHICS option unavailable: %s class", mobile_graphics_options[i].cvar); continue; }
            float original = mobile_graphics_read(var, type);
            if (!isfinite(original) || original < mobile_graphics_options[i].lo || original > mobile_graphics_options[i].hi) {
                LOGF("GRAPHICS option unavailable: %s value=%.4f", mobile_graphics_options[i].cvar, original); continue;
            }
            mobile_graphics_state[i].var = var; mobile_graphics_state[i].type = type;
            if (i == MG_ANISO && original < 8 && mobile_graphics_writable(i) && mobile_graphics_write(i, 8)) original = 8;
            mobile_graphics_state[i].original = original;
            LOGF("GRAPHICS option %s default=%.4f class=%d available=%d flags=%08x", mobile_graphics_options[i].cvar,
                original, type, mobile_graphics_available(i), *(unsigned*)((char*)var + 0x18));
        }
        int wanted = mobile_prefs_graphics(i);
        if (mobile_graphics_state[i].requested != wanted) {
            (void)mobile_graphics_select(i, wanted);
            mobile_graphics_state[i].requested = wanted;  // A blocked or unsupported setting is not retried every frame.
        }
    }
}

void graphics_audit_device(const void* phys, const void* info, int result) {
    static unsigned calls;
    if (++calls > 2 || !phys || !info) return;
    const VkDeviceCreateInfo* ci = info;
    const VkPhysicalDeviceFeatures* enabled = ci->pEnabledFeatures;
    const VkBaseInStructure* chain = ci->pNext;
    for (unsigned i = 0; !enabled && chain && i < 16; i++, chain = chain->pNext)
        if (chain->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            enabled = &((const VkPhysicalDeviceFeatures2*)chain)->features;
    VkPhysicalDeviceProperties props = {0};
    VkPhysicalDeviceFeatures supported = {0};
    PFN_vkGetPhysicalDeviceProperties getp = r_gipa && g_vk_instance ?
        (PFN_vkGetPhysicalDeviceProperties)r_gipa(g_vk_instance, "vkGetPhysicalDeviceProperties") : 0;
    PFN_vkGetPhysicalDeviceFeatures getf = r_gipa && g_vk_instance ?
        (PFN_vkGetPhysicalDeviceFeatures)r_gipa(g_vk_instance, "vkGetPhysicalDeviceFeatures") : 0;
    if (getp) getp((VkPhysicalDevice)phys, &props);
    if (getf) getf((VkPhysicalDevice)phys, &supported);
    if (!result && getp && getf && enabled && enabled->samplerAnisotropy && supported.samplerAnisotropy)
        __atomic_store_n(&mobile_graphics_aniso_limit, (int)props.limits.maxSamplerAnisotropy, __ATOMIC_RELEASE);
    LOGF("GRAPHICS GPU '%s' result=%d anisotropy supported=%d enabled=%d limit=%.1f ETC2=%d ASTC_LDR=%d BC=%d",
         props.deviceName, result, getf ? (int)supported.samplerAnisotropy : -1,
         enabled ? (int)enabled->samplerAnisotropy : -1, getp ? props.limits.maxSamplerAnisotropy : -1.0f,
         getf ? (int)supported.textureCompressionETC2 : -1,
         getf ? (int)supported.textureCompressionASTC_LDR : -1,
         getf ? (int)supported.textureCompressionBC : -1);
}

void graphics_audit_sampler(const void* info, int result) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static struct { unsigned enabled; float cap; VkSamplerMipmapMode mip; } seen[16];
    static unsigned count;
    if (!info) return;
    pthread_mutex_lock(&mutex);
    if (count >= 16) { pthread_mutex_unlock(&mutex); return; }
    const VkSamplerCreateInfo* ci = info;
    for (unsigned i = 0; i < count; i++)
        if (seen[i].enabled == ci->anisotropyEnable && seen[i].cap == ci->maxAnisotropy &&
            seen[i].mip == ci->mipmapMode && !result) {
            pthread_mutex_unlock(&mutex); return;
        }
    seen[count].enabled = ci->anisotropyEnable;
    seen[count].mip = ci->mipmapMode;
    seen[count++].cap = ci->maxAnisotropy;
    LOGF("GRAPHICS sampler anisotropy=%u cap=%.1f filters=%d/%d mip=%d lod=%.1f..%.1f result=%d",
         ci->anisotropyEnable, ci->maxAnisotropy, ci->magFilter, ci->minFilter, ci->mipmapMode,
         ci->minLod, ci->maxLod, result);
    pthread_mutex_unlock(&mutex);
}
#endif
