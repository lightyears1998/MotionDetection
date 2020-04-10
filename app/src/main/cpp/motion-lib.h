#ifndef MOTION_LIB_H
#define MOTION_LIB_H

#include <android/log.h>
#include <android/asset_manager_jni.h>
#include <android/sensor.h>
#include <yaml-cpp/yaml.h>
#include <mutex>
#include <string>
#include <chrono>
#include <ratio>
#include <vector>
#include <utility>
#include <algorithm>
#include <exception>

#define LOG_TAG    "MotionLib"
#define LOG_V(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char PACKAGE_NAME[] = "net.qfstudio.motion";
const static int HISTORY_LENGTH = 100;
const static int SENSOR_REFRESH_RATE_HZ = 100;
const static int constexpr SENSOR_REFRESH_PERIOD_US = 1000000 / SENSOR_REFRESH_RATE_HZ;
const static float constexpr SENSOR_FILTER_ALPHA = 0.1f;
const static int QUIESCENT_THRESHOLD = 16;

struct AccelerometerReadings {
    float x;
    float y;
    float z;
};

enum struct Direction {
    STILL = 0,
    LEFT, RIGHT, UP, DOWN, FORWARD, BACKWARD
};

struct AccelerationDirectionData {
    Direction direction;
    int during = 1;
    bool isProcessed = false;

    const static int MAX_DURING = 25;

    std::string toString() const {
        switch (this->direction) {
            case Direction::STILL:
                return "静止";
            case Direction::RIGHT:
                return "向右";
            case Direction::LEFT:
                return "向左";
            case Direction::UP:
                return "向上";
            case Direction::DOWN:
                return "向下";
            case Direction::FORWARD:
                return "向前";
            case Direction::BACKWARD:
                return "向后";
        }
    }
};

struct MoveDirectionData {
    Direction direction;
    bool isProcessed = false;

    std::string toString() const {
        switch (this->direction) {
            case Direction::STILL:
                return "静止";
            case Direction::LEFT:
                return "左移";
            case Direction::RIGHT:
                return "右移";
            case Direction::UP:
                return "上移";
            case Direction::DOWN:
                return "下移";
            case Direction::FORWARD:
                return "前移";
            case Direction::BACKWARD:
                return "后移";
        }
    }
};

struct Gesture {
    std::string name;
    std::vector<Direction> directions;
};

extern ALooper_callbackFunc sensorEventCallback;

#endif // MOTION_LIB_H
