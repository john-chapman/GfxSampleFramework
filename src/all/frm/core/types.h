#pragma once

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable: 4244) // possible loss of data
	#pragma warning(disable: 4723) // possible DBZ
	#pragma warning(disable: 4146) // unary minus applied to unsigned
#endif

namespace frm {

enum DataType
{
	DataType_Invalid,

 // integer types
	DataType_Sint8,
	DataType_Uint8,
	DataType_Sint16,
	DataType_Uint16,
	DataType_Sint32,
	DataType_Uint32,
	DataType_Sint64,		
	DataType_Uint64,

 // normalized integer types
	DataType_Sint8N,
	DataType_Uint8N,
	DataType_Sint16N,
	DataType_Uint16N,
	DataType_Sint32N,
	DataType_Uint32N,
	DataType_Sint64N,
	DataType_Uint64N,

 // float types
	DataType_Float16,
	DataType_Float32,
	DataType_Float64,

	DataType_Count,
	DataType_Sint  = DataType_Sint64,
	DataType_Uint  = DataType_Uint64,
	DataType_Float = DataType_Float32,
};
extern const char* kDataTypeStr[DataType_Count];

inline constexpr bool DataTypeIsInt(DataType _type)
{
	return _type >= DataType_Sint8 && _type <= DataType_Uint64N;
}
inline constexpr bool DataTypeIsFloat(DataType _type)
{
	return _type >= DataType_Float16 && _type <= DataType_Float64;
}
inline constexpr bool DataTypeIsSigned(DataType _type)
{
	return (_type % 2) != 0 || _type >= DataType_Float16;
}
inline constexpr bool DataTypeIsNormalized(DataType _type)
{
	return _type >= DataType_Sint8N && _type <= DataType_Uint64N;
}


namespace internal {

template <typename tBase, DataType kEnum>
struct DataTypeBase
{
	typedef tBase BaseType;
	static const DataType Enum = kEnum;

	DataTypeBase() = default;

	template <typename tValue>
	DataTypeBase(tValue _value): m_val(_value) {}

