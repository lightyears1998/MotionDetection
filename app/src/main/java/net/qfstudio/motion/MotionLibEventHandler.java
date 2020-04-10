package net.qfstudio.motion;

public interface MotionLibEventHandler {
    void onDirectionChanged(String direction);

    void onMovementDetected(String movement);

    void onGestureDetected(String gestureName);
}
