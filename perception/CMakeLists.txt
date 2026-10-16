cmake_minimum_required(VERSION 3.20)
project(audio_pipeline_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(audio_test
    src/main.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(audio_test PRIVATE Threads::Threads)
target_include_directories(audio_test PRIVATE include/perception)