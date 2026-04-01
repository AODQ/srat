cmake_minimum_required(VERSION 3.20)
project(rasterizer)

# ------------------------------------------------------------------------------
# -- global cmake properties
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# ------------------------------------------------------------------------------
# -- sanitization
# ------------------------------------------------------------------------------

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	set(snort_sanitize_flags -fsanitize=address -fno-omit-frame-pointer)
else()
	set(snort_sanitize_flags "")
endif()

# ------------------------------------------------------------------------------
# -- ccache
# ------------------------------------------------------------------------------
# add ccache if found
find_program(CCACHE_FOUND ccache)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK    ccache)

# ------------------------------------------------------------------------------
# -- oneTBB
# ------------------------------------------------------------------------------

option(TBB_TEST "Enable testing" OFF)
option(TBB_EXAMPLES "Enable examples" OFF)
option(TBB_STRICT "Treat compiler warnings as errors" OFF)
add_subdirectory(external/oneTBB)

# ------------------------------------------------------------------------------
# -- tracy
# ------------------------------------------------------------------------------

option(BUILD_TRACY "Build with Tracy profiler" ON)
option(LEGACY "Instead of Wayland, use the legacy X11 backend on Linux" ON)
add_subdirectory(external/tracy)

# ------------------------------------------------------------------------------
# -- raylib
# ------------------------------------------------------------------------------

# Find raylib
find_package(raylib REQUIRED)

## rlImGui
#set(RLIMGUI_DIR external/rlImGui)

# ------------------------------------------------------------------------------
# -- imgui
# ------------------------------------------------------------------------------

# imgui + rlimgui
set(IMGUI_DIR external/imgui)
set(RLIMGUI_DIR external/rlImGui)
add_library(
	imgui
	STATIC
		${IMGUI_DIR}/imgui.cpp
		${IMGUI_DIR}/imgui_draw.cpp
		${IMGUI_DIR}/imgui_tables.cpp
		${IMGUI_DIR}/imgui_widgets.cpp
		${RLIMGUI_DIR}/rlImGui.cpp
)
target_link_libraries(imgui PRIVATE raylib)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${RLIMGUI_DIR})

# ------------------------------------------------------------------------------
# -- rasterizer
# ------------------------------------------------------------------------------

add_library(
	srat
	STATIC
	srat/src/command-buffer.cpp
	srat/src/image.cpp
	srat/src/rasterizer-binning.cpp
	srat/src/rasterizer-rasterize.cpp
	srat/src/rasterizer-stage-vertex.cpp
	srat/src/rasterizer-tiled.cpp
	srat/src/tile-grid.cpp
	srat/src/virtual-range-allocator.cpp
)
set_target_properties(
	srat
	PROPERTIES
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
	UNITY_BUILD OFF

)
target_precompile_headers(
	srat
	PRIVATE
	srat/include/srat/math.hpp
)
target_include_directories(
	srat
	PUBLIC
	#${IMGUI_DIR}
	srat/include
	SYSTEM PUBLIC external/tracy/public
)

target_compile_options(
	srat
	PRIVATE
	-Wall -Wextra -Wpedantic -mavx2 -mfma
	$<$<CONFIG:Debug>:-O0 -g3 -ggdb>
	$<$<CONFIG:RelWithDebInfo>:-O2 -g3 -ggdb -fno-omit-frame-pointer>
	$<$<CONFIG:Release>:-O3>
	${snort_sanitize_flags}
	INTERFACE -mavx2 -mfma
	#-DTRACY_ENABLE
)
target_compile_definitions(
	srat
	PRIVATE
	$<$<CONFIG:Debug>:SRAT_DEBUG_IMPL=1>
	$<$<CONFIG:RelWithDebInfo>:SRAT_DEBUG_IMPL=1>
	$<$<CONFIG:Release>:SRAT_DEBUG_IMPL=0>
)
target_link_options(
	srat
	PRIVATE
	"-fuse-ld=mold"
	$<$<CONFIG:Debug>:-fsanitize=address>
	$<$<CONFIG:RelWithDebInfo>:-fsanitize=address>
)
target_link_libraries(
	srat
	PUBLIC
	#TracyClient
	PRIVATE
	TBB::tbb
)

# ------------------------------------------------------------------------------
# -- app
# ------------------------------------------------------------------------------

add_executable(app)
target_sources(
	app
	PRIVATE
	app/source.cpp
	app/unit-tests.cpp
	app/unit-test-tile-grid.cpp
	app/unit-test-virtual-block-allocator.cpp
	app/unit-test-handles.cpp
	app/unit-test-math.cpp
	app/unit-test-rasterizer.cpp
)
target_include_directories(
	app
	PRIVATE
	external/doctest
)
target_link_libraries(app PRIVATE srat raylib imgui)
target_link_options(
	app
	PRIVATE
	-fuse-ld=mold
	$<$<CONFIG:Debug>:-fsanitize=address>
	$<$<CONFIG:RelWithDebInfo>:-fsanitize=address>
	${snort_sanitize_flags}
)
target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(
	app
	PROPERTIES
	BUILD_RPATH "$ORIGIN:$ORIGIN/../lib"
	INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
)

# install
install(
	TARGETS app
	RUNTIME DESTINATION bin
)
