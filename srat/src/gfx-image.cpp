#include <srat/gfx-image.hpp>

#include <srat/core-handle.hpp>

#include <vector>

// -----------------------------------------------------------------------------
// -- private
// -----------------------------------------------------------------------------

namespace {

struct ImplImage {
	std::vector<u8> data;
	u32v2 dim;
	srat::gfx::ImageLayout layout;
	srat::gfx::ImageFormat format;
};

static srat::HandlePool<srat::gfx::Image, ImplImage> sImagePool = (
	srat::HandlePool<srat::gfx::Image, ImplImage>::create(128, "ImagePool")
);

}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

srat::gfx::Image srat::gfx::image_create(ImageCreateInfo const & createInfo) {
	Let pixelCount = (u64)createInfo.dim.x * createInfo.dim.y;
	Let byteCount = [&]() -> u64 {
		switch (createInfo.format) {
		case ImageFormat::r8g8b8a8_unorm:
			return pixelCount * 4;
		case ImageFormat::depth16_unorm:
			return pixelCount * 2;
		default:
			SRAT_ASSERT(false);
			return 0u;
		}
	}();
	Let impl = ImplImage {
		.data = std::vector<u8>(byteCount),
		.dim = createInfo.dim,
		.layout = createInfo.layout,
		.format = createInfo.format,
	};

	return sImagePool.allocate(impl);
}

void srat::gfx::image_destroy(Image const & image) {
	sImagePool.free(image);
}

u32v2 srat::gfx::image_dim(Image const & image) {
	ImplImage const * const impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	return impl->dim;
}

srat::slice<u8> srat::gfx::image_data8(Image const & image) {
	Let impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	return { impl->data.data(), impl->data.size() };
}

srat::slice<u16> srat::gfx::image_data16(Image const & image) {
	Let impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	SRAT_ASSERT(impl->data.size() % 2 == 0);
	return {
		reinterpret_cast<u16 *>(impl->data.data()),
		impl->data.size() / 2
	};
}

srat::slice<u32> srat::gfx::image_data32(Image const & image) {
	Let impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	SRAT_ASSERT(impl->data.size() % 4 == 0);
	return {
		reinterpret_cast<u32 *>(impl->data.data()),
		impl->data.size() / 4
	};
}
