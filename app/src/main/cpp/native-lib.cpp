#include <jni.h>
#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/sensor.h>
#include <string>
#include <mutex>
#include <vector>
#include <algorithm>

#define PACKAGE_NAME "net.qfstudio.motion"
#define LOG_TAG "native-lib"
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using std::string;
using std::to_string;
using std::vector;
using std::min;

const int LOOPER_ID = 2;
const int SENSOR_HISTORY_LENGTH = 100;
const int SENSOR_REFRESH_RATE_HZ = 100;
constexpr int32_t SENSOR_REFRESH_PERIOD_US = int32_t(1000000 / SENSOR_REFRESH_RATE_HZ);
const float SENSOR_FILTER_ALPHA = 0.1f;

struct AccelerometerData {
    float x;
    float y;
    float z;
};

enum struct MicroState {
    STAY_STILL = 0,
    RIGHT, LEFT, UP, DOWN, FORWARD, BACKWARD
};

// 实际采样频率为每秒10次。
struct MicroStateData {
    MicroState state;
    int during = 1;
    bool isProcessed = false;
};

enum struct Movement {
    STAY_STILL = 0,
    MOVE_RIGHT, MOVE_LEFT,
    MOVE_UP, MOVE_DOWN,
    MOVE_FORWARD, MOVE_BACKWARD,
};

struct Gesture {
    string name;
    vector<Movement> movements;
};

int currentGestureId = 0;
string currentGestureName = "静止";

class SensorHandler {
    ASensorManager *sensorManager;
    const ASensor *accelerometer;
    ALooper *looper;
    ASensorEventQueue *accelerometerEventQueue;
    vector<Gesture> registeredGestures;

public:
    AccelerometerData data[SENSOR_HISTORY_LENGTH];
    AccelerometerData dataFilter = {0, 0, 0};
    int dataIndex = 0;

    MicroStateData msData[SENSOR_HISTORY_LENGTH] = {{MicroState::STAY_STILL, 10, false}};
    int msIndex = 1;

    Movement movements[SENSOR_HISTORY_LENGTH] = {Movement::STAY_STILL};
    bool movementIsProcess[SENSOR_HISTORY_LENGTH] = {false};
    int movementCount = 1;
    int movementIndex = 1;

public:
    inline int prevIndex(int index) {
        return (index - 1 + SENSOR_HISTORY_LENGTH) % SENSOR_HISTORY_LENGTH;
    }

    inline int nextIndex(int index) {
        return (index + 1) % SENSOR_HISTORY_LENGTH;
    }

    void registerGesture(const string &name, const vector<Movement> &movements) {
        registeredGestures.push_back({name, movements});
    }

public:
    SensorHandler() {
        registerGesture("数字1", {
                Movement::MOVE_DOWN, Movement::MOVE_DOWN
        });
        registerGesture("数字2", {
                Movement::MOVE_RIGHT, Movement::MOVE_DOWN, Movement::MOVE_LEFT,
                Movement::MOVE_DOWN, Movement::MOVE_RIGHT
        });
        registerGesture("数字3", {
                Movement::MOVE_RIGHT, Movement::MOVE_DOWN,
                Movement::MOVE_LEFT, Movement::MOVE_RIGHT,
                Movement::MOVE_DOWN, Movement::MOVE_LEFT
        });
    }

