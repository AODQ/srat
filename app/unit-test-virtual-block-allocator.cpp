#include <srat/types.hpp>
#include <srat/virtual-range-allocator.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <random>

// -- note many of these unit tests are AI generated so that they may
//	have very high coverage (though I also write edge case tests)

TEST_SUITE("virtual block allocator") {

TEST_CASE("virtual block allocator sanity") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto const allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 16,
		})
	);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator one allocation") {
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 16,
		})
	);

	srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
	CHECK(block.valid(allocator));
	CHECK(!allocator.empty());
	allocator.free(block.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator alloc+free+alloc+free") {
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 16,
		})
	);

	{
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
		CHECK(block.valid(allocator));
		CHECK(!allocator.empty());
		allocator.free(block.handle);
		CHECK(allocator.empty());
	}

	{
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
		CHECK(block.valid(allocator));
		CHECK(!allocator.empty());
		allocator.free(block.handle);
		CHECK(allocator.empty());
	}
}

TEST_CASE("virtual block allocate overflow") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 16,
		})
	);

	srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 129,});
	srat::VirtualRangeBlock block3 = allocator.allocate({.elementCount = 128,});
	srat::VirtualRangeBlock block2 = allocator.allocate({.elementCount = 1,});

	CHECK(!block.valid(allocator));
	CHECK(block3.valid(allocator));
	CHECK(!block2.valid(allocator));
}

TEST_CASE("virtual block reset one chunk") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 128,
		})
	);

	srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
	CHECK(block.valid(allocator));
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset large chunks") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 128,
		})
	);

	for (size_t it =0; it < 16; ++it) {
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 8,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset small chunks") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 128,
		})
	);

	for (size_t it =0; it < 128; ++it) {
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 1,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset then reset") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 128,
		})
	);

	for (size_t it =0; it < 16; ++it) {
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 8,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
	for (size_t it = 0; it < 127; ++it) {
		srat::VirtualRangeBlock block = allocator.allocate({.elementCount = 1,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator many allocations then free") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = (
		srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
			.elementCount = 128,
			.maxBlockAllocations = 16,
		})
	);

	// free in forward order
	for (size_t allocs = 1; allocs < 8; ++allocs) {
		std::vector<srat::VirtualRangeBlock> blocks;
		for (size_t it = 0; it < allocs; ++it) {
			auto const block = allocator.allocate({.elementCount = 8,});
			CHECK(block.valid(allocator));
			blocks.emplace_back(block);
		}
		
		for (auto & block : blocks) {
			allocator.free(block.handle);
		}
		CHECK_MESSAGE(allocator.empty(), "allocs: ", allocs);
	}

	// free in reverse order
	for (size_t allocs = 1; allocs < 8; ++allocs) {
		std::vector<srat::VirtualRangeBlock> blocks;
		for (size_t it = 0; it < allocs; ++it) {
			auto const block = allocator.allocate({.elementCount = 8,});
			CHECK(block.valid(allocator));
			blocks.insert(blocks.begin(), block);
		}
		
		for (auto & block : blocks) {
			allocator.free(block.handle);
		}
		CHECK_MESSAGE(allocator.empty(), "allocs: ", allocs);
	}
}

