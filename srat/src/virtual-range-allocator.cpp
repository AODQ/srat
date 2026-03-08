#include <srat/virtual-range-allocator.hpp>

#include <srat/handle.hpp>

#include <utility>
#include <set> // only for tracking allocators

// -----------------------------------------------------------------------------
// -- virtual range internal data structure
// -----------------------------------------------------------------------------

namespace
{

struct VirtualRangeBlockInternal
{
	u64 elementOffset;
	u64 elementCount;
};

struct VirtualRangeAllocatorData
{
	u64 elementCount;
	u64 maxBlockAllocations;
	VirtualRangeBlockInternal * allocatedBlocks;
	u32 * nextFreeIndices;
	u32 * generations;
	u32 freeListHeadIndex { 0u };

	bool slotAvailable(u32 index) const
	{
		return generations[index] % 2 == 0;
	}
};
#define AllocatorData(allocator, ...) ( \
	*reinterpret_cast<VirtualRangeAllocatorData __VA_ARGS__ *>( \
		(allocator)._internalData \
	) \
)
static_assert(
	sizeof(VirtualRangeAllocatorData) == sizeof(srat::VirtualRangeAllocator),
	"fit mismatch"
);

static constexpr u32 skFreeListEnd = UINT32_MAX;

#if SRAT_DEBUG
std::set<srat::VirtualRangeAllocator *> sAllocators;
#endif

} // namespace

// -----------------------------------------------------------------------------
// -- virtual range block implementation
// -----------------------------------------------------------------------------

bool srat::VirtualRangeBlock::valid(
    VirtualRangeAllocator const & allocator
 ) const
 {
 	if (this->elementCount == 0) { return false; }
	u32 const gen = (
		AllocatorData(allocator, const).generations[handle_index(this->handle)]
	);
	return handle_generation(this->handle) == gen;
 }

// -----------------------------------------------------------------------------
// -- virtual range allocator implementation
// -----------------------------------------------------------------------------

srat::VirtualRangeAllocator srat::VirtualRangeAllocator::create(
	VirtualRangeCreateParams const & params
)
{
	SRAT_ASSERT(params.elementCount > 0);
	SRAT_ASSERT(params.maxBlockAllocations > 0);
	SRAT_ASSERT(params.maxBlockAllocations < skFreeListEnd);
	VirtualRangeAllocator allocator {};
	VirtualRangeAllocatorData & self = AllocatorData(allocator);
	// initialize the allocator with the provided parameters
	self = {
		.elementCount = params.elementCount,
		.allocatedBlocks = new VirtualRangeBlockInternal[
			params.maxBlockAllocations
		],
		.nextFreeIndices = new u32[params.maxBlockAllocations],
		.generations = new u32[params.maxBlockAllocations],
		.maxBlockAllocations = params.maxBlockAllocations,
		.freeListHeadIndex = 0u,
	};
	for (size_t it = 0; it < params.maxBlockAllocations; ++it) {
		self.allocatedBlocks[it] = { .elementCount = 0, .elementOffset = 0 };
		self.nextFreeIndices[it] = skFreeListEnd;
		self.generations[it] = 0u; // initialize to free
	}
	// initialize the allocated blocks to an empty state
	self.allocatedBlocks[0] = VirtualRangeBlockInternal {
		.elementOffset = 0,
		.elementCount = params.elementCount,
	};
	self.nextFreeIndices[0] = skFreeListEnd;
	self.generations[0] = 0u; // initialize to free
#if SRAT_DEBUG
	// track the allocator for debugging purposes
	sAllocators.insert(&allocator);
#endif
	return allocator;
}

