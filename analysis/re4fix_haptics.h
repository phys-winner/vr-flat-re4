// Controller rumble -> phone vibrator. The original FOculusInput::SetHapticFeedbackValues forwards each changed
// per-hand amplitude to ovrp_SetControllerVibration2(controller, frequency, amplitude); the game's own
// EnableControllerRumble option is honoured natively before that call. A helper thread mixes both hands and
// drives MobileAndroid.vibrate(amplitude, ms) with short refreshed one-shots (no stuck buzz).
#ifndef RE4FIX_HAPTICS_H
#define RE4FIX_HAPTICS_H
#include <jni.h>
static volatile float g_hap_amp[2];
static volatile long g_hap_set_ms[2];
static volatile int g_hap_peak;              // strongest request since the helper's last poll (short pulses)
static volatile long g_gt_alive_ms;          // last GameThread hand tick (app foreground/unpaused)
static JavaVM* g_hap_vm;
static jclass g_hap_class;
static jmethodID g_hap_vibrate;
static unsigned g_hap_logs;

static long hap_now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L; }
int h5_ovrp_SetControllerVibration2(int controller, float frequency, float amplitude) {
    (void)frequency;
    if (!isfinite(amplitude) || amplitude < 0) amplitude = 0;
    if (amplitude > 1) amplitude = 1;
    long now = hap_now_ms();
    if (controller & 0x01000001) { g_hap_amp[0] = amplitude; g_hap_set_ms[0] = now; }
    if (controller & 0x02000002) { g_hap_amp[1] = amplitude; g_hap_set_ms[1] = now; }
    int milli = (int)(amplitude * 1000.0f), peak = __atomic_load_n(&g_hap_peak, __ATOMIC_RELAXED);
    while (milli > peak && !__atomic_compare_exchange_n(&g_hap_peak, &peak, milli, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
    if (g_hap_logs < 12 && amplitude > 0) { g_hap_logs++; LOGF("HAPTIC controller=%x freq=%.2f amp=%.2f", controller, frequency, amplitude); }
    return 0;
}
// Buffered haptics are not produced: HapticsDesc reports no native sample rate, so the engine uses amplitude mode.
int h5_ovrp_SetControllerHaptics2(int controller, void* buffer) { (void)controller; (void)buffer; return 0; }

static void hap_call(JNIEnv* env, int amplitude, int ms) {
    (*env)->CallStaticVoidMethod(env, g_hap_class, g_hap_vibrate, (jint)amplitude, (jint)ms);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
static void* haptic_thread(void* arg) {
    (void)arg;
    JNIEnv* env = 0;
    JavaVMAttachArgs args = {JNI_VERSION_1_6, "RE4Haptics", 0};
    if ((*g_hap_vm)->AttachCurrentThread(g_hap_vm, &env, &args) != JNI_OK || !env) return 0;
    int last = 0; long refresh_at = 0;
    for (;;) {
        usleep(16000);
        long now = hap_now_ms();
        float a = 0;
        for (int side = 0; side < 2; side++) {
            // A constant authored rumble is re-sent only on change; cap one unchanged value at 3 s.
            if (now - g_hap_set_ms[side] < 3000 && g_hap_amp[side] > a) a = g_hap_amp[side];
        }
        int alive = now - __atomic_load_n(&g_gt_alive_ms, __ATOMIC_ACQUIRE) <= 250;   // not background/loading
        float peak = (float)__atomic_exchange_n(&g_hap_peak, 0, __ATOMIC_RELAXED) / 1000.0f;
        if (!alive) a = peak = 0;
        int target = a < 0.04f ? 0 : (int)(40.0f + 215.0f * a);
        if (target > 255) target = 255;
        if (!target) {
            if (peak >= 0.04f) {            // a pulse that started and ended between two polls
                hap_call(env, (int)(40.0f + 215.0f * fminf(peak, 1.0f)), 45);
                last = 0; continue;
            }
            if (last) { hap_call(env, 0, 0); last = 0; }
            continue;
        }
        if (abs(target - last) >= 20 || now >= refresh_at) {
            hap_call(env, target, 600);
            last = target; refresh_at = now + 500;
        }
    }
    return 0;
}
JNIEXPORT void JNICALL Java_com_epicgames_ue4_MobileAndroid_nativeBind(JNIEnv* env, jclass type) {
    if (!env || !type || __atomic_load_n(&g_hap_class, __ATOMIC_ACQUIRE)) return;
    JavaVM* vm = 0;
    if ((*env)->GetJavaVM(env, &vm) != JNI_OK || !vm) return;
    jmethodID method = (*env)->GetStaticMethodID(env, type, "vibrate", "(II)V");
    if (!method || (*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
    jclass global = (jclass)(*env)->NewGlobalRef(env, type);
    if (!global) return;
    g_hap_vm = vm; g_hap_vibrate = method;
    __atomic_store_n(&g_hap_class, global, __ATOMIC_RELEASE);
    pthread_t t; pthread_create(&t, 0, haptic_thread, 0); pthread_detach(t);
    LOGF("HAPTIC phone vibrator bridge ready");
}
#endif
