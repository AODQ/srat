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
		printf(
			"image_create: copying initial data of size %zu bytes (size: %zu)\n",
			(uint64_t)createInfo.optInitialData.size(),
			(uint64_t)impl.data.size()
		);
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
	Let impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	SRAT_ASSERT(impl->format == ImageFormat::r8g8b8a8_unorm);
	// only support reasonably sized textures for now
	SRAT_ASSERT(impl->dim.x > 0 && impl->dim.x <= 4096);
	SRAT_ASSERT(impl->dim.y > 0 && impl->dim.y <= 4096);
	i32v2 const & dim = impl->dim;
	// -- compute texel coord wrapped
	f32v2x8 const scaledUV = uv * f32v2x8_splat((f32)dim.x, (f32)dim.y);
	// wrap the scaled UV
	f32v2x8 const wrappedUV = (
		f32v2x8_modulo(scaledUV, f32v2x8_splat((f32)dim.x, (f32)dim.y))
	);
	// {
	// 	// for now just return the UV
	// 	f32v4x8 debugColor = f32v4x8 {
	// 		wrappedUV.x,
	// 		wrappedUV.y,
	// 		f32x8_splat(0.0f),
	// 		f32x8_splat(0.0f)
	// 	};
	// 	// normalize for debug visualization
	// 	debugColor = debugColor / f32v4x8_splat((f32)dim.x, (f32)dim.y, 1.0f, 1.0f);
	// 	return debugColor;
	// }
	i32v2x8 const texelCoord = f32v2x8_to_i32v2x8_floor(wrappedUV);
	// below reference for clamped
	i32v2x8 const clampedCoord = (
		i32v2x8_clamp(
			texelCoord,
			i32v2x8_splat(0, 0),
			i32v2x8_splat((i32)dim.x - 1, (i32)dim.y - 1)
		)
	);
	// -- convert to indices
	i32x8 const idx = (clampedCoord.y * i32x8_splat(dim.x)) + clampedCoord.x;
	// -- gather texels
	f32x8 const texR = f32x8_memory_gather(impl->channelR.data(), idx);
	f32x8 const texG = f32x8_memory_gather(impl->channelG.data(), idx);
	f32x8 const texB = f32x8_memory_gather(impl->channelB.data(), idx);
	f32x8 const texA = f32x8_memory_gather(impl->channelA.data(), idx);
	return f32v4x8 { texR, texG, texB, texA };
}

f32v4 srat::gfx::image_reference_sample(
	Image const & image,
	f32v2 const & uv
) {
	Let impl = sImagePool.get(image);
	SRAT_ASSERT(impl != nullptr);
	SRAT_ASSERT(impl->format == ImageFormat::r8g8b8a8_unorm);
	i32v2 const & dim = impl->dim;
	// -- compute texel coord clamped
	f32v2 const scaledUV = uv * f32v2((f32)dim.x, (f32)dim.y);
	i32v2 const texelCoord = i32v2(
		(int)scaledUV.x,
		(int)scaledUV.y
	);
	i32v2 const clampedCoord = i32v2(
		i32_clamp(texelCoord.x, 0, (int)dim.x - 1),
		i32_clamp(texelCoord.y, 0, (int)dim.y - 1)
	);
	u64 const idx = (u64)(clampedCoord.y * dim.x + clampedCoord.x);
	return {
		impl->channelR[idx],
		impl->channelG[idx],
		impl->channelB[idx],
		impl->channelA[idx]
 };
}
