#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/sensor.h>
#include <jni.h>
#include <string>
#include <mutex>
#include <vector>
#include <algorithm>

#define LOG_TAG    "motion-lib"
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using std::string;
using std::to_string;
using std::vector;
using std::min;

const char PACKAGE_NAME[] = "net.qfstudio.motion";

struct AccelerometerData {
    float x;
    float y;
    float z;
};

enum struct Direction {
    STILL = 0,
    LEFT, RIGHT, UP, DOWN, FORWARD, BACKWARD
};

struct DirectionData {
    Direction direction;
    int during = 1;
    bool isProcessed = false;

    const static int constexpr MAX_DURING = 25;
};

struct MoveData {
    Direction direction;
    bool isProcessed = false;
};

struct Gesture {
    string name;
    vector<Direction> directions;
};

int gestureCount = 0;
string currentGestureName = "静止";

class MotionMan {
    ASensorManager *sensorManager;
    const ASensor *accelerometer;
    ALooper *looper;
    ASensorEventQueue *accelerometerEventQueue;
    vector<Gesture> registeredGestures;

public:
    const static int LOOPER_ID = 2;
    const static int HISTORY_LENGTH = 100;
    const static int SENSOR_REFRESH_RATE_HZ = 100;
    const static int32_t constexpr SENSOR_REFRESH_PERIOD_US = int32_t(
            1000000 / SENSOR_REFRESH_RATE_HZ);
    const static float constexpr SENSOR_FILTER_ALPHA = 0.1f;

    AccelerometerData meterData[HISTORY_LENGTH];
    AccelerometerData meterDataFilter = {0, 0, 0};
    int nextMeterDataIndex = 0;

    DirectionData directionData[HISTORY_LENGTH] = {{Direction::STILL, DirectionData::MAX_DURING, false}};
    int nextDirectionDataIndex = 1;

    MoveData moveData[HISTORY_LENGTH] = {{Direction::STILL, false}};
    int moveDataCount = 0;
    int nextMoveDataIndex = 1;

public:
    inline int prevIndex(int index) {
        return (index - 1 + HISTORY_LENGTH) % HISTORY_LENGTH;
    }

    inline int nextIndex(int index) {
        return (index + 1) % HISTORY_LENGTH;
    }

    void registerGesture(const string &name, const vector<Direction> &directions) {
        registeredGestures.push_back({name, directions});
    }