	operator BaseType&()                       { return m_val; }
	operator const BaseType&() const           { return m_val; }
private:
	BaseType m_val;
};

} // namespace internal


// Sized integer types.
typedef std::int8_t                                             sint8;
typedef std::uint8_t                                            uint8;
typedef std::int16_t                                            sint16;
typedef std::uint16_t                                           uint16;
typedef std::int32_t                                            sint32;
typedef std::uint32_t                                           uint32;
typedef std::int64_t                                            sint64;
typedef std::uint64_t                                           uint64;

// Sized normalized integer types (use DataTypeConvert() for conversion to/from floating point types).
typedef internal::DataTypeBase<std::int8_t,   DataType_Sint8N>  sint8N;
typedef internal::DataTypeBase<std::uint8_t,  DataType_Uint8N>  uint8N;
typedef internal::DataTypeBase<std::int16_t,  DataType_Sint16N> sint16N;
typedef internal::DataTypeBase<std::uint16_t, DataType_Uint16N> uint16N;
typedef internal::DataTypeBase<std::int32_t,  DataType_Sint32N> sint32N;
typedef internal::DataTypeBase<std::uint32_t, DataType_Uint32N> uint32N;
typedef internal::DataTypeBase<std::int64_t,  DataType_Sint64N> sint64N;
typedef internal::DataTypeBase<std::uint64_t, DataType_Uint64N> uint64N;

// Sized floating point types.
// Note that float16 is a storage type only and has no arithmetic operators.
typedef internal::DataTypeBase<std::uint16_t, DataType_Float16> float16;
typedef float                                                   float32;
typedef double                                                  float64;

typedef std::ptrdiff_t sint;
typedef std::size_t    uint;


namespace internal {

// Type traits
struct ScalarT {};
	struct FloatT: public ScalarT {};
	struct IntT:   public ScalarT {};
struct CompositeT {};
	struct MatT:   public CompositeT {};
	struct VecT:   public CompositeT {};

template <typename tType>
struct TypeTraits 
{
	typedef typename tType::BaseType  BaseType;
	typedef typename tType::Family    Family; 
	enum 
	{ 
		kCount = tType::kCount // number of elements for composite types, 1 for scalar types
	}; 
};
	template<> struct TypeTraits<sint8>      { typedef IntT   Family; typedef sint8   BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint8>      { typedef IntT   Family; typedef uint8   BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint16>     { typedef IntT   Family; typedef sint16  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint16>     { typedef IntT   Family; typedef uint16  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint32>     { typedef IntT   Family; typedef sint32  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint32>     { typedef IntT   Family; typedef uint32  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint64>     { typedef IntT   Family; typedef sint64  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint64>     { typedef IntT   Family; typedef uint64  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint8N>     { typedef IntT   Family; typedef sint8N  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint8N>     { typedef IntT   Family; typedef uint8N  BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint16N>    { typedef IntT   Family; typedef sint16N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint16N>    { typedef IntT   Family; typedef uint16N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint32N>    { typedef IntT   Family; typedef sint32N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint32N>    { typedef IntT   Family; typedef uint32N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<sint64N>    { typedef IntT   Family; typedef sint64N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<uint64N>    { typedef IntT   Family; typedef uint64N BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<float16>    { typedef IntT   Family; typedef uint16  BaseType; enum { kCount = 1 };  }; // float16 is an int type without floating point operators
	template<> struct TypeTraits<float32>    { typedef FloatT Family; typedef float32 BaseType; enum { kCount = 1 };  };
	template<> struct TypeTraits<float64>    { typedef FloatT Family; typedef float64 BaseType; enum { kCount = 1 };  };

#define FRM_TRAITS_BASE_TYPE(_type) typename frm::internal::TypeTraits<_type>::BaseType
#define FRM_TRAITS_FAMILY(_type)    typename frm::internal::TypeTraits<_type>::Family()
#define FRM_TRAITS_COUNT(_type)     frm::internal::TypeTraits<_type>::kCount

// Instantiate _macro for type/datatype pairs
#define FRM_DataType_decl(_macro) \
	_macro(frm::sint8,   frm::DataType_Sint8  ) \
	_macro(frm::uint8,   frm::DataType_Uint8  ) \
	_macro(frm::sint16,  frm::DataType_Sint16 ) \
	_macro(frm::uint16,  frm::DataType_Uint16 ) \
	_macro(frm::sint32,  frm::DataType_Sint32 ) \
	_macro(frm::uint32,  frm::DataType_Uint32 ) \
	_macro(frm::sint64,  frm::DataType_Sint64 ) \
	_macro(frm::uint64,  frm::DataType_Uint64 ) \
	_macro(frm::sint8N,  frm::DataType_Sint8N ) \
	_macro(frm::uint8N,  frm::DataType_Uint8N ) \
	_macro(frm::sint16N, frm::DataType_Sint16N) \
	_macro(frm::uint16N, frm::DataType_Uint16N) \
	_macro(frm::sint32N, frm::DataType_Sint32N) \
	_macro(frm::uint32N, frm::DataType_Uint32N) \
	_macro(frm::sint64N, frm::DataType_Sint64N) \
	_macro(frm::uint64N, frm::DataType_Uint64N) \
	_macro(frm::float16, frm::DataType_Float16) \
	_macro(frm::float32, frm::DataType_Float32) \
	_macro(frm::float64, frm::DataType_Float64)

template <DataType kEnum> struct DataType_EnumToType {};
	#define FRM_DataType_EnumToType(_type, _enum) \
		template<> struct DataType_EnumToType<_enum> { typedef _type Type; };
	FRM_DataType_decl(FRM_DataType_EnumToType)
	#undef FRM_DataType_EnumToType
	// required to compile instantiations of DataType_IntNToIntN
	template<> struct DataType_EnumToType<DataType_Invalid> { typedef frm::sint8 Type; };
	template<> struct DataType_EnumToType<DataType_Count>   { typedef frm::uint8 Type; };

template <typename tDataType> struct DataType_TypeToEnum {};
	#define FRM_DataType_TypeToEnum(_type, _enum) \
		template<> struct DataType_TypeToEnum<_type> { static const DataType Enum = _enum; };
	FRM_DataType_decl(FRM_DataType_TypeToEnum)
	#undef FRM_DataType_TypeToEnum

template <typename tDataType> struct DataType_Limits {};
	#define FRM_DataType_Limits(_type, _enum) \
		template<> struct DataType_Limits<_type> { static const _type kMin; static const _type kMax; };
	FRM_DataType_decl(FRM_DataType_Limits)
	#undef FRM_DataType_Limits

#define FRM_DATA_TYPE_TO_ENUM(_type)   frm::internal::DataType_TypeToEnum<_type>::Enum
#define FRM_DATA_TYPE_FROM_ENUM(_enum) typename frm::internal::DataType_EnumToType<_enum>::Type
#define FRM_DATA_TYPE_MIN(_type)       frm::internal::DataType_Limits<_type>::kMin
#define FRM_DATA_TYPE_MAX(_type)       frm::internal::DataType_Limits<_type>::kMax

// Conversion helpers

template <typename tDst, typename tSrc>
tDst DataType_FloatToFloat(tSrc _src);

template <typename tDst, typename tSrc>
inline tDst DataType_FloatToIntN(tSrc _src)
{
	FRM_ASSERT(DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tDst)));
	FRM_ASSERT(DataTypeIsFloat(FRM_DATA_TYPE_TO_ENUM(tSrc)));
	_src = std::max(std::min(_src, (tSrc)1), (tSrc)-1);
	return _src < 0 ? (tDst)-(_src * FRM_DATA_TYPE_MIN(tDst))
	                : (tDst) (_src * FRM_DATA_TYPE_MAX(tDst));
}
template <typename tDst, typename tSrc>
inline tDst DataType_IntNToFloat(tSrc _src)
{
	FRM_ASSERT(DataTypeIsFloat(FRM_DATA_TYPE_TO_ENUM(tDst)));
	FRM_ASSERT(DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tSrc)));
	return _src < 0 ? -((tDst)_src / FRM_DATA_TYPE_MIN(tSrc))
	                :   (tDst)_src / FRM_DATA_TYPE_MAX(tSrc);
}
template <typename tDst, typename tSrc>
inline tDst DataType_IntNPrecisionChange(tSrc _src)
{
	return _src;
	FRM_ASSERT(DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tSrc)) == DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tDst))); // perform signed -> unsigned conversion before precision change
	if (sizeof(tSrc) > sizeof(tDst)) {
		tDst mn = FRM_DATA_TYPE_MIN(tDst) == (tDst)0 ? (tDst)1 : FRM_DATA_TYPE_MIN(tDst); // prevent DBZ
		tDst mx = FRM_DATA_TYPE_MAX(tDst);
		return (tDst)(_src < 0 ? -(_src / (FRM_DATA_TYPE_MIN(tSrc) / mn))
		                       :   _src / (FRM_DATA_TYPE_MAX(tSrc) / mx));
	} else if (sizeof(tSrc) < sizeof(tDst)) {
		tSrc mn = FRM_DATA_TYPE_MIN(tSrc) == (tSrc)0 ? (tSrc)1 : FRM_DATA_TYPE_MIN(tSrc); // prevent DBZ
		tSrc mx = FRM_DATA_TYPE_MAX(tSrc);
		return (tDst)(_src < 0 ? -(_src * (FRM_DATA_TYPE_MIN(tDst) / mn))
	                           :   _src * (FRM_DATA_TYPE_MAX(tDst) / mx));
	} else {
		return _src;
	}
}
template <typename tDst, typename tSrc>
inline tDst DataType_IntNToIntN(tSrc _src)
{
	if (DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tSrc)) && !DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tDst))) { // signed -> unsigned
		typedef FRM_DATA_TYPE_FROM_ENUM((DataType)(FRM_DATA_TYPE_TO_ENUM(tSrc) + 1)) tUnsigned;
		tUnsigned tmp = (tUnsigned)(_src * 2);
		return DataType_IntNPrecisionChange<tDst, tUnsigned>(tmp);
	} else if (!DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tSrc)) && DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(tDst))) { // unsigned -> signed
		typedef FRM_DATA_TYPE_FROM_ENUM((DataType)(FRM_DATA_TYPE_TO_ENUM(tSrc) - 1)) tSigned;
		tSigned tmp = (tSigned)(_src / 2);
		return DataType_IntNPrecisionChange<tDst, tSigned>(tmp);
	} else {
		return DataType_IntNPrecisionChange<tDst, tSrc>(_src);
	}
}

} // namespace internal

// Convert from tSrc -> tDst.
template <typename tDst, typename tSrc>
inline tDst DataTypeConvert(tSrc _src)
{
	if (FRM_DATA_TYPE_TO_ENUM(tSrc) == FRM_DATA_TYPE_TO_ENUM(tDst)) {
		return _src;
	}
	if (FRM_DATA_TYPE_TO_ENUM(tSrc) == DataType_Float16) {
		//return DataTypeConvert<tDst>(internal::DataType_FloatToFloat<float32>(_src));
		FRM_ASSERT(false); return (tDst)0;
	} else if (FRM_DATA_TYPE_TO_ENUM(tDst) == DataType_Float16) {
		//return internal::DataType_FloatToFloat<float16>(DataTypeConvert<float32>(_src));
		FRM_ASSERT(false); return (tDst)0;
	} else if (DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tSrc)) && DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tDst))) {
		return internal::DataType_IntNToIntN<tDst, tSrc>(_src);
	} else if (DataTypeIsFloat(FRM_DATA_TYPE_TO_ENUM(tSrc)) && DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tDst))) {
		return internal::DataType_FloatToIntN<tDst, tSrc>(_src);
	} else if (DataTypeIsNormalized(FRM_DATA_TYPE_TO_ENUM(tSrc)) && DataTypeIsFloat(FRM_DATA_TYPE_TO_ENUM(tDst))) {
		return internal::DataType_IntNToFloat<tDst, tSrc>(_src);
	} else {
		return (tDst)_src;
	}
	FRM_ASSERT(false);
	return (tDst)0;
}

