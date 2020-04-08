package net.qfstudio.motion;

public class SensorHandlerJNI {
    static {
        System.loadLibrary("native-lib");
    }

    public static native void init();

    public static native void onResume();

    public static native void onPause();

    public static native void capture();

    public static native float[] getLastAcceleration();

    public static native String getLastMicroState();

    public static native String getLastMovement();

    public static native String getLastGesture();
}