void srat::VirtualRangeAllocator::moveFrom(VirtualRangeAllocator && o)
{
	// move the internal data from the source allocator to this allocator
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	VirtualRangeAllocatorData & otherSelf = AllocatorData(o);

	// apply the move constructor to the internal debugging data
#if SRAT_DEBUG
	{
		auto it = sAllocators.find(&o);
		SRAT_ASSERT(it != sAllocators.end());
		sAllocators.erase(it);
		sAllocators.insert(this);
	}
#endif

	// move the internal data
	self = std::move(otherSelf);
	// invalidate the source allocator's internal data
	otherSelf = {
		.elementCount = 0,
		.maxBlockAllocations = 0,
		.allocatedBlocks = nullptr,
		.nextFreeIndices = nullptr,
		.generations = nullptr,
		.freeListHeadIndex = skFreeListEnd,
	};
}

srat::VirtualRangeAllocator::VirtualRangeAllocator(VirtualRangeAllocator && o)
{
	moveFrom(std::move(o));
}

srat::VirtualRangeAllocator & srat::VirtualRangeAllocator::operator=(
	VirtualRangeAllocator && o
)
{
	if (this != &o) {
		// free existing resources
		VirtualRangeAllocatorData & self = AllocatorData(*this);
		delete[] self.allocatedBlocks;
		delete[] self.nextFreeIndices;
		delete[] self.generations;
		moveFrom(std::move(o));
	}
	return *this;
}

srat::VirtualRangeAllocator::~VirtualRangeAllocator()
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	delete[] self.allocatedBlocks;
	delete[] self.nextFreeIndices;
	delete[] self.generations;
#if SRAT_DEBUG
	if (self.allocatedBlocks != nullptr)
	{
		auto it = sAllocators.find(this);
		SRAT_ASSERT(it != sAllocators.end());
		sAllocators.erase(it);
	}
#endif
}

// -----------------------------------------------------------------------------
// -- virtual range allocator interface
// -----------------------------------------------------------------------------

srat::VirtualRangeBlock srat::VirtualRangeAllocator::allocate(
	VirtualRangeAllocateParams const & request
)
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);

	// -- find a free block that can satisfy the allocation request
	u64 prevIdx = skFreeListEnd;
	u64 currIdx = self.freeListHeadIndex;
	while (currIdx != skFreeListEnd) {
		VirtualRangeBlockInternal & curr = self.allocatedBlocks[currIdx];
		u64 const alignedOffset = (
			alignUp(curr.elementOffset, request.elementAlignment)
		);
		u64 const leadingPad = alignedOffset - curr.elementOffset;
		u64 const totalRequired = leadingPad + request.elementCount;
		if (curr.elementCount < totalRequired) {
			prevIdx = currIdx;
			currIdx = self.nextFreeIndices[currIdx];
			continue;
		}
		// -- found a block that can satisfy the request, try to split
		//    the block into an allocated and free block
		u64 const trailingCount = curr.elementCount - totalRequired;
		u64 actualElementCount = request.elementCount;

		// -- find required slots
		u64 allocSlot = currIdx;
		if (leadingPad > 0) {
			allocSlot = skFreeListEnd;
			for (u64 it = 0; it < self.maxBlockAllocations; ++it) {
				if (it != currIdx && self.slotAvailable(it)) {
					allocSlot = it;
					break;
				}
			}
			if (allocSlot == skFreeListEnd) {
				// no slot available for the allocation identity, skip this block
				prevIdx = currIdx;
				currIdx = self.nextFreeIndices[currIdx];
				continue;
			}
		}

		u64 trailingSlot = skFreeListEnd;
		if (trailingCount > 0) {
			for (u64 it = 0; it < self.maxBlockAllocations; ++it) {
				if (it != currIdx && it != allocSlot && self.slotAvailable(it)) {
					trailingSlot = it;
					break;
				}
			}
			if (trailingSlot == skFreeListEnd) {
				// absorb trailing into allocation
				actualElementCount += trailingCount;
			}
		}

		// -- now modify the free list
		if (leadingPad > 0) {
			// curr stays as the leading fragment
			curr.elementCount = leadingPad;

			if (trailingCount > 0 && trailingSlot != skFreeListEnd) {
				// insert trailing fragment after curr
				self.allocatedBlocks[trailingSlot] = VirtualRangeBlockInternal {
					.elementCount = trailingCount,
					.elementOffset = alignedOffset + actualElementCount,
				};
				self.generations[trailingSlot] = 0u;
				self.nextFreeIndices[trailingSlot] = self.nextFreeIndices[currIdx];
				self.nextFreeIndices[currIdx] = trailingSlot;
			}

			// set up allocSlot as the allocation identity
			self.allocatedBlocks[allocSlot] = VirtualRangeBlockInternal {
				.elementCount = actualElementCount,
				.elementOffset = alignedOffset,
			};
			generation_inc(self.generations[allocSlot]);
			SRAT_ASSERT(self.generations[allocSlot] % 2 == 1); // odd=alive

		} else {
			// no leading fragment — currIdx is the allocation slot
			u32 const nextIdx = self.nextFreeIndices[currIdx];

			if (trailingCount > 0 && trailingSlot != skFreeListEnd) {
				// reuse currIdx for trailing fragment, splice in place
				self.allocatedBlocks[trailingSlot] = VirtualRangeBlockInternal {
					.elementCount = trailingCount,
					.elementOffset = alignedOffset + actualElementCount,
				};
				self.generations[trailingSlot] = self.generations[currIdx];
				self.nextFreeIndices[trailingSlot] = nextIdx;

				// unlink currIdx from free list, replace with trailingSlot
				if (prevIdx == skFreeListEnd) {
					self.freeListHeadIndex = trailingSlot;
				} else {
					self.nextFreeIndices[prevIdx] = trailingSlot;
				}
			} else {
				// no trailing — just unlink currIdx from free list
				if (prevIdx == skFreeListEnd) {
					self.freeListHeadIndex = nextIdx;
				} else {
					self.nextFreeIndices[prevIdx] = nextIdx;
				}
			}

			// mark currIdx as the allocation
			self.allocatedBlocks[currIdx] = VirtualRangeBlockInternal {
				.elementCount = actualElementCount,
				.elementOffset = alignedOffset,
			};
			generation_inc(self.generations[currIdx]);
			SRAT_ASSERT(self.generations[currIdx] % 2 == 1); // odd=alive
		}

		return {
			.elementCount = actualElementCount,
			.elementOffset = alignedOffset,
			.handle = handle_make(allocSlot, self.generations[allocSlot]),
		};
	}

	// no free block remaining
	return srat::VirtualRangeBlock{
		.elementCount = 0,
		.elementOffset = 0,
		.handle = 0,
	};
}

