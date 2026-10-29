// Native options-menu integration for phone settings (data-driven UVR4MenuComponent).
// Included after re4fix_input.h / re4fix_mobile_android.h / re4fix_mobile_extras.h.
//
// The original OptionsUIScreen builds every row from a DataTable through UVR4MenuComponent::AddEntry.
// Our rows are ordinary FVR4MenuEntryDescription values passed to that same native AddEntry, so the
// original widgets, scrolling, Accept/Reset buttons and sounds are used. Values of our own rows go
// through the original generic Get/Apply functions; those four natives are hooked and serve only our
// property FNames ("Mobile*"). Native options (camera bob, rumble) keep their original properties.
// VR-only rows that the phone build normalises are not added to the menu at all.
#ifndef RE4FIX_MOBILE_MENU_H
#define RE4FIX_MOBILE_MENU_H

// ---------------- persistent phone preferences (native owner) ----------------
// Existing saved aim/quick-turn choices survive updates; new installations use hold-to-aim and no quick turn.
struct mobile_prefs { int sens, size, posx, posy, aim_toggle, fps_index, assist, fov, ads_zoom, qturn;
    int aim_walk, intro_skip, start_confirm, hide_gamepad_hints, weapon_fov, gyro, gyro_sensitivity, aggression; int graphics[MG_COUNT]; };
#define MOBILE_PREFS_DEFAULT {100, 100, 0, 0, 0, 2, 1, 90, 1, 0, 1, 0, 0, 0, 90, 1, 100, 0}
static const struct mobile_prefs mobile_prefs_default = MOBILE_PREFS_DEFAULT;
static struct mobile_prefs g_mprefs = MOBILE_PREFS_DEFAULT;
static volatile int g_display_milli_hz = 60000;
static const char mobile_prefs_path[] = "/data/data/com.Armature.VR4/re4_mobile_prefs_v2";

