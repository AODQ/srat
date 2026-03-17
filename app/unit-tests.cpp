#include <srat/types.hpp>

#include <srat/virtual-range-allocator.hpp>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

void unit_tests(i32 const argc, char const * const * argv)
{
	doctest::Context context;
	context.applyCommandLine(argc, argv);
	int res = context.run();
	if (context.shouldExit())
		exit(res);

	SRAT_CLEAN_EXIT();
}