void srat::VirtualRangeAllocator::clear()
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	for (size_t it = 0; it < self.maxBlockAllocations; ++it) {
		self.allocatedBlocks[it] = VirtualRangeBlockInternal { 0, 0, };
		self.nextFreeIndices[it] = skFreeListEnd;
		// kill alive blocks by incrementing generation
		if (!self.slotAvailable(it)) {
			generation_inc(self.generations[it]);
			SRAT_ASSERT(self.generations[it] % 2 == 0); // even=dead
		}
	}
	self.freeListHeadIndex = 0;
	self.generations[0] = 0u;
	self.allocatedBlocks[0] = VirtualRangeBlockInternal {
		.elementCount = self.elementCount,
		.elementOffset = 0,
	};
	self.nextFreeIndices[0] = skFreeListEnd;
}

void srat::VirtualRangeAllocator::free(VirtualRangeBlock const & block)
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);

	// -- invalidate the block by incrementing its generation
	{
		u32 const blockIdx = handle_index(block.handle);
		generation_inc(self.generations[blockIdx]);
		SRAT_ASSERT(self.generations[blockIdx] % 2 == 0); // even=dead
	}

	u32 freeSlot = handle_index(block.handle);

	// -- insert the freed block into the free list
	u32 prevIdx = skFreeListEnd;
	u32 currIdx = self.freeListHeadIndex;
	while (currIdx != skFreeListEnd) {
		if (self.allocatedBlocks[currIdx].elementOffset > block.elementOffset) {
			break;
		}
		prevIdx = currIdx;
		currIdx = self.nextFreeIndices[currIdx];
	}

	// -- write the block into the free list and wire it in
	{
		VirtualRangeBlockInternal & slot = self.allocatedBlocks[freeSlot];
		slot = {
			.elementCount = block.elementCount,
			.elementOffset = block.elementOffset,
		};
		self.nextFreeIndices[freeSlot] = currIdx;

		if (prevIdx == skFreeListEnd) {
			self.freeListHeadIndex = freeSlot;
		} else {
			self.nextFreeIndices[prevIdx] = freeSlot;
		}
	}

	// -- coalesce with previous block if adjacent
	{
		VirtualRangeBlockInternal & merge = self.allocatedBlocks[freeSlot];
		if (prevIdx != skFreeListEnd) {
			VirtualRangeBlockInternal & prev = self.allocatedBlocks[prevIdx];
			if (prev.elementOffset + prev.elementCount == merge.elementOffset) {
				prev.elementCount += merge.elementCount;
				self.nextFreeIndices[prevIdx] = self.nextFreeIndices[freeSlot];
				// invalidate the merge block
				merge = { .elementCount = 0, .elementOffset = 0, };
				freeSlot = prevIdx;
			}
		}
	}

	// -- coalesce with next block if adjacent
	{
		VirtualRangeBlockInternal & merge = self.allocatedBlocks[freeSlot];
		u32 const nextIdx = self.nextFreeIndices[freeSlot];
		if (nextIdx != skFreeListEnd) {
			VirtualRangeBlockInternal & next = self.allocatedBlocks[nextIdx];
			if (merge.elementOffset + merge.elementCount == next.elementOffset) {
				merge.elementCount += next.elementCount;
				self.nextFreeIndices[freeSlot] = self.nextFreeIndices[nextIdx];
				// invalidate the next block
				next = { .elementCount = 0, .elementOffset = 0, };
			}
		}
	}
}