// Copy _count objects from _src to _dst, converting from _srcType to _dstType.
void DataTypeConvert(DataType _srcType, DataType _dstType, const void* _src, void* dst_, uint _count = 1);

inline uint DataTypeSizeBytes(DataType _dataType)
{
	#define FRM_DataType_case_decl(_type, _enum) \
		case _enum : return sizeof(_type);
	switch (_dataType) {
		FRM_DataType_decl(FRM_DataType_case_decl)
		default: return 0;
	};
	#undef FRM_DataType_case_decl
}

// Get _enum as a string.
const char* DataTypeString(DataType _enum);

// Create a bit mask covering _count bits.
template <typename tType>
inline tType BitfieldMask(int _count) 
{ 
	return (1 << _count) - 1; 
}

// Create a bit mask covering _count bits starting at _offset.
template <typename tType>
inline tType BitfieldMask(int _offset, int _count) 
{ 
	return ((1 << _count) - 1) << _offset; 
}

// Insert _count least significant bits from _insert into _base at _offset.
template <typename tType>
inline tType BitfieldInsert(tType _base, tType _insert, int _offset, int _count) 
{ 
	tType mask = BitfieldMask<tType>(_count);
	return (_base & ~(mask << _offset)) | ((_insert & mask) << _offset);
}

