#include <srat/gfx-image.hpp>

#include <srat/core-handle.hpp>

#include <vector>

// -----------------------------------------------------------------------------
// -- private
// -----------------------------------------------------------------------------

namespace {

struct ImplImage {
	// raw data
	std::vector<u8> data;
	i32v2 dim;
	srat::gfx::ImageLayout layout;
	srat::gfx::ImageFormat format;
	// converted for sampling
	std::vector<float> channelR;
	std::vector<float> channelG;
	std::vector<float> channelB;
	std::vector<float> channelA;
};

static srat::HandlePool<srat::gfx::Image, ImplImage> sImagePool = (
	srat::HandlePool<srat::gfx::Image, ImplImage>::create(128, "ImagePool")
);

} // namespace

static u64 image_pixel_count(ImplImage const & impl) {
	return (u64)impl.dim.x * (u64)impl.dim.y;
}

static void image_apply_rgba_channels(
	ImplImage & impl
) {
	// if RGBA8 convert to RGBA float channels for sampling
	u64 const pixelCount = image_pixel_count(impl);
	if (impl.format == srat::gfx::ImageFormat::r8g8b8a8_unorm) {
		for (u64 i = 0; i < pixelCount; ++i) {
			impl.channelR[i] = (f32)impl.data[i * 4 + 0] / 255.f;
			impl.channelG[i] = (f32)impl.data[i * 4 + 1] / 255.f;
			impl.channelB[i] = (f32)impl.data[i * 4 + 2] / 255.f;
			impl.channelA[i] = (f32)impl.data[i * 4 + 3] / 255.f;
		}
	}
	// do same for depth but just R channel
	else if (impl.format == srat::gfx::ImageFormat::depth16_unorm) {
		for (u64 i = 0; i < pixelCount; ++i) {
			impl.channelR[i] = (
				(f32)(((u16 *)impl.data.data())[i]) / (f32)UINT16_MAX
			);
		}
	}
}

static f32v4x8 image_sample_nearest(
	ImplImage const & impl,
	i32v2x8 const & texelCoord
) {
	i32v2 const & dim = impl.dim;
	// -- convert to indices
	i32x8 const idx = (texelCoord.y * i32x8_splat(dim.x)) + texelCoord.x;
	// -- gather texels
	f32x8 const texR = f32x8_memory_gather(impl.channelR.data(), idx);
	f32x8 const texG = f32x8_memory_gather(impl.channelG.data(), idx);
	f32x8 const texB = f32x8_memory_gather(impl.channelB.data(), idx);
	f32x8 const texA = f32x8_memory_gather(impl.channelA.data(), idx);
	return f32v4x8 { texR, texG, texB, texA };
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
	auto impl = ImplImage {
		.data = {},
		.dim = createInfo.dim,
		.layout = createInfo.layout,
		.format = createInfo.format,
		.channelR = std::vector<float>(pixelCount),
		.channelG = std::vector<float>(pixelCount),
		.channelB = std::vector<float>(pixelCount),
		.channelA = std::vector<float>(pixelCount),
	};
	impl.data.resize(byteCount);

	if (createInfo.optInitialData.ptr() != nullptr) {
		SRAT_ASSERT(createInfo.optInitialData.size() == byteCount);
		std::memcpy(
			impl.data.data(),
			createInfo.optInitialData.ptr(),
			createInfo.optInitialData.size()
		);
		image_apply_rgba_channels(impl);
	}

	return sImagePool.allocate(impl);
}

void srat::gfx::image_destroy(Image const & image) {
	sImagePool.free(image);
}

i32v2 srat::gfx::image_dim(Image const & image) {
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

f32v4x8 srat::gfx::image_sample(
	Image const & image,
	f32v2x8 const & uv
) {
	ImplImage const * const impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	// -- bilinear filter (for now)
	i32v2 const dim = impl->dim;

	f32v2x8 const uvWrapped = f32v2x8_fract(uv);

	f32v2x8 const uvScaled = (
		  uvWrapped * f32v2x8_splat((f32)dim.x, (f32)dim.y)
		- f32v2x8_splat(0.5f, 0.5f)
	);

	// -- compute texel coordinates
	i32x8 const texelX0 = (
		i32x8_clamp(
			i32x8_floor(uvScaled.v[0]),
			i32x8_splat(0),
			i32x8_splat(dim.x - 2)
		)
	);
	i32x8 const texelY0 = (
		i32x8_clamp(
			i32x8_floor(uvScaled.v[1]),
			i32x8_splat(0),
			i32x8_splat(dim.y - 2)
		)
	);
	i32x8 const texelX1 = texelX0 + i32x8_splat(1);
	i32x8 const texelY1 = texelY0 + i32x8_splat(1);

	// -- fetch texels
	f32v4x8 const c00 = image_sample_nearest(*impl, i32v2x8 {texelX0, texelY0});
	f32v4x8 const c10 = image_sample_nearest(*impl, i32v2x8 {texelX1, texelY0});
	f32v4x8 const c01 = image_sample_nearest(*impl, i32v2x8 {texelX0, texelY1});
	f32v4x8 const c11 = image_sample_nearest(*impl, i32v2x8 {texelX1, texelY1});

	// -- interpolate
	f32x8 const dx = uvScaled.v[0] - f32x8_from_i32x8(texelX0);
	f32x8 const dy = uvScaled.v[1] - f32x8_from_i32x8(texelY0);
	f32v4x8 const c0 = f32v4x8_mix(c00, c10, dx);
	f32v4x8 const c1 = f32v4x8_mix(c01, c11, dx);
	return f32v4x8_mix(c0, c1, dy);
}
