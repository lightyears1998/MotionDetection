package net.qfstudio.motion;

public class MotionLibJNI {
    static {
        System.loadLibrary("motion-lib");
    }

    public static native void init();

    public static native void resume();

    public static native void pause();

    public static native void update();

    public static native float[] getLastMeterValue();

    public static native String getLastDirection();

    public static native String getLastMovement();

    public static native String getLastGesture();
}
