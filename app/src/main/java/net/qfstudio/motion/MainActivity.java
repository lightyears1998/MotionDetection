package net.qfstudio.motion;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

import androidx.appcompat.app.AppCompatActivity;


public class MainActivity extends AppCompatActivity {

    private MotionLib motion;

    private Timer updateTimer;
    private TextView sensorValueTextView;
    private TextView meterDirectionTextView;
    private TextView movementTextView;
    private TextView gestureTextView;

    private MotionLibEventHandler motionHandler = new MotionLibEventHandler() {
        int directionCount = 0;
        int movementCount = 0;
        int gestureCount = 0;

        @Override
        public void onDirectionChanged(final String direction) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s", directionCount++, direction);
                    meterDirectionTextView.append(text);
                }
            });
        }

        @Override
        public void onMovementDetected(final String movement) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s", movementCount++, movement);
                    movementTextView.append(text);
                }
            });
        }

        @Override
        public void onGestureDetected(final String gestureName) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s", gestureCount++, gestureName);
                    gestureTextView.append(text);
                }
            });
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        this.motion = new MotionLib(getAssets());
        this.motion.setHandler(this.motionHandler);
        this.sensorValueTextView = findViewById(R.id.sensorText);
        this.meterDirectionTextView = findViewById(R.id.meterDirectionText);
        this.movementTextView = findViewById(R.id.movementText);
        this.gestureTextView = findViewById(R.id.gestureText);
        Button clearScreenButton = findViewById(R.id.btnClearScreen);
        clearScreenButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                clearScreen();
            }
        });
    }

    private void setMeterUpdateTimer() {
        this.updateTimer = new Timer();
        this.updateTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                float[] acceleration = motion.getLastMeterValue();
                final String accelerationStr = String.format(
                        Locale.getDefault(),
                        "x: %f\ny: %f\nz: %f\n",
                        acceleration[0], acceleration[1], acceleration[2]
                );

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        sensorValueTextView.setText(accelerationStr);
                    }
                });
            }
        }, 0, 40);
    }

    private void cancelMeterUpdateTimer() {
        if (this.updateTimer != null) {
            this.updateTimer.cancel();
            this.updateTimer = null;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        motion.resume();
        this.setMeterUpdateTimer();
    }

    @Override
    protected void onPause() {
        super.onPause();

        motion.pause();
        this.cancelMeterUpdateTimer();
    }

    void clearScreen() {
        this.meterDirectionTextView.setText(R.string.txtMeterDirection);
        this.movementTextView.setText(R.string.txtMovement);
        this.gestureTextView.setText(R.string.txtGesture);
    }
}
