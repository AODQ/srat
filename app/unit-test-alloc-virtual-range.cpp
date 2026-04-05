#include <srat/core-types.hpp>
#include <srat/alloc-virtual-range.hpp>

#include <doctest/doctest.h> // NOLINT

#include <algorithm>
#include <random>

// -- note many of these unit tests are AI generated so that they may
//	have very high coverage (though I also write edge case tests)

TEST_SUITE("alloc virtual block") {

// -----------------------------------------------------------------------------
// -- freelist allocation strategy
// -----------------------------------------------------------------------------

TEST_CASE("virtual block allocator [freelist] sanity") {
	auto const allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator [freelist] one allocation") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
	CHECK(block.valid(allocator));
	CHECK(!allocator.empty());
	allocator.free(block.handle);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator [freelist] alloc+free+alloc+free") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	{
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
		CHECK(block.valid(allocator));
		CHECK(!allocator.empty());
		allocator.free(block.handle);
		CHECK(allocator.empty());
	}

	{
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
		CHECK(block.valid(allocator));
		CHECK(!allocator.empty());
		allocator.free(block.handle);
		CHECK(allocator.empty());
	}
}

TEST_CASE("virtual block allocate [freelist] overflow") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 129,});
	srat::AllocVirtualRangeBlock block3 = allocator.allocate({.elementCount = 128,});
	srat::AllocVirtualRangeBlock block2 = allocator.allocate({.elementCount = 1,});

	CHECK(!block.valid(allocator));
	CHECK(block3.valid(allocator));
	CHECK(!block2.valid(allocator));
}

TEST_CASE("virtual block reset [freelist] one chunk") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 128,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 128,});
	CHECK(block.valid(allocator));
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset [freelist] large chunks") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 128,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	for (size_t it =0; it < 16; ++it) {
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 8,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset [freelist] small chunks") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 128,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	for (size_t it =0; it < 128; ++it) {
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 1,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block reset [freelist] then reset") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 128,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	for (size_t it =0; it < 16; ++it) {
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 8,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
	for (size_t it = 0; it < 127; ++it) {
		srat::AllocVirtualRangeBlock block = allocator.allocate({.elementCount = 1,});
		CHECK(block.valid(allocator));
	}
	allocator.clear();
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator [freelist] many allocations then free") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 16,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
		})
	);

	// free in forward order
	for (size_t allocs = 1; allocs < 8; ++allocs) {
		std::vector<srat::AllocVirtualRangeBlock> blocks;
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
		std::vector<srat::AllocVirtualRangeBlock> blocks;
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

TEST_CASE("virtual block allocator [freelist] alignment") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] stress test") {

	static constexpr size_t kElementCount = 1024;
	static constexpr size_t kMaxBlocks = 1024;
	static constexpr size_t kIterations = 64;

	auto allocator = srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
		.debugName = "test allocator",
		.elementCount = kElementCount,
		.maxBlockAllocations = kMaxBlocks,
			.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	std::mt19937 rng(42);

	for (size_t iter = 0; iter < kIterations; ++iter) {
		std::vector<srat::AllocVirtualRangeBlock> live;

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
		std::vector<srat::AllocVirtualRangeBlock> remaining;
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

TEST_CASE("virtual block allocator [freelist] exact fit after fragmentation") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 32,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] alignment already satisfied") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] block invalid after clear") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] alternating free order") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 64,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] block invalid after free") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] stale block after reallocation") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] alignment exact fit to end") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

TEST_CASE("virtual block allocator [freelist] elementCount preserved") {
	auto allocator = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
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

// -----------------------------------------------------------------------------
// -- linear allocation strategy
// -----------------------------------------------------------------------------

TEST_CASE("virtual block allocator [linear] sanity") {
	auto const allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);
	CHECK(allocator.empty());
}

TEST_CASE("virtual block allocator [linear] one allocation") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	auto block = allocator.allocate({ .elementCount = 128 });
	CHECK(block.elementCount == 128);
	CHECK(block.elementOffset == 0);
	CHECK(!allocator.empty());
}

TEST_CASE("virtual block allocator [linear] sequential offsets") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	auto a = allocator.allocate({ .elementCount = 7 });
	auto b = allocator.allocate({ .elementCount = 13 });
	auto c = allocator.allocate({ .elementCount = 32 });
	CHECK(a.elementOffset == 0);
	CHECK(a.elementCount == 7);
	CHECK(b.elementOffset == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementOffset == 20);
	CHECK(c.elementCount == 32);
}

TEST_CASE("virtual block allocator [linear] overflow") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	auto a = allocator.allocate({ .elementCount = 128 });
	CHECK(a.elementCount == 128);

	// no space left
	auto b = allocator.allocate({ .elementCount = 1 });
	CHECK(b.elementCount == 0);
	CHECK(b.handle == 0);

	// too large from the start
	allocator.clear();
	auto c = allocator.allocate({ .elementCount = 129 });
	CHECK(c.elementCount == 0);
	CHECK(c.handle == 0);
}

TEST_CASE("virtual block allocator [linear] clear resets") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	for (u32 i = 0; i < 16; ++i) {
		auto block = allocator.allocate({ .elementCount = 8 });
		CHECK(block.elementCount == 8);
	}
	CHECK(!allocator.empty());

	allocator.clear();
	CHECK(allocator.empty());

	// full capacity available again after clear
	auto block = allocator.allocate({ .elementCount = 128 });
	CHECK(block.elementCount == 128);
	CHECK(block.elementOffset == 0);
}

