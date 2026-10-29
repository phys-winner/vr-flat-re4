package com.epicgames.ue4;

import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import java.lang.reflect.Field;

public final class MobileGyroTest {
    private static boolean enabled, missingNative;
    private static int activeCalls, calls;
    private static float yaw, pitch;
    private static long stamp;
    private static final GameActivity activity = new GameActivity();
    private static SensorEventListener listener;
    private static Runnable watch;
    private static Handler handler;

    public static boolean active() {
        activeCalls++;
        if (missingNative) throw new UnsatisfiedLinkError("test native library pending");
        return enabled;
    }
    public static void gyro(float horizontal, float vertical) { calls++; yaw=horizontal; pitch=vertical; }
    private static Object field(String name) throws Exception {
        Field value=MobileSettings.class.getDeclaredField(name);value.setAccessible(true);return value.get(null);
    }
    private static void send(long delta, float... values) {
        stamp+=delta;listener.onSensorChanged(new SensorEvent(stamp, values));
    }
    private static void first() {
        stamp=1000000000L;calls=0;send(0,.5f,.75f,0);assert calls==0;
    }
    private static void restart() {
        MobileSettings.pause(activity);MobileSettings.apply(activity);assert activity.sensors.listener==listener;first();
    }
    private static void expect(long delta,float x,float y,float horizontal,float vertical) {
        int before=calls;send(delta,x,y,0);assert calls==before+1;
        assert Math.abs(yaw-horizontal)<.000001f : "yaw "+yaw+" expected "+horizontal;
        assert Math.abs(pitch-vertical)<.000001f : "pitch "+pitch+" expected "+vertical;
    }
    public static void main(String[] args) throws Exception {
        listener=(SensorEventListener)field("gyroListener");watch=(Runnable)field("gyroWatch");handler=(Handler)field("gyroHandler");
        SensorManager sensors=activity.sensors;
        MobileSettings.apply(activity);assert sensors.registers==0 && handler.pending==watch && handler.delay==100;
        enabled=true;watch.run();assert sensors.registers==1 && sensors.listener==listener;
        watch.run();assert sensors.registers==1;
        float[][] outputs={{.015f,.01f},{-.01f,.015f},{-.015f,-.01f},{.01f,-.015f}};
        for(int rotation=0;rotation<4;rotation++) {
            activity.window.display.rotation=rotation;restart();
            expect(20000000L,.5f,.75f,outputs[rotation][0],outputs[rotation][1]);
            expect(20000000L,-.5f,-.75f,-outputs[rotation][0],-outputs[rotation][1]);
        }
        activity.window.display.rotation=0;restart();
        expect(100000000L,.5f,.75f,.075f,.05f);
        int before=calls;send(100001000L,.5f,.75f,0);assert calls==before;
        expect(20000000L,.5f,.75f,.015f,.01f);
        before=calls;send(0,.5f,.75f,0);send(-20000000L,.5f,.75f,0);assert calls==before;
        expect(20000000L,.5f,.75f,.015f,.01f);
        before=calls;send(150000000L,.5f,.75f,0);assert calls==before;
        expect(20000000L,.5f,.75f,.015f,.01f);
        before=calls;send(20000000L,.5f,.75f);assert calls==before;
        expect(20000000L,.5f,.75f,.015f,.01f);
        expect(20000000L,.012217f,-.012217f,0,0);
        expect(20000000L,.012218f,-.012218f,-.012218f*.02f,.012218f*.02f);
        expect(20000000L,.01f,.5f,.01f,0);
        expect(20000000L,-.5f,.01f,0,-.01f);
        System.out.println("PASS: real Java rotation0/90/180/270 signed axes, 0.1s boundary, first/duplicate/backward/dropout/short-vector samples, independent deadzones");

        int unregisters=sensors.unregisters,registers=sensors.registers;
        enabled=false;watch.run();assert sensors.unregisters==unregisters+1 && sensors.listener==null;
        before=calls;send(20000000L,.5f,.75f,0);send(20000000L,.5f,.75f,0);assert calls==before;
        enabled=true;watch.run();assert sensors.registers==registers+1;first();expect(20000000L,.5f,.75f,.015f,.01f);
        activity.focus=false;before=calls;send(20000000L,.5f,.75f,0);assert calls==before;
        int checks=activeCalls;unregisters=sensors.unregisters;watch.run();
        assert activeCalls==checks && sensors.unregisters==unregisters+1;
        activity.focus=true;watch.run();first();expect(20000000L,.5f,.75f,.015f,.01f);
        missingNative=true;unregisters=sensors.unregisters;watch.run();assert sensors.unregisters==unregisters+1;
        missingNative=false;watch.run();assert sensors.listener==listener;
        unregisters=sensors.unregisters;MobileSettings.pause(new GameActivity());
        assert sensors.unregisters==unregisters && handler.pending==watch;
        MobileSettings.pause(activity);assert sensors.unregisters==unregisters+1 && handler.pending==null;
        assert !(Boolean)field("resumed") && !(Boolean)field("gyroListening") && (Long)field("gyroTimestamp")==0;
        before=calls;send(20000000L,.5f,.75f,0);send(20000000L,.5f,.75f,0);assert calls==before;
        checks=activeCalls;watch.run();assert activeCalls==checks && handler.pending==null;
        MobileSettings.apply(activity);assert handler.pending==watch && sensors.listener==listener;
        first();expect(20000000L,.5f,.75f,.015f,.01f);
        MobileSettings.pause(activity);sensors.present=false;registers=sensors.registers;MobileSettings.apply(activity);
        assert sensors.registers==registers && sensors.listener==null;
        sensors.present=true;sensors.registerOk=false;watch.run();assert sensors.registers==registers+1 && sensors.listener==null;
        sensors.registerOk=true;watch.run();assert sensors.registers==registers+2 && sensors.listener==listener;
        MobileSettings.pause(activity);
        System.out.println("PASS: native-active/focus gates, 100ms watch, missing native/sensor/register retry, unregister/pause identity/reset, queued-callback refusal and clean resume");
    }
}
