# Diable all warnings for 3rdparty libraries.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything")
endif ()

# YAML-CPP 0.6.3
# https://github.com/jbeder/yaml-cpp/tree/yaml-cpp-0.6.3
# MIT License
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_BUILD_CONTRIB OFF)
set(YAML_CPP_INSTALL OFF)
add_subdirectory(yaml-cpp-0.6.3)

# Export aliases.
add_library(yaml ALIAS yaml-cpp)
