#pragma once
#include "handle.hpp"

#include <new> // for std::align_val_t

template <typename Handle, typename InternalResource>
srat::HandlePool<Handle, InternalResource>
srat::HandlePool<Handle, InternalResource>::create(u32 const maxHandles)
{
	InternalResource * const resourceAllocations = (
		static_cast<InternalResource *>(
			operator new[](
				sizeof(InternalResource) * maxHandles,
				std::align_val_t{alignof(InternalResource)}
			)
		)
	);
	return srat::HandlePool<Handle, InternalResource>(
		VirtualRangeAllocator::create(
			VirtualRangeCreateParams {
				.elementCount = maxHandles,
				.maxBlockAllocations = maxHandles,
			}
		),
		resourceAllocations,
		maxHandles
	);
}

template <typename Handle, typename InternalResource>
void srat::HandlePool<Handle, InternalResource>::deleteInternal() {
	for (u32 i = 0; i < maxHandles; ++i) {
		if (allocator.isIndexAlive(i)) {
			u64 const elementOffset = allocator.elementOffset(i);
			SRAT_ASSERT(elementOffset < UINT32_MAX);
			resourceAllocations[elementOffset].~InternalResource();
		}
	}
	operator delete[](
		resourceAllocations,
		std::align_val_t{alignof(InternalResource)}
	);
}

template <typename Handle, typename InternalResource>
srat::HandlePool<Handle, InternalResource>::HandlePool::~HandlePool() {
	deleteInternal();
}

template <typename Handle, typename InternalResource>
srat::HandlePool<Handle, InternalResource>::HandlePool(HandlePool && o)
	: allocator(std::move(o.allocator))
	, maxHandles(o.maxHandles)
	, resourceAllocations(o.resourceAllocations)
{
	o.resourceAllocations = nullptr;
}

template <typename Handle, typename InternalResource>
srat::HandlePool<Handle, InternalResource> &
srat::HandlePool<Handle, InternalResource>::operator=(HandlePool && o)
{
	if (this != &o) {
		allocator = std::move(o.allocator);
		deleteInternal();
		resourceAllocations = o.resourceAllocations;
		maxHandles = o.maxHandles;
		o.resourceAllocations = nullptr;
	}
	return *this;
}

template <typename Handle, typename InternalResource>
Handle srat::HandlePool<Handle, InternalResource>::allocate(
	InternalResource const & resource
) {
	srat::VirtualRangeBlock const handleId = (
		allocator.allocate({.elementCount = 1})
	);
	auto const elementOffset = handleId.elementOffset;
	SRAT_ASSERT(elementOffset < UINT32_MAX);
	if (!handleId.valid(allocator)) {
		return Handle{0}; // invalid handle
		
	}
	new (&resourceAllocations[elementOffset])InternalResource(resource);
	return { handleId.handle };
}

template <typename Handle, typename InternalResource>
Handle srat::HandlePool<Handle, InternalResource>::allocate(
	InternalResource && resource
) {
	srat::VirtualRangeBlock const handleId = (
		allocator.allocate({.elementCount = 1})
	);
	auto const elementOffset = handleId.elementOffset;
	printf("element offset: %u\n", (u32)elementOffset);
	SRAT_ASSERT(elementOffset < UINT32_MAX);
	if (!handleId.valid(allocator)) {
		return Handle{0}; // invalid handle
	}
	new (&resourceAllocations[elementOffset])
		InternalResource(std::move(resource));
	return Handle { handleId.handle };
}

template <typename Handle, typename InternalResource>
void srat::HandlePool<Handle, InternalResource>::free(Handle const & handle)
{
	if (!valid(handle)) { return; }
	u64 const index = allocator.elementOffset(srat::handle_index(handle.id));
	// free the handle from virtual range allocator
	allocator.free(handle.id);
	// free the resource
	resourceAllocations[index].~InternalResource();
}

template <typename Handle, typename InternalResource>
bool srat::HandlePool<Handle, InternalResource>::valid(
	Handle const & handle
) const {
	if (handle.id == 0) { return false; }
	u32 const index = srat::handle_index(handle.id);
	if (index >= maxHandles) { return false; }
	return allocator.isHandleAlive(handle.id);
}

template <typename Handle, typename InternalResource>
InternalResource * srat::HandlePool<Handle, InternalResource>::get(
	Handle const & handle
) {
	if (!valid(handle)) { return nullptr; }
	u32 const index = allocator.elementOffset(srat::handle_index(handle.id));
	return &resourceAllocations[index];
}
