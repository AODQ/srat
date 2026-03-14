#pragma once

#include "srat/types.hpp"

#include <cmath>
#include <cstring>
#include <immintrin.h>

static size_t constexpr skSimdLaneWidth = 8;

// -----------------------------------------------------------------------------
// -- i32 math
// -----------------------------------------------------------------------------

inline i32 i32_min(i32 const a, i32 const b) { return (a < b) ? a : b; }
inline i32 i32_max(i32 const a, i32 const b) { return (a > b) ? a : b; }
inline i32 i32_clamp(i32 const v, i32 const min, i32 const max) {
	return i32_min(i32_max(v, min), max);
}

// -----------------------------------------------------------------------------
// -- u32v2
// -----------------------------------------------------------------------------

struct u32v2 {
	u32 x, y;
};

// -----------------------------------------------------------------------------
// -- i32v2
// -----------------------------------------------------------------------------

struct i32v2 {
	i32 x, y;

	i32v2 operator /(i32v2 const s) const { return { x/s.x, y/s.y, }; }
	i32v2 operator -(i32v2 const s) const { return { x-s.x, y-s.y, }; }
};

inline i32v2 i32v2_clamp(i32v2 const v, i32v2 const min, i32v2 const max) {
	return {
		i32_clamp(v.x, min.x, max.x),
		i32_clamp(v.y, min.y, max.y),
	};
}

// -----------------------------------------------------------------------------
// -- f32v2
// -----------------------------------------------------------------------------

struct f32v2 {
	f32 x, y;

	f32v2 operator /(f32v2 const s) const { return { x/s.x, y/s.y, }; }
};

// -----------------------------------------------------------------------------
// -- f32v3
// -----------------------------------------------------------------------------

struct f32v3 {
	f32 x, y, z;

	f32v2 xy() const { return { x, y }; }

	f32v3 operator /(f32v3 const s) const { return { x/s.x, y/s.y, z/s.z }; }
};

// -----------------------------------------------------------------------------
// -- f32v4
// -----------------------------------------------------------------------------

struct f32v4 {
	f32 x {0.0f}, y {0.0f}, z {0.0f}, w {0.0f};

	f32v2 xy() const { return { x, y }; }
	f32v3 xyz() const { return { x, y, z }; }

	f32v4 operator /(f32v4 const s) const {
		return { x/s.x, y/s.y, z/s.z, w/s.w };
	}
};

// -----------------------------------------------------------------------------
// -- i32x8/u32x8
// -----------------------------------------------------------------------------

struct i32x8 { __m256i v; };
struct u32x8 {
	__m256i v;
	u32x8 operator&(u32x8 const & o) const {
		return { _mm256_and_si256(v, o.v) };
	}
	u32x8 operator|(u32x8 const & o) const {
		return { _mm256_or_si256(v, o.v) };
	}
	u32x8 operator^(u32x8 const & o) const {
		return { _mm256_xor_si256(v, o.v) };
	}
	u32x8 operator~() const {
		return { _mm256_xor_si256(v, _mm256_set1_epi32(0xFFFFFFFF)) };
	}

};

inline i32x8 i32x8_load(int const in[8]) {
	return { _mm256_loadu_si256((__m256i const *)in) };
}
inline u32x8 u32x8_load(unsigned int const in[8]) {
	return { _mm256_loadu_si256((__m256i const *)in) };
}
inline i32 i32x8_lane0(i32x8 const & v) { return _mm256_extract_epi32(v.v, 0); }
inline u32 u32x8_lane0(u32x8 const & v) { return _mm256_extract_epi32(v.v, 0); }
inline void i32x8_store(i32x8 const & v, int out[8]) {
	_mm256_storeu_si256((__m256i *)out, v.v);
}
inline void u32x8_store(u32x8 const & v, unsigned int out[8]) {
	_mm256_storeu_si256((__m256i *)out, v.v);
}
inline i32x8 i32x8_splat(int i) { return { _mm256_set1_epi32(i) }; }
inline u32x8 u32x8_splat(unsigned int i) { return { _mm256_set1_epi32(i) }; }
inline i32x8 i32x8_zero() { return i32x8_splat(0); }
inline u32x8 u32x8_zero() { return u32x8_splat(0); }