TEST_CASE("virtual block allocator alignment") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	// alloc 1 element to misalign the next free block offset to 1
	auto misalign = allocator.allocate({ .elementCount = 1 });
	CHECK(misalign.valid(allocator));

	// now the free block starts at offset 1
	// allocate with alignment 8 — should align up to offset 8
	// leading pad [1, 8) = 7 elements must be kept as a free block
	auto aligned = allocator.allocate({ .elementCount = 8, .elementAlignment = 8 });
	CHECK(aligned.valid(allocator));
	CHECK(aligned.elementCount == 8);

	// alloc another element immediately after — should be at offset 16
	// verifies the trailing free block after `aligned` is intact
	auto after = allocator.allocate({ .elementCount = 1 });
	CHECK(after.valid(allocator));

	// free aligned — sits between leading pad free block [1,8)
	// and `after` which is still allocated, so no coalesce expected
	allocator.free(aligned.handle);

	// free misalign — offset 0, count 1, adjacent to leading pad [1,8)
	// should coalesce into [0, 8)
	allocator.free(misalign.handle);

	// free after — offset 16, count 1
	// free list should now be: [0,8) and [16,17) and [17, 128)
	// the last two should coalesce into [16, 128)
	allocator.free(after.handle);

	// now free list should be [0,8), [8,16), [16,128)
	// wait — [8,16) is `aligned` which was already freed above
	// so after freeing misalign and after, we have:
	// [0,8) coalesced from misalign+leadingpad, [8,16) from aligned, [16,128) from after+tail
	// all three are adjacent so the allocator should be fully empty
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator stress test") {
	CHECK(srat::virtual_range_allocator_all_empty());

	static constexpr size_t kElementCount = 1024;
	static constexpr size_t kMaxBlocks = 1024;
	static constexpr size_t kIterations = 64;

	auto allocator = srat::VirtualRangeAllocator::create(srat::VirtualRangeCreateParams {
		.elementCount = kElementCount,
		.maxBlockAllocations = kMaxBlocks,
	});

	std::mt19937 rng(42);

	for (size_t iter = 0; iter < kIterations; ++iter) {
		std::vector<srat::VirtualRangeBlock> live;

		// -- fill phase: allocate random sized blocks until full
		while (true) {
			size_t const size = (rng() % 16) + 1;
			auto block = allocator.allocate({ .elementCount = size });
			if (!block.valid(allocator)) { break; }
			live.emplace_back(block);
		}
		CHECK_FALSE(live.empty());

		// -- random free phase: free in random order
		std::shuffle(live.begin(), live.end(), rng);
		for (auto & block : live) {
			allocator.free(block.handle);
		}

		CHECK_MESSAGE(allocator.empty(), "iter: ", iter);
		live.clear();

		// -- partial alloc/free phase: alloc some, free half, alloc more, free rest
		for (size_t i = 0; i < 8; ++i) {
			auto block = allocator.allocate({ .elementCount = (rng() % 16) + 1 });
			if (!block.valid(allocator)) { break; }
			live.emplace_back(block);
		}
		// free every other block
		std::vector<srat::VirtualRangeBlock> remaining;
		for (size_t i = 0; i < live.size(); ++i) {
			if (i % 2 == 0) {
				allocator.free(live[i].handle);
			} else {
				remaining.emplace_back(live[i]);
			}
		}
		// alloc into the gaps
		for (size_t i = 0; i < 4; ++i) {
			auto block = allocator.allocate({ .elementCount = (rng() % 8) + 1 });
			if (!block.valid(allocator)) { break; }
			remaining.emplace_back(block);
		}
		// free everything remaining
		std::shuffle(remaining.begin(), remaining.end(), rng);
		for (auto & block : remaining) {
			allocator.free(block.handle);
		}

		CHECK_MESSAGE(allocator.empty(), "iter (partial): ", iter);

		// -- clear phase: alloc some blocks then clear
		for (size_t i = 0; i < 4; ++i) {
			auto block = allocator.allocate({ .elementCount = (rng() % 32) + 1 });
			(void)block;
		}
		allocator.clear();
		CHECK_MESSAGE(allocator.empty(), "iter (clear): ", iter);
	}
}

