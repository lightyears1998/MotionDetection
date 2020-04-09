package net.qfstudio.motion;

import android.content.res.AssetManager;

public class MotionLibJNI {
    static {
        System.loadLibrary("motion-lib");
    }

    static void init(final AssetManager assetManager) {
        // TODO 添加同步机制
        new Thread() {
            @Override
            public void run() {
                super.run();
                initNativeLib(assetManager);
            }
        }.start();
    }

    private static native void initNativeLib(AssetManager assetManager);

    public static native void resume();

    public static native void pause();

    public static native void update();

    public static native float[] getLastMeterValue();

    public static native String getLastDirection();

    public static native String getLastMovement();

    public static native String getLastGesture();
}
