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
	u32 nextFreeIndex;
	u32 generation;
	bool allocated;
};

struct VirtualRangeAllocatorData
{
	u64 elementCount;
	u32 maxBlockAllocations;
	VirtualRangeBlockInternal * allocatedBlocks;
	u32 freeListHeadIndex { 0u };

	bool isDead(u32 index) const
	{
		return !srat::generation_alive(allocatedBlocks[index].generation);
	}

	bool isAlive(u32 index) const
	{
		u32 const gen = allocatedBlocks[index].generation;
		return gen != 0 && srat::generation_alive(gen);
	}

	void sortFreeList();
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

void VirtualRangeAllocatorData::sortFreeList()
{
	// -- sort the free list by element offset using insertion sort
	u32 sortedHead = skFreeListEnd;
	u32 it = freeListHeadIndex;
	while (it != skFreeListEnd) {
		auto & currBlock = allocatedBlocks[it];
		u32 const nextIt = currBlock.nextFreeIndex;
		// insert current block into sorted list
		if (
			   sortedHead == skFreeListEnd
			|| currBlock.elementOffset < allocatedBlocks[sortedHead].elementOffset
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
			auto & sortedBlock = allocatedBlocks[sortedIt];
			u32 const sortedNext = sortedBlock.nextFreeIndex;
			if (
					sortedNext == skFreeListEnd
				|| (
					  currBlock.elementOffset
					< allocatedBlocks[sortedNext].elementOffset
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
	freeListHeadIndex = sortedHead;
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
	SRAT_ASSERT(idx < self.maxBlockAllocations);
	auto const & block = self.allocatedBlocks[idx];
	if (!block.allocated) { return false; }
	return handle_generation(this->handle) == block.generation;
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
	SRAT_ASSERT(params.maxBlockAllocations + 1 < skFreeListEnd);
	VirtualRangeAllocator allocator {};
	VirtualRangeAllocatorData & self = AllocatorData(allocator);
	// initialize the allocator with the provided parameters
	self = {
		.elementCount = params.elementCount,
		// note; need extra block for free block otherwise would run out of
		//       memory when user allocates the max block allocations
		.maxBlockAllocations = params.maxBlockAllocations + 1,
		.allocatedBlocks = new VirtualRangeBlockInternal[
			params.maxBlockAllocations + 1
		],
		.freeListHeadIndex = 0u,
	};
	for (u32 it = 0; it < params.maxBlockAllocations + 1; ++it) {
		self.allocatedBlocks[it] = {
			.elementOffset = 0u,
			.elementCount = 0u,
			.nextFreeIndex = skFreeListEnd,
			.generation = 0u, // invalid state
			.allocated = false,
		};
	}
	// initialize the allocated blocks to an empty state
	self.allocatedBlocks[0] = VirtualRangeBlockInternal {
		.elementOffset = 0,
		.elementCount = params.elementCount,
		.nextFreeIndex = skFreeListEnd,
		.generation = 1u, // initialize to free (odd=free)
		.allocated = false,
	};
#if SRAT_DEBUG
	// track the allocator for debugging purposes
	sAllocators.insert(&allocator);
#endif
	return allocator;
}

bool srat::VirtualRangeAllocator::isIndexAlive(u32 blockIndex) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	SRAT_ASSERT(blockIndex < self.maxBlockAllocations);
	return self.isAlive(blockIndex);
}

bool srat::VirtualRangeAllocator::isHandleAlive(u64 const blockHandle) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	// recreate the VirtualRangeBlock
	auto const block = VirtualRangeBlock {
		.elementCount = 0, // not needed for validity check
		.elementOffset = 0, // not needed for validity check
		.handle = blockHandle,
	};
	return block.valid(*this) && self.isAlive(handle_index(blockHandle));
}

u64 srat::VirtualRangeAllocator::elementOffset(u32 blockIndex) const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	SRAT_ASSERT(self.isAlive(blockIndex));
	return self.allocatedBlocks[blockIndex].elementOffset;
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
	otherSelf = VirtualRangeAllocatorData {
		.elementCount = 0,
		.maxBlockAllocations = 0,
		.allocatedBlocks = nullptr,
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
		moveFrom(std::move(o));
	}
	return *this;
}

srat::VirtualRangeAllocator::~VirtualRangeAllocator()
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	delete[] self.allocatedBlocks;
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
) {
	VirtualRangeAllocatorData & self = AllocatorData(*this);

	// -- walk the free list to find a block that can satisfy the request
	u32 prevFreeIndex = skFreeListEnd;
	u32 freeIndex = self.freeListHeadIndex;
	while (freeIndex != skFreeListEnd) {
		// check if the current free block can satisfy the request
		auto & freeBlock = self.allocatedBlocks[freeIndex];
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
				self.freeListHeadIndex = freeBlock.nextFreeIndex;
			} else {
				self.allocatedBlocks[prevFreeIndex].nextFreeIndex = (
					freeBlock.nextFreeIndex
				);
			}
			// invalidate free block handle
			if (srat::generation_alive(freeBlock.generation)) {
				SRAT_ASSERT(freeBlock.allocated == false);
				srat::generation_inc(freeBlock.generation);
				SRAT_ASSERT(self.isDead(freeIndex));
			}
			freeBlock.elementOffset = 0;
			freeBlock.elementCount = 0;
			freeBlock.allocated = false;
			freeBlock.nextFreeIndex = skFreeListEnd;
		}
		// -- create a new allocated block for the allocated range
		u32 allocatedIndex = 1;
		for (; allocatedIndex < self.maxBlockAllocations; ++allocatedIndex) {
			if (self.isDead(allocatedIndex)) {
				break;
			}
		}
		SRAT_ASSERT(allocatedIndex < self.maxBlockAllocations);
		SRAT_ASSERT(self.isDead(allocatedIndex));
		self.allocatedBlocks[allocatedIndex] = VirtualRangeBlockInternal {
			.elementOffset = leadingFreeBlockOffset,
			.elementCount = request.elementCount + padding,
			.nextFreeIndex = skFreeListEnd,
			.generation = self.allocatedBlocks[allocatedIndex].generation,
			.allocated = true,
		};
		generation_inc(self.allocatedBlocks[allocatedIndex].generation);
		SRAT_ASSERT(self.isAlive(allocatedIndex));

