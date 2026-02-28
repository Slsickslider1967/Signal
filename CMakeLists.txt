cmake_minimum_required(VERSION 3.16)

project(Signals)

set(CMAKE_CXX_STANDARD 17)
add_executable(Signals
    src/WaveFormGenerator.cpp
    src/Functions/Window.cpp
)

target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/external/imgui)
target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include/Functions)

# ImGui sources (single-file project integration)
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)

target_sources(Signals PRIVATE ${IMGUI_SOURCES})

# Find and link GLFW and OpenGL
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(Signals PRIVATE glfw OpenGL::GL)