    void init() {
        sensorManager = ASensorManager_getInstanceForPackage(PACKAGE_NAME);
        assert(sensorManager != NULL);
        accelerometer = ASensorManager_getDefaultSensor(sensorManager,
                                                        ASENSOR_TYPE_LINEAR_ACCELERATION);
        assert(accelerometer != NULL);
        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        assert(looper != NULL);
        accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                                  LOOPER_ID, NULL, NULL);
        assert(accelerometerEventQueue != NULL);
        auto status = ASensorEventQueue_enableSensor(accelerometerEventQueue,
                                                     accelerometer);
        assert(status >= 0);
        status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                accelerometer,
                                                SENSOR_REFRESH_PERIOD_US);
        assert(status >= 0);
    }

    void pause() {
        ASensorEventQueue_disableSensor(accelerometerEventQueue, accelerometer);
    }

    void resume() {
        ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
        auto status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                     accelerometer,
                                                     SENSOR_REFRESH_PERIOD_US);
        assert(status >= 0);
    }

    MicroState lastMicroState() {
        return msData[prevIndex(msIndex)].state;
    }

    void commitMicroState(MicroState state) {
        if (state != lastMicroState()) {
            msData[msIndex] = {state, 1, false};
            msIndex = nextIndex(msIndex);
        } else {
            int prev = prevIndex(msIndex);
            msData[prev] = {state, min(msData[prev].during + 1, 10), false};
        }
    }

    void capture() {
        ALooper_pollAll(0, NULL, NULL, NULL);
        ASensorEvent event;
        while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
            float a = SENSOR_FILTER_ALPHA;
            dataFilter.x = a * event.acceleration.x + (1.0f - a) * dataFilter.x;
            dataFilter.y = a * event.acceleration.y + (1.0f - a) * dataFilter.y;
            dataFilter.z = a * event.acceleration.z + (1.0f - a) * dataFilter.z;
        }
        data[dataIndex] = dataFilter;
        dataIndex = (dataIndex + 1) % SENSOR_HISTORY_LENGTH;

        if (isStill(dataFilter.x) && isStill(dataFilter.y) && isStill(dataFilter.z)) {
            commitMicroState(MicroState::STAY_STILL);
        } else if (isNegative(dataFilter.x)) {
            commitMicroState(MicroState::LEFT);
        } else if (isPositive(dataFilter.x)) {
            commitMicroState(MicroState::RIGHT);
        } else if (isNegative(dataFilter.y)) {
            commitMicroState(MicroState::BACKWARD);
        } else if (isPositive(dataFilter.y)) {
            commitMicroState(MicroState::FORWARD);
        } else if (isNegative(dataFilter.z)) {
            commitMicroState(MicroState::DOWN);
        } else if (isPositive(dataFilter.z)) {
            commitMicroState(MicroState::UP);
        }

        detectMovement();
    }

    void commitMovement(Movement move) {
        movements[movementIndex] = move;
        movementIsProcess[movementIndex] = false;
        movementIndex = nextIndex(movementIndex);
        movementCount++;
    }

    inline bool isPositive(float ac) {
        return ac > 2;
    }

    inline bool isNegative(float ac) {
        return ac < -2;
    }

    // 静止判断条件。
    inline bool isStill(float ac) {
        return !isPositive(ac) && !isNegative(ac);
    }

    // 假定移动方向总是平行于立方体的棱。
    void detectMovement() {
        auto lastDataIndex = prevIndex(dataIndex);
        auto lastData = data[lastDataIndex];
        auto lastMicroState = lastMicroStateData();

        int currentMsIndex = prevIndex(prevIndex(msIndex));
        if (lastMicroState.state == MicroState::STAY_STILL && lastMicroState.during >= 3) {
            MicroStateData firstMicroState = msData[currentMsIndex];
            while (!(msData[currentMsIndex].state == MicroState::STAY_STILL &&
                     msData[currentMsIndex].during >= 3)) {
                firstMicroState = msData[currentMsIndex];
                currentMsIndex = prevIndex(currentMsIndex);
            }
            currentMsIndex = nextIndex(currentMsIndex);
            if (firstMicroState.isProcessed) {
                return;
            }
            switch (firstMicroState.state) {
                case MicroState::BACKWARD:
                    commitMovement(Movement::MOVE_BACKWARD);
                    break;
                case MicroState::FORWARD:
                    commitMovement(Movement::MOVE_FORWARD);
                    break;
                case MicroState::DOWN:
                    commitMovement(Movement::MOVE_DOWN);
                    break;
                case MicroState::RIGHT:
                    commitMovement(Movement::MOVE_RIGHT);
                    break;
                case MicroState::UP:
                    commitMovement(Movement::MOVE_UP);
                    break;
                case MicroState::LEFT:
                    commitMovement(Movement::MOVE_LEFT);
                    break;
                default:
                    break;
            }
            msData[currentMsIndex].isProcessed = true;
        }

        detectGesture();
    }

    void detectGesture() {
        int curMovementIndex = prevIndex(movementIndex);

        if (!movementIsProcess[curMovementIndex]) {
            for (const Gesture &gesture : registeredGestures) {
                bool matched = true;

                auto move_iter = gesture.movements.rbegin();
                for (int cur = curMovementIndex;
                     move_iter != gesture.movements.rend(); cur = prevIndex(cur)) {
                    if (movementIsProcess[cur] || movements[cur] != *move_iter) {
                        matched = false;
                        break;
                    }
                    ++move_iter;
                }

                if (matched) {
                    currentGestureName = gesture.name;
                    movementIsProcess[curMovementIndex] = true;
                    ++currentGestureId;
                    break;
                }
            }
        }
    }

    void log(const AccelerometerData &data) {
        string log = (to_string(data.x) + ", " + to_string(data.y) + ", " + to_string(data.z));
        LOG_I("%s", log.c_str());
    }

    AccelerometerData lastData() {
        return data[(dataIndex - 1 + SENSOR_HISTORY_LENGTH) % SENSOR_HISTORY_LENGTH];
    }

    MicroStateData lastMicroStateData() {
        return msData[prevIndex(msIndex)];
    };
};

