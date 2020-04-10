package net.qfstudio.motion.demo;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import net.qfstudio.motion.MotionLib;
import net.qfstudio.motion.MotionLibEventHandler;
import net.qfstudio.motion.R;

import java.util.Locale;
import java.util.Timer;
import java.util.TimerTask;

import androidx.appcompat.app.AppCompatActivity;


public class MainActivity extends AppCompatActivity {

    private MotionLib motion;

    private Timer refreshTimer;
    private TextView meterReadingsTextView;
    private TextView accelerationDirectionTextView;
    private TextView movementDirectionTextView;
    private TextView gestureTextView;

    private MotionLibEventHandler motionHandler = new MotionLibEventHandler() {
        int receivedAccelerationDirectionCount = 0;
        int receivedMovementDirectionCount = 0;
        int receivedGestureCount = 0;

        @Override
        public void onDirectionChanged(final String direction) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s",
                            receivedAccelerationDirectionCount++,
                            direction);
                    accelerationDirectionTextView.append(text);
                }
            });
        }

        @Override
        public void onMovementDetected(final String movement) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s",
                            receivedMovementDirectionCount++,
                            movement);
                    movementDirectionTextView.append(text);
                }
            });
        }

        @Override
        public void onGestureDetected(final String gestureName) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String text = String.format(Locale.getDefault(), " %d:%s",
                            receivedGestureCount++,
                            gestureName);
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
        this.meterReadingsTextView = findViewById(R.id.txtMeterReadings);
        this.accelerationDirectionTextView = findViewById(R.id.txtAccelerationDirections);
        this.movementDirectionTextView = findViewById(R.id.txtMovementDirections);
        this.gestureTextView = findViewById(R.id.txtGestureNames);
        Button clearScreenButton = findViewById(R.id.btnClearScreen);
        clearScreenButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                clearScreen();
            }
        });
    }

    private void setupRefreshTimer() {
        this.refreshTimer = new Timer();
        this.refreshTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                float[] readings = motion.getLastMeterReadings();
                final String text = String.format(
                        Locale.getDefault(),
                        "x: %+f\ny: %+f\nz: %+f\n",
                        readings[0], readings[1], readings[2]
                );

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        meterReadingsTextView.setText(text);
                    }
                });
            }
        }, 0, 40);
    }

    private void cancelMeterUpdateTimer() {
        if (this.refreshTimer != null) {
            this.refreshTimer.cancel();
            this.refreshTimer = null;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        this.motion.resume();
        this.setupRefreshTimer();
    }

    @Override
    protected void onPause() {
        super.onPause();

        this.motion.pause();
        this.cancelMeterUpdateTimer();
    }

    void clearScreen() {
        this.accelerationDirectionTextView.setText(R.string.txtMeterDirection);
        this.movementDirectionTextView.setText(R.string.txtMovement);
        this.gestureTextView.setText(R.string.txtGesture);
    }
}
