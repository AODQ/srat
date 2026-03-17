#pragma once

// common types + includes for srat

#include <cstdint>
#include <utility> // for std::move

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;
using f32 = float;

namespace srat
{

template <typename T>
inline T alignUp(T value, T alignment)
{
	return (value + (alignment - 1)) & ~(alignment - 1);
}


} // srat

// logging utility

#include "config.hpp"

#if SRAT_DEBUG()
#include <cstdio>
#include <cstdlib>
#define SRAT_ASSERT(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "Assertion failed: %s\nFile: %s\nLine: %d\n", \
			#expr, __FILE__, __LINE__); \
		std::abort(); \
	} \
} while (0)
#else
#define SRAT_ASSERT(expr)
#endif