TEST_CASE("virtual block allocator [linear] clear then reuse") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	// fill, clear, fill again — offsets should restart from 0
	for (u32 i = 0; i < 16; ++i) {
		allocator.allocate({ .elementCount = 8 });
	}
	allocator.clear();

	auto a = allocator.allocate({ .elementCount = 10 });
	auto b = allocator.allocate({ .elementCount = 20 });
	CHECK(a.elementOffset == 0);
	CHECK(a.elementCount == 10);
	CHECK(b.elementOffset == 10);
	CHECK(b.elementCount == 20);
}

TEST_CASE("virtual block allocator [linear] free is no-op") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	auto a = allocator.allocate({ .elementCount = 32 });
	auto b = allocator.allocate({ .elementCount = 32 });

	// free should not reclaim space — next alloc continues from where we left off
	allocator.free(a.handle);
	allocator.free(b.handle);

	auto c = allocator.allocate({ .elementCount = 32 });
	CHECK(c.elementOffset == 64); // not 0 or 32
	CHECK(c.elementCount == 32);
}

TEST_CASE("virtual block allocator [linear] alignment") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	// misalign by 1
	auto a = allocator.allocate({ .elementCount = 1 });
	CHECK(a.elementOffset == 0);

	// next alloc aligned to 8 should land at offset 8
	auto b = allocator.allocate({ .elementCount = 16, .elementAlignment = 8 });
	CHECK(b.elementOffset == 8);
	CHECK(b.elementCount == 16);

	// next unaligned alloc continues right after
	auto c = allocator.allocate({ .elementCount = 1 });
	CHECK(c.elementOffset == 24);
}

TEST_CASE("virtual block allocator [linear] alignment overflow") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 16,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	// alloc 1 to misalign
	allocator.allocate({ .elementCount = 1 });

	// aligned alloc: aligns to 8 (offset 8) + 10 elements = 18 > 16
	auto b = allocator.allocate({ .elementCount = 10, .elementAlignment = 8 });
	CHECK(b.elementCount == 0);
	CHECK(b.handle == 0);
}

TEST_CASE("virtual block allocator [linear] elementCount preserved") {
	auto allocator = (
		srat::AllocVirtualRange::create(srat::AllocVirtualRangeCreateParams {
			.debugName = "test allocator",
			.elementCount = 128,
			.maxBlockAllocations = 0,
			.strategy = srat::AllocVirtualRangeStrategy::Linear,
		})
	);

	auto a = allocator.allocate({ .elementCount = 7 });
	auto b = allocator.allocate({ .elementCount = 13 });
	auto c = allocator.allocate({ .elementCount = 32 });
	CHECK(a.elementCount == 7);
	CHECK(b.elementCount == 13);
	CHECK(c.elementCount == 32);
}

// -----------------------------------------------------------------------------
// -- move semantics
// -----------------------------------------------------------------------------

TEST_CASE("virtual block allocator move constructor [freelist]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	auto block = a.allocate({ .elementCount = 64 });
	CHECK(block.valid(a));

	// move construct
	auto b = std::move(a);

	// block must still be valid through the new owner
	CHECK(block.valid(b));
	CHECK(!b.empty());
}

TEST_CASE("virtual block allocator move constructor [linear]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 0,
		.strategy = srat::AllocVirtualRangeStrategy::Linear,
	});

	auto block = a.allocate({ .elementCount = 64 });
	CHECK(block.elementCount == 64);
	CHECK(block.elementOffset == 0);

	auto b = std::move(a);

	// state must be preserved — next alloc continues from offset 64
	auto block2 = b.allocate({ .elementCount = 32 });
	CHECK(block2.elementOffset == 64);
	CHECK(block2.elementCount == 32);
}

TEST_CASE("virtual block allocator move assignment [freelist]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	auto b = srat::AllocVirtualRange::create({
		.debugName = "test allocator b",
		.elementCount = 64,
		.maxBlockAllocations = 8,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	auto block = a.allocate({ .elementCount = 64 });
	CHECK(block.valid(a));

	// move assign — b's previous state is discarded
	b = std::move(a);

	CHECK(block.valid(b));
	CHECK(!b.empty());
}

TEST_CASE("virtual block allocator move assignment [linear]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 0,
		.strategy = srat::AllocVirtualRangeStrategy::Linear,
	});

	auto b = srat::AllocVirtualRange::create({
		.debugName = "test allocator b",
		.elementCount = 64,
		.maxBlockAllocations = 0,
		.strategy = srat::AllocVirtualRangeStrategy::Linear,
	});

	a.allocate({ .elementCount = 32 });

	b = std::move(a);

	// cursor must be at 32, not reset to 0
	auto block = b.allocate({ .elementCount = 16 });
	CHECK(block.elementOffset == 32);
	CHECK(block.elementCount == 16);
}

TEST_CASE("virtual block allocator move then allocate [freelist]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	auto b = std::move(a);

	// moved-into allocator must be fully functional
	auto block = b.allocate({ .elementCount = 128 });
	CHECK(block.valid(b));
	b.free(block.handle);
	CHECK(b.empty());
}

TEST_CASE("virtual block allocator move then free [freelist]") {
	auto a = srat::AllocVirtualRange::create({
		.debugName = "test allocator",
		.elementCount = 128,
		.maxBlockAllocations = 16,
		.strategy = srat::AllocVirtualRangeStrategy::FreeList,
	});

	auto block = a.allocate({ .elementCount = 64 });
	CHECK(block.valid(a));

	auto b = std::move(a);

	// must be able to free a block allocated before the move
	b.free(block.handle);
	CHECK(b.empty());
}

} // -- end virtual block allocator test suite
