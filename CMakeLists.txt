cmake_minimum_required(VERSION 3.16)

project(Signals)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG master
)
FetchContent_MakeAvailable(implot)

# Create an interface library for imgui to allow imnodes to link against it
add_library(imgui_lib INTERFACE)
target_include_directories(imgui_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/imgui)

# Add imnodes subdirectory with imgui target name defined
set(IMNODES_IMGUI_TARGET_NAME imgui_lib)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/imnodes)

add_executable(Signals
    src/Modules/Handler.cpp
    src/Modules/WaveFormGenerator.cpp
    src/Modules/Output.cpp
    src/Functions/Window.cpp
    src/Functions/ImGuiUtil.cpp
    src/Functions/Audio.cpp
    src/Functions/WaveForm.cpp
    src/Modules/VoltageControllFilter.cpp
    src/Modules/Voltage-ControlledAmplifier.cpp
    src/Modules/LowFrequencyOscillator.cpp
)

target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/external/imgui)
target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include/Functions)
target_include_directories(Signals PRIVATE ${CMAKE_SOURCE_DIR}/include/Modules)
target_include_directories(Signals PRIVATE ${implot_SOURCE_DIR})

# Ensure glad's generated headers are visible to ImGui backend
target_include_directories(Signals PRIVATE ${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_33/include)

# Instruct ImGui OpenGL backend to use glad as the loader
target_compile_definitions(Signals PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# ImGui sources (single-file project integration)
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_sources(Signals PRIVATE ${IMGUI_SOURCES})

# Integrate glad (generate loader from external/glad)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glad/cmake glad_cmake)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

# Find and link GLFW and OpenGL
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(SDL2 REQUIRED)

target_link_libraries(Signals PRIVATE glfw OpenGL::GL SDL2::SDL2 glad_gl_core_33 imnodes)
# Ensure glad's generated headers/sources are produced before compiling Signals
add_dependencies(Signals glad_gl_core_33)

# Add the expected generated glad include directory so compiler sees it during build
target_include_directories(Signals PRIVATE ${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_33/include)
