#pragma once

#include "types.hpp"

// virtual range allocator
// allocate virtual memory ranges, user has to provide actual backing memory
// the 'element' can be bytes, pages, or any other unit of memory.

// TODO implement 'linear' allocation strategy

namespace srat {

struct VirtualRangeAllocator;

enum struct VirtualRangeAllocationStrategy : u64 {
	FreeList, // default strategy
	Linear, // simple liaenr allocator for fast clear
};

struct VirtualRangeBlock
{
	u64 elementCount;
	u64 elementOffset;
	u64 handle; // opaque handle to identify the allocated block

	// this will be invalid if the block was not allocated or has been freed
	bool valid(VirtualRangeAllocator const & allocator) const;
};

struct VirtualRangeAllocateParams
{
	u64 elementCount;
	u64 elementAlignment = 1u;
};

struct VirtualRangeCreateParams
{
	char const * debugName;
	u64 elementCount;
	// only used for free-list strategy
	u32 maxBlockAllocations;
	VirtualRangeAllocationStrategy strategy;
};

struct VirtualRangeAllocator
{
	// allocate a range of elements
	VirtualRangeBlock allocate(VirtualRangeAllocateParams const & request);

	// free a previously allocated range of elements
	void free(u64 const handle);

	u64 handleCount() const;
	u64 handleCapacity() const;

	// reset allocator to initial data, freeing all virtual memory ranges
	void clear();

	// create a new virtual range allocator
	static VirtualRangeAllocator create(VirtualRangeCreateParams const & params);

	// this returns if the index has data in its slot
	bool isIndexAlive(u32 blockIndex) const;
	// this returns if the handle is alive
	bool isHandleAlive(u64 const handle) const;
	u64 elementOffset(u32 blockIndex) const;

	// check if the allocator has no free blocks available
	bool empty() const;

#if SRAT_DEBUG
	void printAllocationStats() const;
	char const * debugName() const;
#endif

	// virtual range allocators can't be copied
	~VirtualRangeAllocator();
	VirtualRangeAllocator(VirtualRangeAllocator const &) = delete;
	VirtualRangeAllocator & operator=(VirtualRangeAllocator const &) = delete;
	VirtualRangeAllocator(VirtualRangeAllocator &&);
	VirtualRangeAllocator & operator=(VirtualRangeAllocator &&);

private:
	// private constructor to enforce creation through the static create method
	VirtualRangeAllocator() = default;

	void moveFrom(VirtualRangeAllocator && o);

	friend struct VirtualRangeBlock;

	// internal data for the allocator
	u64 _internalData[7];
};

// this verifies all allocators are empty, used at program exit for leaks
#if SRAT_DEBUG
bool virtual_range_allocator_all_empty();
void virtual_range_allocator_verify_all_empty();
#endif

} // namespace srat
