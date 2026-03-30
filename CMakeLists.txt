cmake_minimum_required(VERSION 3.31)

project(Signals LANGUAGES C CXX)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "CMake is using Python interpreter: ${Python3_EXECUTABLE}")
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------- External dependencies (fetched, target-based) ----------

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
	implot
	GIT_REPOSITORY https://github.com/epezent/implot.git
	GIT_TAG master
)
FetchContent_MakeAvailable(implot)

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
FetchContent_MakeAvailable(imgui_knobs)

# ---------- External dependency targets ----------

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui PRIVATE ${CMAKE_BINARY_DIR}/_deps/glfw-src/include)
target_include_directories(imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
	${imgui_SOURCE_DIR}/misc/cpp
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
	src/Handler.cpp
	src/MDU/ModuleLoader.cpp
	src/MDU/FileWatcher.cpp
	src/MDU/mduParser.cpp
	src/MDU/CreateMDU.cpp
	src/Draw/Draw.cpp
	src/Draw/ImGuiUtil.cpp
	src/Draw/Window.cpp
	src/Audio/Audio.cpp
	src/Audio/CV.cpp
	src/Audio/WaveForm.cpp
	src/Functions/ConsoleHandling.cpp
)

	# Export executable symbols for dlopen modules (Linux/Unix only)
	if(NOT WIN32)
		set_target_properties(Signals PROPERTIES LINK_FLAGS "-Wl,--export-dynamic")
	endif()

target_include_directories(Signals PRIVATE
	${CMAKE_SOURCE_DIR}/src/include
	${CMAKE_SOURCE_DIR}/src/include/Draw
	${CMAKE_SOURCE_DIR}/src/include/Audio
	${CMAKE_SOURCE_DIR}/src/include/Functions
	${CMAKE_SOURCE_DIR}/src/include/MDU
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
if(NOT WIN32)
	target_link_libraries(Signals PRIVATE dl)
endif()


add_dependencies(Signals glad_gl_core_33)
 
# ------------------ Doxygen documentation ------------------
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
	set(DOXYFILE_IN ${CMAKE_SOURCE_DIR}/Doxyfile.in)
	set(DOXYFILE_OUT ${CMAKE_BINARY_DIR}/Doxyfile)
	configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

	add_custom_target(doc_doxygen ALL
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMENT "Generating API documentation with Doxygen"
		VERBATIM
	)
else()
	message(STATUS "Doxygen not found - 'doc_doxygen' target will not be available.")
endif()
