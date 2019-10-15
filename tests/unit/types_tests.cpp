#include <catch.hpp>

#include <apt/apt.h>
#include <apt/types.h>
#include <apt/math.h>

#include <apt/hash.h>

using namespace apt;

template <typename tType>
static void FloatToIntN()
{
	REQUIRE(DataTypeConvert<tType>((float32)0.0f)  == (tType)0);
	REQUIRE(DataTypeConvert<tType>((float32)1.0f)  == APT_DATA_TYPE_MAX(tType));
	REQUIRE(DataTypeConvert<tType>((float32)-1.0f) == APT_DATA_TYPE_MIN(tType));
	REQUIRE(DataTypeConvert<tType>((float32)0.5f)  == APT_DATA_TYPE_MAX(tType) / 2);
	REQUIRE(DataTypeConvert<tType>((float32)-0.5f) == APT_DATA_TYPE_MIN(tType) / 2);
}
template <typename tType>
static void IntNToFloat()
{
	float err = 2.0f / powf(2.0f, sizeof(tType) * 8); // (kMax - kMin) fails for 32/64 bit types?
	float mn = DataTypeIsSigned(tType::Enum) ? -1.0f : 0.0f;
	REQUIRE(fabs(DataTypeConvert<float32>(APT_DATA_TYPE_MAX(tType)) - 1.0f) < err);
	REQUIRE(fabs(DataTypeConvert<float32>(APT_DATA_TYPE_MIN(tType)) - mn) < err);
	REQUIRE(fabs(DataTypeConvert<float32>((tType)(APT_DATA_TYPE_MAX(tType) / 2)) - 0.5f) < err);
}
template <typename tType>
static void IntNToIntN()
{
	const DataType enm = APT_DATA_TYPE_TO_ENUM(tType);

 // check signed/unsigned conversions
	if (DataTypeIsSigned(enm)) {
	 // allow an error of 1 in this case; signed -> unsigned clips at zero, so signed types have half the precision of unsigned types in this case
		typedef APT_DATA_TYPE_FROM_ENUM((DataType)(enm + 1)) tUnsigned;
		REQUIRE(DataTypeConvert<tUnsigned>(APT_DATA_TYPE_MAX(tType)) >= (APT_DATA_TYPE_MAX(tUnsigned) - 1));
		REQUIRE(DataTypeConvert<tUnsigned>(APT_DATA_TYPE_MIN(tType)) == 0);
	} else {
		typedef APT_DATA_TYPE_FROM_ENUM((DataType)(enm - 1)) tSigned;
		REQUIRE(DataTypeConvert<tSigned>(APT_DATA_TYPE_MAX(tType)) == APT_DATA_TYPE_MAX(tSigned));
		REQUIRE(DataTypeConvert<tSigned>(APT_DATA_TYPE_MIN(tType)) == 0);
	}

 // check precision conversions
	//if constexpr (enm < DataType_Sint64N) {
	//	typedef APT_DATA_TYPE_FROM_ENUM((DataType)(enm + 2)) tHigher;
	//	REQUIRE(DataTypeConvert<tHigher>(APT_DATA_TYPE_MAX(tType) == APT_DATA_TYPE_MAX(tHigher)));
	//	REQUIRE(DataTypeConvert<tHigher>(APT_DATA_TYPE_MIN(tType) == APT_DATA_TYPE_MIN(tHigher)));
	//}
	//if constexpr (enm > DataType_Uint8N) {
	//	typedef APT_DATA_TYPE_FROM_ENUM((DataType)(enm - 2)) tLower;
	//	REQUIRE(DataTypeConvert<tHigher>(APT_DATA_TYPE_MAX(tType) == APT_DATA_TYPE_MAX(tLower)));
	//	REQUIRE(DataTypeConvert<tHigher>(APT_DATA_TYPE_MIN(tType) == APT_DATA_TYPE_MIN(tLower)));
	//}
}

