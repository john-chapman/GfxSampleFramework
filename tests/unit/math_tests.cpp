#include <catch.hpp>

#include <apt/log.h>
#include <apt/math.h>
#include <apt/Time.h>

#include <EASTL/vector.h>

using namespace apt;

// \todo probably move this to types_tests

namespace {

template <typename tType>
struct AlignCheck: public tType
{
	AlignCheck()
		: tType()
	{
		REQUIRE((std::ptrdiff_t)this % alignof(tType) == 0);
	}

	static void Test()
	{
		const int kArraySize = 16;

	 // stack alignment
		AlignCheck c;

	 // heap alignment
		AlignCheck* p = new AlignCheck[kArraySize];
		delete p;

	 // container alignment
		eastl::vector<AlignCheck> vec;
		for (int i = 0; i < kArraySize; ++i) {
			vec.push_back(AlignCheck());
		}
	}
};

} // namespace

TEST_CASE("Validate type alignment", "[math]")
{
	AlignCheck<vec2>::Test();
	AlignCheck<vec3>::Test();
	AlignCheck<vec4>::Test();
	AlignCheck<quat>::Test();
}
