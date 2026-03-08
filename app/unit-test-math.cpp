#include <srat/types.hpp>
#include <srat/math.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <random>

// -- note many of these unit tests are AI generated so that they may
//	have very high coverage (though I also write edge case tests)

TEST_SUITE("math") {

TEST_CASE("simple f32x8") {
	// verify that f32x8_load/f32x8_store works
	alignas(32) float data[8] = { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
	f32x8 v = f32x8_load(data);
	alignas(32) float out[8];
	f32x8_store(v, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == data[i]);
	}

	// verify that basic arithmetic works
	{
		f32x8 a = f32x8_load(data);
		f32x8 b = f32x8_splat(2.f);
		f32x8 c = a * b + b;
		f32x8_store(c, out);
		for (size_t i = 0; i < 8; ++i) {
			CHECK(out[i] == data[i] * 2.f + 2.f);
		}
	}
	// verify that basic arithmetic works
	{
		f32x8 a = f32x8_load(data);
		f32x8 b = f32x8_splat(2.f);
		f32x8 c = a / b - b;
		f32x8_store(c, out);
		for (size_t i = 0; i < 8; ++i) {
			CHECK(out[i] == data[i] / 2.f - 2.f);
		}
	}
}

TEST_CASE("f32x8 negation") {
	alignas(32) float data[8] = { 1.f, -2.f, 3.f, -4.f, 5.f, -6.f, 7.f, -8.f };
	f32x8 v = f32x8_load(data);
	f32x8 neg = -v;
	alignas(32) float out[8];
	f32x8_store(neg, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == -data[i]);
	}
}

TEST_CASE("f32x8 sqrt") {
	alignas(32) float data[8] = { 1.f, 4.f, 9.f, 16.f, 25.f, 36.f, 49.f, 64.f };
	f32x8 v = f32x8_load(data);
	f32x8 sqrt = f32x8_sqrt(v);
	alignas(32) float out[8];
	f32x8_store(sqrt, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == std::sqrt(data[i]));
	}
}

TEST_CASE("f32x8 comparison and select") {
	alignas(32) float a_data[8] = { 1.f, 5.f, 3.f, 7.f, 2.f, 6.f, 4.f, 8.f };
	f32x8 a = f32x8_load(a_data);
	f32x8 b = f32x8_splat(3.f);
	u32x8 mask = a < b;
	f32x8 result = f32x8_select(mask, b, a);
	alignas(32) float out[8];
	f32x8_store(result, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_data[i] < 3.f ? a_data[i] : 3.f));
	}
}

TEST_CASE("f32x8 min max") {
	alignas(32) float a_data[8] = { 1.f, 5.f, 3.f, 7.f, 2.f, 6.f, 4.f, 8.f };
	f32x8 a = f32x8_load(a_data);
	f32x8 b = f32x8_splat(4.f);
	alignas(32) float out[8];

	f32x8_store(f32x8_min(a, b), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == std::min(a_data[i], 4.f));
	}

	f32x8_store(f32x8_max(a, b), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == std::max(a_data[i], 4.f));
	}
}

TEST_CASE("f32x8 fmadd") {
	alignas(32) float a_data[8] = { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
	f32x8 a = f32x8_load(a_data);
	f32x8 b = f32x8_splat(3.f);
	f32x8 c = f32x8_splat(1.f);
	alignas(32) float out[8];
	f32x8_store(f32x8_fmadd(a, b, c), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == a_data[i] * 3.f + 1.f);
	}
}

TEST_CASE("f32x8 zero") {
	alignas(32) float out[8];
	f32x8_store(f32x8_zero(), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 0.f);
	}
}

