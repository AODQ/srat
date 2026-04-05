#include <srat/core-types.hpp>
#include <srat/alloc-arena.hpp>

#include <doctest/doctest.h> // NOLINT

// (the below comment was AI generated which is pretty funny it claims
//  to be "hand-written")
// -- note: unlike virtual range allocator tests, these are hand-written
//	to cover the specifics of arena allocation semantics

//NOLINTBEGIN

TEST_SUITE("arena allocator") {

// -----------------------------------------------------------------------------
// -- AllocArena<T>
// -----------------------------------------------------------------------------

TEST_CASE("arena [sanity]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	CHECK(arena.empty());
	CHECK(arena.capacity() == 128);
}

TEST_CASE("arena [single allocation]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * ptr = arena.allocate(1);
	CHECK(ptr != nullptr);
	CHECK(!arena.empty());
	// pointer must be within the arena's data range
	CHECK(ptr >= arena.data_ptr().ptr());
	CHECK(ptr < arena.data_ptr().ptr() + arena.capacity());
}

TEST_CASE("arena [allocate full capacity]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * ptr = arena.allocate(128);
	CHECK(ptr != nullptr);
	CHECK(!arena.empty());
}

TEST_CASE("arena [overflow returns nullptr]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * a = arena.allocate(128);
	CHECK(a != nullptr);
	u32 * b = arena.allocate(1);
	CHECK(b == nullptr);
}

TEST_CASE("arena [overflow from start returns nullptr]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * ptr = arena.allocate(129);
	CHECK(ptr == nullptr);
	CHECK(arena.empty());
}

TEST_CASE("arena [sequential allocations are contiguous]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * a = arena.allocate(16);
	u32 * b = arena.allocate(16);
	u32 * c = arena.allocate(16);
	CHECK(a != nullptr);
	CHECK(b != nullptr);
	CHECK(c != nullptr);
	// linear allocator — each block starts right after the previous
	CHECK(b == a + 16);
	CHECK(c == b + 16);
}

TEST_CASE("arena [clear resets]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	arena.allocate(128);
	CHECK(!arena.empty());
	arena.clear();
	CHECK(arena.empty());
}

TEST_CASE("arena [clear then reallocate]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * first = arena.allocate(64);
	CHECK(first != nullptr);
	arena.clear();
	// after clear the cursor resets — should get same pointer back
	u32 * second = arena.allocate(64);
	CHECK(second != nullptr);
	CHECK(second == first);
}

TEST_CASE("arena [clear then full capacity again]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	for (u32 i = 0; i < 8; ++i) {
		arena.allocate(16);
	}
	arena.clear();
	CHECK(arena.empty());
	u32 * ptr = arena.allocate(128);
	CHECK(ptr != nullptr);
}

TEST_CASE("arena [written data is preserved]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * a = arena.allocate(4);
	u32 * b = arena.allocate(4);
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
	b[0] = 5; b[1] = 6; b[2] = 7; b[3] = 8;
	CHECK(a[0] == 1); CHECK(a[1] == 2); CHECK(a[2] == 3); CHECK(a[3] == 4);
	CHECK(b[0] == 5); CHECK(b[1] == 6); CHECK(b[2] == 7); CHECK(b[3] == 8);
}

TEST_CASE("arena [alignment default]") {
	// default alignment is alignof(T) — u64 is 8 bytes
	auto arena = srat::AllocArena<u64>::create(128, "test");
	// alloc 1 element then check next alloc is still aligned
	u64 * a = arena.allocate(1);
	u64 * b = arena.allocate(1);
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	CHECK(reinterpret_cast<uintptr_t>(a) % alignof(u64) == 0);
	CHECK(reinterpret_cast<uintptr_t>(b) % alignof(u64) == 0);
}

