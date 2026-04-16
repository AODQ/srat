#pragma once

namespace srat {

template <typename T>
struct InterpolantRow;

template <> struct InterpolantRow<f32> {
	using type = f32x8;
	static constexpr type splat(f32 const v) { return f32x8_splat(v); }
};
template <> struct InterpolantRow<f32v4> {
	using type = f32v4x8;
	static constexpr type splat(f32v4 const v) {
		return f32v4x8_splat(v.x, v.y, v.z, v.w);
	}
};
template <> struct InterpolantRow<f32v2> {
	using type = f32v2x8;
	static constexpr type splat(f32v2 const v) {
		return f32v2x8_splat(v.x, v.y);
	}
};

template <typename T>
struct Interpolant {
	using Row = InterpolantRow<T>;
	T value;
	T ddx;
	T ddy;
	typename Row::type ddxStep8;

	[[nodiscard]] constexpr static Interpolant<T> make(
		T const a0, T const a1, T const a2,
		f32 const dw0dx, f32 const dw1dx, f32 const dw2dx,
		f32 const dw0dy, f32 const dw1dy, f32 const dw2dy,
		f32 const w0Start, f32 const w1Start, f32 const w2Start
	) {
		T const ddx = a0*dw0dx + a1*dw1dx + a2*dw2dx;
		T const ddy = a0*dw0dy + a1*dw1dy + a2*dw2dy;
		T const value = a0*w0Start + a1*w1Start + a2*w2Start;
		return { value, ddx, ddy, Row::splat(ddx*8.0f), };
	}

	// broadcast row value across 8 lanes adjusting for offsets
	[[nodiscard]] constexpr typename InterpolantRow<T>::type
	simdRow(f32x8 const laneOffsetsX) const {
		return Row::splat(value) + Row::splat(ddx) * laneOffsetsX;
	}

	inline constexpr void stepRow() { value += ddy; }
};

} // srat
