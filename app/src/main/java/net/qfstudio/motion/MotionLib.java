package net.qfstudio.motion;

import android.content.res.AssetManager;
import android.os.Looper;


public class MotionLib {
    static {
        System.loadLibrary("motion-lib");
    }

    private boolean isInitialized = false;
    private MotionLibEventHandler handler;

    MotionLib(final AssetManager assetManager) {
        try {
            final Object lock = new Object();

            new Thread() {
                @Override
                public void run() {
                    Looper.prepare();

                    synchronized (lock) {
                        initUnderlyingNativeLib(assetManager);
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

    void setHandler(MotionLibEventHandler handler) {
        this.handler = handler;
    }

    private native void initUnderlyingNativeLib(AssetManager assetManager);

    public native void resume();

    public native void pause();

    public native void update();

    public native float[] getLastMeterValue();

    public native String getLastDirection();

    public native String getLastMovement();

    public native String getLastGesture();

    private void handleDirectionChange(String direction) {
        if (this.handler != null) {
            handler.onDirectionChanged(direction);
        }
        System.out.println("Having fun wiht you! a");
    }

    private void handleMovementDetected(String movement) {
        if (this.handler != null) {
            handler.onMovementDetected(movement);
        }
        System.out.println("Having fun wiht you! b");
    }

    private void handleGestureDetected(String gestureName) {
        if (this.handler != null) {
            handler.onGestureDetected(gestureName);
        }
        System.out.println("Having fun wiht you! c");
    }
}