static int mprefs_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void mobile_prefs_sanitize(struct mobile_prefs* p) {
    p->sens = mprefs_clamp(p->sens, 50, 200); p->size = mprefs_clamp(p->size, 75, 125);
    p->posx = mprefs_clamp(p->posx, -8, 8); p->posy = mprefs_clamp(p->posy, -8, 8);
    p->aim_toggle = mprefs_clamp(p->aim_toggle, 0, 1); p->fps_index = mprefs_clamp(p->fps_index, 0, 3);
    p->assist = mprefs_clamp(p->assist, 0, 1); p->fov = mprefs_clamp(p->fov, 70, 110);
    p->ads_zoom = mprefs_clamp(p->ads_zoom, 0, 1); p->qturn = mprefs_clamp(p->qturn, 0, 1);
    p->aim_walk = mprefs_clamp(p->aim_walk, 0, 1); p->intro_skip = mprefs_clamp(p->intro_skip, 0, 1);
    p->start_confirm = mprefs_clamp(p->start_confirm, 0, 1); p->hide_gamepad_hints = mprefs_clamp(p->hide_gamepad_hints, 0, 1);
    p->weapon_fov = mprefs_clamp(p->weapon_fov, 60, 110);
    p->aggression = mprefs_clamp(p->aggression, 0, 1);
    p->gyro = mprefs_clamp(p->gyro, 0, 1); p->gyro_sensitivity = mprefs_clamp(p->gyro_sensitivity, 25, 300);
    for (int i = 0; i < MG_COUNT; ++i) p->graphics[i] = mprefs_clamp(p->graphics[i], 0, mobile_graphics_options[i].count - 1);
}
static void mobile_prefs_publish(void) {
    static const int fps[3] = {30000, 45000, 60000};
    int hz = __atomic_load_n(&g_display_milli_hz, __ATOMIC_ACQUIRE);
    int target = g_mprefs.fps_index < 3 ? fps[g_mprefs.fps_index] : (hz >= 24000 && hz <= 240000 ? hz : 60000);
    (void)touch_mobile_settings_apply(g_mprefs.sens * 10, g_mprefs.size * 10, g_mprefs.posx * 10, g_mprefs.posy * 10, target);
}
static void mobile_prefs_load(void) {
    struct mobile_prefs p = mobile_prefs_default;
    int fd = open(mobile_prefs_path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[512]; int n = (int)read(fd, buf, sizeof buf - 1); close(fd);
        if (n > 0) {
            buf[n] = 0;
            for (char* line = strtok(buf, "\n"); line; line = strtok(0, "\n")) {
                char key[32]; int value;
                if (sscanf(line, "%31[^=]=%d", key, &value) != 2) continue;
                if (!strcmp(key, "sens")) p.sens = value; else if (!strcmp(key, "size")) p.size = value;
                else if (!strcmp(key, "posx")) p.posx = value; else if (!strcmp(key, "posy")) p.posy = value;
                else if (!strcmp(key, "aim")) p.aim_toggle = value; else if (!strcmp(key, "fps")) p.fps_index = value;
                else if (!strcmp(key, "assist")) p.assist = value; else if (!strcmp(key, "fov")) p.fov = value;
                else if (!strcmp(key, "adsz")) p.ads_zoom = value; else if (!strcmp(key, "qturn")) p.qturn = value;
                else if (!strcmp(key, "aimwalk")) p.aim_walk = value; else if (!strcmp(key, "introskip")) p.intro_skip = value;
                else if (!strcmp(key, "startconfirm")) p.start_confirm = value;
                else if (!strcmp(key, "hidepad")) p.hide_gamepad_hints = value; else if (!strcmp(key, "wfov")) p.weapon_fov = value;
                else if (!strcmp(key, "aggression")) p.aggression = value;
                else if (!strcmp(key, "gyro")) p.gyro = value; else if (!strcmp(key, "gyrosens")) p.gyro_sensitivity = value;
                else for (int i = 0; i < MG_COUNT; ++i) if (!strcmp(key, mobile_graphics_options[i].key)) p.graphics[i] = value;
            }
        }
    }
    mobile_prefs_sanitize(&p);
    g_mprefs = p;
    mobile_prefs_publish();
    LOGF("MOBILEPREFS loaded sens=%d size=%d pos=%d,%d aim_toggle=%d fps=%d assist=%d fov=%d ads_zoom=%d qturn=%d aim_walk=%d intro_skip=%d start_confirm=%d hidepad=%d weapon_fov=%d",
         p.sens, p.size, p.posx, p.posy, p.aim_toggle, p.fps_index, p.assist, p.fov,
         p.ads_zoom, p.qturn, p.aim_walk, p.intro_skip, p.start_confirm, p.hide_gamepad_hints, p.weapon_fov);
}
static void mobile_prefs_save(void) {
    char buf[512];
    int n = snprintf(buf, sizeof buf, "sens=%d\nsize=%d\nposx=%d\nposy=%d\naim=%d\nfps=%d\n"
                     "assist=%d\nfov=%d\nadsz=%d\nqturn=%d\naimwalk=%d\nintroskip=%d\nstartconfirm=%d\nhidepad=%d\nwfov=%d\ngyro=%d\ngyrosens=%d\naggression=%d\n",
                     g_mprefs.sens, g_mprefs.size, g_mprefs.posx, g_mprefs.posy, g_mprefs.aim_toggle, g_mprefs.fps_index,
                     g_mprefs.assist, g_mprefs.fov, g_mprefs.ads_zoom, g_mprefs.qturn,
                     g_mprefs.aim_walk, g_mprefs.intro_skip, g_mprefs.start_confirm, g_mprefs.hide_gamepad_hints, g_mprefs.weapon_fov, g_mprefs.gyro, g_mprefs.gyro_sensitivity, g_mprefs.aggression);
    if (n <= 0 || n >= (int)sizeof buf) return;
    for (int i = 0; i < MG_COUNT; ++i) {
        int extra = snprintf(buf + n, sizeof buf - (size_t)n, "%s=%d\n", mobile_graphics_options[i].key, g_mprefs.graphics[i]);
        if (extra <= 0 || extra >= (int)sizeof buf - n) return;
        n += extra;
    }
    char tmp[sizeof mobile_prefs_path + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", mobile_prefs_path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return;
    int ok = write(fd, buf, (size_t)n) == n;
    fsync(fd); close(fd);
    if (ok) rename(tmp, mobile_prefs_path); else unlink(tmp);
}
static int mobile_prefs_aim_toggle(void) { return __atomic_load_n(&g_mprefs.aim_toggle, __ATOMIC_RELAXED); }
static int mobile_prefs_assist(void) { return __atomic_load_n(&g_mprefs.assist, __ATOMIC_RELAXED); }
static int mobile_prefs_fov(void) { return __atomic_load_n(&g_mprefs.fov, __ATOMIC_RELAXED); }
static int mobile_prefs_ads_zoom(void) { return __atomic_load_n(&g_mprefs.ads_zoom, __ATOMIC_RELAXED); }
static int mobile_prefs_qturn(void) { return __atomic_load_n(&g_mprefs.qturn, __ATOMIC_RELAXED); }
static int mobile_prefs_aim_walk(void) { return __atomic_load_n(&g_mprefs.aim_walk, __ATOMIC_RELAXED); }
static int mobile_prefs_intro_skip(void) { return __atomic_load_n(&g_mprefs.intro_skip, __ATOMIC_RELAXED); }
static int mobile_prefs_start_confirm(void) { return __atomic_load_n(&g_mprefs.start_confirm, __ATOMIC_RELAXED); }
static int mobile_prefs_hide_gamepad_hints(void) { return __atomic_load_n(&g_mprefs.hide_gamepad_hints, __ATOMIC_RELAXED); }
static int mobile_prefs_weapon_fov(void) { return __atomic_load_n(&g_mprefs.weapon_fov, __ATOMIC_RELAXED); }
static int mobile_prefs_aggression(void) { return __atomic_load_n(&g_mprefs.aggression, __ATOMIC_RELAXED); }
static int mobile_prefs_gyro(void) { return __atomic_load_n(&g_mprefs.gyro, __ATOMIC_RELAXED); }
static int mobile_prefs_gyro_sensitivity(void) { return __atomic_load_n(&g_mprefs.gyro_sensitivity, __ATOMIC_RELAXED); }
static int mobile_prefs_graphics(int index) {
    return index >= 0 && index < MG_COUNT ? __atomic_load_n(&g_mprefs.graphics[index], __ATOMIC_RELAXED) : 0;
}

// Java MobileSettings.apply (GameActivity.onResume + display listener) reports the current panel rate.
__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileSettings_nativeDisplay(void* env, void* type, int milli_hz) {
    (void)env; (void)type;
    if (milli_hz < 24000 || milli_hz > 240000) return;
    __atomic_store_n(&g_display_milli_hz, milli_hz, __ATOMIC_RELEASE);
    mobile_prefs_publish();
}
__attribute__((visibility("default")))
void Java_com_epicgames_ue4_MobileSettings_nativeDensity(void* env, void* type, int milli_density) {
    (void)env; (void)type;
    if (milli_density >= 500 && milli_density <= 8000)
        __atomic_store_n(&g_touch_density_milli, milli_density, __ATOMIC_RELEASE);
}

// ---------------- engine ABI used to build menu rows ----------------
struct mm_ftext { void* object; void* controller; unsigned flags, pad; };
struct mm_fstring { unsigned short* data; int num, max; };
_Static_assert(sizeof(struct mm_ftext) == 24, "FText ABI");
static void* (*mm_desc_struct)(void);
static void (*mm_struct_init)(const void*, void*, int);
static void (*mm_struct_destroy)(const void*, void*, int);
static void (*mm_fname_ctor)(void*, const unsigned short*, int);
static struct mm_ftext (*mm_from_string)(struct mm_fstring*);
static void (*mm_text_dtor)(void*);
static void* (*mm_malloc)(unsigned long, unsigned);
static void (*mm_free)(void*);
static int (*mm_slider_int)(const void*, float);
static float (*mm_slider_float)(const void*, int);
static const void* (*mm_text_string)(const void*);
static void (*mm_show_files)(void), (*mm_show_language)(void);
static int mm_ready;
static unsigned mm_logs;

unsigned long g_tgt_menu_add_entries, g_tgt_menu_add_entry, g_tgt_opt_get, g_tgt_opt_apply, g_tgt_opt_get_slider,
    g_tgt_opt_apply_slider, g_tgt_menu_exec, g_tgt_opt_reset;
__attribute__((naked)) static void gate_menu_add_entries(void* self, void* table) {
    asm volatile(".inst 0xd10103ff\n" ".inst 0xf90013f3\n" ".inst 0xa9037bfd\n" ".inst 0x9100c3fd\n"
        "adrp x16, :got:g_tgt_menu_add_entries\n" "ldr x16, [x16, :got_lo12:g_tgt_menu_add_entries]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_menu_add_entry(void* self, unsigned long long row, const void* desc) {
    asm volatile(".inst 0xd102c3ff\n" ".inst 0xa9056ffc\n" ".inst 0xa90667fa\n" ".inst 0xa9075ff8\n"
        "adrp x16, :got:g_tgt_menu_add_entry\n" "ldr x16, [x16, :got_lo12:g_tgt_menu_add_entry]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_opt_get(void* self, const void* desc) {
    asm volatile(".inst 0xd10103ff\n" ".inst 0xa90157f6\n" ".inst 0xa9024ff4\n" ".inst 0xa9037bfd\n"
        "adrp x16, :got:g_tgt_opt_get\n" "ldr x16, [x16, :got_lo12:g_tgt_opt_get]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_opt_apply(void* self, const void* desc, int value) {
    asm volatile(".inst 0xd10403ff\n" ".inst 0xa90c5ff8\n" ".inst 0xa90d57f6\n" ".inst 0xa90e4ff4\n"
        "adrp x16, :got:g_tgt_opt_apply\n" "ldr x16, [x16, :got_lo12:g_tgt_opt_apply]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static int gate_opt_get_slider(void* self, const void* desc) {
    asm volatile(".inst 0xd10103ff\n" ".inst 0xa90157f6\n" ".inst 0xa9024ff4\n" ".inst 0xa9037bfd\n"
        "adrp x16, :got:g_tgt_opt_get_slider\n" "ldr x16, [x16, :got_lo12:g_tgt_opt_get_slider]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_opt_apply_slider(void* self, const void* desc, int value) {
    asm volatile(".inst 0xd10403ff\n" ".inst 0xa90c5ff8\n" ".inst 0xa90d57f6\n" ".inst 0xa90e4ff4\n"
        "adrp x16, :got:g_tgt_opt_apply_slider\n" "ldr x16, [x16, :got_lo12:g_tgt_opt_apply_slider]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_menu_set_current(void* self, unsigned long long menu) {
    asm volatile(".inst 0xa9be4ff4\n" ".inst 0xa9017bfd\n" ".inst 0x910043fd\n" ".inst 0xb9811809\n"
        "adrp x16, :got:g_tgt_menu_exec\n" "ldr x16, [x16, :got_lo12:g_tgt_menu_exec]\n" "ldr x16, [x16]\n" "br x16\n");
}
__attribute__((naked)) static void gate_opt_reset(void* self, unsigned long long menu) {
    asm volatile(".inst 0xfc190fe8\n" ".inst 0xa9016ffc\n" ".inst 0xa90267fa\n" ".inst 0xa9035ff8\n"
        "adrp x16, :got:g_tgt_opt_reset\n" "ldr x16, [x16, :got_lo12:g_tgt_opt_reset]\n" "ldr x16, [x16]\n" "br x16\n");
}

// ---------------- strings ----------------
static int mm_utf8_to_16(const char* s, unsigned short* out, int max) {
    int n = 0;
    while (*s && n < max - 1) {
        unsigned c = (unsigned char)*s++;
        if (c >= 0xe0 && s[0] && s[1]) { c = ((c & 0x0f) << 12) | ((s[0] & 0x3f) << 6) | (s[1] & 0x3f); s += 2; }
        else if (c >= 0xc0 && s[0]) { c = ((c & 0x1f) << 6) | (s[0] & 0x3f); s += 1; }
        out[n++] = (unsigned short)c;
    }
    out[n] = 0;
    return n;
}
static unsigned long long mm_fname(const char* ascii) {
    unsigned short wide[96]; unsigned long long name = 0;
    mm_utf8_to_16(ascii, wide, 96);
    mm_fname_ctor(&name, wide, 1);   // EFindName::FNAME_Add
    return name;
}
static void mm_set_text(unsigned char* dst, const char* utf8) {
    unsigned short wide[256];
    int n = mm_utf8_to_16(utf8 ? utf8 : "", wide, 256);
    struct mm_fstring fs = {(unsigned short*)mm_malloc((unsigned long)(n + 1) * 2, 0), n + 1, n + 1};
    if (!fs.data) return;
    memcpy(fs.data, wide, (size_t)(n + 1) * 2);
    struct mm_ftext text = mm_from_string(&fs);
    if (fs.data) mm_free(fs.data);
    mm_text_dtor(dst);
    memcpy(dst, &text, sizeof text);
}
static void mm_fstring_utf8(const void* fstr, char* out, int n) {
    out[0] = 0;
    if (!fstr) return;
    const unsigned short* data = *(const unsigned short* const*)fstr;
    int num = *(const int*)((const char*)fstr + 8);
    if (!data || num <= 0 || num > 4096) return;
    int k = 0;
    for (int i = 0; i < num && k < n - 4; i++) {
        unsigned c = data[i];
        if (!c) break;
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xc0 | (c >> 6)); out[k++] = (char)(0x80 | (c & 0x3f)); }
        else { out[k++] = (char)(0xe0 | (c >> 12)); out[k++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[k++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[k] = 0;
}
static void mm_name_utf8(const void* name, char* out, int n) {
    out[0] = 0;
    if (!name || !f_FName_ToString) return;
    struct mm_fstring fs = {0, 0, 0};
    f_FName_ToString(name, &fs);
    mm_fstring_utf8(&fs, out, n);
    if (fs.data && mm_free) mm_free(fs.data);
}

// Localised labels: the native culture (FInternationalization) selects the table at row creation.
enum { L_TITLE, L_SENS, L_SIZE, L_POSX, L_POSY, L_AIM, L_AIM_HOLD, L_AIM_TAP, L_FPS, L_FPS_DISPLAY, L_OFF, L_ON,
       L_BOB, L_RUMBLE, L_FILES, L_EXTRAS, L_X_MERC, L_X_PRO, L_X_MAFIA, L_X_WEAPONS, L_X_MODES, L_X_DONE,
       L_D_SENS, L_D_SIZE, L_D_POS, L_D_AIM, L_D_FPS, L_D_BOB, L_D_RUMBLE, L_D_FILES, L_D_EXTRAS, L_D_MERC,
       L_OPEN, L_ASSIST, L_FOV, L_ADSZ, L_QTURN,
       L_D_ASSIST, L_D_FOV, L_D_ADSZ, L_D_QTURN,
       L_GRAPHICS, L_GDEFAULT, L_GANISO, L_GMIP, L_GVIEW, L_GSTATIC, L_GSKEL, L_GSHADOW,
       L_D_GRAPHICS, L_D_GANISO, L_D_GMIP, L_D_GVIEW, L_D_GSTATIC, L_D_GSKEL, L_D_GSHADOW,
       L_AIM_WALK, L_INTRO_SKIP, L_START_CONFIRM, L_HIDE_PAD, L_WEAPON_FOV,
       L_D_AIM_WALK, L_D_INTRO_SKIP, L_D_START_CONFIRM, L_D_HIDE_PAD, L_D_WEAPON_FOV,
       L_D_X_WEAPONS, L_D_X_MODES, L_X_PENDING, L_X_RETRY, L_GYRO, L_GYRO_SENS, L_D_GYRO, L_D_GYRO_SENS, L_AGGRESSION, L_D_AGGRESSION, L_LANGUAGE, L_D_LANGUAGE, L_COUNT };
static const char* const mm_text_en[L_COUNT] = {
    "Touch controls", "Camera sensitivity", "Button size", "Buttons: horizontal shift", "Buttons: vertical shift",
    "Aiming", "Hold", "Tap", "Frame limit", "Display", "Off", "On",
    "View bobbing", "Vibration", "Save files and language", "Extra content", "Unlock Mercenaries",
    "Unlock Professional difficulty", "Unlock Mafia costume", "Bonus weapons",
    "Special modes", "Available",
    "Speed of looking around with a finger swipe.", "Size of the on-screen buttons.",
    "Moves the on-screen buttons together.", "Hold: aim while the button is held. Tap: tap once to aim, tap again to stop.",
    "Maximum frame rate. Display uses the current refresh rate of the screen.",
    "Camera bobbing while walking.", "Phone vibration for shots, hits and other game effects.",
    "Import or export save files and choose the interface language.",
    "Unlock bonus content through the game's own unlock functions.",
    "The first Mercenaries unlock performs the game's initial reset of that mode's separate progress.", "Open",
    "Aim assist", "Field of view", "Zoom when aiming",
    "Double tap: turn 180\xc2\xb0",
    "Slows the camera while the centre of the view is on an enemy, including with Laser Sight off.",
    "Horizontal field of view during gameplay. Menus and cutscenes keep their original view.",
    "Slightly narrows the view while aiming (not through scopes).",
    "Double-tap the right half of the screen to turn around quickly.",
    "Graphics", "Game default", "Anisotropic filtering", "Texture mip bias", "View distance", "Environment detail", "Character detail", "Shadow resolution",
    "Configure gameplay field of view, texture filtering, draw distance, mesh detail and existing dynamic shadows.",
    "Sharpness of textures viewed at an angle. Options follow the GPU's supported limit.",
    "Negative values sharpen texture sampling; positive values soften it. Existing texture data is retained.",
    "Scales the authored draw distance. Does not load new areas or replace visibility set by the level.",
    "Lower values keep detailed environment meshes visible farther away. Uses the game's existing mesh levels.",
    "0 keeps the most detailed available character mesh; higher values select simpler existing levels.",
    "Maximum size of existing dynamic shadow maps. Baked shadows and unsupported shadow types are unchanged.",
    "Walk while aiming", "Skip opening intro", "Confirm Start", "Hide gamepad buttons in hints", "Weapon field of view",
    "Limits movement to walking speed while aiming. Release Aim to run again.",
    "Automatically skips the opening story video when the game allows it. Other cutscenes still use hold to skip.",
    "Ask for confirmation before using the on-screen Start button.",
    "Hides controller button pictures in hints while keeping their text and touch actions.",
    "Changes the weapon's apparent distance while held at the hip. World view and aimed sights stay aligned.",
    "Unlock Chicago Typewriter and Hand Cannon through the game's original bonus weapon system.",
    "Unlock Horror, Big Head, High Speed and Golden Guns. These modes can change difficulty and balance.",
    "Saving...", "Retry save", "Gyroscope aiming", "Gyroscope sensitivity",
    "Turn the phone to fine-tune your aim while Aim is held. Swipe controls remain available.",
    "Sensitivity of gyroscope aiming, from 25 to 300 percent.", "Aggressive enemies",
    "Ganados pursue and attack more often using the original highest AI aggression. Turn off to restore their original behaviour.",
    "Language", "Choose the interface language. Changes apply at the next game launch." };
static const char* const mm_text_ru[L_COUNT] = {
    "Сенсорное управление", "Чувствительность камеры", "Размер кнопок", "Сдвиг кнопок по горизонтали",
    "Сдвиг кнопок по вертикали", "Прицеливание", "Удержание", "Нажатие", "Ограничение кадров", "Экран",
    "Выкл.", "Вкл.", "Покачивание камеры", "Вибрация", "Сохранения и язык",
    "Дополнительный контент", "Открыть Mercenaries", "Открыть сложность Professional", "Открыть костюм Mafia",
    "Бонусное оружие", "Особые режимы", "Доступно",
    "Скорость поворота камеры свайпом.", "Размер экранных кнопок.", "Сдвигает все экранные кнопки вместе.",
    "Удержание: прицел, пока кнопка зажата. Нажатие: одно касание включает прицел, повторное — выключает.",
    "Максимальная частота кадров. «Экран» — текущая частота обновления дисплея.",
    "Покачивание камеры при ходьбе.", "Вибрация телефона при выстрелах, попаданиях и других игровых эффектах.",
    "Импорт и экспорт сохранений, выбор языка интерфейса.",
    "Открыть бонусный контент штатными функциями игры.",
    "Первое открытие Mercenaries выполняет предусмотренный игрой сброс отдельного прогресса этого режима.", "Открыть",
    "Помощь в прицеливании",
    "Угол обзора", "Приближение при прицеливании", "Двойное касание: разворот на 180\xc2\xb0",
    "Замедляет камеру при наведении центра экрана на противника, в том числе при выключенном лазерном прицеле.",
    "Горизонтальный угол обзора в игре. Меню и ролики сохраняют исходный вид.",
    "Слегка приближает изображение при прицеливании (кроме оптических прицелов).",
    "Двойное касание правой половины экрана быстро разворачивает камеру.",
    "Графика", "По умолчанию", "Анизотропная фильтрация", "Смещение mip-уровней", "Дальность прорисовки", "Детализация окружения", "Детализация персонажей", "Разрешение теней",
    "Настройка игрового угла обзора, фильтрации текстур, дальности прорисовки, детализации моделей и существующих динамических теней.",
    "Чёткость текстур под углом. Доступные значения учитывают возможности графического процессора.",
    "Отрицательные значения делают текстуры чётче, положительные — мягче. Исходные текстуры сохраняются.",
    "Множитель авторской дальности прорисовки. Не загружает новые области и не меняет видимость, заданную уровнем.",
    "Меньшие значения дольше сохраняют детальные модели окружения. Используются готовые уровни детализации игры.",
    "0 выбирает самую подробную доступную модель персонажа; большие значения — более простые готовые модели.",
    "Максимальный размер существующих динамических карт теней. Запечённые тени и неподдерживаемые типы не меняются.",
    "Ходьба при прицеливании", "Пропуск вступительного ролика", "Подтверждение кнопки Старт", "Скрыть кнопки геймпада в подсказках", "Угол обзора оружия",
    "Ограничивает скорость движения до ходьбы при зажатом прицеле. Отпустите прицел, чтобы снова бежать.",
    "Автоматически пропускает вступительный сюжетный ролик, когда игра разрешает пропуск. Остальные ролики пропускаются удержанием.",
    "Запрашивает подтверждение перед нажатием экранной кнопки Старт.",
    "Скрывает изображения кнопок контроллера в подсказках, сохраняя текст и сенсорные действия.",
    "Меняет видимое расстояние до оружия от бедра. Обзор мира и совмещение прицела сохраняются.",
    "Открыть Chicago Typewriter и Hand Cannon штатными функциями бонусного оружия.",
    "Открыть Horror, Big Head, High Speed и Golden Guns. Эти режимы могут менять сложность и баланс.",
    "Сохранение...", "Повторить", "Прицеливание гироскопом", "Чувствительность гироскопа",
    "Поворачивайте телефон для точного наведения при включённом прицеле. Свайпы также работают.",
    "Чувствительность прицеливания гироскопом от 25 до 300 процентов.", "Агрессивные противники",
    "Ганадо чаще преследуют и атакуют, используя исходную максимальную агрессивность ИИ. Выключите, чтобы вернуть обычное поведение.",
    "Язык", "Выберите язык интерфейса. Изменение применяется при следующем запуске игры." };
static const char* const* mm_strings(void) {
    const char* lang = mobile_android_language();
    return lang && lang[0] == 'r' && lang[1] == 'u' ? mm_text_ru : mm_text_en;
}

// ---------------- names ----------------
static struct {
    unsigned long long none, top, load_link, menu, menu_link, extras, extras_link, comfort, audio, graphics, language;
    unsigned long long p_sens, p_size, p_posx, p_posy, p_aim, p_fps, p_files, p_language, p_x[5];
    unsigned long long p_bob, p_rumble, p_extras_open, p_graphics_open, p_graphics[MG_COUNT];
    unsigned long long p_assist, p_fov, p_adsz, p_qturn, p_aim_walk, p_intro_skip, p_start_confirm, p_hide_pad, p_weapon_fov, p_tutorial, p_gyro, p_gyro_sens, p_aggression;
    unsigned long long f_get, f_apply, f_get_slider, f_apply_slider, f_disp_pct, f_disp_val, f_language_get, f_language_apply;
    unsigned long long hidden[40]; int hidden_count;
    int ready;
} mn;
static const char* const mm_hidden_rows[] = {
    // VR-only rows normalised by re4fix_flat_controls.h or meaningless without motion controllers.
    "QuickComfort", "SeatedMode", "seatedHeight", "MovementMode", "TurnStyle", "TurnStyle_Angle", "TurnStyle_Speed",
    "Quick180Turn", "SteeringMethod", "FirstPersonFacing", "UseHolsterOffsets", "ChestHolsterDistance",
    "ChestHolsterSpread", "ChestHolsterHeight", "WaistHolsterDistance", "WaistHolsterSpread", "WaistHolsterHeight",
    // ActionTransitionMode stays visible: the authored 3rd-person transitions (ladders, jumps, kicks) suit a flat screen.
    "BowDrawDistance", "CinematicWindow", "BoatControlMode", "DominantHand", "MovementStick",
    "WatchHand", "WeaponSelect", "QuickSelectHand", "QuickSelectStick", "CombatStyle", "ReloadStyle", "GrenadeStyle",
    "EnableStrafing", "Strafing", "HeadSteering", "CameraBob", "ControllerRumble",
    "StandingHeightOffset" };
static void mm_names(void) {
    if (mn.ready) return;
    mn.top = mm_fname("TopLevel"); mn.load_link = mm_fname("LoadGame_Link");
    mn.menu = mm_fname("MobileControls"); mn.menu_link = mm_fname("MobileControls_Link");
    mn.extras = mm_fname("MobileExtras"); mn.extras_link = mm_fname("MobileExtras_Link");
    mn.comfort = mm_fname("ComfortOptions"); mn.audio = mm_fname("AudioOptions"); mn.graphics = mm_fname("MobileGraphics");
    mn.language = mm_fname("Language"); mn.p_language = mm_fname("MobileLanguageOpen");
    mn.f_language_get = mm_fname("GetLanguageSetting"); mn.f_language_apply = mm_fname("ApplyLanguageSetting");
    mn.p_graphics_open = mm_fname("MobileGraphicsOpen");
    for (int i = 0; i < MG_COUNT; ++i) {
        char key[40]; snprintf(key, sizeof key, "MobileGraphics%d", i + 1); mn.p_graphics[i] = mm_fname(key);
    }
    mn.p_sens = mm_fname("MobileCameraSensitivity"); mn.p_size = mm_fname("MobileButtonSize");
    mn.p_posx = mm_fname("MobileButtonOffsetX"); mn.p_posy = mm_fname("MobileButtonOffsetY");
    mn.p_aim = mm_fname("MobileAimMode"); mn.p_fps = mm_fname("MobileFrameLimit");
    mn.p_files = mm_fname("MobileSaveFiles"); mn.p_extras_open = mm_fname("MobileExtrasOpen");
    for (int i = 0; i < 5; i++) { char key[32]; snprintf(key, sizeof key, "MobileExtra%d", i + 1); mn.p_x[i] = mm_fname(key); }
    mn.p_bob = mm_fname("EnableCameraBobbing"); mn.p_rumble = mm_fname("EnableControllerRumble");
    mn.p_assist = mm_fname("MobileAimAssist"); mn.p_fov = mm_fname("MobileFieldOfView");
    mn.p_adsz = mm_fname("MobileAimZoom"); mn.p_qturn = mm_fname("MobileQuickTurn");
    mn.p_aim_walk = mm_fname("MobileAimWalk"); mn.p_intro_skip = mm_fname("MobileIntroSkip");
    mn.p_aggression = mm_fname("MobileEnemyAggression");
    mn.p_gyro = mm_fname("MobileGyro"); mn.p_gyro_sens = mm_fname("MobileGyroSensitivity");
    mn.p_start_confirm = mm_fname("MobileStartConfirm"); mn.p_hide_pad = mm_fname("MobileHideGamepadHints");
    mn.p_weapon_fov = mm_fname("MobileWeaponFieldOfView"); mn.p_tutorial = mm_fname("EnableTutorialUI");
    mn.f_get = mm_fname("Get_PlayerSettingsValue"); mn.f_apply = mm_fname("Apply_PlayerSettingsValue");
    mn.f_get_slider = mm_fname("Get_PlayerSettingsSliderValue"); mn.f_apply_slider = mm_fname("Apply_PlayerSettingsSliderValue");
    mn.f_disp_pct = mm_fname("Display_SliderValueAsRawPercentage"); mn.f_disp_val = mm_fname("Display_SliderValue");
    mn.hidden_count = 0;
    for (unsigned i = 0; i < sizeof mm_hidden_rows / sizeof *mm_hidden_rows && mn.hidden_count < 40; i++)
        mn.hidden[mn.hidden_count++] = mm_fname(mm_hidden_rows[i]);
    mn.ready = 1;
}
static unsigned long long mm_u64(const unsigned char* p) { unsigned long long v; memcpy(&v, p, 8); return v; }
static void mm_put(unsigned char* p, unsigned long long v) { memcpy(p, &v, 8); }

// ---------------- row construction ----------------
static unsigned char mm_current_table_kind;   // 1 = OptionsMenu_DT (lobby/pause options), 2 = other tables
static int mm_injected_this_table;
static int mm_graphics_open_injected;
static struct cinema_ui_weak mm_extras_component = {-1, 0};
static unsigned mm_extras_display_state;
static unsigned mm_extras_rejections,mm_extras_refresh_logs;
static void mm_row(void* component, const char* row, int widget, unsigned long long owning, unsigned long long link,
                   unsigned long long prop, float lo, float hi, float snap, unsigned long long get, unsigned long long apply,
                   unsigned long long display, const char* label, const char* help, const char* const* toggles, int count) {
    unsigned char desc[0x100] __attribute__((aligned(16)));
    const void* st = mm_desc_struct();
    if (!st) return;
    memset(desc, 0, sizeof desc);
    mm_struct_init(st, desc, 1);
    mm_set_text(desc + 0x8, label);
    mm_put(desc + 0x20, owning);
    desc[0x28] = (unsigned char)widget;
    if (count > 0) {
        struct mm_ftext* items = (struct mm_ftext*)mm_malloc((unsigned long)count * sizeof(struct mm_ftext), 0);
        if (items) {
            for (int i = 0; i < count; i++) {
                unsigned short wide[96]; int n = mm_utf8_to_16(toggles[i], wide, 96);
                struct mm_fstring fs = {(unsigned short*)mm_malloc((unsigned long)(n + 1) * 2, 0), n + 1, n + 1};
                if (fs.data) memcpy(fs.data, wide, (size_t)(n + 1) * 2);
                items[i] = mm_from_string(&fs);
                if (fs.data) mm_free(fs.data);
            }
            *(void**)(desc + 0x30) = items; *(int*)(desc + 0x38) = count; *(int*)(desc + 0x3c) = count;
        }
    }
    mm_put(desc + 0x40, link); mm_put(desc + 0x48, prop);
    memcpy(desc + 0x50, &lo, 4); memcpy(desc + 0x54, &hi, 4); memcpy(desc + 0x58, &snap, 4);
    mm_put(desc + 0x5c, get); mm_put(desc + 0x64, apply); mm_put(desc + 0x6c, mn.none); mm_put(desc + 0x74, display);
    mm_set_text(desc + 0x80, help);
    desc[0x98] = 0; desc[0x99] = 0;
    gate_menu_add_entry(component, mm_fname(row), desc);
    mm_struct_destroy(st, desc, 1);
}
static const char* mm_language_autonym(const char* language) {
    static const char* const codes[] = {"en", "fr", "es", "de", "ja", "ru"};
    static const char* const names[] = {"English", "Français", "Español", "Deutsch", "日本語", "Русский"};
    if (language) for (unsigned i = 0; i < sizeof codes / sizeof *codes; ++i)
        if (!strncmp(language, codes[i], 2) && (!language[2] || language[2] == '-' || language[2] == '_')) return names[i];
    return names[0];
}
static void mm_inject(void* component) {
    const char* const* t = mm_strings();
    const char* off_on[2] = {t[L_OFF], t[L_ON]};
    const char* aim[2] = {t[L_AIM_HOLD], t[L_AIM_TAP]};
    const char* fps[4] = {"30", "45", "60", t[L_FPS_DISPLAY]};
    unsigned long long M = mn.comfort;   // the original dynamic "Controls / Camera" list
    mm_row(component, "MobileSensitivity", 4, M, mn.none, mn.p_sens, 50, 200, 5, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_pct, t[L_SENS], t[L_D_SENS], 0, 0);
    mm_row(component, "MobileButtonSize", 4, M, mn.none, mn.p_size, 75, 125, 5, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_pct, t[L_SIZE], t[L_D_SIZE], 0, 0);
    mm_row(component, "MobileButtonOffsetX", 4, M, mn.none, mn.p_posx, -8, 8, 1, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_val, t[L_POSX], t[L_D_POS], 0, 0);
    mm_row(component, "MobileButtonOffsetY", 4, M, mn.none, mn.p_posy, -8, 8, 1, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_val, t[L_POSY], t[L_D_POS], 0, 0);
    mm_row(component, "MobileAimMode", 3, M, mn.none, mn.p_aim, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_AIM], t[L_D_AIM], aim, 2);
    mm_row(component, "MobileAimWalk", 3, M, mn.none, mn.p_aim_walk, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_AIM_WALK], t[L_D_AIM_WALK], off_on, 2);
    mm_row(component, "MobileGyro", 3, M, mn.none, mn.p_gyro, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_GYRO], t[L_D_GYRO], off_on, 2);
    mm_row(component, "MobileGyroSensitivity", 4, M, mn.none, mn.p_gyro_sens, 25, 300, 5, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_pct, t[L_GYRO_SENS], t[L_D_GYRO_SENS], 0, 0);
    mm_row(component, "MobileEnemyAggression", 3, M, mn.none, mn.p_aggression, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_AGGRESSION], t[L_D_AGGRESSION], off_on, 2);
    mm_row(component, "MobileAimAssist", 3, M, mn.none, mn.p_assist, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_ASSIST], t[L_D_ASSIST], off_on, 2);
    mm_row(component, "MobileAimZoom", 3, M, mn.none, mn.p_adsz, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_ADSZ], t[L_D_ADSZ], off_on, 2);
    mm_row(component, "MobileQuickTurn", 3, M, mn.none, mn.p_qturn, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_QTURN], t[L_D_QTURN], off_on, 2);
    mm_row(component, "MobileFrameLimit", 3, M, mn.none, mn.p_fps, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_FPS], t[L_D_FPS], fps, 4);
    mm_row(component, "MobileCameraBob", 3, M, mn.none, mn.p_bob, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_BOB], t[L_D_BOB], off_on, 2);
    mm_row(component, "MobileRumble", 3, M, mn.none, mn.p_rumble, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_RUMBLE], t[L_D_RUMBLE], off_on, 2);
    mm_row(component, "MobileIntroSkip", 3, M, mn.none, mn.p_intro_skip, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_INTRO_SKIP], t[L_D_INTRO_SKIP], off_on, 2);
    mm_row(component, "MobileStartConfirm", 3, M, mn.none, mn.p_start_confirm, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_START_CONFIRM], t[L_D_START_CONFIRM], off_on, 2);
    mm_row(component, "MobileHideGamepadHints", 3, M, mn.none, mn.p_hide_pad, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_HIDE_PAD], t[L_D_HIDE_PAD], off_on, 2);
    // Action rows are Toggle rows inside the scrolled list (a dynamic list stacks Link rows as overlapping footer
    // buttons): any change of the single-label toggle runs the action.
    const char* open[2] = {t[L_OPEN], t[L_OPEN]};
    mm_row(component, "MobileSaveFiles", 3, M, mn.none, mn.p_files, 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_FILES], t[L_D_FILES], open, 2);
    if (touch_mobile_extras_snapshot().menu_ready)   // opens the MobileExtras list added after the authored table
        mm_row(component, "MobileExtras_Link", 3, M, mn.none, mn.p_extras_open, 0, 1, 1, mn.f_get, mn.f_apply, mn.none,
               t[L_EXTRAS], t[L_D_EXTRAS], open, 2);
    if (mm_logs < 8) { mm_logs++; LOGF("MOBILEMENU native touch-controls section added component=%p", component); }
}
// After the authored table: a new Menu entry grows the component's Menus array, which must not happen while
// the native AddEntries loop still holds its current menu (rows after our section were lost or overwritten).
static int mm_extras_value(struct touch_mobile_extras extras, int action) {
    if (extras.failed & (1u << (action - 1))) return 3;
    if (extras.pending & (1u << (action - 1))) return 2;
    return !!(extras.unlocked & (1u << (action - 1)));
}
static unsigned mm_extras_values(struct touch_mobile_extras extras) {
    unsigned values = 0;
    for (int action = 1; action <= 5; ++action) values |= (unsigned)mm_extras_value(extras, action) << (2 * (action - 1));
    return values;
}
static void mm_inject_extras(void* component) {
    const char* const* t = mm_strings();
    unsigned long long M = mn.comfort;
    struct touch_mobile_extras extras = touch_mobile_extras_snapshot();
    if (extras.menu_ready) {
        if (cu_weak_assign) cu_weak_assign(&mm_extras_component, component);
        mm_extras_display_state = mm_extras_values(extras);
        mm_row(component, "MobileExtras", 0, mn.none, M, mn.none, 0, 1, 1, mn.none, mn.none, mn.none, t[L_EXTRAS], "", 0, 0);
        for (int i = 0; i < 5; i++) {
            char row[32];
            snprintf(row, sizeof row, "MobileExtra%d", i + 1);
            const char* values[4] = {t[L_OPEN], t[L_X_DONE], t[L_X_PENDING], t[L_X_RETRY]};
            mm_row(component, row, 3, mn.extras, mn.none, mn.p_x[i], 0, 1, 1, mn.f_get, mn.f_apply, mn.none, t[L_X_MERC + i],
                   t[i == 0 ? L_D_MERC : i == 3 ? L_D_X_WEAPONS : i == 4 ? L_D_X_MODES : L_D_EXTRAS], values, 4);
        }
    }
    if (mm_logs < 8) { mm_logs++; LOGF("MOBILEMENU extra content rows added component=%p extras=%d", component, extras.menu_ready); }
}
static void mm_inject_graphics_open(void* component) {
    const char* const* t = mm_strings();
    const char* open[2] = {t[L_OPEN], t[L_OPEN]};
    mm_row(component, "MobileGraphics_Open", 3, mn.audio, mn.none, mn.p_graphics_open, 0, 1, 1, mn.f_get, mn.f_apply,
           mn.none, t[L_GRAPHICS], t[L_D_GRAPHICS], open, 2);
}
static void mm_inject_graphics(void* component) {
    const char* const* t = mm_strings();
    int available = 0;
    for (int i = 0; i < MG_COUNT; ++i) available += mobile_graphics_available(i);
    // Menu declarations grow Menus: add them after the authored AddEntries loop, like the extras list.
    mm_row(component, "MobileGraphics", 0, mn.none, mn.audio, mn.none, 0, 1, 1, mn.none, mn.none, mn.none,
           t[L_GRAPHICS], "", 0, 0);
    mm_row(component, "MobileFieldOfView", 4, mn.graphics, mn.none, mn.p_fov, 70, 110, 5, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_val, t[L_FOV], t[L_D_FOV], 0, 0);
    mm_row(component, "MobileWeaponFieldOfView", 4, mn.graphics, mn.none, mn.p_weapon_fov, 60, 110, 5, mn.f_get_slider, mn.f_apply_slider,
           mn.f_disp_val, t[L_WEAPON_FOV], t[L_D_WEAPON_FOV], 0, 0);
    const char* values[MG_COUNT][7] = {
        {t[L_GDEFAULT], "1x", "2x", "4x", "8x", "16x"},
        {t[L_GDEFAULT], "-1.0", "-0.5", "0", "+0.5", "+1.0"},
        {t[L_GDEFAULT], "50%", "75%", "100%", "125%", "150%", "200%"},
        {t[L_GDEFAULT], "0.5x", "0.75x", "1x", "1.5x", "2x"},
        {t[L_GDEFAULT], "0", "+1", "+2"},
        {t[L_GDEFAULT], "512", "1024", "2048"},
    };
    for (int i = 0; i < MG_COUNT; ++i) if (mobile_graphics_available(i)) {
        char row[40]; snprintf(row, sizeof row, "MobileGraphics%d", i + 1);
        mm_row(component, row, 3, mn.graphics, mn.none, mn.p_graphics[i], 0, 1, 1, mn.f_get, mn.f_apply,
               mn.none, t[L_GANISO + i], t[L_D_GANISO + i], values[i], mobile_graphics_choice_count(i));
    }
    LOGF("MOBILEMENU graphics section added under AudioOptions component=%p options=%d", component, available);
}

