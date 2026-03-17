#include <srat/virtual-range-allocator.hpp>

#include <srat/handle.hpp>

#include <string>
#include <utility>
#include <set> // only for tracking allocators

// -----------------------------------------------------------------------------
// -- virtual range internal data structure
// -----------------------------------------------------------------------------

// TODO: remove the odd/even generation for alive/dead tracking, i already
//       use an allocated bool so it's a bit redundant

namespace
{

struct VirtualRangeBlockInternal
{
	u64 elementOffset;
	u64 elementCount;
	u32 nextFreeIndex;
	u32 generation;
	bool allocated;
};

struct VirtualRangeAllocatorDataFreelist
{
	u32 maxBlockAllocations;
	VirtualRangeBlockInternal * allocatedBlocks;
	u32 freeListHeadIndex { 0u };
};

struct VirtualRangeAllocatorDataLinear
{
	u32 nextIndex;
};

struct VirtualRangeAllocatorData
{
	char const * debugName;
	u64 elementCount;
	union {
		VirtualRangeAllocatorDataFreelist freelist;
		VirtualRangeAllocatorDataLinear linear;
	};
	srat::VirtualRangeAllocationStrategy strategy {};

	bool isAlive(u32 index) const
	{
		u32 const gen = freelist.allocatedBlocks[index].generation;
		bool const allocated = freelist.allocatedBlocks[index].allocated;
		return gen != 0 && srat::generation_alive(gen) && allocated;
	}

	bool isDead(u32 index) const
	{
		u32 const gen = freelist.allocatedBlocks[index].generation;
		bool const allocated = freelist.allocatedBlocks[index].allocated;
		return gen == 0 || (!srat::generation_alive(gen) && !allocated);
	}

	void sortFreeList();
};
#define AllocatorData(allocator, ...) ( \
	*reinterpret_cast<VirtualRangeAllocatorData __VA_ARGS__ *>( \
		(allocator)._internalData \
	) \
)
static_assert(
	sizeof(VirtualRangeAllocatorData) <= sizeof(srat::VirtualRangeAllocator),
	"fit mismatch"
);

static constexpr u32 skFreeListEnd = UINT32_MAX;

#if SRAT_DEBUG()
std::set<srat::VirtualRangeAllocator *> sAllocators;
#endif

} // namespace

void VirtualRangeAllocatorData::sortFreeList()
{
	// -- sort the free list by element offset using insertion sort
	u32 sortedHead = skFreeListEnd;
	u32 it = freelist.freeListHeadIndex;
	while (it != skFreeListEnd) {
		auto & currBlock = freelist.allocatedBlocks[it];
		u32 const nextIt = currBlock.nextFreeIndex;
		// insert current block into sorted list
		if (
			   sortedHead == skFreeListEnd
			|| (
				currBlock.elementOffset
				< freelist.allocatedBlocks[sortedHead].elementOffset
			)
		) {
			// insert at head of sorted list
			currBlock.nextFreeIndex = sortedHead;
			sortedHead = it;
			it = nextIt;
			continue;
		}
		// insert into middle or end of sorted list
		u32 sortedIt = sortedHead;
		while (true) {
			auto & sortedBlock = freelist.allocatedBlocks[sortedIt];
			u32 const sortedNext = sortedBlock.nextFreeIndex;
			if (
					sortedNext == skFreeListEnd
				|| (
					  currBlock.elementOffset
					< freelist.allocatedBlocks[sortedNext].elementOffset
				)
			) {
				// insert after sortedIt
				currBlock.nextFreeIndex = sortedNext;
				sortedBlock.nextFreeIndex = it;
				break;
			}
			sortedIt = sortedNext;
		}
		it = nextIt;
	}
	freelist.freeListHeadIndex = sortedHead;
}

// -----------------------------------------------------------------------------
// -- virtual range block implementation
// -----------------------------------------------------------------------------

bool srat::VirtualRangeBlock::valid(
    VirtualRangeAllocator const & allocator
 ) const
 {
	u32 const idx = handle_index(this->handle);
	VirtualRangeAllocatorData const & self = AllocatorData(allocator, const);
	SRAT_ASSERT(idx < self.freelist.maxBlockAllocations);
	auto const & block = self.freelist.allocatedBlocks[idx];
	if (!block.allocated) { return false; }
	return handle_generation(this->handle) == block.generation;
 }

