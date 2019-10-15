#include <catch.hpp>

#include <apt/Filesystem.h>

using namespace apt;

TEST_CASE("Matches", "[FileSystem]")
{
 // no wildcards
	REQUIRE(FileSystem::Matches("abcdefg", "abcdefg")  == true);
	REQUIRE(FileSystem::Matches("abcd",    "abcdefg")  == false);
	REQUIRE(FileSystem::Matches("abcdefg", "abcd")     == false);

 // ?
	REQUIRE(FileSystem::Matches("?at",     "Cat")      == true);
	REQUIRE(FileSystem::Matches("?at",     "cat")      == true);
	REQUIRE(FileSystem::Matches("?at",     "at")       == false);

 // *
	REQUIRE(FileSystem::Matches("Law*",    "Law")      == true);
	REQUIRE(FileSystem::Matches("Law*",    "Laws")     == true);
	REQUIRE(FileSystem::Matches("Law*",    "Lawyer")   == true);
	REQUIRE(FileSystem::Matches("Law*",    "GrokLaw")  == false);
	REQUIRE(FileSystem::Matches("Law*",    "La")       == false);
	REQUIRE(FileSystem::Matches("Law*",    "aw")       == false);
	REQUIRE(FileSystem::Matches("*Law*",   "Law")      == true);
	REQUIRE(FileSystem::Matches("*Law*",   "GrokLaw")  == true);
	REQUIRE(FileSystem::Matches("*Law*",   "Lawyer")   == true);
	REQUIRE(FileSystem::Matches("*Law*",   "La")       == false);
	REQUIRE(FileSystem::Matches("*Law*",   "aw")       == false);
}
