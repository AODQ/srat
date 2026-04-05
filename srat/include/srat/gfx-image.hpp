#pragma once

// an image that can be used as a render target or texture source

#include <srat/core-math.hpp>
#include <srat/core-types.hpp>

namespace srat::gfx {

struct Viewport {
	u32v2 offset;
	u32v2 dim;
};

struct Image { u64 id; };

enum struct ImageLayout {
	Linear,
	Tiled,
};

enum struct ImageFormat {
	r8g8b8a8_unorm,
	depth16_unorm,
};


struct ImageCreateInfo {
	u32v2 dim;
	ImageLayout layout;
	ImageFormat format;
};

Image image_create(ImageCreateInfo const & createInfo);
void image_destroy(Image const &image);

u32v2 image_dim(Image const & image);
srat::slice<u8> image_data8(Image const & image);
srat::slice<u16> image_data16(Image const & image);
srat::slice<u32> image_data32(Image const & image);

} // namespace srat
