#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/sensor.h>
#include <jni.h>
#include <string>
#include <mutex>
#include <vector>
#include <utility>
#include <algorithm>

#define LOG_TAG    "motion-lib"
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using std::string;
using std::to_string;
using std::vector;
using std::pair;
using std::min;
using std::max_element;

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

    const static int MAX_DURING = 25;
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

    const static int LOOPER_ID = 2;
    const static int HISTORY_LENGTH = 100;
    const static int SENSOR_REFRESH_RATE_HZ = 100;
    const static int32_t constexpr SENSOR_REFRESH_PERIOD_US = int32_t(
            1000000 / SENSOR_REFRESH_RATE_HZ);
    const static float constexpr SENSOR_FILTER_ALPHA = 0.1f;

    AccelerometerData meterData[HISTORY_LENGTH];
    AccelerometerData meterDataFilter = {0, 0, 0};
    int nextMeterDataIndex = 1;

    DirectionData directionData[HISTORY_LENGTH] = {{Direction::STILL, DirectionData::MAX_DURING, true}};
    int nextDirectionDataIndex = 1;

    MoveData moveData[HISTORY_LENGTH] = {{Direction::STILL, true}};
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
        registerGesture("数字4", {
                Direction::DOWN, Direction::RIGHT, Direction::UP, Direction::DOWN, Direction::DOWN
        });
        registerGesture("数字5", {
                Direction::LEFT, Direction::DOWN, Direction::RIGHT, Direction::DOWN, Direction::LEFT
        });
        registerGesture("字母c", {
                Direction::FORWARD, Direction::LEFT, Direction::DOWN, Direction::RIGHT
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

    MoveData getLastMoveData() {
        return moveData[prevIndex(nextMoveDataIndex)];
    }

    int getMoveDataCount() {
        return moveDataCount;
    }

    void commitDirectionData(Direction direction) {
        if (direction != getLastDirection()) {
            directionData[nextDirectionDataIndex] = {direction, 1, false};
            nextDirectionDataIndex = nextIndex(nextDirectionDataIndex);
        } else {
            int index = prevIndex(nextDirectionDataIndex);
            int maxDuring = DirectionData::MAX_DURING;
            int during = min<int>(directionData[index].during + 1, maxDuring);
            directionData[index] = {direction, during, directionData[index].isProcessed};
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

    inline bool isZero(float val) {
        return !isPositive(val) && !isNegative(val);
    }

    void readFromMeter() {
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
        const int STILL_THRESHOLD = 2;

        auto lastDirectionDataIndex = prevIndex(nextDirectionDataIndex);
        auto lastDirectionData = directionData[lastDirectionDataIndex];

        int currentDirectionDataIndex = prevIndex(lastDirectionDataIndex);
        if (!lastDirectionData.isProcessed &&
            lastDirectionData.direction == Direction::STILL &&
            lastDirectionData.during >= STILL_THRESHOLD) {
            DirectionData firstDirectionDataAfterLastStill;
            while (!(directionData[currentDirectionDataIndex].direction == Direction::STILL &&
                     directionData[currentDirectionDataIndex].during >= STILL_THRESHOLD)) {
                firstDirectionDataAfterLastStill = directionData[currentDirectionDataIndex];
                currentDirectionDataIndex = prevIndex(currentDirectionDataIndex);
            }
            int indexOfCurrentDirectionDataIndex = nextIndex(currentDirectionDataIndex);

            if (firstDirectionDataAfterLastStill.isProcessed) {
                return;
            }

            directionData[indexOfCurrentDirectionDataIndex].isProcessed = true;
            commitMoveData(firstDirectionDataAfterLastStill.direction);
        }
    }

    void detectGesture() {
        int currentMoveDataIndex = prevIndex(nextMoveDataIndex);
        if (!moveData[currentMoveDataIndex].isProcessed) {
            using GestureDirectionCountAndGestureName = pair<int, string>;
            vector<GestureDirectionCountAndGestureName> candidates;

            for (const Gesture &gesture : registeredGestures) {
                bool matched = true;

                int moveDataIndex = currentMoveDataIndex;
                int gestureDirectionIndex = gesture.directions.size() - 1;
                while (matched && gestureDirectionIndex >= 0) {
                    if (moveData[moveDataIndex].isProcessed || moveData[moveDataIndex].direction !=
                                                               gesture.directions[gestureDirectionIndex]) {
                        matched = false;
                        break;
                    }
                    moveDataIndex = prevIndex(moveDataIndex);
                    --gestureDirectionIndex;
                }

                if (matched) {
                    candidates.push_back({gesture.directions.size(), gesture.name});
                }
            }

            if (candidates.size() > 0) {
                auto gestureDirectionCountAndGestureName = *max_element(candidates.begin(),
                                                                        candidates.end());
                int directionCount = gestureDirectionCountAndGestureName.first;
                string name = gestureDirectionCountAndGestureName.second;

                for (int idx = currentMoveDataIndex; directionCount; idx = prevIndex(
                        idx), --directionCount) {
                    moveData[idx].isProcessed = true;
                }
                currentGestureName = name;
                ++gestureCount;
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
    (void) env;
    (void) clazz;

    motionMan.resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_pause(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;
    motionMan.pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_update(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;
    
    motionMan.update();
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_net_qfstudio_motion_MotionLibJNI_getLastMeterValue(JNIEnv *env, jclass clazz) {
    (void) clazz;

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
    (void) clazz;

    MoveData moveData = motionMan.getLastMoveData();
    string str = to_string(motionMan.getMoveDataCount()) + ":";
    switch (moveData.direction) {
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