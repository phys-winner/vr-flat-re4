"""Execute production pointer state/action construction with Android-only calls doubled."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JDK = Path(os.environ["JAVA_HOME"]) / "bin"
sources = {
    "android/os/SystemClock.java": "package android.os; public class SystemClock {static long now=1000;public static long uptimeMillis(){return ++now;}}",
    "android/view/InputEvent.java": "package android.view; public class InputEvent {}",
    "android/view/InputDevice.java": """package android.view; public class InputDevice {
        public static final int SOURCE_TOUCHSCREEN=4098;
        public static int[] getDeviceIds(){return new int[]{3,8};} public static InputDevice getDevice(int id){return id==8?new InputDevice():null;}
        public boolean supportsSource(int source){return source==SOURCE_TOUCHSCREEN;}
    }""",
    "android/view/MotionEvent.java": """package android.view; public class MotionEvent extends InputEvent {
        public static final int ACTION_DOWN=0,ACTION_UP=1,ACTION_MOVE=2,ACTION_CANCEL=3,ACTION_POINTER_DOWN=5,ACTION_POINTER_UP=6,ACTION_POINTER_INDEX_SHIFT=8,TOOL_TYPE_FINGER=1;
        public static class PointerProperties { public int id,toolType; }
        public static class PointerCoords { public float x,y,size,pressure; }
        public long down,time;public int action,count,device,source,display;public PointerProperties[] properties;public PointerCoords[] coordinates;
        public static int created,recycled;
        public static MotionEvent obtain(long down,long time,int action,int count,PointerProperties[] properties,PointerCoords[] coordinates,int meta,int button,float xp,float yp,int device,int edge,int source,int flags){
            assert count>0&&properties.length==count&&coordinates.length==count&&meta==0&&button==0&&xp==1&&yp==1&&edge==0&&flags==0;
            MotionEvent event=new MotionEvent();event.down=down;event.time=time;event.action=action;event.count=count;event.properties=properties;event.coordinates=coordinates;event.device=device;event.source=source;++created;return event;
        }
        public void setDisplayId(int id){display=id;}public void recycle(){++recycled;}
    }""",
    "android/hardware/input/InputManagerGlobal.java": """package android.hardware.input; public class InputManagerGlobal {
        public static final InputManagerGlobal instance=new InputManagerGlobal();public static InputManagerGlobal getInstance(){return instance;}
        public int count,fail=-1;public final java.util.List<android.view.MotionEvent> events=new java.util.ArrayList<android.view.MotionEvent>();
        public boolean injectInputEvent(android.view.InputEvent raw,int mode){
            android.view.MotionEvent event=(android.view.MotionEvent)raw;assert mode==2&&event.source==4098&&event.device==8&&event.display==0&&event.down<=event.time;
            for(int i=0;i<event.count;i++)assert event.properties[i].toolType==1&&event.coordinates[i].size==1&&event.coordinates[i].pressure==((event.action&255)==3?0:1);
            events.add(event);return count++!=fail;
        }
    }""",
    "TouchInjectTest.java": """public class TouchInjectTest {
        static String[] args(String... steps){String[] out=new String[5+steps.length];String[] prefix={"--display","0","--size","2400","1080"};System.arraycopy(prefix,0,out,0,5);System.arraycopy(steps,0,out,5,steps.length);return out;}
        public static void main(String[] argv)throws Exception{
            android.hardware.input.InputManagerGlobal manager=android.hardware.input.InputManagerGlobal.instance;
            RE4TouchInject.main(args("down:0,200,300","down:1,1700,650","down:2,1200,500","move:2,1400,450","up:0","up:2","up:1","down:9,0,0","cancel"));
            int[] actions={0,261,517,2,6,262,1,0,3};int[][] ids={{0},{0,1},{0,1,2},{0,1,2},{0,1,2},{1,2},{1},{9},{9}};
            assert manager.count==actions.length;long last=0,down=0;
            for(int i=0;i<actions.length;i++){
                android.view.MotionEvent e=manager.events.get(i);assert e.action==actions[i]&&e.count==ids[i].length&&e.time>=last;last=e.time;
                if((e.action&255)==0)down=e.down;assert e.down==down;
                for(int j=0;j<e.count;j++)assert e.properties[j].id==ids[i][j];
            }
            manager.events.clear();manager.count=0;RE4TouchInject.main(args("down:1,100,200"));
            assert manager.count==2&&manager.events.get(1).action==3;
            manager.events.clear();manager.count=0;manager.fail=1;
            try{RE4TouchInject.main(args("down:0,100,200","down:1,200,300"));throw new AssertionError();}catch(IllegalStateException expected){}
            assert manager.count==3&&manager.events.get(2).action==3&&manager.events.get(2).count==2;
            assert android.view.MotionEvent.created==android.view.MotionEvent.recycled;
            System.out.println("PASS: actual injector 3-pointer IDs/action indices, middle-pointer removal, shared downTime/monotonic events, display/source/device/pressure, event recycling, automatic CANCEL and injection-failure cleanup");
        }
    }""",
}
with tempfile.TemporaryDirectory(prefix="re4-touch-injector-test-") as temporary:
    work = Path(temporary)
    paths = []
    for name, body in sources.items():
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(body, encoding="utf8")
        paths.append(str(target))
    paths.append(str(ROOT / "analysis/research/RE4TouchInject.java"))
    subprocess.run([str(JDK / "javac.exe"), "--release", "8", "-encoding", "UTF-8", "-d", str(work), *paths], check=True)
    subprocess.run([str(JDK / "java.exe"), "-ea", "-cp", str(work), "TouchInjectTest"], check=True)
