#include "motion-lib.h"
#include <jni.h>

int gestureCount = 0;
std::string currentGestureName = "静止";

class MotionMan {
    ASensorManager *sensorManager;
    const ASensor *accelerometer;
    ALooper *looper;
    ASensorEventQueue *accelerometerEventQueue;
    std::vector<Gesture> registeredGestures;

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

    void registerGesture(const std::string &name, const std::vector<Direction> &directions) {
        registeredGestures.push_back({name, directions});
    }

    void loadGestureFromAsset(AAssetManager *assetManager) {
        const char *gestureDefinitionFilename = "gesture.yml";

        AAsset *gestureAsset = AAssetManager_open(assetManager, gestureDefinitionFilename,
                                                  AASSET_MODE_BUFFER);
        assert(gestureAsset != NULL);

        const void *gestureAssetBuf = AAsset_getBuffer(gestureAsset);
        assert(gestureAssetBuf != NULL);
        off_t gestureAssetLength = AAsset_getLength(gestureAsset);
        auto gestureDefinitions = std::string(
                (const char *) gestureAssetBuf,
                (size_t) gestureAssetLength);
        AAsset_close(gestureAsset);

        YAML::Node gestureLists = YAML::Load(gestureDefinitions.c_str());
        if (gestureLists.IsSequence()) {
            try {
                for (size_t i = 0; i < gestureLists.size(); ++i) {
                    std::string gestureName = gestureLists[i][0].as<std::string>();
                    std::string gestureDirectionsStr = gestureLists[i][1].as<std::string>();
                    std::vector<Direction> gestureDirections;
                    for (const char &dir : gestureDirectionsStr) {
                        switch (dir) {
                            case 'L':
                                gestureDirections.push_back(Direction::LEFT);
                                break;
                            case 'R':
                                gestureDirections.push_back(Direction::RIGHT);
                                break;
                            case 'U':
                                gestureDirections.push_back(Direction::UP);
                                break;
                            case 'D':
                                gestureDirections.push_back(Direction::DOWN);
                                break;
                            case 'F':
                                gestureDirections.push_back(Direction::FORWARD);
                                break;
                            case 'B':
                                gestureDirections.push_back(Direction::BACKWARD);
                                break;
                            default:
                                throw std::exception();
                        }
                    }
                    registerGesture(gestureName, gestureDirections);
                    LOG_I("Gesture registered: %s [%s]", gestureName.c_str(),
                          gestureDirectionsStr.c_str());
                }
            } catch (std::exception e) {
                LOG_E("Error loading gesture definitions from %s.", gestureDefinitionFilename);
                LOG_E("%s", e.what());
            }
        } else {
            LOG_E("Bad gesture definition format: %s.", gestureDefinitionFilename);
        }
    }

    void initSensor() {
        sensorManager = ASensorManager_getInstanceForPackage(PACKAGE_NAME);
        assert(sensorManager != NULL);
        accelerometer = ASensorManager_getDefaultSensor(sensorManager,
                                                        ASENSOR_TYPE_LINEAR_ACCELERATION);
        assert(accelerometer != NULL);
        looper = ALooper_forThread();
        if (looper != NULL) {
            throw std::runtime_error("MotionLib must be initialized in a dedicated thread.");
        }
        assert(looper == NULL);
        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        assert(looper != NULL);
        const int accelerometerEventId = 1;
        accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                                  accelerometerEventId,
                                                                  NULL,
                                                                  NULL);
        assert(accelerometerEventQueue != NULL);
        auto status = ASensorEventQueue_enableSensor(accelerometerEventQueue,
                                                     accelerometer);
        assert(status >= 0);
        status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                accelerometer,
                                                SENSOR_REFRESH_PERIOD_US);
        assert(status >= 0);
    }

    void init(AAssetManager *assetManager) {
        loadGestureFromAsset(assetManager);
        initSensor();

        LOG_I("%s", "Successful initialized.");
        LOG_I("An update took %fms.", measureAverageUpdateTimeInMilliseconds());
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
            int during = std::min<int>(directionData[index].during + 1, maxDuring);
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
        const int STILL_THRESHOLD = 8;

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
            using GestureDirectionCountAndGestureName = std::pair<int, std::string>;
            std::vector<GestureDirectionCountAndGestureName> candidates;

            for (const Gesture &gesture : registeredGestures) {
                bool matched = true;

                int moveDataIndex = currentMoveDataIndex;
                int gestureDirectionIndex = gesture.directions.size() - 1;
                while (gestureDirectionIndex >= 0) {
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
                auto gestureDirectionCountAndGestureName = *std::max_element(candidates.begin(),
                                                                             candidates.end());
                int directionCount = gestureDirectionCountAndGestureName.first;
                std::string name = gestureDirectionCountAndGestureName.second;

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

    float measureAverageUpdateTimeInMilliseconds() {
        int repeat = 100;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < repeat; ++i) {
            update();
        }
        auto stop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> diff = stop - start;
        return diff.count() / repeat;
    }
};

MotionMan motionMan;

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLibJNI_initNativeLib(JNIEnv *env, jclass clazz,
                                                    jobject assetManager) {
    (void) env;
    (void) clazz;

    AAssetManager *nativeAssetManager = AAssetManager_fromJava(env, assetManager);
    motionMan.init(nativeAssetManager);
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
    std::string str = std::to_string(motionMan.getMoveDataCount()) + ":";
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

    std::string str = std::to_string(gestureCount) + ":" + currentGestureName;
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLibJNI_getLastDirection(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;

    auto data = motionMan.getLastDirectionData();
    std::string str;
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