inline u32 u32x8_ballot(u32x8 const & v) {
	return (u32)_mm256_movemask_ps(_mm256_castsi256_ps(v.v));
}
inline bool u32x8_any(u32x8 const & v) { return u32x8_ballot(v) != 0; }
inline bool u32x8_all(u32x8 const & v) { return u32x8_ballot(v) == 0xFF; }

// -----------------------------------------------------------------------------
// -- f32x8
// -----------------------------------------------------------------------------

struct f32x8 {
	__m256 v;

	f32 lane(size_t lane) const {
		#define extract_low(x) \
			_mm256_cvtss_f32(_mm256_permute_ps(v, _MM_SHUFFLE(x,x,x,x))) \

		#define extract_high(x) \
			_mm_cvtss_f32( \
				_mm_permute_ps( \
					_mm256_extractf128_ps(v, 1), \
					_MM_SHUFFLE(x,x,x,x) \
				) \
			) \

		switch (lane) {
			case 0: return _mm256_cvtss_f32(v);
			case 1: return extract_low(1);
			case 2: return extract_low(2);
			case 3: return extract_low(3);
			case 4: return _mm_cvtss_f32(_mm256_extractf128_ps(v, 1));
			case 5: return extract_high(1);
			case 6: return extract_high(2);
			case 7: return extract_high(3);
		}
		return 0.0f; // silence warning
	}

	f32x8 operator+(f32x8 const & o) const { return { _mm256_add_ps(v, o.v) }; }
	f32x8 operator-(f32x8 const & o) const { return { _mm256_sub_ps(v, o.v) }; }
	f32x8 operator*(f32x8 const & o) const { return { _mm256_mul_ps(v, o.v) }; }
	f32x8 operator/(f32x8 const & o) const { return { _mm256_div_ps(v, o.v) }; }

	f32x8 operator-() const { return { _mm256_sub_ps(_mm256_setzero_ps(), v) }; }

	u32x8 operator<(f32x8 const & o) const
		{ return { _mm256_castps_si256(_mm256_cmp_ps(v, o.v, _CMP_LT_OQ)) }; }
	u32x8 operator>(f32x8 const & o) const
		{ return { _mm256_castps_si256(_mm256_cmp_ps(v, o.v, _CMP_GT_OQ)) }; }
	u32x8 operator<=(f32x8 const & o) const
		{ return { _mm256_castps_si256(_mm256_cmp_ps(v, o.v, _CMP_LE_OQ)) }; }
	u32x8 operator>=(f32x8 const & o) const
		{ return { _mm256_castps_si256(_mm256_cmp_ps(v, o.v, _CMP_GE_OQ)) }; }
};

inline f32x8 f32x8_load(float const in[8]) { return { _mm256_loadu_ps(in) }; }
inline float f32x8_lane0(f32x8 const & v) { return _mm256_cvtss_f32(v.v); }
inline void f32x8_store(f32x8 const & v, float out[8])
	{ _mm256_storeu_ps(out, v.v); }
inline f32x8 f32x8_splat(float f) { return { _mm256_set1_ps(f) }; }
inline f32x8 f32x8_zero() { return f32x8_splat(0.0f); }

// Fused multiply-add: a * b + c
inline f32x8 f32x8_fmadd(f32x8 const & a, f32x8 const & b, f32x8 const & c) {
	return { _mm256_fmadd_ps(a.v, b.v, c.v) };
}
// blend based on select: if mask is true, select a else select b
inline f32x8 f32x8_select(u32x8 const & mask, f32x8 const & a, f32x8 const & b) {
	return { _mm256_blendv_ps(a.v, b.v, _mm256_castsi256_ps(mask.v)) };
}

