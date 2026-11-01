"""Compile the actual editor and execute its production validation/storage methods on the host."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'analysis/android/com/epicgames/ue4/MobileControls.java'
SDK = Path(os.environ['ANDROID_SDK_ROOT'])
JAVA = Path(os.environ['JAVA_HOME']) / 'bin'
source = SOURCE.read_text(encoding='utf8')


def method(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


header = re.search(r'private static final int COUNT[^;]+;', source).group()
program = r'''
import java.io.*;
import java.nio.file.*;
import java.util.*;
class Context {
    final File directory;
    Context(File directory) { this.directory=directory; }
    File getExternalFilesDir(String unused) { return directory; }
}
class Log { static int warnings; static void w(String tag,String message,Throwable error) { warnings++; } }
class AtomicFile {
    static boolean fail;
    static int rollbacks;
    final File file,temporary;
    AtomicFile(File file) { this.file=file;temporary=new File(file+".new"); }
    FileInputStream openRead() throws IOException { return new FileInputStream(file); }
    FileOutputStream startWrite() throws IOException {
        return new FileOutputStream(temporary) {
            public void write(byte[] data,int offset,int count) throws IOException {
                if(fail) throw new IOException("injected write failure");
                super.write(data,offset,count);
            }
        };
    }
    void finishWrite(FileOutputStream stream) throws IOException {
        stream.close();Files.move(temporary.toPath(),file.toPath(),StandardCopyOption.REPLACE_EXISTING);
    }
    void failWrite(FileOutputStream stream) throws IOException { stream.close();temporary.delete();rollbacks++; }
}
public class LayoutStorageTest {
    static float[] applied;
    static int applies;
    static boolean nativeApply(float[] data) { assert valid(data);applied=data.clone();applies++;return true; }
''' + header + '\n' + '\n'.join(method(signature) for signature in (
    'private static AtomicFile storage(Context context)', 'static boolean valid(float[] values)',
    'public static void load(Context context)', 'private static void save(Context context, float[] values)',
    'static float labelSize(float radius, float width, float height, float nominal)')) + r'''
    public static void main(String[] args) throws Exception {
        Context context=new Context(new File(args[0]));
        for(float radius:new float[]{4,12,24,40,80,160}) {
            for(float width:new float[]{10,40,160,1000}) {
                for(float height:new float[]{20,40,80}) {
                    float fitted=labelSize(radius,width,height,32),scale=fitted/32;
                    assert fitted>0 && fitted<=32;
                    assert width*scale<=radius*1.35f+.0001f && height*scale<=radius*1.35f+.0001f;
                    assert Math.hypot(width*scale*.5,height*scale*.5)<radius;
                }
            }
        }
        assert labelSize(0,100,100,32)==0;
        float[] data=new float[FLOATS];
        for(int i=0;i<COUNT;i++) { data[i*4]=.5f;data[i*4+1]=.5f;data[i*4+2]=.06f;data[i*4+3]=i%2; }
        assert valid(data) && !valid(null) && !valid(new float[4]);
        float[] bad=data.clone();
        for(float invalid:new float[]{Float.NaN,Float.POSITIVE_INFINITY,-.01f,1.01f}) {
            bad=data.clone();bad[0]=invalid;assert !valid(bad);
        }
        bad=data.clone();bad[2]=.024f;assert !valid(bad);
        bad[2]=.181f;assert !valid(bad);
        bad=data.clone();bad[3]=.5f;assert !valid(bad);
        load(context);assert applies==0;
        save(context,data);assert applies==1 && Arrays.equals(applied,data);
        File file=new File(context.directory,"re4_touch_layout_v1");
        byte[] original=Files.readAllBytes(file.toPath());assert original.length==4+FLOATS*4;
        applied=null;load(context);assert applies==2 && Arrays.equals(applied,data);
        AtomicFile.fail=true;
        try { save(context,data);throw new AssertionError("write failure not propagated"); } catch(IOException expected) {}
        AtomicFile.fail=false;assert AtomicFile.rollbacks==1 && Arrays.equals(original,Files.readAllBytes(file.toPath()));
        for(int fault=0;fault<4;fault++) {
            byte[] corrupt=original.clone();
            if(fault==0)corrupt[0]^=1;
            if(fault==1)corrupt=Arrays.copyOf(corrupt,corrupt.length-1);
            if(fault==2)corrupt=Arrays.copyOf(corrupt,corrupt.length+1);
            if(fault==3) { corrupt[4]=0x7f;corrupt[5]=(byte)0xc0;corrupt[6]=corrupt[7]=0; }
            Files.write(file.toPath(),corrupt);int before=applies;load(context);
            assert applies==before && Arrays.equals(applied,data);
        }
        assert Log.warnings==4;
        try { save(new Context(null),data);throw new AssertionError(); } catch(IOException expected) {}
        System.out.println("PASS: actual Java layout validation, exact binary roundtrip, absent/corrupt/truncated/trailing/NaN rejection, atomic failure rollback, unavailable storage and measured preview text fits inside circles");
    }
}
'''

with tempfile.TemporaryDirectory(prefix='re4-touch-layout-') as temporary:
    work = Path(temporary)
    classes = work / 'classes'
    classes.mkdir()
    stub = work / 'MobileAndroid.java'
    stub.write_text('package com.epicgames.ue4; public final class MobileAndroid { public static java.util.Locale startupLocale(android.content.Context c) { return java.util.Locale.ENGLISH; } }', encoding='utf8')
    subprocess.run([str(JAVA/'javac.exe'), '-encoding', 'UTF-8', '-source', '8', '-target', '8',
                    '-classpath', str(SDK/'platforms/android-36/android.jar'), '-d', str(classes), str(stub), str(SOURCE)], check=True)
    print('PASS: actual MobileControls.java compiles against Android 36 / Java 8', flush=True)
    fixture = work / 'LayoutStorageTest.java'
    fixture.write_text(program, encoding='utf8')
    subprocess.run([str(JAVA/'javac.exe'), '-encoding', 'UTF-8', '-d', str(classes), str(fixture)], check=True)
    data = work / 'storage'
    data.mkdir()
    subprocess.run([str(JAVA/'java.exe'), '-ea', '-classpath', str(classes), 'LayoutStorageTest', str(data)], check=True)