TEST_CASE("arena [alignment explicit]") {
	auto arena = srat::AllocArena<u8>::create(128, "test");
	// misalign by 1, then request alignment 16
	u8 * a = arena.allocate(1);
	REQUIRE(a != nullptr);
	u8 * b = arena.allocate(16, 16);
	REQUIRE(b != nullptr);
	CHECK(reinterpret_cast<uintptr_t>(b) % 16 == 0);
	// b must be at least 1 byte after a
	CHECK(b > a);
}

TEST_CASE("arena [alignment overflow]") {
	// total capacity 16, alloc 1 to misalign, then aligned alloc that would exceed
	auto arena = srat::AllocArena<u8>::create(16, "test");
	arena.allocate(1);
	// aligning to 8 puts cursor at 8, then requesting 10 elements = 18 > 16
	u8 * ptr = arena.allocate(10, 8);
	CHECK(ptr == nullptr);
}

TEST_CASE("arena [data_ptr returns base pointer]") {
	auto arena = srat::AllocArena<u32>::create(64, "test");
	u32 * base = arena.data_ptr().ptr();
	CHECK(base != nullptr);
	u32 * first = arena.allocate(1);
	// first allocation must be at the base pointer (no alignment padding needed)
	CHECK(first == base);
}

// -----------------------------------------------------------------------------
// -- move semantics
// -----------------------------------------------------------------------------

TEST_CASE("arena [move constructor]") {
	auto arena = srat::AllocArena<u32>::create(128, "test");
	u32 * ptr = arena.allocate(32);
	REQUIRE(ptr != nullptr);

	auto b = std::move(arena);
	CHECK(!b.empty());
	CHECK(b.capacity() == 128);
	// data pointer must be valid through new owner
	CHECK(b.data_ptr().ptr() != nullptr);
}

TEST_CASE("arena [move constructor then allocate]") {
	auto a = srat::AllocArena<u32>::create(128, "test");
	a.allocate(32);

	auto b = std::move(a);
	// cursor must be preserved — next alloc starts at offset 32
	u32 * ptr = b.allocate(32);
	REQUIRE(ptr != nullptr);
	CHECK(ptr == b.data_ptr().ptr() + 32);
}

TEST_CASE("arena [move assignment]") {
	auto a = srat::AllocArena<u32>::create(128, "test");
	auto b = srat::AllocArena<u32>::create(64, "test b");

	u32 * ptr = a.allocate(32);
	REQUIRE(ptr != nullptr);

	b = std::move(a);
	CHECK(!b.empty());
	CHECK(b.capacity() == 128);
	CHECK(b.data_ptr().ptr() != nullptr);
}

TEST_CASE("arena [move assignment then clear]") {
	auto a = srat::AllocArena<u32>::create(128, "test");
	auto b = srat::AllocArena<u32>::create(64, "test b");
	a.allocate(64);

	b = std::move(a);
	b.clear();
	CHECK(b.empty());

	u32 * ptr = b.allocate(128);
	CHECK(ptr != nullptr);
}

// -----------------------------------------------------------------------------
// -- AllocArenaSoA
// -----------------------------------------------------------------------------

TEST_CASE("arena soa [sanity]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	auto [a, b] = soa.allocate(1);
	CHECK(!soa.empty());
	CHECK(a.ptr() != nullptr);
	CHECK(b.ptr() != nullptr);
	CHECK(a.size() == 1);
	CHECK(b.size() == 1);
	soa.clear();
	CHECK(soa.empty());
}

TEST_CASE("arena soa [allocate returns correct sizes]") {
	auto soa = srat::AllocArenaSoA<u32, f32, u8>::create(128, "test");
	auto [a, b, c] = soa.allocate(16);
	CHECK(a.size() == 16);
	CHECK(b.size() == 16);
	CHECK(c.size() == 16);
	CHECK(a.ptr() != nullptr);
	CHECK(b.ptr() != nullptr);
	CHECK(c.ptr() != nullptr);
}

