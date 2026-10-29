#ifndef RE4FIX_MOBILE_VIEW_H
#define RE4FIX_MOBILE_VIEW_H

static float mobile_view_pitch(int context, float gameplay_pitch) {
    return context==2 || context==4 || context==5 || context==7 ? 0.0f : gameplay_pitch;
}

static float mobile_phone_approach(long now, long started) {
    if (started<=0 || now<started) return 0.0f;
    float t=(float)(now-started)/900.0f;
    if (t>=1.0f) return 1.0f;
    return t*t*(3.0f-2.0f*t);
}

// Communicator pose in the OVR head frame (x right, y up, -z forward; metres), eased from below/left.
// End pose = native plane probe calibration (p252/p264 Incoming/Outgoing screens, identical to 1e-5):
// the opened screen pair is centred horizontally, keeps its 1.73 cm vertical offset, sits 22 cm ahead
// (whole display frame visible) and faces the camera exactly: the 0.98 deg yaw and -1.32 deg roll left
// by the plain Rx(60.8) wrist are removed by a 1.65 deg correction about the pair centre.
static void mobile_phone_pose(float alpha, float off[3], float q[4]) {
    static const float start_off[3]={-0.08f,-0.04f,-0.65f}, end_off[3]={-0.19776f,0.01044f,-0.22314f};
    static const float start_q[4]={0.70710678f,0.0f,0.0f,0.70710678f};
    static const float end_q[4]={0.506273f,-0.001556f,0.014289f,0.862253f};
    float n=0;
    for (int i=0;i<3;i++) off[i]=start_off[i]+(end_off[i]-start_off[i])*alpha;
    for (int i=0;i<4;i++) { q[i]=start_q[i]+(end_q[i]-start_q[i])*alpha; n+=q[i]*q[i]; }
    n=1.0f/sqrtf(n);
    for (int i=0;i<4;i++) q[i]*=n;
}

static int mobile_phone_hit(float u, float v) {
    return isfinite(u) && isfinite(v) && u>=0.15f && u<=0.90f && v>=0.10f && v<=0.95f;
}
#endif
