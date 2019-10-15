#include <catch.hpp>

#include <apt/memory.h>
#include <apt/Json.h>

using namespace apt;

template <typename tType>
static void _ValueAccessTest(const char* _name, Json& _json_)
{
	_json_.setValue(tType(1), _name);
	REQUIRE(_json_.getValue<tType>(_name) == tType(1));
}
#define ValueAccessTest(t) _ValueAccessTest<t>(#t, json)

template <typename tType>
static void _ArrayAccessTest(const char* _name, Json& _json_)
{
	static const float kArrayValues[] = { 3, 5, 7, 1, 9 };
	int n = APT_ARRAY_COUNT(kArrayValues);
	
	_json_.beginArray(_name);
		for (auto v : kArrayValues) {
			_json_.pushValue(tType(v));
		}
	_json_.endArray();

	_json_.beginArray(_name);
	{
		REQUIRE(_json_.getArrayLength() == n);
		for (int i = 0; i < n; ++i) {
			auto v = _json_.getValue<tType>(i);
			REQUIRE(v == tType(kArrayValues[i]));
		}
	}
	_json_.endArray();
}
#define ArrayAccessTest(t) _ArrayAccessTest<t>(#t, json)

// instantiate _macro for all types
#define TestTypes(_macro) \
	_macro(sint8);   \
	_macro(uint8);   \
	_macro(sint16);  \
	_macro(uint16);  \
	_macro(sint32);  \
	_macro(uint32);  \
	_macro(sint64);  \
	_macro(uint64);  \
	_macro(float32); \
	_macro(float64); \
	_macro(vec2);    \
	_macro(vec3);    \
	_macro(vec4);    \
	_macro(mat2);    \
	_macro(mat3);    \
	_macro(mat4)

TEST_CASE("ValueAccess", "[Json]")
{
	Json json;
	TestTypes(ValueAccessTest);
}

TEST_CASE("ArrayAccess", "[Json]")
{
	Json json;
	TestTypes(ArrayAccessTest);
}

TEST_CASE("ArrayOfArray", "[SerializerJson]")
{
	Json json;

	{	SerializerJson js(json, SerializerJson::Mode_Write);
		uint in = 4;
		js.beginArray(in, "ArrayOfArrays");
		for (int i = 0; i < in; ++i) {
			uint jn = 3;
			js.beginArray(jn);
				for (int j = 0; j < jn; ++j) {
					int v = i + j;
					js.value(v);
				}
			js.endArray();
		}
		js.endArray();
	}
	{	SerializerJson js(json, SerializerJson::Mode_Read);
		uint in = 4;
		js.beginArray(in, "ArrayOfArrays");
		REQUIRE(in == 4);
		for (int i = 0; i < in; ++i) {
			uint jn = 3;
			js.beginArray(jn);
			REQUIRE(jn == 3);
				for (int j = 0; j < jn; ++j) {
					int v;
					js.value(v);
					REQUIRE(v == i + j);
				}
			js.endArray();
		}
		js.endArray();
	}
}

TEST_CASE("Binary", "[SerializerJson]")
{
	const char* kSrcData = 
		"Man is distinguished, not only by his reason, but by this singular passion from "
		"other animals, which is a lust of the mind, that by a perseverance of delight "
		"in the continued and indefatigable generation of knowledge, exceeds the short "
		"vehemence of any carnal pleasure."
		;
	const uint kSrcDataSize = strlen(kSrcData);

	
	Json json;
	SerializerJson js(json, SerializerJson::Mode_Write);
	
	void* data = (void*)kSrcData;
	uint dataSize = kSrcDataSize;
	js.binary(data, dataSize, "BinaryTest");

	data = nullptr;
	dataSize = 0;
	js.setMode(SerializerJson::Mode_Read);
	js.binary(data, dataSize, "BinaryTest");
	
	REQUIRE(dataSize == kSrcDataSize);
	REQUIRE(data != nullptr);
	REQUIRE(memcmp(data, kSrcData, dataSize) == 0);
}

TEST_CASE("Enum", "[SerializerJson]")
{
	enum Fruit 
	{
		Fruit_Apples,
		Fruit_Pears,

		Fruit_Count
	};
	const char* FruitStr[Fruit_Count + 1] =
	{
		"Fruit_Apples",
		"Fruit_Pears",

		"Fruit_Count"
	};
	
	
	Json json;
	SerializerJson jsWrite(json, SerializerJson::Mode_Write);

	Fruit apples = Fruit_Apples;
	Fruit pears  = Fruit_Pears;
	Fruit count  = Fruit_Count;
	((Serializer&)jsWrite).beginArray("fruits");
		SerializeEnum(jsWrite, apples, FruitStr);
		SerializeEnum(jsWrite, pears,  FruitStr);
		SerializeEnum(jsWrite, count,  FruitStr);
	jsWrite.endArray();

	SerializerJson jsRead(json, SerializerJson::Mode_Read);
	((Serializer&)jsRead).beginArray("fruits");
		SerializeEnum(jsWrite, apples, FruitStr);
		SerializeEnum(jsWrite, pears,  FruitStr);
		SerializeEnum(jsWrite, count,  FruitStr);
	jsWrite.endArray();

	REQUIRE(apples == Fruit_Apples);
	REQUIRE(pears  == Fruit_Pears);
	REQUIRE(count  == Fruit_Count);
}