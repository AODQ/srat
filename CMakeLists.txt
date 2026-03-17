cmake_minimum_required(VERSION 3.20)
project(rasterizer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
	srat/src/virtual-range-allocator.cpp
	srat/src/tile-grid.cpp
	srat/src/rasterizer-tiled.cpp
	srat/src/command-buffer.cpp
	srat/src/image.cpp
)
set_target_properties(
	srat
	PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	UNITY_BUILD ON
)
target_precompile_headers(
	srat
	PRIVATE
	srat/include/srat/math.hpp
)
target_link_options(srat PRIVATE -fuse-ld=mold)
target_include_directories(
	srat
	PUBLIC
	#${IMGUI_DIR}
	srat/include
	SYSTEM PUBLIC external/tracy/public
)

target_compile_options(
	srat
	PRIVATE -Wall -Wextra -Wpedantic -mavx2 -mfma -O3
	INTERFACE -mavx2 -mfma
	#-DTRACY_ENABLE
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
target_link_options(app PRIVATE -fuse-ld=mold)
target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)

# install
install(
	TARGETS app
	RUNTIME DESTINATION bin
)