TEST_CASE("virtual block allocator exact fit after fragmentation") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 32,
		.maxBlockAllocations = 16,
	});

	// alloc A and B filling the range
	auto a = allocator.allocate({ .elementCount = 16 });
	auto b = allocator.allocate({ .elementCount = 16 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));

	// free A — creates an exact 16-element gap at the start
	allocator.free(a.handle);

	// allocate exactly 16 into the gap — must not absorb B or leave fragments
	auto c = allocator.allocate({ .elementCount = 16 });
	CHECK(c.valid(allocator));
	CHECK(c.elementCount == 16);

	// no free space should remain — B and C fill the range exactly
	auto overflow = allocator.allocate({ .elementCount = 1 });
	CHECK_FALSE(overflow.valid(allocator));

	allocator.free(b.handle);
	allocator.free(c.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator alignment already satisfied") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	// free block starts at offset 0, alignment 8 is already satisfied
	// no leading pad should be created, no extra slot consumed
	auto a = allocator.allocate({ .elementCount = 8, .elementAlignment = 8 });
	CHECK(a.valid(allocator));
	CHECK(a.elementCount == 8);

	// allocate the rest to verify no phantom fragments were created
	auto b = allocator.allocate({ .elementCount = 120 });
	CHECK(b.valid(allocator));

	auto overflow = allocator.allocate({ .elementCount = 1 });
	CHECK_FALSE(overflow.valid(allocator));

	allocator.free(a.handle);
	allocator.free(b.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator block invalid after clear") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	auto a = allocator.allocate({ .elementCount = 8 });
	auto b = allocator.allocate({ .elementCount = 8 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));

	// clear invalidates all outstanding blocks
	allocator.clear();
	CHECK_FALSE(a.valid(allocator));
	CHECK_FALSE(b.valid(allocator));
	CHECK(allocator.empty());

	// allocator should be fully usable after clear
	auto c = allocator.allocate({ .elementCount = 128 });
	CHECK(c.valid(allocator));
	allocator.free(c.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator alternating free order") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 64,
		.maxBlockAllocations = 16,
	});

	// alloc A B C D contiguously
	auto a = allocator.allocate({ .elementCount = 8 });
	auto b = allocator.allocate({ .elementCount = 8 });
	auto c = allocator.allocate({ .elementCount = 8 });
	auto d = allocator.allocate({ .elementCount = 8 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));
	CHECK(c.valid(allocator));
	CHECK(d.valid(allocator));

	allocator.free(b.handle);
	CHECK_FALSE(allocator.empty());

	allocator.free(d.handle);
	CHECK_FALSE(allocator.empty());

	allocator.free(c.handle);
	CHECK_FALSE(allocator.empty());

	// free A — adjacent to B+C+D block, should coalesce entire range
	allocator.free(a.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator block invalid after free") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	auto a = allocator.allocate({ .elementCount = 8 });
	auto b = allocator.allocate({ .elementCount = 8 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));

	allocator.free(a.handle);
	CHECK_FALSE(a.valid(allocator));
	// b must still be valid
	CHECK(b.valid(allocator));

	allocator.free(b.handle);
	CHECK_FALSE(a.valid(allocator));
	CHECK_FALSE(b.valid(allocator));
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator stale block after reallocation") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	auto old = allocator.allocate({ .elementCount = 128 });
	CHECK(old.valid(allocator));
	allocator.free(old.handle);
	CHECK_FALSE(old.valid(allocator));

	// reallocate same region
	auto fresh = allocator.allocate({ .elementCount = 128 });
	CHECK(fresh.valid(allocator));
	// old block must still be invalid even though region is live again
	CHECK_FALSE(old.valid(allocator));

	allocator.free(fresh.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator alignment exact fit to end") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	// misalign by 1 then allocate aligned block that fills exactly to end
	// offset 0: 1 element misalign
	// offset 8: 120 elements aligned to 8 — fills exactly to 128
	auto misalign = allocator.allocate({ .elementCount = 1 });
	CHECK(misalign.valid(allocator));

	auto aligned = allocator.allocate({
		.elementCount = 120,
		.elementAlignment = 8,
	});
	CHECK(aligned.valid(allocator));
	CHECK(aligned.elementCount == 120);

	// no trailing space should remain
	auto overflow = allocator.allocate({ .elementCount = 1 });
	CHECK_FALSE(overflow.valid(allocator));

	allocator.free(misalign.handle);
	allocator.free(aligned.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator elementCount preserved") {
	CHECK(srat::virtual_range_allocator_all_empty());
	auto allocator = srat::VirtualRangeAllocator::create({
		.elementCount = 128,
		.maxBlockAllocations = 16,
	});

	auto a = allocator.allocate({ .elementCount = 7 });
	auto b = allocator.allocate({ .elementCount = 13 });
	auto c = allocator.allocate({ .elementCount = 32 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));
	CHECK(c.valid(allocator));
	CHECK(a.elementCount == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementCount == 32);
	allocator.free(a.handle);
	allocator.free(b.handle);
	allocator.free(c.handle);
	CHECK(allocator.empty());

	c = allocator.allocate({ .elementCount = 32 });
	b = allocator.allocate({ .elementCount = 13 });
	a = allocator.allocate({ .elementCount = 7 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));
	CHECK(c.valid(allocator));
	CHECK(a.elementCount == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementCount == 32);
	allocator.free(a.handle);
	allocator.free(b.handle);
	allocator.free(c.handle);
	CHECK(allocator.empty());

	a = allocator.allocate({ .elementCount = 7 });
	c = allocator.allocate({ .elementCount = 32 });
	b = allocator.allocate({ .elementCount = 13 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));
	CHECK(c.valid(allocator));
	CHECK(a.elementCount == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementCount == 32);
	allocator.free(a.handle);
	allocator.free(b.handle);
	allocator.free(c.handle);
	CHECK(allocator.empty());

	b = allocator.allocate({ .elementCount = 13 });
	c = allocator.allocate({ .elementCount = 32 });
	a = allocator.allocate({ .elementCount = 7 });
	CHECK(a.valid(allocator));
	CHECK(b.valid(allocator));
	CHECK(c.valid(allocator));
	CHECK(a.elementCount == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementCount == 32);
	allocator.free(a.handle);
	allocator.free(b.handle);
	allocator.free(c.handle);
	CHECK(allocator.empty());
}

} // -- end virtual block allocator test suite
