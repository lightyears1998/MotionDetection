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
    private TextView tvSensorValue;
    private TextView tvMicroStateText;
    private TextView tvMovement;
    private TextView tvRecognitionResult;
    private Button btnClearScreen;

    private String lastMicroState;
    private String lastMovement;
    private String lastGesture;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SensorHandlerJNI.init();
        this.tvSensorValue = findViewById(R.id.sensorText);
        this.tvMicroStateText = findViewById(R.id.microStateText);
        this.tvMovement = findViewById(R.id.movementText);
        this.tvRecognitionResult = findViewById(R.id.recognitionText);
        this.btnClearScreen = findViewById(R.id.btnClearScreen);
        this.btnClearScreen.setOnClickListener(new View.OnClickListener() {
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
                float[] acceleration = SensorHandlerJNI.getLastAcceleration();
                final String accelerationStr = String.format(
                        Locale.getDefault(),
                        "x: %f\ny: %f\nz: %f\n",
                        acceleration[0], acceleration[1], acceleration[2]
                );

                final String currentMicroState = SensorHandlerJNI.getLastMicroState();
                final String currentMovement = SensorHandlerJNI.getLastMovement();
                final String currentGesture = SensorHandlerJNI.getLastGesture();

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        tvSensorValue.setText(accelerationStr);
                        if (!currentMicroState.equals(lastMicroState)) {
                            tvMicroStateText.append(" " + currentMicroState);
                            lastMicroState = currentMicroState;
                        }
                        if (!currentMovement.equals(lastMovement)) {
                            tvMovement.append(" " + currentMovement);
                            lastMovement = currentMovement;
                        }
                        if (!currentGesture.equals(lastGesture)) {
                            tvRecognitionResult.append(" " + currentGesture);
                            lastGesture = currentGesture;
                        }

                        SensorHandlerJNI.capture();
                    }
                });
            }
        }, 0, 100);
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

        SensorHandlerJNI.onResume();
        this.setTimerTask();
    }

    @Override
    protected void onPause() {
        super.onPause();

        SensorHandlerJNI.onPause();
        this.cancelTimerTask();
    }

    void clearScreen() {
        this.tvMicroStateText.setText(R.string.txtMicroState);
        this.tvMovement.setText(R.string.txtMovement);
        this.tvRecognitionResult.setText(R.string.txtGesture);
    }
}