// -----------------------------------------------------------------------------
// -- virtual range allocator interface
// -----------------------------------------------------------------------------

srat::VirtualRangeAllocator srat::VirtualRangeAllocator::create(
	VirtualRangeCreateParams const & params
)
{
	SRAT_ASSERT(params.elementCount > 0);
	VirtualRangeAllocator allocator {};
	VirtualRangeAllocatorData & self = AllocatorData(allocator);
	// initialize the allocator with the provided parameters
	self = {
		.debugName = nullptr,
		.elementCount = params.elementCount,
		.freelist = {},
		.strategy = params.strategy,
	};
	switch (params.strategy) {
		case VirtualRangeAllocationStrategy::FreeList:
			SRAT_ASSERT(params.maxBlockAllocations > 0);
			SRAT_ASSERT(params.maxBlockAllocations + 1 < skFreeListEnd);
			// note; need extra block for free block otherwise would run out of
			//       memory when user allocates the max block allocations
			self.freelist = {
				.maxBlockAllocations = params.maxBlockAllocations + 1,
				.allocatedBlocks = (
					new VirtualRangeBlockInternal[params.maxBlockAllocations + 1]
				),
				.freeListHeadIndex = 0,
			};
			self.freelist.freeListHeadIndex = 0;
			for (u32 it = 0; it < params.maxBlockAllocations + 1; ++it) {
				self.freelist.allocatedBlocks[it] = VirtualRangeBlockInternal {
					.elementOffset = 0u,
					.elementCount = 0u,
					.nextFreeIndex = skFreeListEnd,
					.generation = 0u, // invalid state
					.allocated = false,
				};
			}
			// initialize the allocated blocks to an empty state
			self.freelist.allocatedBlocks[0] = VirtualRangeBlockInternal {
				.elementOffset = 0,
				.elementCount = params.elementCount,
				.nextFreeIndex = skFreeListEnd,
				.generation = 1u, // initialize to free (odd=free)
				.allocated = false,
			};
		break;
		case VirtualRangeAllocationStrategy::Linear:
			self.linear.nextIndex = 0;
		break;
	}

#if SRAT_DEBUG()
	self.debugName = params.debugName; // TODO alloc+copy this
#endif
#if SRAT_DEBUG()
	// track the allocator for debugging purposes
	sAllocators.insert(&allocator);
#endif
	return allocator;
}

bool srat::VirtualRangeAllocator::isIndexAlive(u32 blockIndex) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	if (self.strategy == VirtualRangeAllocationStrategy::Linear) {
		return blockIndex < self.linear.nextIndex;
	}
	SRAT_ASSERT(blockIndex < self.freelist.maxBlockAllocations);
	return self.isAlive(blockIndex);
}

bool srat::VirtualRangeAllocator::isHandleAlive(u64 const blockHandle) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	if (self.strategy == VirtualRangeAllocationStrategy::Linear) {
		return blockHandle < self.linear.nextIndex;
	}
	// recreate the VirtualRangeBlock
	auto const block = VirtualRangeBlock {
		.elementCount = 0, // not needed for validity check
		.elementOffset = 0, // not needed for validity check
		.handle = blockHandle,
	};
	return block.valid(*this) && self.isAlive(handle_index(blockHandle));
}

u64 srat::VirtualRangeAllocator::elementOffset(u32 const blockIndex) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	if (self.strategy == VirtualRangeAllocationStrategy::Linear) {
		return blockIndex; // in linear strategy, the block index is the offset
	}
	SRAT_ASSERT(self.isAlive(blockIndex));
	return self.freelist.allocatedBlocks[blockIndex].elementOffset;
}

void srat::VirtualRangeAllocator::moveFrom(VirtualRangeAllocator && o)
{
	// move the internal data from the source allocator to this allocator
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	VirtualRangeAllocatorData & otherSelf = AllocatorData(o);

	// apply the move constructor to the internal debugging data
#if SRAT_DEBUG()
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
	otherSelf = VirtualRangeAllocatorData {
		.debugName = {},
		.elementCount = 0,
		.freelist = {},
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
		if (self.strategy == VirtualRangeAllocationStrategy::FreeList) {
			delete[] self.freelist.allocatedBlocks;
		}
		moveFrom(std::move(o));
	}
	return *this;
}