		return VirtualRangeBlock {
			.elementCount = request.elementCount,
			.elementOffset = allocatedOffset,
			.handle = (
				handle_make(
					allocatedIndex,
					self.allocatedBlocks[allocatedIndex].generation
				)
			),
		};
	}

	// no free block could satisfy the request
	return VirtualRangeBlock {
		.elementCount = 0,
		.elementOffset = 0,
		.handle = 0,
	};
}

void srat::VirtualRangeAllocator::free(u64 const handle)
{
	auto const block = VirtualRangeBlock {
		.elementCount = 0, // not needed for free
		.elementOffset = 0, // not needed for free
		.handle = handle,
	};
	if (!block.valid(*this)) { return; }
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	u32 const blockIndex = handle_index(block.handle);
	auto & blockInternal = self.allocatedBlocks[blockIndex];

	// -- increment the generation to invalidate the block's handle
	// but need to increment twice to keep in an alive state
	generation_inc(blockInternal.generation);
	generation_inc(blockInternal.generation);
	SRAT_ASSERT(self.isAlive(blockIndex));
	blockInternal.allocated = false;

	// -- create a new free block for the freed range
	blockInternal.nextFreeIndex = self.freeListHeadIndex;

	// -- insert the new free block into the free list
	self.freeListHeadIndex = blockIndex;

	// -- keep free list sorted by element offset
	self.sortFreeList();

	// -- coalesce adjacent free blocks in the free list
	for (u32 it = self.freeListHeadIndex; it != skFreeListEnd;) {
		auto & currentBlock = self.allocatedBlocks[it];
		u32 const nextIndex = currentBlock.nextFreeIndex;
		if (nextIndex != skFreeListEnd) {
			auto & nextBlock = self.allocatedBlocks[nextIndex];
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

void srat::VirtualRangeAllocator::clear()
{
	VirtualRangeAllocatorData & self = AllocatorData(*this);
	for (u32 it = 0; it < self.maxBlockAllocations; ++it) {
		// kill alive blocks by incrementing generation
		if (self.isAlive(it)) {
			generation_inc(self.allocatedBlocks[it].generation);
			SRAT_ASSERT(self.isDead(it));
		}
		// reset block to an empty state
		u32 const gen = self.allocatedBlocks[it].generation;
		self.allocatedBlocks[it] = VirtualRangeBlockInternal {
			.elementOffset = 0,
			.elementCount = 0,
			.nextFreeIndex = skFreeListEnd,
			.generation = gen, // preserve generation to invalidate handles
			.allocated = false,
		};
	}
	// reinitialize slot 0
	if (self.isDead(0)) { // resurrect as free block
		generation_inc(self.allocatedBlocks[0].generation);
	}
	self.allocatedBlocks[0].elementCount = self.elementCount;
	self.freeListHeadIndex = 0u;
}

void srat::VirtualRangeAllocator::printAllocationStats() const
{
}

bool srat::VirtualRangeAllocator::empty() const
{
	VirtualRangeAllocatorData const & self = AllocatorData(*this, const);
	u32 const freeHead = self.freeListHeadIndex;
	if (freeHead == skFreeListEnd) { return false; }
	VirtualRangeBlockInternal const & firstBlock = (
		self.allocatedBlocks[freeHead]
	);

	return (
		   firstBlock.elementCount == self.elementCount
		&& firstBlock.elementOffset == 0
		&& (firstBlock.generation&1) == 1 // free block
		&& firstBlock.nextFreeIndex == skFreeListEnd
		&& !firstBlock.allocated
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
