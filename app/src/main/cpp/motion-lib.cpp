#include "motion-lib.h"
#include <jni.h>

int gestureCount = 0;
std::string currentGestureName = "静止";

class MotionMan {
    JNIEnv *jniEnv;
    jobject jLib;
    jmethodID jMethodIdHandleDirectionChange;
    jmethodID jMethodIdHandleMovementDetected;
    jmethodID jMethodIdHandleGestureDetected;

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
        assert(looper != NULL);

        accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                                  ALOOPER_POLL_CALLBACK,
                                                                  sensorEventCallback,
                                                                  NULL);
        assert(accelerometerEventQueue != NULL);
    }

    void initJNIEnv(JNIEnv *env, jobject jLib) {
        this->jniEnv = env;
        this->jLib = env->NewGlobalRef(jLib);
        jclass clazz = env->GetObjectClass(this->jLib);
        jMethodIdHandleDirectionChange = env->GetMethodID(clazz, "handleDirectionChange",
                                                          "(Ljava/lang/String;)V");
        jMethodIdHandleMovementDetected = env->GetMethodID(clazz, "handleMovementDetected",
                                                           "(Ljava/lang/String;)V");
        jMethodIdHandleGestureDetected = env->GetMethodID(clazz, "handleGestureDetected",
                                                          "(Ljava/lang/String;)V");
    }

    void init(AAssetManager *assetManager, JNIEnv *env, jobject jLib) {
        loadGestureFromAsset(assetManager);
        initJNIEnv(env, jLib);
        initSensor();

        LOG_I("Initialized.");
    }

    void pause() {
        ASensorEventQueue_disableSensor(accelerometerEventQueue, accelerometer);

        LOG_I("Paused.");
    }

    void resume() {
        ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
        auto status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                     accelerometer,
                                                     SENSOR_REFRESH_PERIOD_US);
        assert(status >= 0);

        LOG_I("Resumed, An update took %fms.", measureAverageUpdateTimeInMilliseconds());
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
            callJavaHandleDirectionChange(directionData[nextDirectionDataIndex]);
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
        callJavaHandleMovementDetected(moveData[nextMoveDataIndex]);
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
                callJavaHandleGestureDetected(currentGestureName);
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

    void callJavaHandleDirectionChange(const DirectionData &directionData) {
        jstring direction = jniEnv->NewStringUTF(directionData.toString().c_str());
        jniEnv->CallVoidMethod(jLib, jMethodIdHandleDirectionChange, direction);
    }

    void callJavaHandleMovementDetected(const MoveData &moveData) {
        jstring movement = jniEnv->NewStringUTF(moveData.toString().c_str());
        jniEnv->CallVoidMethod(jLib, jMethodIdHandleMovementDetected, movement);
    }

    void callJavaHandleGestureDetected(const std::string &gestureName) {
        jstring jGestureName = jniEnv->NewStringUTF(gestureName.c_str());
        jniEnv->CallVoidMethod(jLib, jMethodIdHandleGestureDetected, jGestureName);
    }
};

MotionMan motionMan;

int motionMan_SensorEventCallback(int fd, int events, void *data) {
    (void) fd;
    (void) events;
    (void) data;

    motionMan.update();
    return 1; // To continue receiving callbacks.
}
ALooper_callbackFunc sensorEventCallback = &motionMan_SensorEventCallback;

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLib_initUnderlyingNativeLib(JNIEnv *env, jobject jLib,
                                                           jobject assetManager) {
    (void) env;
    (void) jLib;

    AAssetManager *nativeAssetManager = AAssetManager_fromJava(env, assetManager);
    motionMan.init(nativeAssetManager, env, jLib);
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLib_resume(JNIEnv *env, jobject clazz) {
    (void) env;
    (void) clazz;

    motionMan.resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLib_pause(JNIEnv *env, jobject clazz) {
    (void) env;
    (void) clazz;
    motionMan.pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_net_qfstudio_motion_MotionLib_update(JNIEnv *env, jobject clazz) {
    (void) env;
    (void) clazz;

    motionMan.update();
}

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_net_qfstudio_motion_MotionLib_getLastMeterValue(JNIEnv *env, jobject clazz) {
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
Java_net_qfstudio_motion_MotionLib_getLastMovement(JNIEnv *env, jobject clazz) {
    (void) clazz;

    MoveData moveData = motionMan.getLastMoveData();
    return env->NewStringUTF(moveData.toString().c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLib_getLastGesture(JNIEnv *env, jobject clazz) {
    (void) clazz;

    std::string str = std::to_string(gestureCount) + ":" + currentGestureName;
    return env->NewStringUTF(str.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLib_getLastDirection(JNIEnv *env, jobject clazz) {
    (void) env;
    (void) clazz;

    auto data = motionMan.getLastDirectionData();
    return env->NewStringUTF(data.toString().c_str());
}