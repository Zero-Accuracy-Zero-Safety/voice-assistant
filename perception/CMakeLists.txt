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
    src/miniaudio_impl.cpp
)

find_package(Threads REQUIRED)

target_include_directories(audio_test PRIVATE include)

target_link_libraries(audio_test PRIVATE 
    Threads::Threads
    dl
    m
)

target_link_libraries(audio_test PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)

set_source_files_properties(src/miniaudio_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")

if(UNIX AND NOT APPLE)
    target_link_libraries(audio_test PRIVATE asound)
endif()