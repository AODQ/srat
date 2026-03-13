#pragma once

#include "types.hpp"

#include "virtual-range-allocator.hpp"

// arena allocator for temporary memory allocations during rasterization

namespace srat {

template <typename T>
struct ArenaAllocator {
	static ArenaAllocator<T> create(
		u32 const capacity,
		char const * const debugName = "ArenaAllocator"
	);

	// allocate N contiguous elements
	T * allocate(u32 const elementCount, u32 alignment = alignof(T));

	void clear();
	bool empty() const;

	~ArenaAllocator();

	ArenaAllocator(ArenaAllocator const &) = delete;
	ArenaAllocator & operator=(ArenaAllocator const &) = delete;
	ArenaAllocator(ArenaAllocator &&);
	ArenaAllocator & operator=(ArenaAllocator &&);
private:
	ArenaAllocator(
		VirtualRangeAllocator && allocator, T * const data, u32 const capacity
	);

	VirtualRangeAllocator allocator;
	T * data;
	u32 capacity;
};

}

// -----------------------------------------------------------------------------
// -- arena allocator implementation
// -----------------------------------------------------------------------------

#include <new> // sadly, for std::align_val_t

template <typename T>
srat::ArenaAllocator<T>::ArenaAllocator(
	VirtualRangeAllocator && allocator, T * const data, u32 const capacity
)
	: allocator(std::move(allocator))
	, data(data)
	, capacity(capacity)
{}

template <typename T>
srat::ArenaAllocator<T> srat::ArenaAllocator<T>::create(
	u32 const capacity,
	char const * const debugName
) {
	return ArenaAllocator<T>(
		VirtualRangeAllocator::create(VirtualRangeCreateParams {
			.debugName = debugName,
			.elementCount = capacity,
			.maxBlockAllocations = 0, // not used for linear strategy
			.strategy = VirtualRangeAllocationStrategy::Linear,
		}),
		static_cast<T *>(
			operator new[](sizeof(T) * capacity, std::align_val_t{alignof(T)})
		),
		capacity
	);
}

template <typename T>
srat::ArenaAllocator<T>::ArenaAllocator(ArenaAllocator && o)
	: allocator(std::move(o.allocator))
	, data(o.data)
	, capacity(o.capacity)
{
	o.data = nullptr;
	o.capacity = 0;
}

template <typename T>
srat::ArenaAllocator<T> & srat::ArenaAllocator<T>::operator=(
	ArenaAllocator && o
) {
	if (this != &o) {
		allocator = std::move(o.allocator);
		data = o.data;
		capacity = o.capacity;
		o.data = nullptr;
		o.capacity = 0;
	}
	return *this;
}

template <typename T>
srat::ArenaAllocator<T>::~ArenaAllocator() {
	if (data != nullptr) {
		operator delete[](data, std::align_val_t{alignof(T)});
	}
}

template <typename T>
T * srat::ArenaAllocator<T>::allocate(u32 const elementCount, u32 alignment) {
	auto block = (
		allocator.allocate({
			.elementCount = elementCount,
			.elementAlignment = alignment,
		})
	);
	if (block.elementCount == 0) {
		return nullptr; // allocation failed
	}
	return data + block.elementOffset;
}

template <typename T>
void srat::ArenaAllocator<T>::clear() {
	allocator.clear();
}

template <typename T>
bool srat::ArenaAllocator<T>::empty() const {
	return allocator.empty();
}
