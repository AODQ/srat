#pragma once

#include "types.hpp"
#include "virtual-range-allocator.hpp"

#include <tuple>

// arena allocator for temporary memory allocations during rasterization

// -----------------------------------------------------------------------------
// -- arena allocator
// -----------------------------------------------------------------------------

namespace srat {

template <typename T>
struct ArenaAllocator {
	static ArenaAllocator<T> create(
		u32 const capacity,
		char const * const debugName = "ArenaAllocator"
	);

	// allocate N contiguous elements
	T * allocate(u32 const elementCount, u32 alignment = alignof(T));

	inline T * data_ptr() { return data; }

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
// -- struct of array arena allocator
// -----------------------------------------------------------------------------

namespace srat {

template <typename ... Ts>
struct SoAArenaAllocator {
	// create an arena allocator for each type, with given capacity
	static SoAArenaAllocator<Ts ...> create(
		u32 const capacity,
		char const * const debugName = "SoAArenaAllocator"
	) {
		return SoAArenaAllocator<Ts...>(
			std::make_tuple(ArenaAllocator<Ts>::create(capacity, debugName) ...)
		);
	}
	// allocate N contiguous elements for each type, returns tuple of pointers
	std::tuple<Ts * ...> allocate(u32 const elementCount) {
		return allocate_impl(elementCount, std::index_sequence_for<Ts ...>{});
	}

	std::tuple<Ts * ...> data_ptr() {
		return data_ptr_impl(std::index_sequence_for<Ts ...>{});
	}

	// clear all allocators
	void clear() {
		clear_impl(std::index_sequence_for<Ts ...>{});
	}

private:

	SoAArenaAllocator(std::tuple<ArenaAllocator<Ts> ...> && allocators) :
		allocators(std::move(allocators))
	{}

	template <usize ... Is>
	std::tuple<Ts * ...> allocate_impl(
		u32 const elementCount,
		std::index_sequence<Is ...>
	) {
		return std::make_tuple(
			std::get<Is>(allocators).allocate(elementCount) ...
		);
	}

	template <usize ... Is>
	std::tuple<Ts * ...> data_ptr_impl(std::index_sequence<Is ...>) {
		return std::make_tuple(
			std::get<Is>(allocators).data_ptr() ...
		);
	}

	template <usize ... Is>
	void clear_impl(
		std::index_sequence<Is ...>
	) {
		(std::get<Is>(allocators).clear(), ...);
	}


	std::tuple<ArenaAllocator<Ts> ...> allocators;
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
