#include <srat/types.hpp>
#include <srat/handle.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <random>

// -- note many of these unit tests are AI generated so that they may
//	have very high coverage (though I also write edge case tests)

TEST_SUITE("handle pool") {

TEST_CASE("handle pool sanity") {
	struct TestHandle {
		u64 id;
	};
	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
	TestHandle handle = handlePool.allocate(42);
	CHECK(handlePool.valid(handle));
	i32 * const value = handlePool.get(handle);
	CHECK(value != nullptr);
	CHECK(*value == 42);
	// free and check invalid
	handlePool.free(handle);
	CHECK_FALSE(handlePool.valid(handle));
	CHECK(handlePool.get(handle) == nullptr);
	CHECK(handlePool.empty());
}

struct TestHandle { u64 id; };

TEST_CASE("handle pool sanity") {
	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
	TestHandle handle = handlePool.allocate(42);
	CHECK(handlePool.valid(handle));
	i32 * value = handlePool.get(handle);
	CHECK(value != nullptr);
	CHECK(*value == 42);
	handlePool.free(handle);
	CHECK_FALSE(handlePool.valid(handle));
	CHECK(handlePool.get(handle) == nullptr);
	CHECK(handlePool.empty());
}

TEST_CASE("handle pool stale handle after reallocation") {
	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
	TestHandle old = handlePool.allocate(1);
	CHECK(handlePool.valid(old));
	handlePool.free(old);
	CHECK_FALSE(handlePool.valid(old));
	// reallocate — likely reuses the same slot
	TestHandle fresh = handlePool.allocate(2);
	CHECK(handlePool.valid(fresh));
	// old handle must still be invalid even though slot is live again
	CHECK_FALSE(handlePool.valid(old));
	CHECK(handlePool.get(old) == nullptr);
	handlePool.free(fresh);
	CHECK(handlePool.empty());
}

TEST_CASE("handle pool allocate until full") {
	static constexpr u64 kMax = 16;
	auto handlePool = srat::HandlePool<TestHandle, i32>::create(kMax);
	std::vector<TestHandle> handles;
	for (u64 i = 0; i < kMax; ++i) {
		TestHandle h = handlePool.allocate((i32)i);
		CHECK(handlePool.valid(h));
		handles.emplace_back(h);
	}
	// pool is full — next allocation should fail
	TestHandle overflow = handlePool.allocate(999);
	CHECK_FALSE(handlePool.valid(overflow));
	CHECK(overflow.id == 0);
	// free all and verify empty
	for (auto & h : handles) {
		handlePool.free(h);
	}
	CHECK(handlePool.empty());
}

// TEST_CASE("handle pool move") {
// 	auto pool1 = srat::HandlePool<TestHandle, i32>::create(128);
// 	TestHandle handle = pool1.allocate(99);
// 	CHECK(pool1.valid(handle));
// 	// move the pool
// 	auto pool2 = std::move(pool1);
// 	// handle must still be valid against the moved-to pool
// 	CHECK(pool2.valid(handle));
// 	i32 * value = pool2.get(handle);
// 	CHECK(value != nullptr);
// 	CHECK(*value == 99);
// 	pool2.free(handle);
// 	CHECK(pool2.empty());
// }

// TEST_CASE("handle pool multiple allocations correct resources") {
// 	static constexpr u64 kCount = 32;
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	std::vector<TestHandle> handles;
// 	for (i32 i = 0; i < (i32)kCount; ++i) {
// 		handles.emplace_back(handlePool.allocate(i * 10));
// 	}
// 	// verify each handle returns the correct resource
// 	for (i32 i = 0; i < (i32)kCount; ++i) {
// 		i32 * value = handlePool.get(handles[i]);
// 		CHECK(value != nullptr);
// 		CHECK_MESSAGE(*value == i * 10, "index: ", i);
// 	}
// 	for (auto & h : handles) {
// 		handlePool.free(h);
// 	}
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool double free") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	TestHandle handle = handlePool.allocate(42);
// 	CHECK(handlePool.valid(handle));
// 	handlePool.free(handle);
// 	CHECK_FALSE(handlePool.valid(handle));
// 	// second free must be a no-op
// 	handlePool.free(handle);
// 	CHECK(handlePool.empty());
// 	// allocator must still be usable
// 	TestHandle fresh = handlePool.allocate(7);
// 	CHECK(handlePool.valid(fresh));
// 	handlePool.free(fresh);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool rvalue allocate") {
// 	auto handlePool = srat::HandlePool<TestHandle, std::vector<i32>>::create(128);
// 	std::vector<i32> vec = { 1, 2, 3, 4, 5 };
// 	TestHandle handle = handlePool.allocate(std::move(vec));
// 	CHECK(handlePool.valid(handle));
// 	auto * value = handlePool.get(handle);
// 	CHECK(value != nullptr);
// 	CHECK(value->size() == 5);
// 	CHECK((*value)[0] == 1);
// 	CHECK((*value)[4] == 5);
// 	handlePool.free(handle);
// 	CHECK(handlePool.empty());
// }
// TEST_CASE("handle pool large struct resource") {
// 	struct LargeResource {
// 		u8 data[4096];
// 		u64 checksum;
// 		i32 id;

// 		static LargeResource make(i32 id) {
// 			LargeResource r {};
// 			r.id = id;
// 			r.checksum = 0;
// 			for (u64 i = 0; i < sizeof(r.data); ++i) {
// 				r.data[i] = (u8)(id ^ i);
// 				r.checksum += r.data[i];
// 			}
// 			return r;
// 		}

// 		bool verify() const {
// 			u64 sum = 0;
// 			for (u64 i = 0; i < sizeof(data); ++i) {
// 				sum += data[i];
// 			}
// 			return sum == checksum;
// 		}
// 	};

// 	static constexpr u64 kCount = 32;
// 	auto handlePool = srat::HandlePool<TestHandle, LargeResource>::create(kCount);

// 	std::vector<TestHandle> handles;
// 	for (i32 i = 0; i < (i32)kCount; ++i) {
// 		handles.emplace_back(handlePool.allocate(LargeResource::make(i)));
// 	}

// 	// verify all resources are intact and not clobbering each other
// 	for (i32 i = 0; i < (i32)kCount; ++i) {
// 		LargeResource * r = handlePool.get(handles[i]);
// 		CHECK(r != nullptr);
// 		CHECK_MESSAGE(r->id == i, "index: ", i);
// 		CHECK_MESSAGE(r->verify(), "index: ", i);
// 	}

// 	// free half, verify the other half is still intact
// 	for (i32 i = 0; i < (i32)kCount; i += 2) {
// 		handlePool.free(handles[i]);
// 	}
// 	for (i32 i = 1; i < (i32)kCount; i += 2) {
// 		LargeResource * r = handlePool.get(handles[i]);
// 		CHECK(r != nullptr);
// 		CHECK_MESSAGE(r->id == i, "index: ", i);
// 		CHECK_MESSAGE(r->verify(), "index: ", i);
// 	}

// 	// free the rest
// 	for (i32 i = 1; i < (i32)kCount; i += 2) {
// 		handlePool.free(handles[i]);
// 	}
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool null handle") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	TestHandle null { 0 };
// 	CHECK_FALSE(handlePool.valid(null));
// 	CHECK(handlePool.get(null) == nullptr);
// 	// free of null handle must be no-op
// 	handlePool.free(null);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool single slot") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(1);
// 	// allocate the only slot
// 	TestHandle a = handlePool.allocate(1);
// 	CHECK(handlePool.valid(a));
// 	CHECK(*handlePool.get(a) == 1);
// 	// pool is full
// 	TestHandle overflow = handlePool.allocate(2);
// 	CHECK_FALSE(handlePool.valid(overflow));
// 	CHECK(overflow.id == 0);
// 	// free and reallocate
// 	handlePool.free(a);
// 	CHECK_FALSE(handlePool.valid(a));
// 	TestHandle b = handlePool.allocate(3);
// 	CHECK(handlePool.valid(b));
// 	CHECK(*handlePool.get(b) == 3);
// 	// old handle must be stale
// 	CHECK_FALSE(handlePool.valid(a));
// 	CHECK(handlePool.get(a) == nullptr);
// 	handlePool.free(b);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool distinct pointers") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	TestHandle a = handlePool.allocate(1);
// 	TestHandle b = handlePool.allocate(2);
// 	TestHandle c = handlePool.allocate(3);
// 	i32 * pa = handlePool.get(a);
// 	i32 * pb = handlePool.get(b);
// 	i32 * pc = handlePool.get(c);
// 	CHECK(pa != nullptr);
// 	CHECK(pb != nullptr);
// 	CHECK(pc != nullptr);
// 	CHECK(pa != pb);
// 	CHECK(pb != pc);
// 	CHECK(pa != pc);
// 	// verify values are not clobbering each other
// 	CHECK(*pa == 1);
// 	CHECK(*pb == 2);
// 	CHECK(*pc == 3);
// 	handlePool.free(a);
// 	handlePool.free(b);
// 	handlePool.free(c);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool valid handle survives other frees") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	TestHandle a = handlePool.allocate(1);
// 	TestHandle b = handlePool.allocate(2);
// 	TestHandle c = handlePool.allocate(3);
// 	// free a and c, b must remain valid and correct
// 	handlePool.free(a);
// 	handlePool.free(c);
// 	CHECK(handlePool.valid(b));
// 	i32 * pb = handlePool.get(b);
// 	CHECK(pb != nullptr);
// 	CHECK(*pb == 2);
// 	handlePool.free(b);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool generation wrap safety") {
// 	// cycle one slot many times to verify generation never wraps to 0
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	static constexpr u32 kCycles = 100000;
// 	for (u32 i = 0; i < kCycles; ++i) {
// 		TestHandle h = handlePool.allocate((i32)i);
// 		CHECK(handlePool.valid(h));
// 		CHECK(h.id != 0); // must never look like null handle
// 		handlePool.free(h);
// 		CHECK_FALSE(handlePool.valid(h));
// 	}
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool stress") {
// 	static constexpr u64 kMax = 64;
// 	static constexpr u64 kIterations = 64;
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(kMax);
// 	std::mt19937 rng(42);

// 	for (u64 iter = 0; iter < kIterations; ++iter) {
// 		// -- fill phase
// 		std::vector<TestHandle> live;
// 		for (u64 i = 0; i < kMax; ++i) {
// 			TestHandle h = handlePool.allocate((i32)i);
// 			if (!handlePool.valid(h)) { break; }
// 			live.emplace_back(h);
// 		}
// 		CHECK_FALSE(live.empty());

// 		// -- verify all resources correct before any frees
// 		for (u64 i = 0; i < live.size(); ++i) {
// 			i32 * v = handlePool.get(live[i]);
// 			CHECK(v != nullptr);
// 			CHECK_MESSAGE(*v == (i32)i, "iter: ", iter, " index: ", i);
// 		}

// 		// -- random free order
// 		std::shuffle(live.begin(), live.end(), rng);
// 		for (auto & h : live) {
// 			CHECK(handlePool.valid(h));
// 			handlePool.free(h);
// 			CHECK_FALSE(handlePool.valid(h));
// 		}
// 		CHECK_MESSAGE(handlePool.empty(), "iter: ", iter);

// 		// -- partial phase: alloc some, free half, alloc more, free rest
// 		std::vector<TestHandle> remaining;
// 		for (u64 i = 0; i < 8; ++i) {
// 			TestHandle h = handlePool.allocate((i32)i);
// 			if (!handlePool.valid(h)) { break; }
// 			remaining.emplace_back(h);
// 		}
// 		std::vector<TestHandle> kept;
// 		for (u64 i = 0; i < remaining.size(); ++i) {
// 			if (i % 2 == 0) {
// 				handlePool.free(remaining[i]);
// 				// verify stale immediately
// 				CHECK_FALSE(handlePool.valid(remaining[i]));
// 			} else {
// 				kept.emplace_back(remaining[i]);
// 			}
// 		}
// 		// kept handles must still be valid
// 		for (auto & h : kept) {
// 			CHECK(handlePool.valid(h));
// 		}
// 		// alloc into freed gaps
// 		for (u64 i = 0; i < 4; ++i) {
// 			TestHandle h = handlePool.allocate((i32)i);
// 			if (!handlePool.valid(h)) { break; }
// 			kept.emplace_back(h);
// 		}
// 		std::shuffle(kept.begin(), kept.end(), rng);
// 		for (auto & h : kept) {
// 			handlePool.free(h);
// 		}
// 		CHECK_MESSAGE(handlePool.empty(), "iter (partial): ", iter);
// 	}
// }

// TEST_CASE("handle pool empty false while handles live") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);
// 	CHECK(handlePool.empty());

// 	TestHandle a = handlePool.allocate(1);
// 	CHECK_FALSE(handlePool.empty());

// 	TestHandle b = handlePool.allocate(2);
// 	CHECK_FALSE(handlePool.empty());

// 	handlePool.free(a);
// 	CHECK_FALSE(handlePool.empty());

// 	handlePool.free(b);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool no resource bleed on slot reuse") {
// 	auto handlePool = srat::HandlePool<TestHandle, i32>::create(128);

// 	for (i32 iter = 0; iter < 64; ++iter) {
// 		TestHandle h = handlePool.allocate(iter * 100);
// 		CHECK(handlePool.valid(h));
// 		i32 * v = handlePool.get(h);
// 		CHECK(v != nullptr);
// 		// verify the slot contains exactly what was written, not a previous value
// 		CHECK_MESSAGE(*v == iter * 100, "iter: ", iter);
// 		handlePool.free(h);
// 	}
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool rvalue move semantics") {
// 	struct MoveTracker {
// 		i32 value;
// 		bool wasMoved { false };
// 		MoveTracker(i32 v) : value(v) {}
// 		MoveTracker(MoveTracker const & o) : value(o.value), wasMoved(false) {}
// 		MoveTracker(MoveTracker && o) : value(o.value), wasMoved(true) { o.value = -1; }
// 		MoveTracker & operator=(MoveTracker const & o) {
// 			value = o.value;
// 			wasMoved = false;
// 			return *this;
// 		}
// 		MoveTracker & operator=(MoveTracker && o) {
// 			value = o.value;
// 			wasMoved = true;
// 			o.value = -1;
// 			return *this;
// 		}
// 	};

// 	auto handlePool = srat::HandlePool<TestHandle, MoveTracker>::create(128);
// 	MoveTracker tracker(42);
// 	TestHandle h = handlePool.allocate(std::move(tracker));
// 	CHECK(handlePool.valid(h));

// 	MoveTracker * stored = handlePool.get(h);
// 	CHECK(stored != nullptr);
// 	CHECK(stored->value == 42);
// 	CHECK(stored->wasMoved); // must have been moved not copied

// 	// original must be invalidated by move
// 	CHECK(tracker.value == -1);

// 	handlePool.free(h);
// 	CHECK(handlePool.empty());
// }

// TEST_CASE("handle pool stress large struct") {
// 	struct LargeResource {
// 		u8 data[1024];
// 		u64 checksum;
// 		i32 id;

// 		static LargeResource make(i32 id) {
// 			LargeResource r {};
// 			r.id = id;
// 			r.checksum = 0;
// 			for (u64 i = 0; i < sizeof(r.data); ++i) {
// 				r.data[i] = (u8)(id ^ i);
// 				r.checksum += r.data[i];
// 			}
// 			return r;
// 		}

// 		bool verify() const {
// 			u64 sum = 0;
// 			for (u64 i = 0; i < sizeof(data); ++i) {
// 				sum += data[i];
// 			}
// 			return sum == checksum;
// 		}
// 	};

// 	static constexpr u64 kMax = 32;
// 	static constexpr u64 kIterations = 16;
// 	auto handlePool = srat::HandlePool<TestHandle, LargeResource>::create(kMax);
// 	std::mt19937 rng(42);

// 	for (u64 iter = 0; iter < kIterations; ++iter) {
// 		std::vector<TestHandle> live;

// 		// fill phase
// 		for (u64 i = 0; i < kMax; ++i) {
// 			TestHandle h = handlePool.allocate(LargeResource::make((i32)i));
// 			if (!handlePool.valid(h)) { break; }
// 			live.emplace_back(h);
// 		}
// 		CHECK_FALSE(live.empty());

// 		// verify all intact
// 		for (u64 i = 0; i < live.size(); ++i) {
// 			LargeResource * r = handlePool.get(live[i]);
// 			CHECK(r != nullptr);
// 			CHECK_MESSAGE(r->id == (i32)i, "iter: ", iter, " i: ", i);
// 			CHECK_MESSAGE(r->verify(), "iter: ", iter, " i: ", i);
// 		}

// 		// free in random order
// 		std::shuffle(live.begin(), live.end(), rng);
// 		for (auto & h : live) {
// 			handlePool.free(h);
// 			CHECK_FALSE(handlePool.valid(h));
// 		}
// 		CHECK_MESSAGE(handlePool.empty(), "iter: ", iter);
// 	}
// }
} // TEST_SUITE("handle pool")