// min/max
inline f32x8 f32x8_min(f32x8 const & a, f32x8 const & b)
	{ return { _mm256_min_ps(a.v, b.v) }; }
inline f32x8 f32x8_max(f32x8 const & a, f32x8 const & b)
	{ return { _mm256_max_ps(a.v, b.v) }; }

// sqrt
inline f32x8 f32x8_sqrt(f32x8 const & v) { return { _mm256_sqrt_ps(v.v) }; }
inline f32x8 f32x8_rsqrt(f32x8 const & v) { return { _mm256_rsqrt_ps(v.v) }; }

// -----------------------------------------------------------------------------
// -- f32x8x2
// -----------------------------------------------------------------------------

struct f32v2x8 { f32x8 v[2]; };

// -----------------------------------------------------------------------------
// -- f32x8x3
// -----------------------------------------------------------------------------

struct f32v3x8 { f32x8 v[3]; };

// -----------------------------------------------------------------------------
// -- f32x8x4
// -----------------------------------------------------------------------------

struct f32v4x8 {
	union {
		f32x8 v[4];
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wpedantic"
		struct { f32x8 x, y, z, w; };
		#pragma GCC diagnostic pop
	};

	f32v4x8() = default;
	f32v4x8(f32x8 const & x, f32x8 const & y, f32x8 const & z, f32x8 const & w)
		: v{ x, y, z, w }
	{}

	f32v4x8 operator+(f32v4x8 const & o) const {
		return {
			v[0] + o.v[0],
			v[1] + o.v[1],
			v[2] + o.v[2],
			v[3] + o.v[3],
		};
	}

	f32v4x8 operator-(f32v4x8 const & o) const {
		return {
			v[0] - o.v[0],
			v[1] - o.v[1],
			v[2] - o.v[2],
			v[3] - o.v[3],
		};
	}

	f32v4x8 operator*(f32v4x8 const & o) const {
		return {
			v[0] * o.v[0],
			v[1] * o.v[1],
			v[2] * o.v[2],
			v[3] * o.v[3],
		};
	}

	f32v4x8 operator/(f32v4x8 const & o) const {
		return {
			v[0] / o.v[0],
			v[1] / o.v[1],
			v[2] / o.v[2],
			v[3] / o.v[3],
		};
	}

	f32v4x8 operator*(f32x8 const & s) const {
		return { v[0] * s, v[1] * s, v[2] * s, v[3] * s, };
	}
};

inline f32v4x8 f32v4x8_load(float const in[32]) {
	return f32v4x8 {
		f32x8_load(in + 0),
		f32x8_load(in + 8),
		f32x8_load(in + 16),
		f32x8_load(in + 24),
	};
}

inline void f32v4x8_store(f32v4x8 const & v, float out[32]) {
	f32x8_store(v.v[0], out + 0);
	f32x8_store(v.v[1], out + 8);
	f32x8_store(v.v[2], out + 16);
	f32x8_store(v.v[3], out + 24);
}

inline f32v4x8 f32v4x8_splat(float const in[4]) {
	return {
		f32x8_splat(in[0]),
		f32x8_splat(in[1]),
		f32x8_splat(in[2]),
		f32x8_splat(in[3]),
	};
}

inline f32v4x8 f32v4x8_splat(f32v4 const & v) {
	return {
		f32x8_splat(v.x),
		f32x8_splat(v.y),
		f32x8_splat(v.z),
		f32x8_splat(v.w),
	};
}

inline f32v4x8 f32v4x8_splat(
	float const x, float const y, float const z, float const w
) {
	return {
		f32x8_splat(x),
		f32x8_splat(y),
		f32x8_splat(z),
		f32x8_splat(w),
	};
}

inline f32v4x8 f32v4x8_zero() {
	static constexpr float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	return f32v4x8_splat(zero);
}