    MotionMan() {
        registerGesture("数字1", {
                Direction::DOWN, Direction::DOWN
        });
        registerGesture("数字2", {
                Direction::RIGHT, Direction::DOWN, Direction::LEFT,
                Direction::DOWN, Direction::RIGHT
        });
        registerGesture("数字3", {
                Direction::RIGHT, Direction::DOWN,
                Direction::LEFT, Direction::RIGHT,
                Direction::DOWN, Direction::LEFT
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

        LOG_I("%s", "Successful initialized.");
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

    AccelerometerData getLastMeterData() {
        return meterData[prevIndex(nextMeterDataIndex)];
    }

    DirectionData getLastDirectionData() {
        return directionData[prevIndex(nextDirectionDataIndex)];
    };

    Direction getLastDirection() {
        return getLastDirectionData().direction;
    }

    void commitDirectionData(Direction direction) {
        if (direction != getLastDirection()) {
            directionData[nextDirectionDataIndex] = {direction, 1, false};
            nextDirectionDataIndex = nextIndex(nextDirectionDataIndex);
        } else {
            int index = prevIndex(nextDirectionDataIndex);
            int during = min(directionData[index].during + 1, DirectionData::MAX_DURING);
            directionData[index] = {direction, during, false};
        }
    }

    void commitMoveData(Direction direction) {
        moveData[nextMoveDataIndex] = {direction, false};
        nextMoveDataIndex = nextIndex(nextMoveDataIndex);
        moveDataCount++;
    }

    inline bool isPositive(float val) {
        return val > 2;
    }

    inline bool isNegative(float val) {
        return val < -2;
    }

    inline bool isZero(float ac) {
        return !isPositive(ac) && !isNegative(ac);
    }

    void readFromMeter() {
        ALooper_pollAll(0, NULL, NULL, NULL);
        ASensorEvent event;
        while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
            float a = SENSOR_FILTER_ALPHA;
            meterDataFilter.x = a * event.acceleration.x + (1.0f - a) * meterDataFilter.x;
            meterDataFilter.y = a * event.acceleration.y + (1.0f - a) * meterDataFilter.y;
            meterDataFilter.z = a * event.acceleration.z + (1.0f - a) * meterDataFilter.z;
        }
        meterData[nextMeterDataIndex] = meterDataFilter;
        nextMeterDataIndex = (nextMeterDataIndex + 1) % HISTORY_LENGTH;

        if (isZero(meterDataFilter.x) && isZero(meterDataFilter.y) && isZero(meterDataFilter.z)) {
            commitDirectionData(Direction::STILL);
        } else if (isNegative(meterDataFilter.x)) {
            commitDirectionData(Direction::LEFT);
        } else if (isPositive(meterDataFilter.x)) {
            commitDirectionData(Direction::RIGHT);
        } else if (isNegative(meterDataFilter.y)) {
            commitDirectionData(Direction::BACKWARD);
        } else if (isPositive(meterDataFilter.y)) {
            commitDirectionData(Direction::FORWARD);
        } else if (isNegative(meterDataFilter.z)) {
            commitDirectionData(Direction::DOWN);
        } else if (isPositive(meterDataFilter.z)) {
            commitDirectionData(Direction::UP);
        }
    }

    void detectMovement() {
        auto lastDataIndex = prevIndex(nextMeterDataIndex);
        auto lastData = meterData[lastDataIndex];
        auto lastMicroState = getLastDirectionData();

        int currentMsIndex = prevIndex(prevIndex(nextDirectionDataIndex));
        if (lastMicroState.direction == Direction::STILL && lastMicroState.during >= 2) {
            DirectionData firstMicroState = directionData[currentMsIndex];
            while (!(directionData[currentMsIndex].direction == Direction::STILL &&
                     directionData[currentMsIndex].during >= 2)) {
                firstMicroState = directionData[currentMsIndex];
                currentMsIndex = prevIndex(currentMsIndex);
            }
            currentMsIndex = nextIndex(currentMsIndex);
            if (firstMicroState.isProcessed) {
                return;
            }
            switch (firstMicroState.direction) {
                case Direction::BACKWARD:
                    commitMoveData(Direction::BACKWARD);
                    break;
                case Direction::FORWARD:
                    commitMoveData(Direction::FORWARD);
                    break;
                case Direction::DOWN:
                    commitMoveData(Direction::DOWN);
                    break;
                case Direction::RIGHT:
                    commitMoveData(Direction::RIGHT);
                    break;
                case Direction::UP:
                    commitMoveData(Direction::UP);
                    break;
                case Direction::LEFT:
                    commitMoveData(Direction::LEFT);
                    break;
                default:
                    break;
            }
            directionData[currentMsIndex].isProcessed = true;
        }
    }

    void detectGesture() {
        int curMovementIndex = prevIndex(nextMoveDataIndex);

        if (!moveData[curMovementIndex].isProcessed) {
            for (const Gesture &gesture : registeredGestures) {
                bool matched = true;

                auto move_iter = gesture.directions.rbegin();
                for (int cur = curMovementIndex;
                     move_iter != gesture.directions.rend(); cur = prevIndex(cur)) {
                    if (moveData[cur].isProcessed || moveData[cur].direction != *move_iter) {
                        matched = false;
                        break;
                    }
                    ++move_iter;
                }

                if (matched) {
                    currentGestureName = gesture.name;
                    moveData[curMovementIndex].isProcessed = true;
                    ++gestureCount;
                    break;
                }
            }
        }
    }

    void update() {
        readFromMeter();
        detectMovement();
        detectGesture();
    }
};

MotionMan motionMan;

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_init(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;

    motionMan.init();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_resume(JNIEnv *env, jclass clazz) {
    motionMan.resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_pause(JNIEnv *env, jclass clazz) {
    motionMan.pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_update(JNIEnv *env, jclass clazz) {
    motionMan.update();
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_net_qfstudio_motion_MotionLibJNI_getLastMeterValue(JNIEnv *env, jclass clazz) {
    auto data = motionMan.getLastMeterData();
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
Java_net_qfstudio_motion_MotionLibJNI_getLastMovement(JNIEnv *env, jclass clazz) {
    MoveData move = motionMan.moveData[motionMan.prevIndex(motionMan.nextMoveDataIndex)];
    string str = to_string(motionMan.moveDataCount) + ":";
    switch (move.direction) {
        case Direction::STILL:
            str += "静止";
            break;
        case Direction::LEFT:
            str += "左移";
            break;
        case Direction::RIGHT:
            str += "右移";
            break;
        case Direction::UP:
            str += "上移";
            break;
        case Direction::DOWN:
            str += "下移";
            break;
        case Direction::FORWARD:
            str += "前移";
            break;
        case Direction::BACKWARD:
            str += "后移";
            break;
    }
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLibJNI_getLastGesture(JNIEnv *env, jclass clazz) {
    (void) clazz;
    string str = to_string(gestureCount) + ":" + currentGestureName;
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLibJNI_getLastDirection(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;

    auto data = motionMan.getLastDirectionData();
    string str;
    switch (data.direction) {
        case Direction::STILL:
            str = "静止";
            break;
        case Direction::RIGHT:
            str = "向右";
            break;
        case Direction::LEFT:
            str = "向左";
            break;
        case Direction::UP:
            str = "向上";
            break;
        case Direction::DOWN:
            str = "向下";
            break;
        case Direction::FORWARD:
            str = "向前";
            break;
        case Direction::BACKWARD:
            str = "向后";
            break;
    }
    return env->NewStringUTF(str.c_str());
}