TEST_CASE("f32x8 comparisons") {
	alignas(32) float a_data[8] = { 1.f, 5.f, 3.f, 7.f, 2.f, 6.f, 4.f, 8.f };
	alignas(32) float out[8];
	f32x8 a = f32x8_load(a_data);
	f32x8 b = f32x8_splat(4.f);

	// operator>=
	f32x8_store(f32x8_select(a >= b, f32x8_splat(1.f), f32x8_splat(0.f)), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_data[i] < 4.f ? 1.f : 0.f));
	}

	// operator<=
	f32x8_store(f32x8_select(a <= b, f32x8_splat(1.f), f32x8_splat(0.f)), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_data[i] > 4.f ? 1.f : 0.f));
	}

	// operator>
	f32x8_store(f32x8_select(a > b, f32x8_splat(1.f), f32x8_splat(0.f)), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_data[i] <= 4.f ? 1.f : 0.f));
	}

	// operator<
	f32x8_store(f32x8_select(a < b, f32x8_splat(1.f), f32x8_splat(0.f)), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_data[i] >= 4.f ? 1.f : 0.f));
	}
}
TEST_CASE("f32v4x8 dot") {
	f32v4x8 a = f32v4x8_splat(1.f, 0.f, 0.f, 0.f);
	f32v4x8 b = f32v4x8_splat(1.f, 0.f, 0.f, 0.f);
	alignas(32) float out[8];
	f32x8_store(f32v4x8_dot(a, b), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 1.f);
	}

	// dot of (1,2,3,4) with (1,2,3,4) = 1+4+9+16 = 30
	a = f32v4x8_splat(1.f, 2.f, 3.f, 4.f);
	b = f32v4x8_splat(1.f, 2.f, 3.f, 4.f);
	f32x8_store(f32v4x8_dot(a, b), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 30.f);
	}

	// perpendicular vectors
	a = f32v4x8_splat(1.f, 0.f, 0.f, 0.f);
	b = f32v4x8_splat(0.f, 1.f, 0.f, 0.f);
	f32x8_store(f32v4x8_dot(a, b), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 0.f);
	}
}

TEST_CASE("f32v4x8 cross") {
	alignas(32) float out_x[8], out_y[8], out_z[8];

	// x cross y = z
	f32v4x8 x_axis = f32v4x8_splat(1.f, 0.f, 0.f, 0.f);
	f32v4x8 y_axis = f32v4x8_splat(0.f, 1.f, 0.f, 0.f);
	f32v4x8 result = f32v4x8_cross(x_axis, y_axis);
	f32x8_store(result.x, out_x);
	f32x8_store(result.y, out_y);
	f32x8_store(result.z, out_z);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out_x[i] == 0.f);
		CHECK(out_y[i] == 0.f);
		CHECK(out_z[i] == 1.f);
	}

	// y cross x = -z
	result = f32v4x8_cross(y_axis, x_axis);
	f32x8_store(result.x, out_x);
	f32x8_store(result.y, out_y);
	f32x8_store(result.z, out_z);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out_x[i] == 0.f);
		CHECK(out_y[i] == 0.f);
		CHECK(out_z[i] == -1.f);
	}

	// y cross z = x
	f32v4x8 z_axis = f32v4x8_splat(0.f, 0.f, 1.f, 0.f);
	result = f32v4x8_cross(y_axis, z_axis);
	f32x8_store(result.x, out_x);
	f32x8_store(result.y, out_y);
	f32x8_store(result.z, out_z);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out_x[i] == 1.f);
		CHECK(out_y[i] == 0.f);
		CHECK(out_z[i] == 0.f);
	}
}

TEST_CASE("f32v4x8 normalize") {
	alignas(32) float out_x[8], out_y[8], out_z[8];

	// normalizing an axis vector should return itself
	f32v4x8 x_axis = f32v4x8_splat(1.f, 0.f, 0.f, 0.f);
	f32v4x8 result = f32v4x8_normalize(x_axis);
	f32x8_store(result.x, out_x);
	f32x8_store(result.y, out_y);
	f32x8_store(result.z, out_z);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out_x[i] == doctest::Approx(1.f).epsilon(0.001f));
		CHECK(out_y[i] == doctest::Approx(0.f).epsilon(0.001f));
		CHECK(out_z[i] == doctest::Approx(0.f).epsilon(0.001f));
	}

	// normalized length should be 1
	f32v4x8 v = f32v4x8_splat(1.f, 2.f, 3.f, 0.f);
	result = f32v4x8_normalize(v);
	f32x8 len_sq = f32v4x8_dot(result, result);
	alignas(32) float len_out[8];
	f32x8_store(len_sq, len_out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(len_out[i] == doctest::Approx(1.f).epsilon(0.001f));
	}
}