// Extract _count bits from _base starting at _offset into the _count least significant bits of the result.
template <typename tType>
inline tType BitfieldExtract(tType _base, int _offset, int _count) 
{ 
	tType mask = BitfieldMask<tType>(_count) << _offset;
	return (_base & mask) >> _offset;
}

// Reverse the sequence of _bits.
template <typename tType>
inline tType BitfieldReverse(tType _bits) 
{ 
	tType ret = _bits;
	int s = sizeof(tType) * 8 - 1;
	for (_bits >>= 1; _bits != 0; _bits >>= 1) {   
		ret <<= 1;
		ret |= _bits & 1;
		--s;
	}
	ret <<= s;
	return ret;
}

// Get/set a single bit as bool.
template <typename tType>
inline bool BitfieldGet(tType _bits, int _offset)
{
	tType mask = tType(1) << _offset;
	return (_bits & mask) != 0;
}
template <typename tType>
inline tType BitfieldSet(tType _bits, int _offset, bool _value)
{
	tType mask = tType(1) << _offset;
	return _value ? (_bits | mask) : (_bits & ~mask);
}

namespace internal {
	union iee754_f32
	{
		struct {
			uint32 m_mantissa : 23;
			uint32 m_exponent : 8;
			uint32 m_sign     : 1;
		} bits;
		uint32  u;
		float32 f;
	};
}

// Pack/unpack IEEE754 float with arbitrary precision for sign, exponent and mantissa.
uint32 PackFloat(float _value, int _signBits, int _exponentBits, int _mantissaBits);
float UnpackFloat(uint32 _value, int _signBits, int _exponentBits, int _mantissaBits);

// Pack/unpack 16-bit float via the generic functions.
inline uint16 PackFloat16(float _f32)
{
	return (uint16)PackFloat(_f32, 1, 5, 10);
}
inline float UnpackFloat16(uint16 _f16)
{
	return UnpackFloat((uint32)_f16, 1, 5, 10);
}

} // namespace frm

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
