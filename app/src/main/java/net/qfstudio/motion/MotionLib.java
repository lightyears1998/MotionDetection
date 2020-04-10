package net.qfstudio.motion;

import android.content.res.AssetManager;
import android.os.Looper;

public class MotionLib {
    static {
        System.loadLibrary("motion-lib");
    }

    private boolean isInitialized = false;

    MotionLib(final AssetManager assetManager) {
        try {
            final Object lock = new Object();

            new Thread() {
                @Override
                public void run() {
                    Looper.prepare();

                    synchronized (lock) {
                        initUnderlyingNativeCode(assetManager);
                        MotionLib.this.isInitialized = true;
                        lock.notifyAll();
                    }

                    Looper.loop();
                }
            }.start();

            synchronized (lock) {
                while (!this.isInitialized) {
                    lock.wait();
                }
            }
        } catch (InterruptedException ignored) {
        }
    }

    private native void initUnderlyingNativeCode(AssetManager assetManager);

    public native void resume();

    public native void pause();

    public native void update();

    public native float[] getLastMeterValue();

    public native String getLastDirection();

    public native String getLastMovement();

    public native String getLastGesture();
}