TEST_CASE("f32m44x8 broadcast and store") {
	float m[16] = {
		1.f, 0.f, 0.f, 0.f,  // col 0
		0.f, 1.f, 0.f, 0.f,  // col 1
		0.f, 0.f, 1.f, 0.f,  // col 2
		0.f, 0.f, 0.f, 1.f,  // col 3
	};
	f32m44x8 mat = f32m44x8_broadcast(m);
	float out[16];
	f32m44x8_store(mat, out);
	// every lane should have the same matrix
	for (size_t col = 0; col < 4; ++col) {
		for (size_t row = 0; row < 4; ++row) {
			CHECK(out[col * 4 + row] == m[col * 4 + row]);
		}
	}
}

TEST_CASE("f32m44x8 mul_vec identity") {
	float m[16] = {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f,
	};
	f32m44x8 mat = f32m44x8_broadcast(m);
	f32v4x8 v = f32v4x8_splat(2.f, 3.f, 4.f, 1.f);
	f32v4x8 result = f32m44x8_mul_vec(mat, v);
	alignas(32) float out[8];
	f32x8_store(result.v[0], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(2.f).epsilon(0.001f)); }
	f32x8_store(result.v[1], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(3.f).epsilon(0.001f)); }
	f32x8_store(result.v[2], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(4.f).epsilon(0.001f)); }
	f32x8_store(result.v[3], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(1.f).epsilon(0.001f)); }
}

TEST_CASE("f32m44x8 mul_vec translation") {
	// column major translation matrix: translate by (5, 6, 7)
	float m[16] = {
		1.f, 0.f, 0.f, 0.f,  // col 0
		0.f, 1.f, 0.f, 0.f,  // col 1
		0.f, 0.f, 1.f, 0.f,  // col 2
		5.f, 6.f, 7.f, 1.f,  // col 3
	};
	f32m44x8 mat = f32m44x8_broadcast(m);
	f32v4x8 v = f32v4x8_splat(1.f, 2.f, 3.f, 1.f);
	f32v4x8 result = f32m44x8_mul_vec(mat, v);
	alignas(32) float out[8];
	f32x8_store(result.v[0], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(6.f).epsilon(0.001f)); }
	f32x8_store(result.v[1], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(8.f).epsilon(0.001f)); }
	f32x8_store(result.v[2], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(10.f).epsilon(0.001f)); }
}

TEST_CASE("f32m44x8 mul_vec scale") {
	float m[16] = {
		2.f, 0.f, 0.f, 0.f,
		0.f, 3.f, 0.f, 0.f,
		0.f, 0.f, 4.f, 0.f,
		0.f, 0.f, 0.f, 1.f,
	};
	f32m44x8 mat = f32m44x8_broadcast(m);
	f32v4x8 v = f32v4x8_splat(1.f, 2.f, 3.f, 1.f);
	f32v4x8 result = f32m44x8_mul_vec(mat, v);
	alignas(32) float out[8];
	f32x8_store(result.v[0], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(2.f).epsilon(0.001f)); }
	f32x8_store(result.v[1], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(6.f).epsilon(0.001f)); }
	f32x8_store(result.v[2], out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == doctest::Approx(12.f).epsilon(0.001f)); }
}


TEST_CASE("i32x8 load/store roundtrip") {
	alignas(32) int in[8]  = { 0, 1, -2, 3, -4, 5, -6, 7 };
	alignas(32) int out[8] = { 0 };

	i32x8 v = i32x8_load(in);
	i32x8_store(v, out);

	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == in[i]);
	}
}

TEST_CASE("u32x8 load/store roundtrip") {
	alignas(32) unsigned int in[8]  = {
		0u, 1u, 2u, 3u, 0x80000000u, 0xFFFFFFFFu, 42u, 999u
	};
	alignas(32) unsigned int out[8] = { 0 };

	u32x8 v = u32x8_load(in);
	u32x8_store(v, out);

	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == in[i]);
	}
}

TEST_CASE("i32x8 lane0") {
	alignas(32) int in[8] = { 123, 1, 2, 3, 4, 5, 6, 7 };
	i32x8 v = i32x8_load(in);
	CHECK(i32x8_lane0(v) == 123);
}

TEST_CASE("u32x8 lane0") {
	alignas(32) unsigned int in[8] = { 123u, 1u, 2u, 3u, 4u, 5u, 6u, 7u };
	u32x8 v = u32x8_load(in);
	CHECK(u32x8_lane0(v) == 123u);
}