void srat::VirtualRangeAllocator::printAllocationStats() const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
    printf("-- allocation stats --\n");
    printf("   elementCount: %llu, maxBlockAllocations: %llu\n",
        self.elementCount, self.maxBlockAllocations);
    printf("   freeListHead: %u\n", self.freeListHeadIndex);

    u32 idx = self.freeListHeadIndex;
    u32 count = 0;
    while (idx != skFreeListEnd) {
        VirtualRangeBlockInternal const & block = self.allocatedBlocks[idx];
        printf("   [%u] offset: %llu, count: %llu, next: %u, gen: %u\n",
            idx,
            block.elementOffset,
            block.elementCount,
            self.nextFreeIndices[idx],
            self.generations[idx]
        );
        idx = self.nextFreeIndices[idx];
        ++count;
        if (count > self.maxBlockAllocations) {
            printf("   !! free list corrupt\n");
            break;
        }
    }
    printf("   free block count: %u\n", count);

    printf("   all slots:\n");
    for (u64 i = 0; i < self.maxBlockAllocations; ++i) {
        printf("   [%llu] offset: %llu, count: %llu, gen: %u, available: %d\n",
            i,
            self.allocatedBlocks[i].elementOffset,
            self.allocatedBlocks[i].elementCount,
            self.generations[i],
            self.slotAvailable(i)
        );
    }
    printf("-- end allocation stats --\n");
}

bool srat::VirtualRangeAllocator::empty() const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	u32 const freeHead = self.freeListHeadIndex;
	if (freeHead == skFreeListEnd) { return false; }
	VirtualRangeBlockInternal const & firstBlock = (
		self.allocatedBlocks[freeHead]
	);

	// also check all generations are free
	for (u64 it = 0; it < self.maxBlockAllocations; ++it) {
		if (!self.slotAvailable(it)) {
			return false;
		}
	}

	return (
		   firstBlock.elementCount == self.elementCount
		&& firstBlock.elementOffset == 0
	);
}

#if SRAT_DEBUG
bool srat::virtual_range_allocator_all_empty()
{
	for (auto allocator : sAllocators) {
		if (!allocator->empty()) {
			return false;
		}
	}
	return true;
}
#endif