srat::VirtualRangeAllocator::~VirtualRangeAllocator()
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	if (self.strategy == VirtualRangeAllocationStrategy::FreeList) {
		delete[] self.freelist.allocatedBlocks;
	}
#if SRAT_DEBUG()
	auto it = sAllocators.find(this);
	if (it != sAllocators.end()) {
		sAllocators.erase(it);
	}
#endif
}

// -----------------------------------------------------------------------------
// -- virtual range allocator implementation linear
// -----------------------------------------------------------------------------

srat::VirtualRangeBlock virtual_range_linear_allocate(
	VirtualRangeAllocatorData & self,
	srat::VirtualRangeAllocateParams const & request
) {
	SRAT_ASSERT(request.elementCount < UINT32_MAX);
	SRAT_ASSERT(request.elementAlignment < UINT32_MAX);
	// bump up linear allocator by alignment requirement
	u32 const allocIndex = (
		srat::alignUp(self.linear.nextIndex, (u32)request.elementAlignment)
	);
	if (allocIndex + request.elementCount > self.elementCount) {
		printf(
			"VirtualRangeAllocator '%s' out of memory: "
			"requested %zu elements with alignment %zu, "
			"however container is currently %zu/%zu\n",
			self.debugName,
			request.elementCount,
			request.elementAlignment,
			(u64)allocIndex,
			self.elementCount
		);
		// not enough space to satisfy allocation
		return srat::VirtualRangeBlock {
			.elementCount = 0,
			.elementOffset = 0,
			.handle = 0,
		};
	}
	self.linear.nextIndex = allocIndex + (u32)request.elementCount;
	return srat::VirtualRangeBlock {
		.elementCount = request.elementCount,
		.elementOffset = allocIndex,
		.handle = 0, // linear allocator doesn't support freeing so no handle
	};
}

void virtual_range_linear_free(
	[[maybe_unused]]VirtualRangeAllocatorData & self,
	[[maybe_unused]]u64 const handle
) {
	// linear allocator doesn't support freeing, so this is a no-op
}

void virtual_range_linear_clear(
	VirtualRangeAllocatorData & self
) {
	self.linear.nextIndex = 0;
}

bool virtual_range_linear_empty(
	VirtualRangeAllocatorData const & self
) {
	return self.linear.nextIndex == 0;
}

// -----------------------------------------------------------------------------
// -- virtual range allocator implementation free-list
// -----------------------------------------------------------------------------

srat::VirtualRangeBlock virtual_range_freelist_allocate(
	VirtualRangeAllocatorData & self,
	srat::VirtualRangeAllocateParams const & request
) {
	// -- walk the free list to find a block that can satisfy the request
	u32 prevFreeIndex = skFreeListEnd;
	u32 freeIndex = self.freelist.freeListHeadIndex;
	auto & freelist = self.freelist;
	while (freeIndex != skFreeListEnd) {
		// check if the current free block can satisfy the request
		auto & freeBlock = freelist.allocatedBlocks[freeIndex];
		u64 const alignedOffset = (
			srat::alignUp(freeBlock.elementOffset, request.elementAlignment)
		);
		u64 const padding = alignedOffset - freeBlock.elementOffset;
		u64 const leadingFreeBlockOffset = freeBlock.elementOffset;
		if (freeBlock.elementCount < request.elementCount + padding) {
			// move to the next free block
			prevFreeIndex = freeIndex;
			freeIndex = freeBlock.nextFreeIndex;
			continue;
		}
		// -- found a free block that can satisfy allocation
		u64 const remainingCount = (
			freeBlock.elementCount - request.elementCount - padding
		);
		u64 const allocatedOffset = alignedOffset;
		// -- update the free block to reflect the allocated range
		freeBlock.elementOffset += request.elementCount + padding;
		freeBlock.elementCount = remainingCount;
		if (freeBlock.elementCount == 0) {
			// remove the free block from the free list if it's fully allocated
			if (prevFreeIndex == skFreeListEnd) {
				freelist.freeListHeadIndex = freeBlock.nextFreeIndex;
			} else {
				freelist.allocatedBlocks[prevFreeIndex].nextFreeIndex = (
					freeBlock.nextFreeIndex
				);
			}
			// invalidate free block handle
			if (srat::generation_alive(freeBlock.generation)) {
				SRAT_ASSERT(freeBlock.allocated == false);
				srat::generation_inc(freeBlock.generation);
				freeBlock.nextFreeIndex = skFreeListEnd;
				freeBlock.allocated = false;
				SRAT_ASSERT(self.isDead(freeIndex));
			}
			freeBlock.elementOffset = 0;
			freeBlock.elementCount = 0;
			freeBlock.allocated = false;
			freeBlock.nextFreeIndex = skFreeListEnd;
		}
		// -- create a new allocated block for the allocated range
		u32 allocatedIndex = 1;
		for (; allocatedIndex < freelist.maxBlockAllocations; ++allocatedIndex) {
			if (self.isDead(allocatedIndex)) {
				break;
			}
		}
		SRAT_ASSERT(allocatedIndex < freelist.maxBlockAllocations);
		SRAT_ASSERT(self.isDead(allocatedIndex));
		freelist.allocatedBlocks[allocatedIndex] = VirtualRangeBlockInternal {
			.elementOffset = leadingFreeBlockOffset,
			.elementCount = request.elementCount + padding,
			.nextFreeIndex = skFreeListEnd,
			.generation = freelist.allocatedBlocks[allocatedIndex].generation,
			.allocated = true,
		};
		srat::generation_inc(freelist.allocatedBlocks[allocatedIndex].generation);
		freelist.allocatedBlocks[allocatedIndex].allocated = true;
		SRAT_ASSERT(self.isAlive(allocatedIndex));

		return srat::VirtualRangeBlock {
			.elementCount = request.elementCount,
			.elementOffset = allocatedOffset,
			.handle = (
				srat::handle_make(
					allocatedIndex,
					freelist.allocatedBlocks[allocatedIndex].generation
				)
			),
		};
	}

	// no free block could satisfy the request
	return srat::VirtualRangeBlock {
		.elementCount = 0,
		.elementOffset = 0,
		.handle = 0,
	};
}

