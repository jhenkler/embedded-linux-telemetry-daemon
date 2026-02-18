cmake_minimum_required(VERSION 3.16)
project(embedded-linux-telemetry-daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# ---- Threads ----
find_package(Threads REQUIRED)

# ---- Mosquitto via pkg-config ----
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

# ---- nlohmann/json via CMake -> pkg-config fallback ----
find_package(nlohmann_json 3.2.0 QUIET)
if (NOT nlohmann_json_FOUND)
    pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
endif()

# ------------------------------------
# Core library
# ------------------------------------
add_library(telemetry_core
    src/mqtt_client.cpp
    src/simulated_sensor.cpp
    src/sensor_factory.cpp
)

target_include_directories(telemetry_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(telemetry_core
    PUBLIC 
        PkgConfig::MOSQUITTO 
        Threads::Threads
)

if (nlohmann_json_FOUND)
    target_link_libraries(telemetry_core PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(telemetry_core PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
    target_compile_options(telemetry_core PRIVATE ${NLOHMANN_JSON_CFLAGS_OTHER})
endif()

# ------------------------------------
# Daemon executable
# ------------------------------------

add_executable(embedded-linux-telemetry-daemon
    src/main.cpp
)

target_link_libraries(embedded-linux-telemetry-daemon
    PRIVATE telemetry_core
)

# ------------------------------------
# Tests
# ------------------------------------
include(CTest)
if (BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)


    add_executable(embedded-linux-telemetry-daemon-tests
        tests/test_backoff.cpp
        tests/test_config.cpp
        tests/test_payload.cpp
        tests/test_reconnect.cpp
        tests/test_topics.cpp
    )

    target_link_libraries(embedded-linux-telemetry-daemon-tests
        PRIVATE
            telemetry_core
            GTest::gtest_main
    )

    target_compile_definitions(embedded-linux-telemetry-daemon-tests
        PRIVATE UNIT_TESTS=1
    )

    include(GoogleTest)
    gtest_discover_tests(embedded-linux-telemetry-daemon-tests)
endif()