// ---------------- hooks ----------------
static int mm_owner_is_options(void* component) {
    void* owner = component ? *(void* const*)((const char*)component + 0xa0) : 0;
    char cls[64] = "";
    if (owner) obj_name(*(void* const*)((const char*)owner + 0x10), cls, sizeof cls);
    return !strcmp(cls, "OptionsUIScreen_BP_C");
}
void hook_menu_add_entries(void* self, void* table) {
    char name[96] = "";
    if (table) obj_name(table, name, sizeof name);
    mm_current_table_kind = !strcmp(name, "OptionsMenu_DT") ? 1 : 2;
    mm_injected_this_table = 0;
    mm_graphics_open_injected = 0;
    if (mm_logs < 8) { mm_logs++; LOGF("MOBILEMENU AddEntries table='%s' component=%p", name, self); }
    gate_menu_add_entries(self, table);
    int injected = mm_current_table_kind == 1 && mm_injected_this_table;
    mm_current_table_kind = 0;
    if (injected && mm_owner_is_options(self)) { mm_inject_extras(self); mm_inject_graphics(self); }
}
static unsigned mm_row_logs;
static volatile int g_mm_row_trace;   // dev console: "menurows" logs the next options table build
void hook_menu_add_entry(void* self, unsigned long long row, const unsigned char* desc) {
    if (mm_ready && self && desc) {
        mm_names();
        if (mm_owner_is_options(self) && mm_u64(desc + 0x48) == mn.p_tutorial) return;
        if (mm_row_logs < 120 && mm_current_table_kind == 1 && phone_inspect_ready && g_mm_row_trace) {   // dev: row names
            char rn[64], owner[64], prop[64];
            phone_inspect_fname(&row, rn, sizeof rn); phone_inspect_fname(desc + 0x20, owner, sizeof owner);
            phone_inspect_fname(desc + 0x48, prop, sizeof prop);
            mm_row_logs++; LOGF("MOBILEMENU row='%s' menu='%s' property='%s' widget=%u", rn, owner, prop, desc[0x28]);
        }
        if (mm_owner_is_options(self))   // both OptionsMenu_DT and NewGameOptions_DT: VR-only rows are not added
            for (int i = 0; i < mn.hidden_count; i++) if (row == mn.hidden[i]) return;
        if (mm_current_table_kind == 1 && mm_owner_is_options(self)) {
            // SpawnMenuEntry's ValidIndex is shared by all submenus; extend Audio's first batch before another menu resets it.
            if (!mm_graphics_open_injected && desc[0x28] != 0 && mm_u64(desc + 0x20) == mn.audio) {
                mm_graphics_open_injected = 1;
                mm_inject_graphics_open(self);
            }
            if (row == mn.language && desc[0x28] == 3 && mm_u64(desc + 0x20) == mn.audio
                && mm_u64(desc + 0x48) == mn.none && mm_u64(desc + 0x5c) == mn.f_language_get
                && mm_u64(desc + 0x64) == mn.f_language_apply) {
                const char* const* t = mm_strings();
                const char* name = mm_language_autonym(mobile_android_language());
                const char* labels[2] = {name, name};
                // The original five-language Blueprint cannot select ru; use the persisted Android language picker.
                mm_row(self, "Language", 3, mn.audio, mn.none, mn.p_language, 0, 1, 1, mn.f_get, mn.f_apply,
                       mn.none, t[L_LANGUAGE], t[L_D_LANGUAGE], labels, 2);
                return;
            }
            // Our rows open the original "Controls / Camera" list, before its first authored row.
            if (!mm_injected_this_table && desc[0x28] != 0 && mm_u64(desc + 0x20) == mn.comfort) {
                mm_injected_this_table = 1;
                mm_inject(self);
            }
        }
    }
    gate_menu_add_entry(self, row, desc);
}
static int mm_own(const unsigned char* desc) {
    if (!mn.ready || !desc) return 0;
    unsigned long long p = mm_u64(desc + 0x48);
    for (int i = 0; i < 5; ++i) if (p == mn.p_x[i]) return 1;
    return p == mn.p_sens || p == mn.p_size || p == mn.p_posx || p == mn.p_posy || p == mn.p_aim || p == mn.p_fps
        || p == mn.p_files || p == mn.p_language || p == mn.p_extras_open || p == mn.p_assist || p == mn.p_fov || p == mn.p_adsz
        || p == mn.p_qturn || p == mn.p_graphics_open || p == mn.p_aim_walk || p == mn.p_intro_skip
        || p == mn.p_start_confirm || p == mn.p_hide_pad || p == mn.p_weapon_fov || p == mn.p_gyro || p == mn.p_gyro_sens || p == mn.p_aggression;
}
static int mm_graphics_index(const unsigned char* desc) {
    if (!mn.ready || !desc) return -1;
    unsigned long long p = mm_u64(desc + 0x48);
    for (int i = 0; i < MG_COUNT; ++i) if (p == mn.p_graphics[i]) return i;
    return -1;
}
static void mm_bind_extras(void* owner) {
    if (!owner || !pv_alive(owner) || !cu_weak_assign) return;
    void* component = *(void* const*)((const char*)owner + 0x258);
    if (component && pv_alive(component) && mm_owner_is_options(component))
        cu_weak_assign(&mm_extras_component, component);
}
int hook_opt_get(void* self, const unsigned char* desc) {
    int graphics = mm_graphics_index(desc);
    if (graphics >= 0) return mobile_graphics_current_choice(graphics, g_mprefs.graphics[graphics]);
    if (mm_own(desc)) {
        unsigned long long p = mm_u64(desc + 0x48);
        for (int i = 0; i < 5; ++i) if (p == mn.p_x[i]) {
            mm_bind_extras(self);
            return mm_extras_value(touch_mobile_extras_snapshot(), i + 1);
        }
        return p == mn.p_aim ? g_mprefs.aim_toggle : p == mn.p_fps ? g_mprefs.fps_index
            : p == mn.p_assist ? g_mprefs.assist : p == mn.p_adsz ? g_mprefs.ads_zoom : p == mn.p_qturn ? g_mprefs.qturn
            : p == mn.p_aim_walk ? g_mprefs.aim_walk : p == mn.p_intro_skip ? g_mprefs.intro_skip
            : p == mn.p_start_confirm ? g_mprefs.start_confirm : p == mn.p_hide_pad ? g_mprefs.hide_gamepad_hints : p == mn.p_gyro ? g_mprefs.gyro : p == mn.p_aggression ? g_mprefs.aggression : 0;
    }
    return gate_opt_get(self, desc);
}
static void* volatile mm_pending_menu_component;
static volatile long mm_pending_menu_ms;
static unsigned long long mm_pending_menu_target;
static int mm_reset_replay;
void hook_opt_apply(void* self, const unsigned char* desc, int value) {
    int graphics = mm_graphics_index(desc);
    if (graphics >= 0) {
        if (mobile_graphics_select(graphics, value)) { g_mprefs.graphics[graphics] = value; mobile_prefs_save(); }
        else LOGF("MOBILEMENU graphics setting rejected index=%d choice=%d", graphics, value);
        return;
    }
    if (mm_own(desc)) {
        unsigned long long p = mm_u64(desc + 0x48);
        for (int i = 0; i < 5; ++i) if (p == mn.p_x[i]) {
            mm_bind_extras(self);
            if (!mm_reset_replay) {
                const char* lang = mobile_android_language();
                int accepted = mobile_android_confirm_request(i + 1, lang && !strncmp(lang, "ru", 2), ptr_now_ms());
                LOGF("MOBILEMENU extra content confirmation action=%d queued=%d", i + 1, accepted);
            }
            return;
        }
        // Native reset replays Get/Apply for every row; opening another screen is not a setting reset.
        if (mm_reset_replay && (p == mn.p_files || p == mn.p_language || p == mn.p_extras_open || p == mn.p_graphics_open)) return;
        if (p == mn.p_files) { if (mm_show_files) mm_show_files(); LOGF("MOBILEMENU save files/language requested"); return; }
        if (p == mn.p_language) { if (mm_show_language) mm_show_language(); LOGF("MOBILEMENU language picker requested"); return; }
        if (p == mn.p_extras_open || p == mn.p_graphics_open) {   // navigate after the click handler returns (menu hand tick)
            void* component = self ? *(void**)((char*)self + 0x258) : 0;
            __atomic_store_n(&mm_pending_menu_target, p == mn.p_graphics_open ? mn.graphics : mn.extras, __ATOMIC_RELEASE);
            __atomic_store_n(&mm_pending_menu_component, component, __ATOMIC_RELEASE);
            __atomic_store_n(&mm_pending_menu_ms, ptr_now_ms(), __ATOMIC_RELEASE);
            return;
        }
        if (p == mn.p_aim) g_mprefs.aim_toggle = value; else if (p == mn.p_fps) g_mprefs.fps_index = value;
        else if (p == mn.p_assist) g_mprefs.assist = value;
        else if (p == mn.p_adsz) g_mprefs.ads_zoom = value; else if (p == mn.p_qturn) g_mprefs.qturn = value;
        else if (p == mn.p_aim_walk) g_mprefs.aim_walk = value; else if (p == mn.p_intro_skip) g_mprefs.intro_skip = value;
        else if (p == mn.p_start_confirm) g_mprefs.start_confirm = value;
        else if (p == mn.p_hide_pad) g_mprefs.hide_gamepad_hints = value;
        else if (p == mn.p_gyro) g_mprefs.gyro = value;
        else if (p == mn.p_aggression) g_mprefs.aggression = value;
        mobile_prefs_sanitize(&g_mprefs); mobile_prefs_save(); mobile_prefs_publish();
        return;
    }
    gate_opt_apply(self, desc, value);
}
int hook_opt_get_slider(void* self, const unsigned char* desc) {
    if (mm_own(desc)) {
        unsigned long long p = mm_u64(desc + 0x48);
        int v = p == mn.p_sens ? g_mprefs.sens : p == mn.p_size ? g_mprefs.size : p == mn.p_posx ? g_mprefs.posx
            : p == mn.p_fov ? g_mprefs.fov : p == mn.p_weapon_fov ? g_mprefs.weapon_fov : p == mn.p_gyro_sens ? g_mprefs.gyro_sensitivity : g_mprefs.posy;
        return mm_slider_int(desc, (float)v);
    }
    return gate_opt_get_slider(self, desc);
}
void hook_opt_apply_slider(void* self, const unsigned char* desc, int value) {
    if (mm_own(desc)) {
        unsigned long long p = mm_u64(desc + 0x48);
        int v = (int)lroundf(mm_slider_float(desc, value));
        if (p == mn.p_sens) g_mprefs.sens = v; else if (p == mn.p_size) g_mprefs.size = v;
        else if (p == mn.p_posx) g_mprefs.posx = v;
        else if (p == mn.p_fov) g_mprefs.fov = v; else if (p == mn.p_weapon_fov) g_mprefs.weapon_fov = v;
        else if (p == mn.p_gyro_sens) g_mprefs.gyro_sensitivity = v; else g_mprefs.posy = v;
        mobile_prefs_sanitize(&g_mprefs); mobile_prefs_save(); mobile_prefs_publish();
        return;
    }
    gate_opt_apply_slider(self, desc, value);
}
// Dynamic lists draw Link rows as buttons; a Link whose target is one of our action names runs it.
void hook_menu_set_current(void* self, unsigned long long menu) {
    if (mn.ready && menu) {
        if (menu == mn.p_files) { if (mm_show_files) mm_show_files(); LOGF("MOBILEMENU save files/language requested"); return; }
        for (int i = 0; i < 5; i++) if (menu == mn.p_x[i]) {
            const char* lang = mobile_android_language();
            int accepted = mobile_android_confirm_request(i + 1, lang && !strncmp(lang, "ru", 2), ptr_now_ms());
            LOGF("MOBILEMENU extra content confirmation action=%d queued=%d", i + 1, accepted);
            return;
        }
    }
    gate_menu_set_current(self, menu);
}
static void (*mm_refresh_menu)(void*, unsigned long long);
void hook_opt_reset(void* self, unsigned long long menu) {
    int ours = mn.ready && menu == mn.comfort;
    if (ours) {
        struct mobile_prefs reset = mobile_prefs_default;
        memcpy(reset.graphics, g_mprefs.graphics, sizeof reset.graphics);
        reset.fov = g_mprefs.fov;
        reset.weapon_fov = g_mprefs.weapon_fov;
        g_mprefs = reset; mobile_prefs_save(); mobile_prefs_publish();
    }
    if (mn.ready && (menu == mn.graphics || menu == mn.audio)) {
        for (int i = 0; i < MG_COUNT; ++i) { g_mprefs.graphics[i] = 0; (void)mobile_graphics_select(i, 0); }
        g_mprefs.fov = mobile_prefs_default.fov;
        g_mprefs.weapon_fov = mobile_prefs_default.weapon_fov;
        mobile_prefs_save();
        if (menu == mn.graphics) {
            void* component = self ? *(void**)((char*)self + 0x258) : 0;
            if (component && mm_refresh_menu) mm_refresh_menu(component, menu);
            LOGF("MOBILEMENU graphics reset to phone-port defaults");
            return;
        }
    }
    ++mm_reset_replay;
    gate_opt_reset(self, menu);
    --mm_reset_replay;
    if (ours && qs_settings && qs_copy && qs_set && qs_destroy) {
        // The authored reset restores VR defaults; phone defaults keep view bobbing on and tunneling off.
        const unsigned char* source = qs_settings();
        if (source && (source[0x3a] != 1 || source[0x31] != 0)) {
            struct __attribute__((aligned(16))) { unsigned char bytes[0xb8]; } copy;
            qs_copy(&copy, source); copy.bytes[0x3a] = 1; copy.bytes[0x31] = 0; qs_set(&copy); qs_destroy(&copy);
        }
        void* component = self ? *(void**)((char*)self + 0x258) : 0;
        if (component && mm_refresh_menu) mm_refresh_menu(component, menu);
        LOGF("MOBILEMENU touch controls reset to phone defaults");
    }
}

