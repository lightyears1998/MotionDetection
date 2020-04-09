# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.4.1)

find_library(android-logcat log)

add_subdirectory(3rdparty)

add_library(
        motion-lib
        SHARED
        motion-lib.cpp
)

target_link_libraries(
        motion-lib
        android
        ${android-logcat}
        yaml
)