TEST_CASE("i32x8 splat/zero") {
	alignas(32) int out[8];

	i32x8_store(i32x8_zero(), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 0);
	}

	i32x8_store(i32x8_splat(-7), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == -7);
	}
}

TEST_CASE("u32x8 splat/zero") {
	alignas(32) unsigned int out[8];

	u32x8_store(u32x8_zero(), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 0u);
	}

	u32x8_store(u32x8_splat(0xDEADBEEFu), out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == 0xDEADBEEFu);
	}
}

TEST_CASE("u32x8 bitwise and/or/xor/not") {
	alignas(32) unsigned int a_in[8] = {
		0x00000000u, 0xFFFFFFFFu, 0xAAAAAAAAu, 0x55555555u,
		0x12345678u, 0x87654321u, 0x80000000u, 0x00000001u
	};
	alignas(32) unsigned int b_in[8] = {
		0xFFFFFFFFu, 0x00000000u, 0x0F0F0F0Fu, 0xF0F0F0F0u,
		0xFFFF0000u, 0x0000FFFFu, 0x7FFFFFFFu, 0x00000003u
	};

	u32x8 a = u32x8_load(a_in);
	u32x8 b = u32x8_load(b_in);

	alignas(32) unsigned int out[8];

	u32x8_store(a & b, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_in[i] & b_in[i]));
	}

	u32x8_store(a | b, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_in[i] | b_in[i]));
	}

	u32x8_store(a ^ b, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (a_in[i] ^ b_in[i]));
	}

	u32x8_store(~a, out);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(out[i] == (~a_in[i]));
	}
}

TEST_CASE("u32x8 ballot/any/all uses sign bit") {
	// movemask_ps looks at the sign bit of each 32-bit lane (as float).
	// So lanes with high bit set (0x80000000) should produce 1 bits.
	alignas(32) unsigned int lanes[8] = {
		0x00000000u, // 0
		0x80000000u, // 1
		0x7FFFFFFFu, // 0
		0xFFFFFFFFu, // 1
		0x80000001u, // 1
		0x00000001u, // 0
		0x00000000u, // 0
		0x80000000u, // 1
	};

	u32x8 v = u32x8_load(lanes);
	u32 mask = u32x8_ballot(v);

	// expected bits: lane i sets bit i
	u32 expected =
		(0u << 0) |
		(1u << 1) |
		(0u << 2) |
		(1u << 3) |
		(1u << 4) |
		(0u << 5) |
		(0u << 6) |
		(1u << 7);

	CHECK(mask == expected);
	CHECK(u32x8_any(v));
	CHECK_FALSE(u32x8_all(v));
}

TEST_CASE("u32x8 all/any edge cases") {
	{
		u32x8 v = u32x8_zero();
		CHECK(u32x8_ballot(v) == 0u);
		CHECK_FALSE(u32x8_any(v));
		CHECK_FALSE(u32x8_all(v));
	}
	{
		// all lanes sign bit set => ballot == 0xFF
		u32x8 v = u32x8_splat(0x80000000u);
		CHECK(u32x8_ballot(v) == 0xFFu);
		CHECK(u32x8_any(v));
		CHECK(u32x8_all(v));
	}
}

TEST_CASE("f32x8_triangle_edge point on left/right/on edge") {
	// edge from (0,0) to (1,0) — horizontal rightward
	// points above (y>0) should be positive (left), below negative (right)
	f32x8 const px = f32x8_splat(0.5f);
	f32x8 const py_above = f32x8_splat( 1.f);
	f32x8 const py_below = f32x8_splat(-1.f);
	f32x8 const py_on    = f32x8_splat( 0.f);

	alignas(32) float out[8];

	f32x8_store(f32x8_barycentric(f32v2(0.f, 0.f), f32v2(1.f, 0.f), px, py_above), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] > 0.f); }

	f32x8_store(f32x8_barycentric(f32v2(0.f, 0.f), f32v2(1.f, 0.f), px, py_below), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] < 0.f); }

	f32x8_store(f32x8_barycentric(f32v2(0.f, 0.f), f32v2(1.f, 0.f), px, py_on), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] == 0.f); }
}

