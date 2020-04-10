package net.qfstudio.motion;

import android.content.res.AssetManager;
import android.os.Looper;


public class MotionLib {
    static {
        System.loadLibrary("motion-lib");
    }

    private boolean isInitialized = false;
    private MotionLibEventHandler handler;

    public MotionLib(final AssetManager assetManager) {
        try {
            new Thread() {
                @Override
                public void run() {
                    Looper.prepare();

                    synchronized (MotionLib.this) {
                        initUnderlyingNativeLib(assetManager);
                        MotionLib.this.isInitialized = true;
                        MotionLib.this.notifyAll();
                    }

                    Looper.loop();
                }
            }.start();

            synchronized (this) {
                while (!this.isInitialized) {
                    this.wait();
                }
            }
        } catch (InterruptedException ignored) {
        }
    }

    public void setHandler(MotionLibEventHandler handler) {
        this.handler = handler;
    }

    private native void initUnderlyingNativeLib(AssetManager assetManager);

    public native void resume();

    public native void pause();

    public native void update();

    public native float[] getLastMeterReadings();

    public native String getLastDirection();

    public native String getLastMovement();

    public native String getLastGesture();

    private void handleDirectionChange(String direction) {
        if (this.handler != null) {
            handler.onDirectionChanged(direction);
        }
    }

    private void handleMovementDetected(String movement) {
        if (this.handler != null) {
            handler.onMovementDetected(movement);
        }
    }

    private void handleGestureDetected(String gestureName) {
        if (this.handler != null) {
            handler.onGestureDetected(gestureName);
        }
    }
}
