#include <catch.hpp>

#include <apt/log.h>
#include <apt/math.h>
#include <apt/rand.h>
#include <apt/String.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

using namespace apt;

template <uint kCapacity>
static void VectorTest()
{
	eastl::vector<String<kCapacity>> strv;
	Rand<> rnd;
	for (int i = 0; i < 10000; ++i) {
		int strl = rnd.get<int>(1, APT_MAX((int)kCapacity * 2, 2));
		String<kCapacity> str;
		for (int j = 0; j < strl; ++j) {
			char c = (char)rnd.get<int>((int)'A', (int)'Z' + 1);
			str.append(&c, 1);
		}
		strv.push_back(str);
	}
	while (!strv.empty()) {
		strv.pop_back();
	}

	strv.clear();
	for (int i = 0; i < 100; ++i) {
		strv.push_back(String<kCapacity>("HELLO"));
		REQUIRE((strv.back() == "HELLO") == true);
	}
}


TEST_CASE("vector", "[String]")
{
	VectorTest<0>();
	VectorTest<16>();
	VectorTest<64>();
}

TEST_CASE("vector_map", "[String]")
{
 // test use of String in a dictionary
	typedef String<16> Str;
	typedef eastl::vector_map<Str, Str> StrMap;

	StrMap map;

	map.insert(eastl::make_pair(Str("Key A"), Str("Value A")));
	REQUIRE(map.find("Key A") != map.end());
	REQUIRE(map.find("Key A")->second == "Value A");
	REQUIRE(map.find("abc") == map.end());

	map.insert(eastl::make_pair(Str("Key B"), Str("Value B")));
	REQUIRE(map.find("Key B") != map.end());
	REQUIRE(map.find("Key B")->second == "Value B");
	REQUIRE(map.find("abc") == map.end());
}

TEST_CASE("toUpperCase, toLowerCase", "[String]")
{
	static const char* lower = "abcdefghijklmnopqrstuvwxyz1234567890";
	static const char* upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	String<0> str(lower);
	str.toUpperCase();
	REQUIRE(str == upper);
	str.toLowerCase();
	REQUIRE(str == lower);
}

TEST_CASE("relational operators", "[String]")
{
	typedef String<16> Str;
	REQUIRE(Str("abc") < Str("xyz"));
	REQUIRE(Str("xyz") > Str("abc"));
	REQUIRE(Str("abc") == Str("abc"));
}

TEST_CASE("findFirst, findLast", "[String]")
{
	typedef String<16> Str;
	Str str("abcdef");

	REQUIRE(*str.findFirst("a") == 'a');
	REQUIRE(*str.findFirst("d") == 'd');
	REQUIRE(*str.findFirst("f") == 'f');
	REQUIRE(str.findFirst("x")  == nullptr);

	REQUIRE(*str.findLast("a") == 'a');
	REQUIRE(*str.findLast("d") == 'd');
	REQUIRE(*str.findLast("f") == 'f');
	REQUIRE(str.findLast("x") == nullptr);
}

TEST_CASE("Move_ctor", "[String]")
{
	static apt::String<64> const local = "/dsgfkldfsgkdfjs/sdfkjhsdf";

	static apt::String<64> const dyn = "/dfsdfdfg/ghty/u/efxdcvngfj/iyuitrer/dfdfvbcnezrt/rytruyjhnbv/vhgjfhgf/dhffdjkgdhsfs";

	SECTION("Create from local")
	{
		apt::String<64> localCpy(local);
		apt::String<64> create_from_local(std::move(localCpy));
		REQUIRE(create_from_local == local);
	}

	SECTION("Create from dyn")
	{
		apt::String<64> dynCpy(dyn);
		apt::String<64> movector(std::move(dynCpy));
		REQUIRE(movector == dyn);
	}
}

TEST_CASE("Move_copy", "[String]")
{
	static apt::String<64> const local = "/dsgfkldfsgkdfjs/sdfkjhsdf";

	static apt::String<64> const dyn = "/dfsdfdfg/ghty/u/efxdcvngfj/iyuitrer/dfdfvbcnezrt/rytruyjhnbv/vhgjfhgf/dhffdjkgdhsfs";

	SECTION("copy local to local")
	{
		apt::String<64> localCpy(local);
		apt::String<64> move_loc_to_loc = "ghfdfkglhndsgf";
		move_loc_to_loc = std::move(localCpy);
		REQUIRE(move_loc_to_loc == local);
	}

	SECTION("copy dyn to local")
	{
		apt::String<64> dynCpy(dyn);
		apt::String<64> move_dyn_to_loc = "pidfnjsdfbzer";
		move_dyn_to_loc = std::move(dynCpy);
		REQUIRE(move_dyn_to_loc == dyn);
	}

	SECTION("copy local to dyn")
	{
		apt::String<64> localCpy(local);
		apt::String<64> move_loc_to_dyn = "ghfdfkglhndsgf/dfhftgjfgj/dfsgdrfghdtyds/bvcbhcvndfdfyg/fdhtyredsgdhffgj/DFGTFYTSgfdgdfh/";
		move_loc_to_dyn = std::move(localCpy);
		REQUIRE(move_loc_to_dyn == local);
	}

	SECTION("copy dyn to dyn")
	{
		apt::String<64> dynCpy(dyn);
		apt::String<64> move_dyn_to_dyn = "ghfdfkglhndsgf/dfhftgjfgj/dfsgdrfghdtyds/bvcbhcvndfdfyg/fdhtyredsgdhffgj/DFGTFYTSgfdgdfh/";
		move_dyn_to_dyn = std::move(dynCpy);
		REQUIRE(move_dyn_to_dyn == dyn);
	}
}
