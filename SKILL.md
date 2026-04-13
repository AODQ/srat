srat C++ Skill
What this codebase is
A software rasterizer in C++ with a tile-based binning pipeline. Key
subsystems: virtual range allocator (FreeList + Linear strategies), arena
allocator, handle pool, SIMD math (f32x8, f32v4x8, f32m44x8), gfx
image abstraction, tile grid, and vertex/bin/rasterize pipeline phases.
Pipeline: vertex → bin → rasterize. The vertex phase transforms
and clips geometry into screen-space; the bin phase assigns triangles to
tiles via bbox; the rasterize phase runs per-tile SIMD rasterization.

Formatting rules
Column limit: 80 characters
Hard limit. Never exceed it. When something doesn't fit, apply the
wrapping rules below.
Indentation

Tabs only for indentation (one tab per level).
Spaces only for alignment within a line.
Never place a tab after a non-whitespace character.

Multi-line expressions
Any expression that spans multiple lines must be wrapped in braces
or parentheses (except where the language forbids it, e.g. using
aliases). The contents go on a new indented line; the closing delimiter
goes on its own line at the outer indent level.
Prefer putting the "contents" on a new line inside outer parens, not
inline with the opening paren:
cpp// GOOD
auto foo = (
	some_long_function_call(arg1, arg2)
);

// BAD — contents inline with opening paren
auto foo = (some_long_function_call(
	arg1, arg2
));
Designated initializers can stay on the same line as the function call:
cppauto block = allocator.allocate(AllocParams {
	.elementCount = 128,
	.elementAlignment = 8,
});
But if the function call + struct name already doesn't fit, drop to:
cppauto block = allocator.allocate(
	AllocParams {
		.elementCount = 128,
		.elementAlignment = 8,
	}
);
Function signatures
When a signature doesn't fit on one line:

First, drop params to their own indented lines:

cppstatic bool sr_parseTriple(
	StreamReader & sr,
	int vsize,
	int vnsize,
	vertex_index_t * ret
) {

If it still doesn't fit, drop the return type to its own line too:

cpptemplate <typename T>
static adjusted_mantissa
compute_float(int64_t q, uint64_t w) noexcept {
Short signatures stay on one line:
cpptemplate <typename T> bool T_example() { return T(); }

Variable declarations
auto const & vs explicit type
Use auto const & (or auto const) only when the type is immediately
obvious from the RHS — e.g. the result of a cast, a constructor call
where the type name appears, or a literal:
cppauto const & impl = *sTileGridPool.get(grid);
auto const count = static_cast<u32>(vec.size());
Otherwise spell out the type explicitly:
cppu32v2 const dim = srat::gfx::image_dim(img);
f32 const area = f32v2_triangle_area(v0, v1, v2);
Do not use Let or Mut in new code. They are deprecated. You will see them
throughout existing code — that's expected. When adding new code or
rewriting existing code, replace them with explicit types or auto const & per the rule above.
SSA / const-first style
Prefer immutable variables. If you need to compute something in steps,
use multiple const variables rather than mutating one:
cpp// GOOD
f32 const dx = v1.x - v0.x;
f32 const dy = v1.y - v0.y;
f32 const len = std::sqrt(dx*dx + dy*dy);

// BAD
f32 result = v1.x - v0.x;
result = std::sqrt(result * result + (v1.y - v0.y) * (v1.y - v0.y));
Only use a mutable variable when you genuinely need mutation (loop
counters, accumulators, cursors).

Type system
Scalar types
AliasMeaningi8int8_tu8uint8_ti16int16_tu16uint16_ti32int32_tu32uint32_ti64int64_tu64uint64_tf32floatusizesize_t
Vector / matrix types
f32v2, f32v3, f32v4 — float vectors.
i32v2, u32v2 — integer vectors.
f32m44 — 4x4 column-major float matrix.
i32bbox2, f32bbox2 — axis-aligned bounding boxes.
SIMD types (8-wide AVX2)
f32x8 — 8 floats. i32x8, u32x8 — 8 ints.
f32v4x8 — struct of four f32x8 (x/y/z/w), also accessible as
v[0..3].
f32m44x8 — 4x4 matrix, each element is f32x8. Used for broadcasting
a uniform matrix across 8 triangle lanes.
Key SIMD ops: f32x8_load, f32x8_store, f32x8_splat,
f32x8_zero, f32x8_sqrt, f32x8_rsqrt, f32x8_fmadd,
f32x8_min, f32x8_max, f32x8_select, f32x8_barycentric.
Comparisons return u32x8. u32x8_ballot, u32x8_any, u32x8_all.
f32x8_load / f32x8_store take srat::slice<f32, 8> or
srat::array<f32, 8>. Stack arrays used with SIMD must be
alignas(32).
u32x8_ballot uses _mm256_movemask_ps — it reads the sign bit of
each 32-bit lane. A lane with 0x80000000 set counts as 1.
Handle types
Resources are managed through HandlePool<H, T>. Handles carry a
generation counter — stale handles return false from .valid() and
nullptr from .get(). Never access freed handle data.
HandlePool::empty() returns true only when all slots are free.
Handle ID layout: low 32 bits = slot index, high 32 bits = generation.
generation_alive(gen) = odd and non-zero. generation_inc wraps but
never goes to 0.
Slices and arrays
srat::slice<T> — pointer + size, not owning. Has .ptr(), .size(),
[], .subslice(), .cast<U>(), .as<N>().
srat::array<T, N> — fixed-size stack array, same interface.

Common patterns
Resource acquisition
cppauto img = srat::gfx::image_create(srat::gfx::ImageCreateInfo {
	.dim = { 512, 512 },
	.layout = srat::gfx::ImageLayout::Linear,
	.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
});
// ... use img ...
srat::gfx::image_destroy(img);
Image data access
cpp// color image (r8g8b8a8_unorm) — 4 bytes per pixel
srat::slice<u8>  color8  = srat::gfx::image_data8(colorImg);
srat::slice<u32> color32 = srat::gfx::image_data32(colorImg);

// depth image (depth16_unorm) — 1 u16 per pixel
srat::slice<u16> depth16 = srat::gfx::image_data16(depthImg);
Never call image_data8 on a depth image or image_data16 on a
color image. There is no runtime guard — it will silently return wrongly
typed data.
Writing a doctest unit test
cppTEST_CASE("subsystem [scenario description]") {
	// arrange
	auto allocator = srat::AllocVirtualRange::create(
		srat::AllocVirtualRangeCreateParams {
			.debugName = "test",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		}
	);

	// act
	auto block = allocator.allocate({ .elementCount = 64 });

	// check
	CHECK(block.valid(allocator));
	CHECK(block.elementCount == 64);

	allocator.free(block.handle);
	CHECK(allocator.empty());
}
Test naming: "subsystem [short scenario]". Use CHECK not ASSERT
unless you genuinely want to abort on failure. Use
CHECK_MESSAGE(expr, "context: ", value) to add debug info to failures.
Use REQUIRE only when later checks are meaningless without it.
Test cases should be independent — no shared mutable state between
cases. Cover: happy path, overflow/OOB, stale handles, clear-then-reuse,
move semantics.

Bug review checklist
When reviewing code for bugs, check all of the following:
Memory and handles

Stale handle used after free() or clear() — valid() is always
the guard; never skip it.
image_data8 called on a depth image, or image_data16 called on a
color image — silently returns wrong-typed data.
SIMD stack arrays missing alignas(32) — f32x8_load/store require
32-byte alignment; unaligned access will crash or silently corrupt.

Arithmetic and overflow

Integer overflow in u32 bbox arithmetic — max.x - min.x can wrap
if vertices are off-screen; cast to i32 or i64 before subtraction.
Division by zero — rcpArea when triangle area ≈ 0 (degenerate tri).
srat_tile_size() returns u64 & (runtime-configurable) — arithmetic
with it can silently widen or narrow; be explicit with casts.

Rasterizer correctness

Off-by-one in tile/bbox math: the correct pattern is
(bounds.max - i32v2(1,1)) before dividing by tile size, and clamping
both min and max tile coords. If either is missing, triangles bleed
into wrong tiles or get skipped.
Bbox binning is conservative — a triangle binned into a tile may not
actually cover any pixels in that tile. The rasterizer must still do a
per-pixel inside test; never skip it.
Depth convention: depth is stored as u16 in [0, UINT16_MAX];
smaller = closer. The correct test is if (depth16 > rowDepths[lane]) continue; — failing means the stored pixel is closer, so discard the
incoming fragment. Getting the comparison backwards produces wrong
depth silently.
Screen-space Y flip: NDC Y is negated when converting to screen coords
((1.0f - ndc.y) * 0.5f * height). Forgetting the flip produces a
vertically mirrored image.
Perspective divide must happen before f32v4_clip_to_screen — passing
raw clip coords produces wrong screen positions.
Column-major matrix multiply order: proj * view applies view first.
Check whether the caller has TRS compose order right.
Back-face cull uses area >= -epsilon (skip if area is not strongly
negative) — the sign convention is CCW-front in screen space after Y
flip. Changing epsilon or the comparison direction breaks culling.

SIMD lane logic

u32x8_ballot reads the sign bit, not the whole lane. A mask from
a float comparison is 0xFFFFFFFF for true lanes, so the sign bit is
correct. A hand-constructed mask with only low bits set will be
invisible to ballot/any/all.
f32x8_select(mask, a, b) — when the mask lane is true (sign bit
set), the result is b (the second arg), not a. This is because
_mm256_blendv_ps blends from the second source when the control bit
is 1.


Optimization notes
When suggesting optimizations:

Prefer branchless SIMD over scalar loops for per-pixel work.
f32x8_rsqrt + Newton-Raphson is faster than 1.f / f32x8_sqrt for
normalizations where ~0.1% error is acceptable.
Prefer SoA (struct of arrays) over AoS for data touched by SIMD.
Tile binning uses a conservative bbox — a tighter per-edge test in
tile_grid_bin_triangle_bbox can reduce overdraw in rasterization.
Arena allocators beat std::vector for per-frame scratch data; suggest
replacing heap allocs in hot paths with arena allocs.
Avoid std::vector resizes inside per-triangle loops; pre-reserve or
use arena-backed storage.
The interpolant system (Interpolant<T>) precomputes ddxStep8 (8×
the per-pixel step) to amortize the multiply across the SIMD group;
keep this pattern when adding new interpolated attributes.