TEST_CASE("Validate type sizes", "[types]")
{
	REQUIRE(sizeof(sint8)    == 1);
	REQUIRE(sizeof(uint8)    == 1);
	REQUIRE(sizeof(sint8N)   == 1);
	REQUIRE(sizeof(uint8N)   == 1);
	REQUIRE(sizeof(sint16)   == 2);
	REQUIRE(sizeof(uint16)   == 2);
	REQUIRE(sizeof(sint16N)  == 2);
	REQUIRE(sizeof(uint16N)  == 2);
	REQUIRE(sizeof(sint32)   == 4);
	REQUIRE(sizeof(uint32)   == 4);
	REQUIRE(sizeof(sint32N)  == 4);
	REQUIRE(sizeof(uint32N)  == 4);
	REQUIRE(sizeof(sint64)   == 8);
	REQUIRE(sizeof(uint64)   == 8);
	REQUIRE(sizeof(sint64N)  == 8);
	REQUIRE(sizeof(uint64N)  == 8);
	REQUIRE(sizeof(float16)  == 2);
	REQUIRE(sizeof(float32)  == 4);
	REQUIRE(sizeof(float64)  == 8);

	REQUIRE(sizeof(vec2) == sizeof(float32) * 2);
	REQUIRE(sizeof(vec3) == sizeof(float32) * 3);
	REQUIRE(sizeof(vec4) == sizeof(float32) * 4);
	REQUIRE(sizeof(quat) == sizeof(float32) * 4);
	REQUIRE(sizeof(mat2) == sizeof(float32) * 4);
	REQUIRE(sizeof(mat3) == sizeof(float32) * 9);
	REQUIRE(sizeof(mat4) == sizeof(float32) * 16);
}

TEST_CASE("Validate conversion functions", "[types]")
{
	FloatToIntN<sint8N>();
	FloatToIntN<uint8N>();
	FloatToIntN<sint16N>();
	FloatToIntN<uint16N>();
	// The following all fail due to floating point precision
	//FloatToIntN<sint32N>();
	//FloatToIntN<uint32N>();
	//FloatToIntN<sint64N>();
	//FloatToIntN<uint64N>();
	
	IntNToFloat<sint8N>();
	IntNToFloat<uint8N>();
	IntNToFloat<sint16N>();
	IntNToFloat<uint16N>();
	IntNToFloat<sint32N>();
	IntNToFloat<uint32N>();
	IntNToFloat<sint64N>();
	IntNToFloat<uint64N>();

	IntNToIntN<uint8N>();
	IntNToIntN<sint8N>();
	IntNToIntN<uint16N>();
	IntNToIntN<sint16N>();
	IntNToIntN<uint32N>();
	IntNToIntN<sint32N>();
	IntNToIntN<uint64N>();
	IntNToIntN<sint64N>();
}

TEST_CASE("Validate metadata functions", "[types]")
{
	REQUIRE(DataTypeIsNormalized(DataType_Sint8)   == false);
	REQUIRE(DataTypeIsNormalized(DataType_Uint8)   == false);
	REQUIRE(DataTypeIsNormalized(DataType_Sint8N)  == true);
	REQUIRE(DataTypeIsNormalized(DataType_Uint8N)  == true);
	REQUIRE(DataTypeIsNormalized(DataType_Sint16)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Uint16)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Sint16N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Uint16N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Sint32)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Uint32)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Sint32N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Uint32N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Sint64)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Uint64)  == false);
	REQUIRE(DataTypeIsNormalized(DataType_Sint64N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Uint64N) == true);
	REQUIRE(DataTypeIsNormalized(DataType_Float16) == false);
	REQUIRE(DataTypeIsNormalized(DataType_Float32) == false);
	REQUIRE(DataTypeIsNormalized(DataType_Float64) == false);

	REQUIRE(DataTypeIsFloat(DataType_Sint8)   == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint8)   == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint8N)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint8N)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint16)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint16)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint16N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint16N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint32)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint32)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint32N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint32N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint64)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint64)  == false);
	REQUIRE(DataTypeIsFloat(DataType_Sint64N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Uint64N) == false);
	REQUIRE(DataTypeIsFloat(DataType_Float16) == true);
	REQUIRE(DataTypeIsFloat(DataType_Float32) == true);
	REQUIRE(DataTypeIsFloat(DataType_Float64) == true);

	REQUIRE(DataTypeIsSigned(DataType_Sint8)   == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint8)   == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint8N)  == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint8N)  == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint16)  == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint16)  == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint16N) == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint16N) == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint32)  == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint32)  == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint32N) == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint32N) == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint64)  == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint64)  == false);
	REQUIRE(DataTypeIsSigned(DataType_Sint64N) == true);
	REQUIRE(DataTypeIsSigned(DataType_Uint64N) == false);
	REQUIRE(DataTypeIsSigned(DataType_Float16) == true);
	REQUIRE(DataTypeIsSigned(DataType_Float32) == true);
	REQUIRE(DataTypeIsSigned(DataType_Float64) == true);
}
