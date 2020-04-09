#ifndef MOTION_LIB_H
#define MOTION_LIB_H

#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/sensor.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <chrono>
#include <ratio>
#include <vector>
#include <utility>
#include <algorithm>
#include <exception>

#define LOG_TAG    "motion-lib"
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char PACKAGE_NAME[] = "net.qfstudio.motion";
const static int HISTORY_LENGTH = 100;
const static int SENSOR_REFRESH_RATE_HZ = 100;
const static int constexpr SENSOR_REFRESH_PERIOD_US = 1000000 / SENSOR_REFRESH_RATE_HZ;
const static float constexpr SENSOR_FILTER_ALPHA = 0.1f;

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
    std::string name;
    std::vector<Direction> directions;
};

#endif // MOTION_LIB_H
