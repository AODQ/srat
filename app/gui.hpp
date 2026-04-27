#pragma once

#include "raylib.h"

void guiDisplayImage(
	Texture const & img,
	char const * label
);

void guiUnitTestImages(
	Image const & referenceColor,
	Image const & referenceDepth,
	Image const & deviceColor,
	Image const & deviceDepth
);
