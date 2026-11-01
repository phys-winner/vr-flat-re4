package com.epicgames.ue4;

import android.os.Build;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import java.lang.reflect.Field;
import java.util.Arrays;

/** Exercises actual production MobileSettings methods; JNI replies and platform objects are counted seams. */
public final class MobileVideoTest {
    public static int frameLimit=60000,renderPercent=100,displayHz,density,modeReports;
    public static long displayModes;
    public static int viewportReports;
    public static int[] viewportSizes;
    private static final GameActivity activity=new GameActivity();

    public static void viewport(int viewWidth,int viewHeight,int renderWidth,int renderHeight) {
        viewportReports++;viewportSizes=new int[]{viewWidth,viewHeight,renderWidth,renderHeight};
    }

    private static Object field(String name) throws Exception {
        Field value=MobileSettings.class.getDeclaredField(name);value.setAccessible(true);return value.get(null);
    }
    private static void expected(SurfaceView view,int inputWidth,int inputHeight,int width,int height) {
        int viewWidth=inputWidth,viewHeight=inputHeight;
        if(view!=null && view.getWidth()>0 && view.getHeight()>0) {viewWidth=view.getWidth();viewHeight=view.getHeight();}
        int before=viewportReports;int[] previous=viewportSizes;
        int[] actual=MobileSettings.surfaceSize(activity,view,inputWidth,inputHeight);
        assert Arrays.equals(actual,new int[]{width,height}) : Arrays.toString(actual)+" expected "+width+"x"+height;
        if(viewWidth<32 || viewHeight<32) {
            assert viewportReports==before && viewportSizes==previous : "invalid extent reached native mapping";
        } else {
            assert viewportReports==before+1;
            assert Arrays.equals(viewportSizes,new int[]{viewWidth,viewHeight,width,height}) : Arrays.toString(viewportSizes);
        }
    }
    private static void surfaceSizes() {
        SurfaceView view=new SurfaceView();
        int[][] cases={{50,1200,536},{67,1608,720},{75,1800,808},{85,2040,912},{100,2400,1080}};
        for(int[] test:cases) {
            renderPercent=test[0];expected(view,2400,1080,test[1],test[2]);
            // surfaceChanged can report the previous buffer extent; the physical view stays authoritative.
            for(int i=0;i<5;i++)expected(view,test[1],test[2],test[1],test[2]);
            assert Math.abs(test[1]*1080-test[2]*2400)<=8*(2400+1080) : "exceeds eight-pixel alignment budget";
            assert ((test[1]+3)&~3)==test[1] && ((test[2]+3)&~3)==test[2] : "native framebuffer exceeds image";
            assert (test[1]&~7)==test[1] && (test[2]&~7)==test[2] : "native window extent differs";
        }
        view.width=view.height=0;
        renderPercent=50;expected(view,2400,1080,1200,536);expected(null,2400,1080,1200,536);
        expected(view,0,1080,0,1080);expected(view,2400,0,2400,0);expected(view,31,1080,31,1080);
        expected(view,-1,1080,-1,1080);expected(view,32,32,32,32);
        for(int bad:new int[]{0,49,51,66,68,74,76,84,86,99,101,-1,Integer.MAX_VALUE}) {
            renderPercent=bad;expected(null,2401,1081,2400,1080);
        }
        renderPercent=75;view.width=2167;view.height=1080;expected(view,1600,800,1624,808);
        int[][] odd={{50,1200,536},{67,1616,720},{75,1808,808},{85,2048,912},{100,2408,1080}};
        for(int[] test:odd) { renderPercent=test[0];expected(null,2414,1080,test[1],test[2]); }
        for(int[] physical:new int[][]{{2401,1081},{1337,751},{1080,2414},{32,32}}) {
            for(int percent:new int[]{50,67,75,85,100}) {
                renderPercent=percent;
                int[] size=MobileSettings.surfaceSize(activity,null,physical[0],physical[1]);
                assert size[0]>=32 && size[1]>=32 && size[0]<=physical[0] && size[1]<=physical[1];
                assert ((size[0]+3)&~3)==size[0] && ((size[1]+3)&~3)==size[1] : "native framebuffer exceeds image";
                assert (size[0]&~7)==size[0] && (size[1]&~7)==size[1] : "native window extent differs";
                assert Math.abs((long)size[0]*physical[1]-(long)size[1]*physical[0])<=8L*(physical[0]+physical[1]);
            }
        }
        renderPercent=100;
        System.out.println("PASS: actual surfaceSize 50/67/75/85/100%, native scene up4 and window down8 quantization are identities, bounded aspect rounding, authoritative view prevents compounding, physical/render JNI mapping, odd/portrait/minimum extents, invalid extent/percent and legacy viewport");
    }
    private static void frameRates() throws Exception {
        Display display=activity.window.display;
        Display.Mode sixty=new Display.Mode(10,2400,1080,60);
        Display.Mode high=new Display.Mode(20,2400,1080,119.88f);
        display.mode=sixty;
        display.modes=new Display.Mode[]{new Display.Mode(90,1920,1080,144),high,sixty,
            new Display.Mode(30,2400,1080,120),new Display.Mode(91,2400,1080,23),new Display.Mode(92,2400,1080,241)};
        SurfaceView view=activity.appWindow.decor.surface;
        Surface surface=view.holder.surface;
        Runnable watch=(Runnable)field("gyroWatch");
        int before=surface.requests;
        for(int cap:new int[]{30000,45000,60000,120000}) {
            frameLimit=cap;int attributes=activity.appWindow.attributes;
            MobileSettings.apply(activity);
            assert surface.requests==++before && surface.rate==cap/1000f;
            int mode=activity.appWindow.getAttributes().preferredDisplayModeId;
            assert mode==(cap==120000?20:10) : "mode "+mode+" at "+cap;
            assert activity.appWindow.attributes-attributes==(cap==30000||cap==120000?1:0);
            assert displayModes==(60L|(120L<<8)) : Long.toHexString(displayModes);
            assert displayHz==60000 && density==2000;
            attributes=activity.appWindow.attributes;
            for(int i=0;i<5;i++) {watch.run();MobileSettings.apply(activity);activity.displays.changed(0);}
            assert surface.requests==before && activity.appWindow.attributes==attributes;
            assert activity.displays.registers==1;
        }
        int reports=modeReports;activity.displays.changed(7);assert modeReports==reports;
        for(int invalid:new int[]{0,23999,240001,-1}) {
            frameLimit=invalid;int attributes=activity.appWindow.attributes;
            MobileSettings.apply(activity);watch.run();
            assert surface.requests==before && activity.appWindow.attributes==attributes;
        }
        // An invalid or not-yet-created surface is retried by the existing periodic watch.
        MobileSettings.pause(activity);surface.valid=false;frameLimit=45000;MobileSettings.apply(activity);
        assert surface.requests==before && (Integer)field("appliedFrameLimit")==0;
        surface.valid=true;watch.run();assert surface.requests==++before && surface.rate==45;
        MobileSettings.pause(activity);activity.appWindow.decor.children=new View[0];frameLimit=30000;MobileSettings.apply(activity);
        assert surface.requests==before && (Integer)field("appliedFrameLimit")==0;
        ViewGroup nested=new ViewGroup();nested.children=new View[]{view};
        activity.appWindow.decor.children=new View[]{new View(),nested};watch.run();assert surface.requests==++before;
        // API 23..29 can select a display mode without calling the API30 Surface method.
        MobileSettings.pause(activity);Build.VERSION.SDK_INT=29;frameLimit=120000;MobileSettings.apply(activity);
        assert surface.requests==before && activity.appWindow.getAttributes().preferredDisplayModeId==20;
        watch.run();assert surface.requests==before && (Integer)field("appliedFrameLimit")==120000;
        MobileSettings.pause(activity);Build.VERSION.SDK_INT=30;
        System.out.println("PASS: actual 30/45/60/120 FPS chooses same-resolution 60/120Hz modes, native mode packing, foreign/duplicate/invalid rate filtering, no repeated setter/callback loop, delayed/nested surface retry and API29 guard");
    }
    public static void main(String[] args) throws Exception { surfaceSizes();frameRates(); }
}
