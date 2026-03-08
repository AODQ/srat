#pragma once
#include "handle.hpp"

#include <new> // for std::align_val_t

template <typename Handle, typename InternalResource>
srat::HandlePool<Handle, InternalResource>
srat::HandlePool<Handle, InternalResource>::create(u64 const maxHandles)
{
	u32 * const generations = new u32[maxHandles];
	for (u64 i = 0; i < maxHandles; ++i) {
		generations[i] = 1u;
	}
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
		generations,
		maxHandles
	);
}

// template <typename Handle, typename InternalResource>
// void srat::HandlePool<Handle, InternalResource>::deleteInternal() {
// 	for (u64 i = 0; i < maxHandles; ++i) {
// 		if (generations[i] % 2 == 0) { // only destroy alive resources
// 			resourceAllocations[i].~InternalResource();
// 		}
// 	}
// 	operator delete[](
// 		resourceAllocations,
// 		std::align_val_t{alignof(InternalResource)}
// 	);
// 	delete[] generations;
// }

// template <typename Handle, typename InternalResource>
// srat::HandlePool<Handle, InternalResource>::~HandlePool() {
// 	deleteInternal();
// }

// template <typename Handle, typename InternalResource>
// srat::HandlePool<Handle, InternalResource>::HandlePool(HandlePool && o)
// 	: allocator(std::move(o.allocator))
// 	, maxHandles(o.maxHandles)
// 	, resourceAllocations(o.resourceAllocations)
// 	, generations(o.generations)
// {
// 	o.resourceAllocations = nullptr;
// 	o.generations = nullptr;
// }

// template <typename Handle, typename InternalResource>
// srat::HandlePool<Handle, InternalResource> &
// srat::HandlePool<Handle, InternalResource>::operator=(HandlePool && o)
// {
// 	if (this != &o) {
// 		allocator = std::move(o.allocator);
// 		deleteInternal();
// 		resourceAllocations = o.resourceAllocations;
// 		generations = o.generations;
// 		maxHandles = o.maxHandles;
// 		o.resourceAllocations = nullptr;
// 		o.generations = nullptr;
// 	}
// 	return *this;
// }

// template <typename Handle, typename InternalResource>
// Handle srat::HandlePool<Handle, InternalResource>::allocate(
// 	InternalResource const & resource
// ) {
// 	srat::VirtualRangeBlock const handleId = (
// 		allocator.allocate({.elementCount = 1})
// 	);
// 	SRAT_ASSERT(handleId.elementOffset < UINT32_MAX);
// 	if (!handleId.valid(allocator)) {
// 		return Handle{0}; // invalid handle
		
// 	}
// 	new (&resourceAllocations[handleId.elementOffset])InternalResource(resource);
// 	u32 const gen = generation_inc(generations[handleId.elementOffset]);
// 	SRAT_ASSERT(generations[handleId.elementOffset] % 2 == 0); // even=alive
// 	return { srat::handle_make(handleId.elementOffset, gen) };
// }

// template <typename Handle, typename InternalResource>
// Handle srat::HandlePool<Handle, InternalResource>::allocate(
// 	InternalResource && resource
// ) {
// 	srat::VirtualRangeBlock const handleId = (
// 		allocator.allocate({.elementCount = 1})
// 	);
// 	SRAT_ASSERT(handleId.elementOffset < UINT32_MAX);
// 	if (!handleId.valid(allocator)) {
// 		return Handle{0}; // invalid handle
// 	}
// 	new (&resourceAllocations[handleId.elementOffset])
// 		InternalResource(std::move(resource));
// 	// set to alive
// 	;
// 	u32 const gen = generation_inc(generations[handleId.elementOffset]);
// 	SRAT_ASSERT(generations[handleId.elementOffset] % 2 == 0); // even=alive
// 	return {srat::handle_make(handleId.elementOffset, gen)};
// }

// template <typename Handle, typename InternalResource>
// void srat::HandlePool<Handle, InternalResource>::free(Handle const & handle)
// {
// 	if (!valid(handle)) { return; }
// 	u32 const index = srat::handle_index(handle.id);
// 	allocator.free(VirtualRangeBlock {
// 		.elementCount = 1,
// 		.elementOffset = index,
// 		.generation = allocator.generation(), // this seems really sus
// 	});
// 	// free the resource
// 	resourceAllocations[index].~InternalResource();
// 	generation_inc(generations[index]); // invalidate allocations
// 	SRAT_ASSERT(generations[index] % 2 == 1); // odd=dead
// }

// template <typename Handle, typename InternalResource>
// bool srat::HandlePool<Handle, InternalResource>::valid(
// 	Handle const & handle
// ) const {
// 	u32 const gen = srat::handle_generation(handle.id);
// 	return gen == generations[srat::handle_index(handle.id)];
// }

// template <typename Handle, typename InternalResource>
// InternalResource * srat::HandlePool<Handle, InternalResource>::get(
// 	Handle const & handle
// ) {
// 	if (!valid(handle)) { return nullptr; }
// 	u32 const index = srat::handle_index(handle.id);
// 	return &resourceAllocations[index];
// }
