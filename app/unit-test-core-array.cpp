#include <srat/core-array.hpp>
#if 0

#include <doctest/doctest.h>

TEST_SUITE("array") {

TEST_CASE("array sanity") {
	srat::array<i32, 4> a {};
	CHECK(a.size() == 4);
	CHECK_FALSE(a.empty());
	for (size_t i = 0; i < a.size(); ++i) {
		a[i] = (i32)i;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		CHECK(a[i] == (i32)i);
	}
}

TEST_CASE("array splat") {
	srat::array<i32, 8> a {};
	a.splat(42);
	for (size_t i = 0; i < a.size(); ++i) {
		CHECK(a[i] == 42);
	}
}

TEST_CASE("array iterators") {
	srat::array<i32, 4> a {};
	for (size_t i = 0; i < a.size(); ++i) {
		a[i] = (i32)i;
	}
	size_t idx = 0;
	for (auto & value : a) {
		CHECK(value == (i32)idx);
		++idx;
	}
}

TEST_CASE("slice") {
	srat::array<i8, 8> a {};
	for (size_t i = 0; i < a.size(); ++i) {
		a[i] = (i8)i;
	}
	auto s = a.subslice(2, 4);
	CHECK(s.size() == 4);
	for (size_t i = 0; i < s.size(); ++i) {
		CHECK(s[i] == (i8)(i + 2));
	}
}

}
#endif
