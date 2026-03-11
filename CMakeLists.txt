cmake_minimum_required(VERSION 3.16)

project(Signals LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------- External dependencies (fetched, target-based) ----------

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
	FetchContent_Populate(imgui)
endif()

FetchContent_Declare(
	implot
	GIT_REPOSITORY https://github.com/epezent/implot.git
	GIT_TAG master
)
FetchContent_GetProperties(implot)
if(NOT implot_POPULATED)
	FetchContent_Populate(implot)
endif()

FetchContent_Declare(
	glad
	GIT_REPOSITORY https://github.com/Dav1dde/glad.git
	GIT_TAG v2.0.8
)
FetchContent_MakeAvailable(glad)
add_subdirectory(${glad_SOURCE_DIR}/cmake glad_cmake)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
	FetchContent_Declare(
		sdl2
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG release-2.32.8
	)
	FetchContent_MakeAvailable(sdl2)
endif()

# imnodes expects an imgui target name to be provided.
set(IMNODES_IMGUI_TARGET_NAME imgui)
FetchContent_Declare(
	imnodes
	GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
	GIT_TAG master
)
FetchContent_MakeAvailable(imnodes)

FetchContent_Declare(
	imgui_knobs
	GIT_REPOSITORY https://github.com/altschuler/imgui-knobs.git
	GIT_TAG 8a43bf7
)
FetchContent_GetProperties(imgui_knobs)
if(NOT imgui_knobs_POPULATED)
	FetchContent_Populate(imgui_knobs)
endif()

# ---------- External dependency targets ----------

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)
target_include_directories(imgui PRIVATE
	${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_33/include
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
add_dependencies(imgui glad_gl_core_33)

add_library(implot_lib STATIC
	${implot_SOURCE_DIR}/implot.cpp
	${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui)

add_library(imgui_knobs_lib STATIC
	${imgui_knobs_SOURCE_DIR}/imgui-knobs.cpp
)
target_include_directories(imgui_knobs_lib PUBLIC ${imgui_knobs_SOURCE_DIR})
target_link_libraries(imgui_knobs_lib PUBLIC imgui)

# ---------- Application target ----------

add_executable(Signals
	src/Modules/Handler.cpp
		src/Modules/ModuleEditor.cpp
	src/Modules/WaveFormGenerator.cpp
	src/Modules/Output.cpp
	src/Functions/Window.cpp
	src/Functions/ImGuiUtil.cpp
	src/Functions/Audio.cpp
	src/Functions/CV.cpp
	src/Functions/WaveForm.cpp
	src/Modules/VoltageControllFilter.cpp
	src/Modules/Voltage-ControlledAmplifier.cpp
	src/Modules/LowFrequencyOscillator.cpp
)

target_include_directories(Signals PRIVATE
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/Functions
	${CMAKE_SOURCE_DIR}/include/Modules
	${CMAKE_BINARY_DIR}/gladsources/glad_gl_core_33/include
)

find_package(OpenGL REQUIRED)

target_link_libraries(Signals PRIVATE
	imgui
	imgui_knobs_lib
	implot_lib
	imnodes
	glfw
	OpenGL::GL
	SDL2::SDL2
	glad_gl_core_33
)

add_dependencies(Signals glad_gl_core_33)
