#include <srat/image.hpp>

#include <srat/handle.hpp>

#include <vector>

// -----------------------------------------------------------------------------
// -- private
// -----------------------------------------------------------------------------

namespace {

struct ImplImage {
	std::vector<u8> data;
	i32v2 dim;
	srat::Layout layout;
	srat::Format format;
};

static srat::HandlePool<srat::Image, ImplImage> sImagePool = (
	srat::HandlePool<srat::Image, ImplImage>::create(128, "ImagePool")
);

}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

srat::Image srat::image_create(ImageCreateInfo const & createInfo) {
	u64 const pixelCount = (u64)createInfo.dim.x * createInfo.dim.y;
	u64 const byteCount = [&]() -> u64 {
		switch (createInfo.format) {
		case Format::r8g8b8a8_unorm:
			return pixelCount * 4;
		case Format::depth16_unorm:
			return pixelCount * 2;
		default:
			SRAT_ASSERT(false);
			return 0u;
		}
	}();
	ImplImage impl {
		.data = std::vector<u8>(byteCount),
		.dim = createInfo.dim,
		.layout = createInfo.layout,
		.format = createInfo.format,
	};
	return sImagePool.allocate(std::move(impl));
}

void srat::image_destroy(Image const & image) {
	sImagePool.free(image);
}

i32v2 srat::image_dim(Image const & image) {
	ImplImage * impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	return impl->dim;
}

u8 * srat::image_data(Image const & image) {
	ImplImage * impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	return impl->data.data();
}