TEST_CASE("f32x8_triangle_edge point inside triangle") {
	// triangle (0,0) (4,0) (2,4) — centroid at (2, 4/3)
	f32x8 const px = f32x8_splat(2.f);
	f32x8 const py = f32x8_splat(1.f);

	alignas(32) float out[8];

	f32x8_store(f32x8_barycentric(f32v2(0.f, 0.f), f32v2(4.f, 0.f), px, py), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] > 0.f); }

	f32x8_store(f32x8_barycentric(f32v2(4.f, 0.f), f32v2(2.f, 4.f), px, py), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] > 0.f); }

	f32x8_store(f32x8_barycentric(f32v2(2.f, 4.f), f32v2(0.f, 0.f), px, py), out);
	for (size_t i = 0; i < 8; ++i) { CHECK(out[i] > 0.f); }
}

TEST_CASE("f32x8 rsqrt") {
	alignas(32) float data[8] = { 1.f, 4.f, 9.f, 16.f, 25.f, 36.f, 49.f, 64.f };
	f32x8 v = f32x8_load(data);
	alignas(32) float out[8];
	f32x8_store(f32x8_rsqrt(v), out);
	for (size_t i = 0; i < 8; ++i) {
		// rsqrt is approximate (~0.1% error)
		CHECK(out[i] == doctest::Approx(1.f / std::sqrt(data[i])).epsilon(0.002f));
	}
}

TEST_CASE("f32x8 lane()") {
	alignas(32) float data[8] = { 10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f };
	f32x8 v = f32x8_load(data);
	for (size_t i = 0; i < 8; ++i) {
		CHECK(v.lane(i) == doctest::Approx(data[i]).epsilon(0.001f));
	}
}

// -----------------------------------------------------------------------------
// -- scalar i32/f32 helpers
// -----------------------------------------------------------------------------

TEST_CASE("i32 min/max/clamp") {
	CHECK(i32_min(3, 5)  == 3);
	CHECK(i32_min(-1, 1) == -1);
	CHECK(i32_min(7, 7)  == 7);

	CHECK(i32_max(3, 5)  == 5);
	CHECK(i32_max(-1, 1) == 1);
	CHECK(i32_max(7, 7)  == 7);

	CHECK(i32_clamp(5,  0, 10) == 5);
	CHECK(i32_clamp(-5, 0, 10) == 0);
	CHECK(i32_clamp(15, 0, 10) == 10);
	CHECK(i32_clamp(0,  0, 10) == 0);
	CHECK(i32_clamp(10, 0, 10) == 10);
}

TEST_CASE("f32 min/max/clamp") {
	CHECK(f32_min(1.f, 2.f)  == 1.f);
	CHECK(f32_min(-1.f, 1.f) == -1.f);

	CHECK(f32_max(1.f, 2.f)  == 2.f);
	CHECK(f32_max(-1.f, 1.f) == 1.f);

	CHECK(f32_clamp(0.5f, 0.f, 1.f) == 0.5f);
	CHECK(f32_clamp(-1.f, 0.f, 1.f) == 0.f);
	CHECK(f32_clamp(2.f,  0.f, 1.f) == 1.f);
}

// -----------------------------------------------------------------------------
// -- f32m44 scalar
// -----------------------------------------------------------------------------

