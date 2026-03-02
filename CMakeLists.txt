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

target_link_libraries(Signals PRIVATE glfw OpenGL::GL SDL2::SDL2 glad_gl_core_33)

# Ensure glad's generated headers/sources are produced before compiling Signals
add_dependencies(Signals glad_gl_core_33)

# Add the expected generated glad include directory so compiler sees it during build
target_include_directories(Signals PRIVATE ${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_33/include)

# # Optional SoundTouch support: detect and link if available (no runtime integration yet)
# find_package(PkgConfig)
# if(PkgConfig_FOUND)
#     pkg_check_modules(SOUNDTOUCH QUIET soundtouch)
# endif()

# if(SOUNDTOUCH_FOUND)
#     message(STATUS "SoundTouch found via pkg-config")
#     target_include_directories(Signals PRIVATE ${SOUNDTOUCH_INCLUDE_DIRS})
#     target_link_libraries(Signals PRIVATE ${SOUNDTOUCH_LIBRARIES})
# else()
#     # fallback to manual search
#     find_library(SOUNDTOUCH_LIB NAMES soundtouch libsoundtouch)
#     find_path(SOUNDTOUCH_INCLUDE NAMES SoundTouch.h PATHS /usr/include /usr/local/include)
#     if(SOUNDTOUCH_LIB AND SOUNDTOUCH_INCLUDE)
#         message(STATUS "SoundTouch found via find_library/find_path")
#         target_include_directories(Signals PRIVATE ${SOUNDTOUCH_INCLUDE})
#         target_link_libraries(Signals PRIVATE ${SOUNDTOUCH_LIB})
#     else()
#         message(STATUS "SoundTouch not found; building without it")
#     endif()
# endif()