void virtual_range_freelist_free(
	srat::VirtualRangeAllocator & selfAlloc,
	VirtualRangeAllocatorData & self,
	u64 const handle
) {
	auto & freelist = self.freelist;
	auto const block = srat::VirtualRangeBlock {
		.elementCount = 0, // not needed for free
		.elementOffset = 0, // not needed for free
		.handle = handle,
	};
	if (!block.valid(selfAlloc)) { return; }
	u32 const blockIndex = srat::handle_index(block.handle);
	auto & blockInternal = freelist.allocatedBlocks[blockIndex];

	// -- increment the generation to invalidate the block's handle
	// but need to increment twice to keep in an alive state
	srat::generation_inc(blockInternal.generation);
	srat::generation_inc(blockInternal.generation);
	SRAT_ASSERT(self.isAlive(blockIndex));
	blockInternal.allocated = false;

	// -- create a new free block for the freed range
	blockInternal.nextFreeIndex = freelist.freeListHeadIndex;

	// -- insert the new free block into the free list
	freelist.freeListHeadIndex = blockIndex;

	// -- keep free list sorted by element offset
	self.sortFreeList();

	// -- coalesce adjacent free blocks in the free list
	for (u32 it = freelist.freeListHeadIndex; it != skFreeListEnd;) {
		auto & currentBlock = freelist.allocatedBlocks[it];
		u32 const nextIndex = currentBlock.nextFreeIndex;
		if (nextIndex != skFreeListEnd) {
			auto & nextBlock = freelist.allocatedBlocks[nextIndex];
			if (
				currentBlock.elementOffset + currentBlock.elementCount ==
				nextBlock.elementOffset
			) {
				// coalesce current block with next block
				currentBlock.elementCount += nextBlock.elementCount;
				currentBlock.nextFreeIndex = nextBlock.nextFreeIndex;
				// invalidate the next block's handle by incrementing its generation
				if (srat::generation_alive(nextBlock.generation)) {
					SRAT_ASSERT(nextBlock.allocated == false);
					srat::generation_inc(nextBlock.generation);
					nextBlock.nextFreeIndex = skFreeListEnd;
					nextBlock.allocated = false;
					SRAT_ASSERT(self.isDead(nextIndex));
				}
				nextBlock.elementOffset = 0;
				nextBlock.elementCount = 0;
				nextBlock.nextFreeIndex = skFreeListEnd;
				nextBlock.allocated = false;
				continue; // check for further coalescing with the new next block
			}
		}
		it = nextIndex;
	}
}