// Refresh only this same live component's visible Extras list; never navigate or run an action here.
static void mm_refresh_extras(void) {
    if (!cinema_on_game_thread() || !mn.ready || !cu_weak_get || !mm_refresh_menu) return;
    void* component = cu_weak_get(&mm_extras_component);
    void* owner = component && pv_alive(component) ? *(void* const*)((const char*)component + 0xa0) : 0;
    if (!owner || !pv_alive(owner) || !mm_owner_is_options(component)
        || *(void* const*)((const char*)owner + 0x258) != component) {
        // The original screen can finish assigning its component after menu construction.
        if (!(mm_extras_rejections & 1u)) {
            mm_extras_rejections |= 1u;
            LOGF("MOBILEMENU extras refresh waiting: component=%p owner=%p", component, owner);
        }
        return;
    }
    struct touch_mobile_extras extras = touch_mobile_extras_snapshot();
    if (!extras.menu_ready) return;
    unsigned values = mm_extras_values(extras);
    void* current = fv_object_property(owner, "CurrentMenu");
    if (!current) { mm_extras_display_state = values; return; }
    // FName is case-insensitive; its canonical display spelling is menuName in the live package.
    const char* name = fv_scalar_property(current, "menuName", "NameProperty", 8);
    if (!name) name = fv_scalar_property(current, "MenuName", "NameProperty", 8);
    if (!name) {
        if (!(mm_extras_rejections & 2u)) {
            mm_extras_rejections |= 2u;
            LOGF("MOBILEMENU extras refresh missing MenuName: component=%p owner=%p current=%p", component, owner, current);
        }
        return;
    }
    if (mm_u64((const unsigned char*)current + *(const int*)(name + 0x4c)) != mn.extras) {
        mm_extras_display_state = values; return;
    }
    if (values != mm_extras_display_state) {
        mm_extras_display_state = values;
        ++mm_reset_replay; // Programmatic row updates cannot reopen an action dialog.
        mm_refresh_menu(component, mn.extras);
        --mm_reset_replay;
        if (mm_extras_refresh_logs < 16) {
            ++mm_extras_refresh_logs;
            LOGF("MOBILEMENU extra content refreshed component=%p values=%x pending=%x failed=%x", component, values, extras.pending, extras.failed);
        }
    }
}
// GameThread (menu hand tick): open a native submenu requested by its action row.
static void touch_mobile_menu_tick(void) {
    long tick_now = ptr_now_ms();
    for (int kind = 1; kind <= 5; ++kind) {
        int answer = mobile_android_confirm_take(kind, tick_now);
        if (answer > 0) {
            int accepted = touch_mobile_extras_request(kind, tick_now);
            LOGF("MOBILEMENU confirmed extra content action=%d accepted=%d", kind, accepted);
        }
    }
    mm_refresh_extras();
    void* component = __atomic_load_n(&mm_pending_menu_component, __ATOMIC_ACQUIRE);
    if (!component) return;
    long at = __atomic_load_n(&mm_pending_menu_ms, __ATOMIC_ACQUIRE), now = ptr_now_ms();
    if (now - at < 50 && now >= at) return;
    __atomic_store_n(&mm_pending_menu_component, 0, __ATOMIC_RELEASE);
    if (now - at > 2000 || now < at || !pv_alive(component) || !mn.ready) return;
    unsigned long long target = __atomic_load_n(&mm_pending_menu_target, __ATOMIC_ACQUIRE);
    if (target != mn.extras && target != mn.graphics) return;
    gate_menu_set_current(component, target);
    LOGF("MOBILEMENU submenu opened: %s", target == mn.graphics ? "Graphics" : "Extra content");
}
static void install_mobile_menu(unsigned long base) {
    static const struct { unsigned long addr; unsigned first[4]; } check[] = {
        {0x630e194, {0xd10103ff, 0xf90013f3, 0xa9037bfd, 0x9100c3fd}},   // UVR4MenuComponent::AddEntries
        {0x630d8d8, {0xd102c3ff, 0xa9056ffc, 0xa90667fa, 0xa9075ff8}},   // UVR4MenuComponent::AddEntry
        {0x6326b78, {0xd10103ff, 0xa90157f6, 0xa9024ff4, 0xa9037bfd}},   // Get_PlayerSettingsValue
        {0x6326bfc, {0xd10403ff, 0xa90c5ff8, 0xa90d57f6, 0xa90e4ff4}},   // Apply_PlayerSettingsValue
        {0x63272ec, {0xd10103ff, 0xa90157f6, 0xa9024ff4, 0xa9037bfd}},   // Get_PlayerSettingsSliderValue
        {0x6327370, {0xd10403ff, 0xa90c5ff8, 0xa90d57f6, 0xa90e4ff4}},   // Apply_PlayerSettingsSliderValue
        {0x630f5c4, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xb9811809}},   // UVR4MenuComponent::SetCurrentMenu
        {0x6326d6c, {0xfc190fe8, 0xa9016ffc, 0xa90267fa, 0xa9035ff8}},   // ResetSettingsOfMenu
        {0x6473650, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xf0020ab3}},   // FVR4MenuEntryDescription::StaticStruct
        {0x688373c, {0xa9bb67fa, 0xa9015ff8, 0xa90257f6, 0xa9034ff4}},   // UScriptStruct::InitializeStruct
        {0x6883aa8, {0xa9bb67fa, 0xa9015ff8, 0xa90257f6, 0xa9034ff4}},   // UScriptStruct::DestroyStruct
        {0x67c7394, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0103e8}},   // FName::FName(const TCHAR*, EFindName)
        {0x6666d20, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xb9400809}},   // FText::FromString(FString&&)
        {0x54e79a8, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xf9400413}},   // FText::~FText
        {0x6615ef0, {0xf81d0ff5, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},   // FMemory::Malloc
        {0x6615fd0, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xb40001c0}},   // FMemory::Free
        {0x630f44c, {0xbd405803, 0x52998ee8, 0x72a64568, 0x1e270101}},   // CalculateSliderIntValue
        {0x630f578, {0xbd405802, 0x52998ee8, 0x72a64568, 0x1e270100}},   // CalculateSliderFloatValue
        {0x6666734, {0xf81e0ff3, 0xa9017bfd, 0x910043fd, 0xaa0003f3}},   // FText::ToString
        {0x630f64c, {0xb9811809, 0x7100053f, 0x5400026b, 0xf940880a}},   // UVR4MenuComponent::RefreshMenu
    };
    mobile_prefs_load();
    for (unsigned i = 0; i < sizeof check / sizeof *check; i++)
        if (memcmp((const void*)(base + check[i].addr), check[i].first, 16)) {
            LOGF("MOBILEMENU disabled: opcode mismatch +%lx", check[i].addr); return;
        }
    mm_desc_struct = (void* (*)(void))(base + 0x6473650);
    mm_struct_init = (void (*)(const void*, void*, int))(base + 0x688373c);
    mm_struct_destroy = (void (*)(const void*, void*, int))(base + 0x6883aa8);
    mm_fname_ctor = (void (*)(void*, const unsigned short*, int))(base + 0x67c7394);
    mm_from_string = (struct mm_ftext (*)(struct mm_fstring*))(base + 0x6666d20);
    mm_text_dtor = (void (*)(void*))(base + 0x54e79a8);
    mm_malloc = (void* (*)(unsigned long, unsigned))(base + 0x6615ef0);
    mm_free = (void (*)(void*))(base + 0x6615fd0);
    mm_slider_int = (int (*)(const void*, float))(base + 0x630f44c);
    mm_slider_float = (float (*)(const void*, int))(base + 0x630f578);
    mm_text_string = (const void* (*)(const void*))(base + 0x6666734);
    mm_show_files = mobile_android_request_files;
    mm_show_language = mobile_android_request_language;
    mm_refresh_menu = (void (*)(void*, unsigned long long))(base + 0x630f64c);
    g_tgt_menu_add_entries = base + 0x630e194 + 16; patch_code((void*)(base + 0x630e194), (void*)&hook_menu_add_entries);
    g_tgt_menu_add_entry = base + 0x630d8d8 + 16; patch_code((void*)(base + 0x630d8d8), (void*)&hook_menu_add_entry);
    g_tgt_opt_get = base + 0x6326b78 + 16; patch_code((void*)(base + 0x6326b78), (void*)&hook_opt_get);
    g_tgt_opt_apply = base + 0x6326bfc + 16; patch_code((void*)(base + 0x6326bfc), (void*)&hook_opt_apply);
    g_tgt_opt_get_slider = base + 0x63272ec + 16; patch_code((void*)(base + 0x63272ec), (void*)&hook_opt_get_slider);
    g_tgt_opt_apply_slider = base + 0x6327370 + 16; patch_code((void*)(base + 0x6327370), (void*)&hook_opt_apply_slider);
    g_tgt_menu_exec = base + 0x630f5c4 + 16; patch_code((void*)(base + 0x630f5c4), (void*)&hook_menu_set_current);
    g_tgt_opt_reset = base + 0x6326d6c + 16; patch_code((void*)(base + 0x6326d6c), (void*)&hook_opt_reset);
    mm_ready = 1;
    LOGF("MOBILEMENU ready: native touch-controls options section");
}
#endif