TEST_CASE("f32m44 identity mul_vec") {
	f32m44 m = f32m44_identity();
	f32v4 v = { 2.f, 3.f, 4.f, 1.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(2.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(3.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(4.f).epsilon(0.001f));
	CHECK(r.w == doctest::Approx(1.f).epsilon(0.001f));
}

TEST_CASE("f32m44 translate mul_vec") {
	f32m44 m = f32m44_translate(5.f, 6.f, 7.f);
	f32v4 v = { 1.f, 2.f, 3.f, 1.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(6.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(8.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(10.f).epsilon(0.001f));
	CHECK(r.w == doctest::Approx(1.f).epsilon(0.001f));
}

TEST_CASE("f32m44 translate w=0 not affected") {
	// direction vectors (w=0) should NOT be translated
	f32m44 m = f32m44_translate(5.f, 6.f, 7.f);
	f32v4 v = { 1.f, 0.f, 0.f, 0.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(1.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.w == doctest::Approx(0.f).epsilon(0.001f));
}

TEST_CASE("f32m44 scale mul_vec") {
	f32m44 m = f32m44_scale(2.f, 3.f, 4.f);
	f32v4 v = { 1.f, 1.f, 1.f, 1.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(2.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(3.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(4.f).epsilon(0.001f));
}

TEST_CASE("f32m44 rotate_x 90deg") {
	f32m44 m = f32m44_rotate_x(3.14159265f * 0.5f);
	// (0,1,0) rotated 90deg around X -> (0,0,1)
	f32v4 v = { 0.f, 1.f, 0.f, 0.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(1.f).epsilon(0.001f));
}

TEST_CASE("f32m44 rotate_y 90deg") {
	f32m44 m = f32m44_rotate_y(3.14159265f * 0.5f);
	// (1,0,0) rotated 90deg around Y -> (0,0,-1)
	f32v4 v = { 1.f, 0.f, 0.f, 0.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(-1.f).epsilon(0.001f));
}

TEST_CASE("f32m44 rotate_z 90deg") {
	f32m44 m = f32m44_rotate_z(3.14159265f * 0.5f);
	// (1,0,0) rotated 90deg around Z -> (0,1,0)
	f32v4 v = { 1.f, 0.f, 0.f, 0.f };
	f32v4 r = m * v;
	CHECK(r.x == doctest::Approx(0.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(1.f).epsilon(0.001f));
	CHECK(r.z == doctest::Approx(0.f).epsilon(0.001f));
}

TEST_CASE("f32m44 operator* mat x mat identity") {
	f32m44 a = f32m44_translate(1.f, 2.f, 3.f);
	f32m44 i = f32m44_identity();
	f32m44 r = a * i;
	f32v4 v = { 0.f, 0.f, 0.f, 1.f };
	f32v4 out = r * v;
	CHECK(out.x == doctest::Approx(1.f).epsilon(0.001f));
	CHECK(out.y == doctest::Approx(2.f).epsilon(0.001f));
	CHECK(out.z == doctest::Approx(3.f).epsilon(0.001f));
}

TEST_CASE("f32m44 operator* mat x mat compose translate + scale") {
	// scale then translate: scale(2,2,2) * translate(1,1,1)
	// point (1,1,1,1): translate -> (2,2,2,1), scale -> (4,4,4,1)
	f32m44 t = f32m44_translate(1.f, 1.f, 1.f);
	f32m44 s = f32m44_scale(2.f, 2.f, 2.f);
	f32m44 r = s * t; // column-major: apply t first, then s
	f32v4 v = { 1.f, 1.f, 1.f, 1.f };
	f32v4 out = r * v;
	CHECK(out.x == doctest::Approx(4.f).epsilon(0.001f));
	CHECK(out.y == doctest::Approx(4.f).epsilon(0.001f));
	CHECK(out.z == doctest::Approx(4.f).epsilon(0.001f));
}

TEST_CASE("f32m44 perspective w component") {
	// a point at z=-near should map to w=near after projection
	f32 const zNear = 0.1f, zFar = 100.f;
	f32m44 p = f32m44_perspective(3.14159265f * 0.5f, 1.f, zNear, zFar);
	f32v4 v = { 0.f, 0.f, -zNear, 1.f };
	f32v4 r = p * v;
	// w = -v.z = zNear (standard perspective: w_clip = -z_view)
	CHECK(r.w == doctest::Approx(zNear).epsilon(0.001f));
}

TEST_CASE("f32m44 perspective far plane maps to w=zFar") {
	f32 const zNear = 0.1f, zFar = 100.f;
	f32m44 p = f32m44_perspective(3.14159265f * 0.5f, 1.f, zNear, zFar);
	f32v4 v = { 0.f, 0.f, -zFar, 1.f };
	f32v4 r = p * v;
	CHECK(r.w == doctest::Approx(zFar).epsilon(0.01f));
	// NDC z after divide should be ~1
	CHECK(r.z / r.w == doctest::Approx(1.f).epsilon(0.001f));
}

// -----------------------------------------------------------------------------
// -- rasterizer math helpers
// -----------------------------------------------------------------------------

TEST_CASE("f32v2_triangle_area CCW positive") {
	// CCW triangle: (0,0),(1,0),(0,1) — area = 0.5, signed = +
	f32 area = f32v2_triangle_area(
		f32v2{0.f,0.f}, f32v2{1.f,0.f}, f32v2{0.f,1.f}
	);
	CHECK(area == doctest::Approx(1.f).epsilon(0.001f)); // 2x area
}

TEST_CASE("f32v2_triangle_area CW negative") {
	// CW winding should give negative
	f32 area = f32v2_triangle_area(
		f32v2{0.f,0.f}, f32v2{0.f,1.f}, f32v2{1.f,0.f}
	);
	CHECK(area == doctest::Approx(-1.f).epsilon(0.001f));
}

TEST_CASE("f32v2_triangle_area degenerate") {
	f32 area = f32v2_triangle_area(
		f32v2{0.f,0.f}, f32v2{1.f,0.f}, f32v2{2.f,0.f}
	);
	CHECK(area == doctest::Approx(0.f).epsilon(0.001f));
}

TEST_CASE("f32v4_clip_to_screen center of screen") {
	// NDC (0,0) should map to center pixel
	i32v2 dim = { 512, 512 };
	// note: w-divide must be done before calling — pass NDC directly as x/y, w=1
	f32v4 ndc = { 0.f, 0.f, 0.f, 1.f };
	i32v2 s = f32v4_clip_to_screen(ndc, dim);
	// (0*0.5+0.5)*512 = 256
	CHECK(s.x == 256);
	CHECK(s.y == 256);
}

// -----------------------------------------------------------------------------
// -- conversion helpers
// -----------------------------------------------------------------------------

TEST_CASE("as_f32v2") {
	i32v2 v = { 3, -7 };
	f32v2 r = as_f32v2(v);
	CHECK(r.x == doctest::Approx(3.f).epsilon(0.001f));
	CHECK(r.y == doctest::Approx(-7.f).epsilon(0.001f));
}

TEST_CASE("as_rgba packing") {
	// red: (1,0,0,1) -> 0xFF0000FF in ABGR
	u32 r = as_rgba(f32v4{1.f, 0.f, 0.f, 1.f});
	CHECK((r & 0xFF)         == 0xFF); // R
	CHECK(((r >> 8)  & 0xFF) == 0x00); // G
	CHECK(((r >> 16) & 0xFF) == 0x00); // B
	CHECK(((r >> 24) & 0xFF) == 0xFF); // A

	// white: (1,1,1,1)
	u32 w = as_rgba(f32v4{1.f, 1.f, 1.f, 1.f});
	CHECK(w == 0xFFFFFFFFu);

	// black: (0,0,0,1)
	u32 b = as_rgba(f32v4{0.f, 0.f, 0.f, 1.f});
	CHECK(b == 0xFF000000u);

	// clamp: values > 1 should clamp to 255
	u32 c = as_rgba(f32v4{2.f, -1.f, 0.5f, 1.f});
	CHECK((c & 0xFF)         == 0xFF); // R clamped
	CHECK(((c >> 8)  & 0xFF) == 0x00); // G clamped
	CHECK(((c >> 16) & 0xFF) == 0x80); // B ~0.5 * 255
}

// -----------------------------------------------------------------------------
// -- bounding box helpers
// -----------------------------------------------------------------------------

TEST_CASE("i32bbox2_from_triangle") {
	i32v2 v0 = { 2, 5 };
	i32v2 v1 = { 8, 1 };
	i32v2 v2 = { 4, 9 };
	i32bbox2 bb = i32bbox2_from_triangle(v0, v1, v2);
	CHECK(bb.min.x == 2);
	CHECK(bb.min.y == 1);
	CHECK(bb.max.x == 8);
	CHECK(bb.max.y == 9);
}

TEST_CASE("f32bbox2_from_triangle") {
	f32v2 v0 = { 1.5f, 3.f };
	f32v2 v1 = { 5.f,  0.5f };
	f32v2 v2 = { 2.f,  7.f };
	f32bbox2 bb = f32bbox2_from_triangle(v0, v1, v2);
	CHECK(bb.min.x == doctest::Approx(1.5f).epsilon(0.001f));
	CHECK(bb.min.y == doctest::Approx(0.5f).epsilon(0.001f));
	CHECK(bb.max.x == doctest::Approx(5.f).epsilon(0.001f));
	CHECK(bb.max.y == doctest::Approx(7.f).epsilon(0.001f));
}

} // TEST_SUITE("math")