inline f32x8 f32v4x8_dot(f32v4x8 const & a, f32v4x8 const & b) {
	return {
		f32x8_fmadd(
			a.v[0], b.v[0],
			f32x8_fmadd(
				a.v[1], b.v[1],
				f32x8_fmadd(a.v[2], b.v[2], (a.v[3] * b.v[3]))
			)
		)
	};
}

inline f32v4x8 f32v4x8_cross(f32v4x8 const & a, f32v4x8 const & b) {
	return {
		f32x8_fmadd(a.v[1], b.v[2], -(a.v[2] * b.v[1])),
		f32x8_fmadd(a.v[2], b.v[0], -(a.v[0] * b.v[2])),
		f32x8_fmadd(a.v[0], b.v[1], -(a.v[1] * b.v[0])),
		f32x8_zero(),
	};
}

inline f32v4x8 f32v4x8_normalize(f32v4x8 const & v) {
	f32x8 const lengthSq = f32v4x8_dot(v, v);
	SRAT_ASSERT(lengthSq.v[0] > 0.0f);
	f32x8 const invLength = f32x8_rsqrt(lengthSq);
	return {
		v.v[0] * invLength,
		v.v[1] * invLength,
		v.v[2] * invLength,
		f32x8_zero(),
	};
}

// -----------------------------------------------------------------------------
// -- f32m44
// -----------------------------------------------------------------------------

// column major matrix
struct f32m44 {
	f32 m[16];

	inline f32m44 operator*(f32m44 const & o) const {
		f32m44 result {};
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				result.m[col*4 + row] = (
					  m[0*4 + row] * o.m[col*4 + 0]
					+ m[1*4 + row] * o.m[col*4 + 1]
					+ m[2*4 + row] * o.m[col*4 + 2]
					+ m[3*4 + row] * o.m[col*4 + 3]
				);
			}
		}
		return result;
	}

	inline f32v4 operator*(f32v4 const & v) const {
		return {
			m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * v.w,
			m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * v.w,
			m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
			m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
		};
	}
};

