// One geometry is shared by HUD rendering, hit testing and the Android layout editor.
#ifndef RE4FIX_TOUCH_LAYOUT_H
#define RE4FIX_TOUCH_LAYOUT_H

#define TOUCH_LAYOUT_LEFT_FIRE C_N
#define TOUCH_LAYOUT_COUNT (C_N + 1)
#define TOUCH_LAYOUT_FLOATS (TOUCH_LAYOUT_COUNT * 4)
_Static_assert(TOUCH_LAYOUT_COUNT==17,"Version the Java/native layout schema when controls change");
static float g_touch_layout[TOUCH_LAYOUT_FLOATS];
static int g_touch_layout_custom;
static struct ctl ctl_geometry(int z);
static void recompute(void);
static void menu_pointer_cancel_locked(void);
static void touch_grenade_retire_fingers(void);

static int touch_layout_valid(const float* values) {
    if (!values) return 0;
    for (int i=0;i<TOUCH_LAYOUT_COUNT;i++) {
        const float* v=values+i*4;
        if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2])
            || v[0]<0 || v[0]>1 || v[1]<0 || v[1]>1 || v[2]<.025f || v[2]>.18f
            || (v[3]!=0 && v[3]!=1)) return 0;
    }
    return 1;
}
static int touch_layout_shown(int slot) {
    return slot>=0 && slot<TOUCH_LAYOUT_COUNT && slot!=C_SETTINGS
        && (!g_touch_layout_custom || g_touch_layout[slot*4+3]!=0);
}
static struct ctl touch_layout_clamp(struct ctl c) {
    float ry=c.r*1.15f,rx=ry*g_th/g_tw;
    c.cx=fmaxf(rx,fminf(1-rx,c.cx));
    c.cy=fmaxf(ry,fminf(1-ry,c.cy));
    return c;
}
static struct ctl touch_left_fire_geometry(void) {
    struct ctl c=g_ctl[C_FIRE];
    c.cx=.105f;c.cy=.515f;c.r=.075f;
    if (g_touch_layout_custom) {
        const float* v=g_touch_layout+TOUCH_LAYOUT_LEFT_FIRE*4;
        c.cx=v[0];c.cy=v[1];c.r=v[2];
    }
    return touch_layout_clamp(c);
}
static int touch_layout_replace(const float* values) {
    if (!touch_layout_valid(values)) return 0;
    pthread_mutex_lock(&g_imx);
    memcpy(g_touch_layout,values,sizeof g_touch_layout);
    g_touch_layout_custom=1;
    // Existing fingers must end before they can press newly positioned buttons.
    touch_grenade_retire_fingers();
    for (int i=0;i<MAXT;i++) g_vt[i].role=RT_NONE;
    memset((void*)g_btn_hold_until,0,sizeof g_btn_hold_until);
    g_aim_latched=0;
    menu_pointer_cancel_locked();
    recompute();
    pthread_mutex_unlock(&g_imx);
    return 1;
}

#ifndef RE4_INPUT_TEST
#include <jni.h>
JNIEXPORT jfloatArray JNICALL Java_com_epicgames_ue4_MobileControls_nativeLayout(JNIEnv* env,jclass type,jboolean defaults) {
    (void)type;
    if (!env || (*env)->ExceptionCheck(env)) return 0;
    float values[TOUCH_LAYOUT_FLOATS];
    pthread_mutex_lock(&g_imx);
    for (int slot=0;slot<TOUCH_LAYOUT_COUNT;slot++) {
        struct ctl c=slot==TOUCH_LAYOUT_LEFT_FIRE ? touch_left_fire_geometry() : ctl_geometry(slot);
        if (defaults) {
            c=slot==TOUCH_LAYOUT_LEFT_FIRE ? g_ctl[C_FIRE] : g_ctl[slot];
            if (slot==TOUCH_LAYOUT_LEFT_FIRE) {c.cx=.105f;c.cy=.515f;c.r=.075f;}
        }
        values[slot*4]=c.cx;values[slot*4+1]=c.cy;values[slot*4+2]=c.r;
        values[slot*4+3]=defaults ? slot!=C_SETTINGS : touch_layout_shown(slot);
    }
    pthread_mutex_unlock(&g_imx);
    jfloatArray result=(*env)->NewFloatArray(env,TOUCH_LAYOUT_FLOATS);
    if (result) (*env)->SetFloatArrayRegion(env,result,0,TOUCH_LAYOUT_FLOATS,values);
    return result;
}
JNIEXPORT jboolean JNICALL Java_com_epicgames_ue4_MobileControls_nativeApply(JNIEnv* env,jclass type,jfloatArray input) {
    (void)type;
    if (!env || !input || (*env)->ExceptionCheck(env) || (*env)->GetArrayLength(env,input)!=TOUCH_LAYOUT_FLOATS) return JNI_FALSE;
    float values[TOUCH_LAYOUT_FLOATS];
    (*env)->GetFloatArrayRegion(env,input,0,TOUCH_LAYOUT_FLOATS,values);
    if ((*env)->ExceptionCheck(env) || !touch_layout_replace(values)) return JNI_FALSE;
    LOGF("TOUCHLAYOUT applied %d controls; left fire=%d",TOUCH_LAYOUT_COUNT,(int)values[TOUCH_LAYOUT_LEFT_FIRE*4+3]);
    return JNI_TRUE;
}
#endif
#endif
