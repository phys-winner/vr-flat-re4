// Flat-screen first-person weapon poses for the virtual hands (OVR node offsets, head frame:
// x right, y up, -z forward, metres; rotations in degrees: yaw about +y, pitch about +x, roll about -z).
// The knife hold/draw/slash are keyframed here; the native knife swing detectors still decide hits.
// Every key can be retuned live from the dev console: "kpose <key> x y z yaw pitch roll".
#ifndef RE4FIX_WEAPON_POSE_H
#define RE4FIX_WEAPON_POSE_H

struct wpose { float x, y, z, yaw, pitch, roll; };
// OVR head-relative metres. The native grenade model sits 3.5cm below the grip.
// Frame that unchanged model in the lower-right/left of the phone's vertical frustum.
static void grenade_held_pose(int side,float horizontal_fov,float aspect,float off[3]) {
    if(!off || side<0 || side>1)return;
    if(!isfinite(horizontal_fov) || horizontal_fov<60 || horizontal_fov>120)horizontal_fov=90;
    if(!isfinite(aspect) || aspect<1 || aspect>3)aspect=2400.0f/1080.0f;
    float tangent=tanf(horizontal_fov*.00872664626f),depth=.45f;
    off[0]=(side?1.0f:-1.0f)*fminf(.18f,depth*tangent*.42f);
    off[1]=.035f-.40f*depth*tangent/aspect;
    off[2]=-depth;
}
enum { KP_REST, KP_DRAW, KP_COCK, KP_STRIKE, KP_COUNT };
static struct wpose g_knife_keys[KP_COUNT] = {
    { 0.21f, -0.18f, -0.44f,  22.0f, -22.0f,  -8.0f },   // rest: glove low right, blade diagonal toward the centre
    { 0.26f, -0.80f, -0.22f,   0.0f, -60.0f,   0.0f },   // draw start: below the view
    { 0.30f,  0.60f, -0.42f,   0.0f,   0.0f,   0.0f },   // wind-up: raised above the upper right edge
    { 0.02f, -0.75f, -0.42f,   0.0f,   0.0f,   0.0f },   // strike end: below the view
};
// Slash timeline (ms): wind-up, strike, hold, recover. The strike keeps a fixed wrist and runs 1.38 m in 90 ms
// (~15 m/s, 12 deg from vertical): the native angle-weighted g1 detector only rates near-vertical cuts relative to
// the held blade (horizontal/diagonal sweeps score ~0), and g1+g2+g3 must all activate (measured with knifediag).
static int g_knife_t[4] = {120, 210, 260, 480};
static int g_knife_draw_ms = 280;
static volatile long g_knife_draw_t0;
static volatile float g_hand_rot_ovr[2][3];   // dev: extra hand rotation (deg) on top of everything
static volatile int g_hand_rot_ovr_on[2];