inline f32m44 f32m44_identity() {
	return {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
};

inline f32m44 f32m44_translate(float x, float y, float z) {
	return {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		x,   y,   z,   1.0f,
	};
};

inline f32m44 f32m44_scale(float x, float y, float z) {
	return {
		x,   0.0f, 0.0f, 0.0f,
		0.0f, y,   0.0f, 0.0f,
		0.0f, 0.0f, z,   0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
};

inline f32m44 f32m44_rotate_z(float angleRadians) {
	float c = cosf(angleRadians);
	float s = sinf(angleRadians);
	return {
		c,   s,   0.0f, 0.0f,
		-s,  c,   0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
};

inline f32m44 f32m44_rotate_x(float angleRadians) {
	float c = cosf(angleRadians);
	float s = sinf(angleRadians);
	return {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, c,   s,   0.0f,
		0.0f, -s,  c,   0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
};

inline f32m44 f32m44_rotate_y(float angleRadians) {
	float c = cosf(angleRadians);
	float s = sinf(angleRadians);
	return {
		c,   0.0f, -s,  0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		s,   0.0f, c,   0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
};

inline f32m44 f32m44_perspective(
	f32 const fovY, f32 const aspect, f32 const zNear, f32 const zFar
) {
	f32 const f = 1.0f / tanf(fovY * 0.5f);
	f32 const d = zFar - zNear;
	return {
		f / aspect, 0.0f,  0.0f,                      0.0f,
		0.0f,       f,     0.0f,                       0.0f,
		0.0f,       0.0f, -(zFar + zNear) / d,        -1.0f,
		0.0f,       0.0f, -(2.0f * zFar * zNear) / d,  0.0f,
	};
}

// -----------------------------------------------------------------------------
// -- f32m44x8
// -----------------------------------------------------------------------------

// this is a matrix that broadcasts the same uniform f32m44 across all 8 lanes
struct f32m44x8 {
	f32v4x8 col[4];
};

// broadcasts the same matrix across all 8 lanes
inline f32m44x8 f32m44x8_broadcast(float const m[16]) {
	#define M(i) f32x8_splat(m[i])
	return {
		f32v4x8 { M(0), M(1), M(2), M(3) },
		f32v4x8 { M(4), M(5), M(6), M(7) },
		f32v4x8 { M(8), M(9), M(10), M(11) },
		f32v4x8 { M(12), M(13), M(14), M(15) },
	};
	#undef M
};

// reads the matrix back into memory
inline void f32m44x8_store(f32m44x8 const & m, float out[16]) {
	out[0]  = f32x8_lane0(m.col[0].v[0]);
	out[1]  = f32x8_lane0(m.col[0].v[1]);
	out[2]  = f32x8_lane0(m.col[0].v[2]);
	out[3]  = f32x8_lane0(m.col[0].v[3]);

	out[4]  = f32x8_lane0(m.col[1].v[0]);
	out[5]  = f32x8_lane0(m.col[1].v[1]);
	out[6]  = f32x8_lane0(m.col[1].v[2]);
	out[7]  = f32x8_lane0(m.col[1].v[3]);

	out[8]  = f32x8_lane0(m.col[2].v[0]);
	out[9]  = f32x8_lane0(m.col[2].v[1]);
	out[10] = f32x8_lane0(m.col[2].v[2]);
	out[11] = f32x8_lane0(m.col[2].v[3]);

	out[12] = f32x8_lane0(m.col[3].v[0]);
	out[13] = f32x8_lane0(m.col[3].v[1]);
	out[14] = f32x8_lane0(m.col[3].v[2]);
	out[15] = f32x8_lane0(m.col[3].v[3]);
}

inline f32v4x8 f32m44x8_mul_vec(f32m44x8 const & m, f32v4x8 const & v) {
	// column-major matrix-vector multiply
	f32v4x8 result = f32v4x8_zero();

	// result += m.col[0] * v.x
	result.x = f32x8_fmadd(m.col[0].v[0], v.x, result.x);
	result.y = f32x8_fmadd(m.col[0].v[1], v.x, result.y);
	result.z = f32x8_fmadd(m.col[0].v[2], v.x, result.z);
	result.w = f32x8_fmadd(m.col[0].v[3], v.x, result.w);

	// result += m.col[1] * v.y
	result.x = f32x8_fmadd(m.col[1].v[0], v.y, result.x);
	result.y = f32x8_fmadd(m.col[1].v[1], v.y, result.y);
	result.z = f32x8_fmadd(m.col[1].v[2], v.y, result.z);
	result.w = f32x8_fmadd(m.col[1].v[3], v.y, result.w);

	// result += m.col[2] * v.z
	result.x = f32x8_fmadd(m.col[2].v[0], v.z, result.x);
	result.y = f32x8_fmadd(m.col[2].v[1], v.z, result.y);
	result.z = f32x8_fmadd(m.col[2].v[2], v.z, result.z);
	result.w = f32x8_fmadd(m.col[2].v[3], v.z, result.w);

	// result += m.col[3] * v.w
	result.x = f32x8_fmadd(m.col[3].v[0], v.w, result.x);
	result.y = f32x8_fmadd(m.col[3].v[1], v.w, result.y);
	result.z = f32x8_fmadd(m.col[3].v[2], v.w, result.z);
	result.w = f32x8_fmadd(m.col[3].v[3], v.w, result.w);

	return result;
}

// -----------------------------------------------------------------------------
// -- f32 math
// -----------------------------------------------------------------------------

inline f32 f32_min(f32 const a, f32 const b) { return (a < b) ? a : b; }
inline f32 f32_max(f32 const a, f32 const b) { return (a > b) ? a : b; }
inline f32 f32_clamp(f32 const v, f32 const min, f32 const max) {
	return f32_min(f32_max(v, min), max);
}

// -----------------------------------------------------------------------------
// -- i32 bounding box
// -----------------------------------------------------------------------------

struct i32bbox2 {
	i32v2 min;
	i32v2 max;
};

inline i32bbox2 i32bbox2_from_triangle(
	i32v2 const & v0,
	i32v2 const & v1,
	i32v2 const & v2
) {
	return i32bbox2 {
		.min = {
			.x = i32_min(i32_min(v0.x, v1.x), v2.x),
			.y = i32_min(i32_min(v0.y, v1.y), v2.y),
		},
		.max = {
			.x = i32_max(i32_max(v0.x, v1.x), v2.x),
			.y = i32_max(i32_max(v0.y, v1.y), v2.y),
		}
	};
}

// -----------------------------------------------------------------------------
// -- f32 bounding box
// -----------------------------------------------------------------------------

struct f32bbox2 {
	f32v2 min;
	f32v2 max;
};

inline f32bbox2 f32bbox2_from_triangle(
	f32v2 const & v0,
	f32v2 const & v1,
	f32v2 const & v2
) {
	return f32bbox2 {
		.min = {
			.x = f32_min(f32_min(v0.x, v1.x), v2.x),
			.y = f32_min(f32_min(v0.y, v1.y), v2.y),
		},
		.max = {
			.x = f32_max(f32_max(v0.x, v1.x), v2.x),
			.y = f32_max(f32_max(v0.y, v1.y), v2.y),
		}
	};
}

// -----------------------------------------------------------------------------
// -- rasterizer specific math
// -----------------------------------------------------------------------------

// computes triangle edge function
// (bx-ax)*(py-ay) - (by-ay)*(px-ax)
inline f32x8 f32x8_barycentric(
	f32v2 const & a,
	f32v2 const & b,
	f32x8 const & px,
	f32x8 const & py
) {
	f32x8 const dbx = f32x8_splat(b.x - a.x);
	f32x8 const dby = f32x8_splat(b.y - a.y);
	f32x8 const dpx = px + f32x8_splat(-a.x);
	f32x8 const dpy = py + f32x8_splat(-a.y);
	return f32x8_fmadd(dbx, dpy, -(dby * dpx));
}

inline f32 f32v2_triangle_area(
	f32v2 const & v0,
	f32v2 const & v1,
	f32v2 const & v2
) {
	return (
		(v1.x - v0.x) * (v2.y - v0.y) -
		(v1.y - v0.y) * (v2.x - v0.x)
	);
}

inline i32v2 f32v4_clip_to_screen(f32v4 const & v, i32v2 const & screenSize) {
	return i32v2 {
		.x = (i32)(( v.x/v.w * 0.5f + 0.5f) * (f32)screenSize.x),
		.y = (i32)((-v.y/v.w * 0.5f + 0.5f) * (f32)screenSize.y),
	};
};

inline f32 depth_ndc(f32v4 const & v) {
	return v.z / v.w * 0.5f + 0.5f;
};

// -----------------------------------------------------------------------------
// -- conversion
// -----------------------------------------------------------------------------

inline f32v2 as_f32v2(i32v2 const & v) {
	return f32v2 { (f32)v.x, (f32)v.y };
};

inline u32 as_rgba(f32v4 const & color) {
	u32 r = (u32)(f32_clamp(color.x, 0.0f, 1.0f) * 255.0f + 0.5f);
	u32 g = (u32)(f32_clamp(color.y, 0.0f, 1.0f) * 255.0f + 0.5f);
	u32 b = (u32)(f32_clamp(color.z, 0.0f, 1.0f) * 255.0f + 0.5f);
	u32 a = (u32)(f32_clamp(color.w, 0.0f, 1.0f) * 255.0f + 0.5f);
	return (a << 24) | (b << 16) | (g << 8) | r;
}