TEST_CASE("arena soa [spans are writable]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	auto [ints, floats] = soa.allocate(4);
	for (u32 i = 0; i < 4; ++i) {
		ints[i]   = i;
		floats[i] = static_cast<f32>(i) * 0.5f;
	}
	CHECK(ints[0] == 0); CHECK(ints[3] == 3);
	CHECK(floats[0] == 0.f); CHECK(floats[3] == 1.5f);
}

TEST_CASE("arena soa [sequential allocations don't overlap]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	auto [a1, b1] = soa.allocate(16);
	auto [a2, b2] = soa.allocate(16);
	// spans within same type must not overlap
	CHECK(a2.ptr() == a1.ptr() + 16);
	CHECK(b2.ptr() == b1.ptr() + 16);
	// spans of different types are in separate allocators so no overlap check needed
}

TEST_CASE("arena soa [clear resets all]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	auto [a1, b1] = soa.allocate(128);
	REQUIRE(a1.ptr() != nullptr);
	REQUIRE(b1.ptr() != nullptr);

	soa.clear();

	CHECK(soa.empty());

	// after clear, should be able to allocate full capacity again
	auto [a2, b2] = soa.allocate(128);
	CHECK(a2.ptr() != nullptr);
	CHECK(b2.ptr() != nullptr);
	// pointers should reset to base
	CHECK(a2.ptr() == a1.ptr());
	CHECK(b2.ptr() == b1.ptr());
}

TEST_CASE("arena soa [overflow returns null data]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	[[maybe_unused]] auto unused = soa.allocate(128);
	auto [a, b] = soa.allocate(1);
	CHECK(a.ptr() == nullptr);
	CHECK(b.ptr() == nullptr);
}

TEST_CASE("arena soa [data_ptr capacity]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(64, "test");
	{
		auto [ints, floats] = soa.data_ptr();
		CHECK_EQ(ints.size(), 0);
		CHECK_EQ(floats.size(), 0);
		CHECK(ints.ptr() != nullptr);
		CHECK(floats.ptr() != nullptr);
	}
	{
		[[maybe_unused]] auto [a, b] = soa.allocate(16);
		auto [ints, floats] = soa.data_ptr();
		CHECK_EQ(ints.size(), 16);
		CHECK_EQ(floats.size(), 16);
		CHECK(ints.ptr() != nullptr);
		CHECK(floats.ptr() != nullptr);
	}
	{
		[[maybe_unused]] auto [a, b] = soa.allocate(16);
		auto [ints, floats] = soa.data_ptr();
		CHECK_EQ(ints.size(), 32);
		CHECK_EQ(floats.size(), 32);
		CHECK(ints.ptr() != nullptr);
		CHECK(floats.ptr() != nullptr);
	}
}

TEST_CASE("arena soa [single type]") {
	auto soa = srat::AllocArenaSoA<u32>::create(64, "test");
	auto [a] = soa.allocate(32);
	CHECK(a.size() == 32);
	CHECK(a.ptr() != nullptr);
}

TEST_CASE("arena soa [move then allocate]") {
	auto a = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	[[maybe_unused]] auto unused = a.allocate(16);

	auto b = std::move(a);
	auto [ints, floats] = b.allocate(16);
	CHECK(ints.size() == 16);
	CHECK(floats.size() == 16);
	CHECK(ints.ptr() != nullptr);
	CHECK(floats.ptr() != nullptr);
}

TEST_CASE("arena soa [move then clear]") {
	auto a = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	[[maybe_unused]] auto unused = a.allocate(64);

	auto b = std::move(a);
	b.clear();
	CHECK(b.empty());

	auto [ints, floats] = b.allocate(128);
	CHECK(ints.ptr() != nullptr);
	CHECK(floats.ptr() != nullptr);
}

TEST_CASE("arena capacity [soa]") {
	auto soa = srat::AllocArenaSoA<u32, f32>::create(128, "test");
	CHECK(soa.capacity() == 128);
}

} // -- end arena allocator test suite

//NOLINTEND
