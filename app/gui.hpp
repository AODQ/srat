#pragma once

#include "raylib.h"

// Camera input collected from the image viewer each frame.
// All values are in raw screen-pixels / scroll-ticks; the caller
// converts them to world-space units via apply_camera_input().
struct ImageViewerCameraInput {
	float orbitDX  { 0.f };   // left-drag  X  → azimuth
	float orbitDY  { 0.f };   // left-drag  Y  → elevation
	float orbitXDX { 0.f };   // middle-drag X → azimuth only (locked elevation)
	float panDX    { 0.f };   // right-drag X
	float panDY    { 0.f };   // right-drag Y
	float scroll   { 0.f };   // scroll-wheel  → zoom radius

	bool resetForward { false };
	bool resetTop     { false };
	bool resetSide    { false };
};

// Display a GPU texture in an ImGui window.
// When cameraInput is non-null the window shows Forward/Top/Side reset
// buttons and routes mouse events to cameraInput instead of
// panning/zooming the image itself.
void guiDisplayImage(
	Texture const & img,
	char const * label,
	ImageViewerCameraInput * cameraInput = nullptr
);

void guiUnitTestImages(
	Image const & referenceColor,
	Image const & referenceDepth,
	Image const & deviceColor,
	Image const & deviceDepth
);
