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

    private Timer timer;
    private TextView sensorValueTextView;
    private TextView meterDirectionTextView;
    private TextView movementTextView;
    private TextView gestureTextView;

    private String lastMicroState;
    private String lastMovement;
    private String lastGesture;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        MotionLibJNI.init(getAssets());
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

    private void setTimerTask() {
        this.timer = new Timer();
        this.timer.schedule(new TimerTask() {
            @Override
            public void run() {
//                MotionLibJNI.update();

                float[] acceleration = MotionLibJNI.getLastMeterValue();
                final String accelerationStr = String.format(
                        Locale.getDefault(),
                        "x: %f\ny: %f\nz: %f\n",
                        acceleration[0], acceleration[1], acceleration[2]
                );

                final String currentMicroState = MotionLibJNI.getLastDirection();
                final String currentMovement = MotionLibJNI.getLastMovement();
                final String currentGesture = MotionLibJNI.getLastGesture();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        sensorValueTextView.setText(accelerationStr);
                        if (!currentMicroState.equals(lastMicroState)) {
                            meterDirectionTextView.append(" " + currentMicroState);
                            lastMicroState = currentMicroState;
                        }
                        if (!currentMovement.equals(lastMovement)) {
                            movementTextView.append(" " + currentMovement);
                            lastMovement = currentMovement;
                        }
                        if (!currentGesture.equals(lastGesture)) {
                            gestureTextView.append(" " + currentGesture);
                            lastGesture = currentGesture;
                        }
                    }
                });
            }
        }, 0, 20);
    }

    private void cancelTimerTask() {
        if (this.timer != null) {
            this.timer.cancel();
            this.timer = null;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        MotionLibJNI.resume();
        this.setTimerTask();
    }

    @Override
    protected void onPause() {
        super.onPause();

        MotionLibJNI.pause();
        this.cancelTimerTask();
    }

    void clearScreen() {
        this.meterDirectionTextView.setText(R.string.txtMeterDirection);
        this.movementTextView.setText(R.string.txtMovement);
        this.gestureTextView.setText(R.string.txtGesture);
    }
}
