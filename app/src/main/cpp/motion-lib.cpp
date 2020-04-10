#include "motion-lib.h"
#include <jni.h>


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

    AccelerometerReadings accelerometerReadings[HISTORY_LENGTH];
    AccelerometerReadings accelerometerReadingsFilter = {0, 0, 0};
    int nextAccelerometerReadingsIndex = 1;


    AccelerationDirectionData accelerationDirectionData[HISTORY_LENGTH] = {
            {Direction::STILL, AccelerationDirectionData::MAX_DURING, true}
    };
    int nextAccelerationDirectionDataIndex = 1;

    int recognizedMoveDirectionCount = 0;
    MoveDirectionData moveDirectionData[HISTORY_LENGTH] = {{Direction::STILL, true}};
    int nextMoveDirectionDataIndex = 1;

    int recognizedGestureCount = 0;
    std::string lastRecognizedGestureName = "静止";

public:
    inline int prevIndex(int index) {
        return (index - 1 + HISTORY_LENGTH) % HISTORY_LENGTH;
    }

    inline int nextIndex(int index) {
        return (index + 1) % HISTORY_LENGTH;
    }

    inline void registerGesture(const std::string &name, const std::vector<Direction> &directions) {
        registeredGestures.push_back({name, directions});
    }

    void readGestureDefinitionFromAsset(AAssetManager *assetManager) {
        const char *gestureAssetFilename = "gesture.yml";

        AAsset *gestureAsset = AAssetManager_open(assetManager, gestureAssetFilename,
                                                  AASSET_MODE_BUFFER);
        assert(gestureAsset != NULL);

        const void *gestureAssetBuf = AAsset_getBuffer(gestureAsset);
        assert(gestureAssetBuf != NULL);
        off_t gestureAssetLength = AAsset_getLength(gestureAsset);
        auto gestureDefinitionsString = std::string(
                (const char *) gestureAssetBuf,
                (size_t) gestureAssetLength);
        AAsset_close(gestureAsset);

        YAML::Node gestureDefinitions = YAML::Load(gestureDefinitionsString.c_str());
        if (gestureDefinitions.IsSequence()) {
            try {
                for (size_t i = 0; i < gestureDefinitions.size(); ++i) {
                    std::string gestureName = gestureDefinitions[i][0].as<std::string>();
                    std::string gestureDirectionsString = gestureDefinitions[i][1].as<std::string>();
                    std::vector<Direction> gestureDirections;
                    for (const char &dir : gestureDirectionsString) {
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
                          gestureDirectionsString.c_str());
                }
            } catch (std::exception e) {
                LOG_E("An error was encountered when reading gesture definitions from file %s.",
                      gestureAssetFilename);
                LOG_E("%s", e.what());
            }
        } else {
            LOG_E("Bad gesture definitions file format: %s.", gestureAssetFilename);
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
        this->jMethodIdHandleDirectionChange = env->GetMethodID(clazz, "handleDirectionChange",
                                                                "(Ljava/lang/String;)V");
        this->jMethodIdHandleMovementDetected = env->GetMethodID(clazz, "handleMovementDetected",
                                                                 "(Ljava/lang/String;)V");
        this->jMethodIdHandleGestureDetected = env->GetMethodID(clazz, "handleGestureDetected",
                                                                "(Ljava/lang/String;)V");
    }

    void init(AAssetManager *assetManager, JNIEnv *env, jobject jLib) {
        readGestureDefinitionFromAsset(assetManager);
        initJNIEnv(env, jLib);
        initSensor();

        LOG_V("Initialized.");
    }

    void pause() {
        ASensorEventQueue_disableSensor(accelerometerEventQueue, accelerometer);

        LOG_V("Paused.");
    }

    void resume() {
        ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
        auto status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                     accelerometer,
                                                     SENSOR_REFRESH_PERIOD_US);
        assert(status >= 0);

        LOG_V("Resumed.");
    }

    AccelerometerReadings getLastAccelerometerReadings() {
        return accelerometerReadings[prevIndex(nextAccelerometerReadingsIndex)];
    }

    AccelerationDirectionData getLastAccelerationDirectionData() {
        return accelerationDirectionData[prevIndex(nextAccelerationDirectionDataIndex)];
    };

    Direction getLastAccelerationDirection() {
        return getLastAccelerationDirectionData().direction;
    }

    MoveDirectionData getLastMoveDirectionData() {
        return moveDirectionData[prevIndex(nextMoveDirectionDataIndex)];
    }

    std::string getLastRecognizedGestureName() {
        return lastRecognizedGestureName;
    }

    void commitAccelerationDirectionData(Direction direction) {
        if (direction != getLastAccelerationDirection()) {
            accelerationDirectionData[nextAccelerationDirectionDataIndex] = {direction, 1, false};
            invokeDirectionChangeJNIHandler(
                    accelerationDirectionData[nextAccelerationDirectionDataIndex]);
            nextAccelerationDirectionDataIndex = nextIndex(nextAccelerationDirectionDataIndex);
        } else {
            int index = prevIndex(nextAccelerationDirectionDataIndex);
            int maxDuring = AccelerationDirectionData::MAX_DURING;
            int during = std::min<int>(accelerationDirectionData[index].during + 1, maxDuring);
            accelerationDirectionData[index] = {direction, during,
                                                accelerationDirectionData[index].isProcessed};
        }
    }

    void commitMoveDirectionData(Direction direction) {
        moveDirectionData[nextMoveDirectionDataIndex] = {direction, false};
        invokeMovementDetectedJNIHandler(moveDirectionData[nextMoveDirectionDataIndex]);
        nextMoveDirectionDataIndex = nextIndex(nextMoveDirectionDataIndex);
        recognizedMoveDirectionCount++;
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

    void readFromAccelerometer() {
        ASensorEvent event;
        while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
            float a = SENSOR_FILTER_ALPHA;
            accelerometerReadingsFilter.x =
                    a * event.acceleration.x + (1.0f - a) * accelerometerReadingsFilter.x;
            accelerometerReadingsFilter.y =
                    a * event.acceleration.y + (1.0f - a) * accelerometerReadingsFilter.y;
            accelerometerReadingsFilter.z =
                    a * event.acceleration.z + (1.0f - a) * accelerometerReadingsFilter.z;
        }
        accelerometerReadings[nextAccelerometerReadingsIndex] = accelerometerReadingsFilter;
        nextAccelerometerReadingsIndex = (nextAccelerometerReadingsIndex + 1) % HISTORY_LENGTH;

        if (isZero(accelerometerReadingsFilter.x) && isZero(accelerometerReadingsFilter.y) &&
            isZero(accelerometerReadingsFilter.z)) {
            commitAccelerationDirectionData(Direction::STILL);
        } else if (isNegative(accelerometerReadingsFilter.x)) {
            commitAccelerationDirectionData(Direction::LEFT);
        } else if (isPositive(accelerometerReadingsFilter.x)) {
            commitAccelerationDirectionData(Direction::RIGHT);
        } else if (isNegative(accelerometerReadingsFilter.y)) {
            commitAccelerationDirectionData(Direction::BACKWARD);
        } else if (isPositive(accelerometerReadingsFilter.y)) {
            commitAccelerationDirectionData(Direction::FORWARD);
        } else if (isNegative(accelerometerReadingsFilter.z)) {
            commitAccelerationDirectionData(Direction::DOWN);
        } else if (isPositive(accelerometerReadingsFilter.z)) {
            commitAccelerationDirectionData(Direction::UP);
        }
    }

    void detectMovement() {
        auto lastDirectionDataIndex = prevIndex(nextAccelerationDirectionDataIndex);
        auto lastDirectionData = accelerationDirectionData[lastDirectionDataIndex];

        int currentDirectionDataIndex = prevIndex(lastDirectionDataIndex);
        if (!lastDirectionData.isProcessed &&
            lastDirectionData.direction == Direction::STILL &&
            lastDirectionData.during >= QUIESCENT_THRESHOLD) {
            AccelerationDirectionData firstDirectionDataAfterLastStill;
            while (!(accelerationDirectionData[currentDirectionDataIndex].direction ==
                     Direction::STILL &&
                     accelerationDirectionData[currentDirectionDataIndex].during >=
                     QUIESCENT_THRESHOLD)) {
                firstDirectionDataAfterLastStill = accelerationDirectionData[currentDirectionDataIndex];
                currentDirectionDataIndex = prevIndex(currentDirectionDataIndex);
            }
            int indexOfCurrentDirectionDataIndex = nextIndex(currentDirectionDataIndex);

            if (firstDirectionDataAfterLastStill.isProcessed) {
                return;
            }

            accelerationDirectionData[indexOfCurrentDirectionDataIndex].isProcessed = true;
            commitMoveDirectionData(firstDirectionDataAfterLastStill.direction);
        }
    }

    void detectGesture() {
        int currentMoveDataIndex = prevIndex(nextMoveDirectionDataIndex);
        if (!moveDirectionData[currentMoveDataIndex].isProcessed) {
            using GestureDirectionCountAndGestureName = std::pair<int, std::string>;
            std::vector<GestureDirectionCountAndGestureName> candidates;

            for (const Gesture &gesture : registeredGestures) {
                bool matched = true;

                int moveDataIndex = currentMoveDataIndex;
                int gestureDirectionIndex = gesture.directions.size() - 1;
                while (gestureDirectionIndex >= 0) {
                    if (moveDirectionData[moveDataIndex].isProcessed ||
                        moveDirectionData[moveDataIndex].direction !=
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
                    moveDirectionData[idx].isProcessed = true;
                }
                lastRecognizedGestureName = name;
                invokeGestureDetectedJNIHandler(lastRecognizedGestureName);
                ++recognizedGestureCount;
            }
        }
    }

    void update() {
        auto start = std::chrono::high_resolution_clock::now();

        readFromAccelerometer();
        detectMovement();
        detectGesture();

        auto stop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> diff = stop - start;
        LOG_V("An update took %fms.", diff.count());
    }

    void invokeDirectionChangeJNIHandler(const AccelerationDirectionData &directionData) {
        jstring direction = jniEnv->NewStringUTF(directionData.toString().c_str());
        jniEnv->CallVoidMethod(jLib, jMethodIdHandleDirectionChange, direction);
    }

    void invokeMovementDetectedJNIHandler(const MoveDirectionData &moveData) {
        jstring movement = jniEnv->NewStringUTF(moveData.toString().c_str());
        jniEnv->CallVoidMethod(jLib, jMethodIdHandleMovementDetected, movement);
    }

    void invokeGestureDetectedJNIHandler(const std::string &gestureName) {
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
Java_net_qfstudio_motion_MotionLib_getLastMeterReadings(JNIEnv *env, jobject clazz) {
    (void) clazz;

    auto data = motionMan.getLastAccelerometerReadings();
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

    MoveDirectionData moveData = motionMan.getLastMoveDirectionData();
    return env->NewStringUTF(moveData.toString().c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLib_getLastGesture(JNIEnv *env, jobject clazz) {
    (void) clazz;

    return env->NewStringUTF(motionMan.getLastRecognizedGestureName().c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_qfstudio_motion_MotionLib_getLastDirection(JNIEnv *env, jobject clazz) {
    (void) env;
    (void) clazz;

    auto data = motionMan.getLastAccelerationDirectionData();
    return env->NewStringUTF(data.toString().c_str());
}