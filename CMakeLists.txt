cmake_minimum_required(VERSION 3.20)
project(rasterizer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

## ImGui sources
#set(IMGUI_DIR external/imgui)
#set(IMGUI_SOURCES
#    ${IMGUI_DIR}/imgui.cpp
#    ${IMGUI_DIR}/imgui_draw.cpp
#    ${IMGUI_DIR}/imgui_tables.cpp
#    ${IMGUI_DIR}/imgui_widgets.cpp
#)
#add_library(imgui STATIC ${IMGUI_SOURCES})
#target_include_directories(imgui PRIVATE ${IMGUI_DIR})

# ------------------------------------------------------------------------------
# -- rasterizer
# ------------------------------------------------------------------------------

add_library(
	srat
	STATIC
	srat/src/virtual-range-allocator.cpp
)
target_include_directories(
	srat
	PUBLIC
	#${IMGUI_DIR}
	srat/include
)

target_compile_options(srat PRIVATE -Wall)
target_link_libraries(srat)

# ------------------------------------------------------------------------------
# -- app
# ------------------------------------------------------------------------------

add_executable(app)
target_sources(
	app
	PRIVATE
	app/source.cpp
	app/unit-tests.cpp
	app/unit-test-virtual-block-allocator.cpp
	app/unit-test-handles.cpp
)
target_include_directories(
	app
	PRIVATE
	external/doctest
)
target_link_libraries(app PRIVATE srat raylib)
target_compile_options(app PRIVATE -Wall)

# install
install(
	TARGETS app
	RUNTIME DESTINATION bin
)
