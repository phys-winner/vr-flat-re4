"""Execute the actual selected-watch pose and its existing easing without an APK/device."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / 'analysis/re4fix.c').read_text(encoding='utf8')
weapon = (ROOT / 'analysis/re4fix_weapon_pose.h').read_text(encoding='utf8')

def block(source, marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        if source[end] == '{': depth += 1
        if source[end] == '}': depth -= 1
        end += 1
    return source[start:end] + '\n'

pose = main[main.index('int h5_ovrp_GetNodePoseState3('):main.index('static void* (*g_ovrp_thunks')]
watch = block(pose, 'if (hi == __atomic_load_n(&g_watch_side,__ATOMIC_ACQUIRE))')
assert pose.index('mobile_watch_offset(') < pose.index('touch_weapon_cycle_pose(') < pose.index('q_rot(position_q, off, w)')
assert 'g_look_yaw=' not in watch and 'g_fov_h_deg=' not in watch
source = '''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {float x,y,z,w;} qf_t;
static long clock_ms,g_watch_since_ms,g_watch_until_ms;
static int g_touch_context=1,g_watch_side;
static long ptr_now_ms(void){return clock_ms;}
#include "re4fix_mobile_view.h"
'''
source += block(main, 'static qf_t q_mul(')
source += block(main, 'static void q_rot(')
source += '''static float g_eye_height=1.6f,g_pause_back=.14f,head_yaw,head_pitch;
static void pose_angles(long node,float* yaw,float* pitch){(void)node;*yaw=head_yaw;*pitch=head_pitch;}
'''
source += block(main, 'static void head_pose_node(')
source += block(weapon, 'static qf_t q_euler_deg(')
source += block(main, 'static float watch_pose_weight(')
source += 'static qf_t apply(int hi,float off[3]) { qf_t q={0,0,0,1};\n' + watch + 'return q;}\n'
source += r'''
static int near(float a,float b){return fabsf(a-b)<.000005f;}
int main(void){
    for(int context=0;context<9;context++)for(int heading=-3;heading<=3;heading++)for(long node=0;node<5;node++){
        g_touch_context=context;head_yaw=heading*.91f;head_pitch=.17f;
        float position[3];qf_t q;head_pose_node(node,&q,position);
        float back=context==8?1.1f:context==2?.14f:0;
        assert(near(position[0],sinf(head_yaw)*back)&&near(position[1],1.6f)&&near(position[2],cosf(head_yaw)*back));
        qf_t expected=q_mul((qf_t){0,sinf(head_yaw*.5f),0,cosf(head_yaw*.5f)},(qf_t){sinf(head_pitch*.5f),0,0,cosf(head_pitch*.5f)});
        assert(!memcmp(&q,&expected,sizeof q));
    }
    g_touch_context=1;
    for(int context=0;context<9;context++)for(int angle=-4;angle<=4;angle++){
        float input=angle*.3f,expected=context==8?-5.0f*.01745329252f:
            context==2||context==4||context==5||context==7?0:input;
        assert(near(mobile_view_pitch(context,input),expected));
    }
    puts("PASS: actual head_pose_node in all9 contexts/7 headings/5 nodes; merchant110cm/pitch-5deg, pause14cm, other positions/rotations and pitch policies unchanged");
    const int times[]={1000,1001,1055,1110,1219,1220,1500};
    for(int selected=0;selected<2;selected++){
        g_watch_side=selected;g_watch_since_ms=1000;g_watch_until_ms=2000;
        float initial[2][3]={{-.20f,-.30f,-.30f},{.20f,-.26f,-.36f}};
        for(unsigned t=0;t<sizeof times/sizeof *times;t++){
            clock_ms=times[t];float alpha=watch_pose_weight(clock_ms);
            for(int hand=0;hand<2;hand++){
                float off[3];memcpy(off,initial[hand],sizeof off);qf_t q=apply(hand,off);
                assert(near(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w,1));
                if(hand!=selected || !alpha){
                    assert(!memcmp(off,initial[hand],sizeof off) && near(q.w,1));
                }else{
                    float target[3]={hand?.10f:-.10f,.13f,-.24f};
                    for(int k=0;k<3;k++){
                        assert(isfinite(off[k]));
                        assert(off[k]>=fminf(initial[hand][k],target[k])-.000001f);
                        assert(off[k]<=fmaxf(initial[hand][k],target[k])+.000001f);
                        if(alpha==1)assert(near(off[k],target[k]));
                    }
                    if(alpha==1){
                        assert(-off[2]>=.20f && -off[2]<=.30f);
                        assert(atan2f(fabsf(off[0]),-off[2])<30*3.14159265f/180);
                    }
                }
            }
        }
    }
    for(clock_ms=1000;clock_ms<=1220;clock_ms+=11){
        qf_t q[2];
        for(int hand=0;hand<2;hand++){
            g_watch_side=hand;float off[3]={0,0,-.3f};q[hand]=apply(hand,off);
        }
        assert(near(q[0].x,q[1].x) && near(q[0].y,-q[1].y));
        assert(near(q[0].z,-q[1].z) && near(q[0].w,q[1].w));
        float v[3]={.1f,.2f,-.3f},mirror[3]={-.1f,.2f,-.3f},a[3],b[3];
        q_rot(q[0],v,a);q_rot(q[1],mirror,b);
        assert(near(a[0],-b[0]) && near(a[1],b[1]) && near(a[2],b[2]));
    }
    clock_ms=1220;
    for(int context=0;context<9;context++)if(context!=1){
        g_touch_context=context;float off[3]={.2f,-.3f,-.4f},before[3];memcpy(before,off,sizeof off);
        qf_t q=apply(g_watch_side,off);assert(!memcmp(off,before,sizeof off) && near(q.w,1));
    }
    puts("PASS: actual selected-side watch branch, 24cm target, unchanged other hand/weight0/contexts, convex eased motion, unit/mirrored wrist orientation; native weapon cycle remains later");
}
'''
sdk = Path(os.environ['ANDROID_SDK_ROOT'])
clang = sdk / 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
vc = Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231')
win = Path(r'C:/Program Files (x86)/Windows Kits/10')
with tempfile.TemporaryDirectory(prefix='re4-watch-pose-') as folder:
    folder = Path(folder)
    program, obj, exe = folder/'watch.c', folder/'watch.obj', folder/'watch.exe'
    program.write_text(source, encoding='utf8')
    subprocess.run([str(clang),'--target=aarch64-linux-android23','-O2','-Werror','-I',str(ROOT/'analysis'),'-fsyntax-only',str(program)],check=True)
    subprocess.run([str(clang),'--target=x86_64-pc-windows-msvc','-O2','-Werror','-I',str(ROOT/'analysis'),
        '-isystem',str(vc/'include'),'-isystem',str(win/'Include/10.0.26100.0/ucrt'),'-c',str(program),'-o',str(obj)],check=True)
    subprocess.run([str(vc/'bin/Hostx64/x64/link.exe'),'/NOLOGO',f'/OUT:{exe}',str(obj),
        'libcmt.lib','libucrt.lib','libvcruntime.lib','oldnames.lib','kernel32.lib',
        f'/LIBPATH:{vc/"lib/x64"}',f'/LIBPATH:{win/"Lib/10.0.26100.0/ucrt/x64"}',f'/LIBPATH:{win/"Lib/10.0.26100.0/um/x64"}'],check=True)
    subprocess.run([str(exe)],check=True)
