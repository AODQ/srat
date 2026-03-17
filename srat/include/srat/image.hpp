#pragma once

// an image that can be used as a render target or texture source

#include "srat/math.hpp"
#include <srat/types.hpp>

namespace srat {

struct Image { u64 id; };

enum struct Layout {
	Linear,
	Tiled,
};

enum struct Format {
	r8g8b8a8_unorm,
	depth16_unorm,
};


struct ImageCreateInfo {
	u32v2 dim;
	Layout layout;
	Format format;
};

Image image_create(ImageCreateInfo const & createInfo);
void image_destroy(Image const &image);

u32v2 image_dim(Image const & image);
u8 * image_data(Image const & image);


} // namespace srat