SensorHandler sensorHandler;

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_init(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;

    sensorHandler.init();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_onResume(JNIEnv *env, jclass clazz) {
    sensorHandler.resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_onPause(JNIEnv *env, jclass clazz) {
    sensorHandler.pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_capture(JNIEnv *env, jclass clazz) {
    sensorHandler.capture();
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_getLastAcceleration(JNIEnv *env, jclass clazz) {
    auto data = sensorHandler.lastData();
    jfloatArray jData = env->NewFloatArray(3);

    jfloat buf[3];
    buf[0] = data.x;
    buf[1] = data.y;
    buf[2] = data.z;

    env->SetFloatArrayRegion(jData, 0, 3, buf);
    return jData;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_getLastMovement(JNIEnv *env, jclass clazz) {
    Movement move = sensorHandler.movements[sensorHandler.prevIndex(sensorHandler.movementIndex)];
    string str = to_string(sensorHandler.movementCount) + ":";
    switch (move) {
        case Movement::STAY_STILL:
            str += "静止";
            break;
        case Movement::MOVE_LEFT:
            str += "左移";
            break;
        case Movement::MOVE_RIGHT:
            str += "右移";
            break;
        case Movement::MOVE_UP:
            str += "上移";
            break;
        case Movement::MOVE_DOWN:
            str += "下移";
            break;
        case Movement::MOVE_FORWARD:
            str += "前移";
            break;
        case Movement::MOVE_BACKWARD:
            str += "后移";
            break;
    }
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_getLastGesture(JNIEnv *env, jclass clazz) {
    (void) clazz;
    string str = to_string(currentGestureId) + ":" + currentGestureName;
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_SensorHandlerJNI_getLastMicroState(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;

    auto data = sensorHandler.lastMicroStateData();
    string str;
    switch (data.state) {
        case MicroState::STAY_STILL:
            str = "静止";
            break;
        case MicroState::RIGHT:
            str = "向右";
            break;
        case MicroState::LEFT:
            str = "向左";
            break;
        case MicroState::UP:
            str = "向上";
            break;
        case MicroState::DOWN:
            str = "向下";
            break;
        case MicroState::FORWARD:
            str = "向前";
            break;
        case MicroState::BACKWARD:
            str = "向后";
            break;
    }
    return env->NewStringUTF(str.c_str());
}