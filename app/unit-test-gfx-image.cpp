#include <srat/gfx-image.hpp>

#include <doctest/doctest.h>

TEST_SUITE("image") {

// -----------------------------------------------------------------------------
// -- image_create / image_destroy
// -----------------------------------------------------------------------------

TEST_CASE("image [create r8g8b8a8_unorm]") {
	auto img = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	CHECK(img.id != 0);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [create depth16_unorm]") {
	auto img = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	CHECK(img.id != 0);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [create tiled layout]") {
	auto img = srat::gfx::image_create({
		.dim	= { 32, 32 },
		.layout = srat::gfx::ImageLayout::Tiled,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	CHECK(img.id != 0);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [create 1x1]") {
	auto img = srat::gfx::image_create({
		.dim	= { 1, 1 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	CHECK(img.id != 0);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [unique ids]") {
	auto a = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto b = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	CHECK(a.id != b.id);
	srat::gfx::image_destroy(a);
	srat::gfx::image_destroy(b);
}

TEST_CASE("image [destroy then recreate]") {
	auto a = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	srat::gfx::image_destroy(a);

	auto b = srat::gfx::image_create({
		.dim	= { 64, 64 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	CHECK(b.id != 0);
	// stale handle must not alias the new image
	CHECK(a.id != b.id);
	srat::gfx::image_destroy(b);
}

// -----------------------------------------------------------------------------
// -- image_dim
// -----------------------------------------------------------------------------

TEST_CASE("image [dim preserved r8g8b8a8]") {
	auto img = srat::gfx::image_create({
		.dim	= { 128, 256 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto const dim = srat::gfx::image_dim(img);
	CHECK(dim.x == 128);
	CHECK(dim.y == 256);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [dim preserved depth16]") {
	auto img = srat::gfx::image_create({
		.dim	= { 320, 240 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	auto const dim = srat::gfx::image_dim(img);
	CHECK(dim.x == 320);
	CHECK(dim.y == 240);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [dim 1x1]") {
	auto img = srat::gfx::image_create({
		.dim	= { 1, 1 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto const dim = srat::gfx::image_dim(img);
	CHECK(dim.x == 1);
	CHECK(dim.y == 1);
	srat::gfx::image_destroy(img);
}

// -----------------------------------------------------------------------------
// -- image_data8
// -----------------------------------------------------------------------------

TEST_CASE("image [data_8 size r8g8b8a8_unorm]") {
	u32 const w = 64, h = 32;
	auto img = srat::gfx::image_create({
		.dim	= { w, h },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto data = srat::gfx::image_data8(img);
	// 4 bytes per pixel
	CHECK(data.size() == (u64)w * h * 4);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_8 zero initialized]") {
	auto img = srat::gfx::image_create({
		.dim	= { 4, 4 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto data = srat::gfx::image_data8(img);
	for (u8 const & i : data) {
		CHECK(data[i] == 0);
	}
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_8 writable]") {
	auto img = srat::gfx::image_create({
		.dim	= { 2, 2 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto data = srat::gfx::image_data8(img);
	for (u64 i = 0; i < data.size(); ++i) {
		data[i] = (u8)(i & 0xFF);
	}
	// re-fetch and verify persistence
	auto data2 = srat::gfx::image_data8(img);
	for (u64 i = 0; i < data2.size(); ++i) {
		CHECK(data2[i] == (u8)(i & 0xFF));
	}
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_8 non-null]") {
	auto img = srat::gfx::image_create({
		.dim	= { 8, 8 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto data = srat::gfx::image_data8(img);
	CHECK(data.ptr() != nullptr);
	srat::gfx::image_destroy(img);
}

// -----------------------------------------------------------------------------
// -- image_data16
// -----------------------------------------------------------------------------

TEST_CASE("image [data_16 size depth16_unorm]") {
	u32 const w = 64, h = 32;
	auto img = srat::gfx::image_create({
		.dim	= { w, h },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	auto data = srat::gfx::image_data16(img);
	// 1 u16 per pixel
	CHECK(data.size() == (u64)w * h);
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_16 zero initialized]") {
	auto img = srat::gfx::image_create({
		.dim	= { 4, 4 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	auto data = srat::gfx::image_data16(img);
	for (u16 const & i : data) {
		CHECK(data[i] == 0);
	}
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_16 writable]") {
	auto img = srat::gfx::image_create({
		.dim	= { 4, 4 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	auto data = srat::gfx::image_data16(img);
	for (u64 i = 0; i < data.size(); ++i) {
		data[i] = (u16)(i * 256);
	}
	auto data2 = srat::gfx::image_data16(img);
	for (u64 i = 0; i < data2.size(); ++i) {
		CHECK(data2[i] == (u16)(i * 256));
	}
	srat::gfx::image_destroy(img);
}

TEST_CASE("image [data_16 non-null]") {
	auto img = srat::gfx::image_create({
		.dim	= { 8, 8 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});
	auto data = srat::gfx::image_data16(img);
	CHECK(data.ptr() != nullptr);
	srat::gfx::image_destroy(img);
}

// -----------------------------------------------------------------------------
// -- multiple live images
// -----------------------------------------------------------------------------

TEST_CASE("image [multiple live images independent data]") {
	auto a = srat::gfx::image_create({
		.dim	= { 4, 4 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto b = srat::gfx::image_create({
		.dim	= { 4, 4 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});

	auto da = srat::gfx::image_data8(a);
	auto db = srat::gfx::image_data8(b);

	// write distinct patterns
	for (u8 & i : da) i = 0xaa;
	for (u8 & i : db) i = 0xbb;

	// verify no aliasing
	auto da2 = srat::gfx::image_data8(a);
	auto db2 = srat::gfx::image_data8(b);
	for (u8 const & i : da2) CHECK(i == 0xAA);
	for (u8 const & i : db2) CHECK(i == 0xBB);

	srat::gfx::image_destroy(a);
	srat::gfx::image_destroy(b);
}

TEST_CASE("image [mixed formats live simultaneously]") {
	auto color = srat::gfx::image_create({
		.dim	= { 16, 16 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	auto depth = srat::gfx::image_create({
		.dim	= { 16, 16 },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});

	auto cd = srat::gfx::image_data8(color);
	auto dd = srat::gfx::image_data16(depth);

	CHECK(cd.size() == 16 * 16 * 4);
	CHECK(dd.size() == 16 * 16);
	CHECK(cd.ptr() != nullptr);
	CHECK(dd.ptr() != nullptr);

	srat::gfx::image_destroy(color);
	srat::gfx::image_destroy(depth);
}

} // -- end gfx image test suite
