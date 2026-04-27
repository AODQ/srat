cmake_minimum_required(VERSION 3.20)
project(rasterizer)

# ------------------------------------------------------------------------------
# -- options
# ------------------------------------------------------------------------------

option(SRAT_TSAN "Override ASAN with TSAN" OFF)

# ------------------------------------------------------------------------------
# -- global cmake properties
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# ------------------------------------------------------------------------------
# -- sanitization
# ------------------------------------------------------------------------------

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	if (SRAT_TSAN)
		set(
			snort_sanitize_flags
			-fsanitize=thread -fno-omit-frame-pointer
		)
		set(
			snort_linker_flags
			-fsanitize=thread
		)
	else()
		set(
			snort_sanitize_flags
			-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
		)
		set(
			snort_linker_flags
			-fsanitize=address -fsanitize=undefined
		)
	endif()
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
	srat/src/profiler.cpp
	srat/src/alloc-virtual-range.cpp
	srat/src/core-config.cpp
	srat/src/gfx-command-buffer.cpp
	srat/src/gfx-device.cpp
	srat/src/gfx-image.cpp
	srat/src/internal/gfx-material-impl.cpp
	srat/src/rasterizer-phase-bin.cpp
	srat/src/rasterizer-phase-rasterize.cpp
	srat/src/rasterizer-phase-vertex.cpp
	srat/src/rasterizer-tile-grid.cpp
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
	srat/include/srat/core-math.hpp
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
	-Wextra -Wpedantic -mavx2 -mfma -fno-rtti -fno-exceptions
	$<$<CONFIG:Debug>:-O0 -g3 -ggdb>
	$<$<CONFIG:RelWithDebInfo>:-O3 -g3 -ggdb -fno-omit-frame-pointer -Wall>
	$<$<CONFIG:Release>:-O3 -Wall>
	${snort_sanitize_flags}
	INTERFACE -mavx2 -mfma
	$<$<CONFIG:RelWithDebInfo>:-DTRACY_ENABLE>
)
target_compile_definitions(
	srat
	INTERFACE
	$<$<CONFIG:RelWithDebInfo>:SRAT_DEBUG_IMPL=0>
	$<$<CONFIG:Release>:SRAT_DEBUG_IMPL=1>
	$<$<CONFIG:Debug>:SRAT_DEBUG_IMPL=1>
)
target_link_options(
	srat
	PRIVATE
	"-fuse-ld=mold"
	$<$<CONFIG:Debug>:${snort_linker_flags}>
	$<$<CONFIG:RelWithDebInfo>:${snort_linker_flags}>
)
target_link_libraries(
	srat
	PUBLIC
	$<$<CONFIG:RelWithDebInfo>:TracyClient>
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
	app/batch-render.cpp
	app/perf-suite.cpp
	app/model-loader.cpp
	app/unit-test-alloc-arena.cpp
	app/unit-test-alloc-virtual-range.cpp
	app/unit-test-core-array.cpp
	app/unit-test-core-handles.cpp
	app/unit-test-core-math.cpp
	app/unit-test-gfx-image.cpp
	app/unit-test-rasterizer.cpp
	app/unit-test-rasterizer-tile-grid.cpp
	app/unit-tests.cpp
	app/gui.cpp
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
	-fuse-ld=mold -fno-rtti -fno-exceptions
	$<$<CONFIG:Debug>:${snort_linker_flags}>
	$<$<CONFIG:RelWithDebInfo>:${snort_linker_flags}>
	${snort_sanitize_flags}
)
# in general for app don't care about warnings or performance issues or anything
# since it's just a testbed for the rasterizer
target_compile_options(
	app PRIVATE
	-Wextra -Wpedantic
)
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
