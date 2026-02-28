cmake_minimum_required(VERSION 3.16)

project(Signals)

set(CMAKE_CXX_STANDARD 17)
add_executable(Signals
    src/WaveFormGenerator.cpp
)

target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include)