void virtual_range_freelist_clear(
	VirtualRangeAllocatorData & self
) {
	auto & freelist = self.freelist;
	for (u32 it = 0; it < freelist.maxBlockAllocations; ++it) {
		// kill alive blocks by incrementing generation
		if (self.isAlive(it)) {
			srat::generation_inc(freelist.allocatedBlocks[it].generation);
			freelist.allocatedBlocks[it].allocated = false;
			SRAT_ASSERT(self.isDead(it));
		}
		// reset block to an empty state
		u32 const gen = freelist.allocatedBlocks[it].generation;
		freelist.allocatedBlocks[it] = VirtualRangeBlockInternal {
			.elementOffset = 0,
			.elementCount = 0,
			.nextFreeIndex = skFreeListEnd,
			.generation = gen, // preserve generation to invalidate handles
			.allocated = false,
		};
	}
	// reinitialize slot 0
	if (self.isDead(0)) { // resurrect as free block
		srat::generation_inc(freelist.allocatedBlocks[0].generation);
		freelist.allocatedBlocks[0].allocated = false;
	}
	freelist.allocatedBlocks[0].elementCount = self.elementCount;
	freelist.freeListHeadIndex = 0u;
}

bool virtual_range_freelist_empty(
	VirtualRangeAllocatorData const & self
) {
	auto const & freelist = self.freelist;
	u32 const freeHead = freelist.freeListHeadIndex;
	if (freeHead == skFreeListEnd) { return false; }
	VirtualRangeBlockInternal const & firstBlock = (
		freelist.allocatedBlocks[freeHead]
	);

	return (
		   firstBlock.elementCount == self.elementCount
		&& firstBlock.elementOffset == 0
		&& (firstBlock.generation&1) == 1 // free block
		&& firstBlock.nextFreeIndex == skFreeListEnd
		&& !firstBlock.allocated
	);
}

// -----------------------------------------------------------------------------
// -- virtual range allocator implementation interface
// -----------------------------------------------------------------------------

srat::VirtualRangeBlock srat::VirtualRangeAllocator::allocate(
	VirtualRangeAllocateParams const & request
) {
	switch (AllocatorData(*this).strategy) {
		case VirtualRangeAllocationStrategy::FreeList:
			return virtual_range_freelist_allocate(AllocatorData(*this), request);
		case VirtualRangeAllocationStrategy::Linear:
			return virtual_range_linear_allocate(AllocatorData(*this), request);
	}
	exit(1); // should never reach here
}

void srat::VirtualRangeAllocator::free(u64 const handle)
{
	switch (AllocatorData(*this).strategy) {
		case VirtualRangeAllocationStrategy::FreeList:
			return virtual_range_freelist_free(*this,AllocatorData(*this), handle);
		case VirtualRangeAllocationStrategy::Linear:
			return virtual_range_linear_free(AllocatorData(*this), handle);
	}
}

void srat::VirtualRangeAllocator::clear()
{
	switch (AllocatorData(*this).strategy) {
		case VirtualRangeAllocationStrategy::FreeList:
			return virtual_range_freelist_clear(AllocatorData(*this));
		case VirtualRangeAllocationStrategy::Linear:
			return virtual_range_linear_clear(AllocatorData(*this));
	}
}

#if SRAT_DEBUG()
void srat::VirtualRangeAllocator::printAllocationStats() const
{
}

char const * srat::VirtualRangeAllocator::debugName() const
{
	return AllocatorData(*this, const).debugName;
}
#endif

bool srat::VirtualRangeAllocator::empty() const
{
	auto const & self = AllocatorData(*this, const);
	switch (self.strategy) {
		case VirtualRangeAllocationStrategy::FreeList:
			return virtual_range_freelist_empty(self);
		case VirtualRangeAllocationStrategy::Linear:
			return virtual_range_linear_empty(self);
	}
	exit(1); // should never reach here
}

#if SRAT_DEBUG()
bool srat::virtual_range_allocator_all_empty()
{
	for (auto allocator : sAllocators) {
		if (!allocator->empty()) {
			printf("allocator '%s' not empty\n", allocator->debugName());
			return false;
		}
	}
	return true;
}

void srat::virtual_range_allocator_verify_all_empty()
{
	for (auto allocator : sAllocators) {
		if (!allocator->empty()) {
			printf(
				"allocator '%s' not empty\n",
				allocator->debugName()
			);
			SRAT_ASSERT(false && "allocator not empty");
		}
	}
}
#endif