static qf_t q_euler_deg(float yaw, float pitch, float roll) {
    float y = yaw * 0.00872665f, p = pitch * 0.00872665f, r = roll * 0.00872665f;
    qf_t qy = {0, sinf(y), 0, cosf(y)}, qp = {sinf(p), 0, 0, cosf(p)}, qr = {0, 0, -sinf(r), cosf(r)};
    return q_mul(qy, q_mul(qp, qr));
}
static float wp_ease_in_out(float t) { return t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3.0f - 2.0f * t); }
static float wp_ease_out(float t) { return t <= 0 ? 0 : t >= 1 ? 1 : 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
static struct wpose wp_lerp(const struct wpose* a, const struct wpose* b, float t) {
    struct wpose r = {a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, a->z + (b->z - a->z) * t,
                      a->yaw + (b->yaw - a->yaw) * t, a->pitch + (b->pitch - a->pitch) * t, a->roll + (b->roll - a->roll) * t};
    return r;
}
static long knife_slash_length(void) { return g_knife_t[3]; }
// Pose of the knife hand for elapsed slash time (or < 0 when idle) and draw progress.
static struct wpose knife_pose(long slash_ms, long now) {
    struct wpose p = g_knife_keys[KP_REST];
    long drawn = now - __atomic_load_n(&g_knife_draw_t0, __ATOMIC_ACQUIRE);
    if (drawn >= 0 && drawn < g_knife_draw_ms)
        p = wp_lerp(&g_knife_keys[KP_DRAW], &g_knife_keys[KP_REST], wp_ease_out((float)drawn / (float)g_knife_draw_ms));
    if (slash_ms >= 0 && slash_ms < g_knife_t[3]) {
        if (slash_ms < g_knife_t[0])
            p = wp_lerp(&p, &g_knife_keys[KP_COCK], wp_ease_in_out((float)slash_ms / (float)g_knife_t[0]));
        else if (slash_ms < g_knife_t[1])   // linear: constant high speed through the target area
            p = wp_lerp(&g_knife_keys[KP_COCK], &g_knife_keys[KP_STRIKE], (float)(slash_ms - g_knife_t[0]) / (float)(g_knife_t[1] - g_knife_t[0]));
        else if (slash_ms < g_knife_t[2]) p = g_knife_keys[KP_STRIKE];
        else p = wp_lerp(&g_knife_keys[KP_STRIKE], &g_knife_keys[KP_REST],
                         wp_ease_out((float)(slash_ms - g_knife_t[2]) / (float)(g_knife_t[3] - g_knife_t[2])));
    }
    return p;
}
// Called when the knife becomes the held weapon.
static void knife_pose_drawn(long now) { __atomic_store_n(&g_knife_draw_t0, now, __ATOMIC_RELEASE); }

// In a shared world projection, change the held gun's distance without rescaling its native grip/sockets.
static float weapon_fov_depth(float depth, float world_fov, float weapon_fov) {
    if (!isfinite(depth) || depth <= 0 || !isfinite(world_fov) || !isfinite(weapon_fov)
        || world_fov < 60 || world_fov > 120 || weapon_fov < 60 || weapon_fov > 110) return depth;
    return depth * tanf(weapon_fov * 0.00872664626f) / tanf(world_fov * 0.00872664626f);
}
struct weapon_sway_state { long time; float yaw, pitch, side, up; };
// Head-relative centimetres and bore-axis roll: the real native fire direction stays aligned with the view.
static void weapon_sway_step(struct weapon_sway_state* s, long now, float yaw, float pitch,
                             float weight, float shift[3], float* roll) {
    shift[0] = shift[1] = shift[2] = *roll = 0;
    if (!isfinite(yaw) || !isfinite(pitch) || !isfinite(weight) || weight <= 0 || now <= 0) {
        memset(s, 0, sizeof *s); return;
    }
    long elapsed = now - s->time;
    if (!s->time || elapsed < 0 || elapsed > 200) {
        *s = (struct weapon_sway_state){now, yaw, pitch, 0, 0}; return;
    }
    if (elapsed > 0) {
        float dt = (float)elapsed / 1000.0f, blend = 1.0f - expf(-dt * 12.0f);
        float side = -0.32f * remainderf(yaw - s->yaw, 6.28318530718f) / dt;
        float up = -0.24f * remainderf(pitch - s->pitch, 6.28318530718f) / dt;
        side = fminf(1.2f, fmaxf(-1.2f, side)); up = fminf(0.8f, fmaxf(-0.8f, up));
        s->side += (side - s->side) * blend; s->up += (up - s->up) * blend;
        s->time = now; s->yaw = yaw; s->pitch = pitch;
    }
    weight = fminf(1.0f, weight);
    shift[1] = s->side * weight; shift[2] = s->up * weight; *roll = s->side * 0.9f * weight;
}
struct weapon_bob_state { long time; float phase, amount; };
// Speed is the native player's horizontal cm/s; blocked movement never creates synthetic footsteps.
static void weapon_bob_step(struct weapon_bob_state* s, long now, float speed, int enabled, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    long elapsed = now - s->time;
    if (!enabled || !isfinite(speed) || speed < 0 || now <= 0) { memset(s, 0, sizeof *s); return; }
    if (!s->time || elapsed < 0 || elapsed > 200) { s->time = now; s->amount = 0; return; }
    if (elapsed > 0) {
        float dt = (float)elapsed / 1000.0f;
        speed = fminf(speed, 400.0f);
        float target = fminf(1.0f, fmaxf(0.0f, (speed - 8.0f) / 140.0f));
        s->amount += (target - s->amount) * (1.0f - expf(-dt * 10.0f));
        s->phase = remainderf(s->phase + speed * dt * 0.012f, 6.28318530718f); s->time = now;
    }
    out[1] = 0.35f * sinf(s->phase) * s->amount;
    out[2] = 0.45f * sinf(2.0f * s->phase) * s->amount;
}

// Dev console: kpose <rest|draw|cock|strike> x y z yaw pitch roll | kpose times a b c d | kpose draw_ms n
static int weapon_pose_command(const char* line) {
    char cmd[16], key[16]; float v[6];
    int n = sscanf(line, "%15s %15s %f %f %f %f %f %f", cmd, key, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    if (n < 2 || strcmp(cmd, "kpose")) return 0;
    static const char* names[KP_COUNT] = {"rest", "draw", "cock", "strike"};
    for (int i = 0; i < KP_COUNT; i++) if (!strcmp(key, names[i]) && n == 8) {
        struct wpose p = {v[0], v[1], v[2], v[3], v[4], v[5]}; g_knife_keys[i] = p;
    }
    if (!strcmp(key, "times") && n >= 6) for (int i = 0; i < 4; i++) g_knife_t[i] = (int)v[i];
    if (!strcmp(key, "draw_ms") && n >= 3) g_knife_draw_ms = (int)v[0];
    for (int i = 0; i < KP_COUNT; i++)
        LOGF("KPOSE %s = %.3f %.3f %.3f yaw=%.1f pitch=%.1f roll=%.1f", names[i], g_knife_keys[i].x, g_knife_keys[i].y,
             g_knife_keys[i].z, g_knife_keys[i].yaw, g_knife_keys[i].pitch, g_knife_keys[i].roll);
    LOGF("KPOSE times=%d,%d,%d,%d draw=%d", g_knife_t[0], g_knife_t[1], g_knife_t[2], g_knife_t[3], g_knife_draw_ms);
    return 1;
}
#endif
