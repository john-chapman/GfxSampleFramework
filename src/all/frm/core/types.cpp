#include "frm.h"

#include <cstring>
#include <limits>

namespace frm { namespace internal {

const sint8   DataType_Limits<sint8  >::kMin = std::numeric_limits<sint8  >::min();
const sint8   DataType_Limits<sint8  >::kMax = std::numeric_limits<sint8  >::max();
const uint8   DataType_Limits<uint8  >::kMin = std::numeric_limits<uint8  >::min();
const uint8   DataType_Limits<uint8  >::kMax = std::numeric_limits<uint8  >::max();
const sint16  DataType_Limits<sint16 >::kMin = std::numeric_limits<sint16 >::min();
const sint16  DataType_Limits<sint16 >::kMax = std::numeric_limits<sint16 >::max();
const uint16  DataType_Limits<uint16 >::kMin = std::numeric_limits<uint16 >::min();
const uint16  DataType_Limits<uint16 >::kMax = std::numeric_limits<uint16 >::max();
const sint32  DataType_Limits<sint32 >::kMin = std::numeric_limits<sint32 >::min();
const sint32  DataType_Limits<sint32 >::kMax = std::numeric_limits<sint32 >::max();
const uint32  DataType_Limits<uint32 >::kMin = std::numeric_limits<uint32 >::min();
const uint32  DataType_Limits<uint32 >::kMax = std::numeric_limits<uint32 >::max();
const sint64  DataType_Limits<sint64 >::kMin = std::numeric_limits<sint64 >::min();
const sint64  DataType_Limits<sint64 >::kMax = std::numeric_limits<sint64 >::max();
const uint64  DataType_Limits<uint64 >::kMin = std::numeric_limits<uint64 >::min();
const uint64  DataType_Limits<uint64 >::kMax = std::numeric_limits<uint64 >::max();
const sint8N  DataType_Limits<sint8N >::kMin = std::numeric_limits<sint8  >::min();
const sint8N  DataType_Limits<sint8N >::kMax = std::numeric_limits<sint8  >::max();
const uint8N  DataType_Limits<uint8N >::kMin = std::numeric_limits<uint8  >::min();
const uint8N  DataType_Limits<uint8N >::kMax = std::numeric_limits<uint8  >::max();
const sint16N DataType_Limits<sint16N>::kMin = std::numeric_limits<sint16 >::min();
const sint16N DataType_Limits<sint16N>::kMax = std::numeric_limits<sint16 >::max();
const uint16N DataType_Limits<uint16N>::kMin = std::numeric_limits<uint16 >::min();
const uint16N DataType_Limits<uint16N>::kMax = std::numeric_limits<uint16 >::max();
const sint32N DataType_Limits<sint32N>::kMin = std::numeric_limits<sint32 >::min();
const sint32N DataType_Limits<sint32N>::kMax = std::numeric_limits<sint32 >::max();
const uint32N DataType_Limits<uint32N>::kMin = std::numeric_limits<uint32 >::min();
const uint32N DataType_Limits<uint32N>::kMax = std::numeric_limits<uint32 >::max();
const sint64N DataType_Limits<sint64N>::kMin = std::numeric_limits<sint64 >::min();
const sint64N DataType_Limits<sint64N>::kMax = std::numeric_limits<sint64 >::max();
const uint64N DataType_Limits<uint64N>::kMin = std::numeric_limits<uint64 >::min();
const uint64N DataType_Limits<uint64N>::kMax = std::numeric_limits<uint64 >::max();
const float16 DataType_Limits<float16>::kMin = 0x400;  // IEEE 754
const float16 DataType_Limits<float16>::kMax = 0x7BFF; // IEEE 754
const float32 DataType_Limits<float32>::kMin = std::numeric_limits<float32>::min();
const float32 DataType_Limits<float32>::kMax = std::numeric_limits<float32>::max();
const float64 DataType_Limits<float64>::kMin = std::numeric_limits<float64>::min();
const float64 DataType_Limits<float64>::kMax = std::numeric_limits<float64>::max();

template <> float16 DataType_FloatToFloat(float16 _src)
{
	return _src;
}
template <> float16 DataType_FloatToFloat(float32 _src)
{
	return float16(PackFloat16(_src));
}
template <> float16 DataType_FloatToFloat(float64 _src)
{
	return float16(PackFloat16((float32)_src));
}
template <> float32 DataType_FloatToFloat(float16 _src)
{
	return UnpackFloat16(_src);
}
template <> float64 DataType_FloatToFloat(float16 _src)
{
	return (float64)UnpackFloat16(_src);
}

} } // namespace frm::internal


namespace frm {

void DataTypeConvert(DataType _srcType, DataType _dstType, const void* _src, void* dst_, uint _count)
{
	if (_srcType == _dstType) {
		memcpy(dst_, _src, DataTypeSizeBytes(_srcType) * _count);

	} else {
	 // \hack need special cases for the Float32 <-> Float16 conversions as DataTypeConvert cannot handle them
		#define DataType_case_decl(_srcType, _srcEnum) \
			case _srcEnum: \
				switch (_dstType) { \
					case DataType_Sint8:   *((sint8*  )dst_) = DataTypeConvert<sint8,   _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint8:   *((uint8*  )dst_) = DataTypeConvert<uint8,   _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint16:  *((sint16* )dst_) = DataTypeConvert<sint16,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint16:  *((uint16* )dst_) = DataTypeConvert<uint16,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint32:  *((sint32* )dst_) = DataTypeConvert<sint32,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint32:  *((uint32* )dst_) = DataTypeConvert<uint32,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint64:  *((sint64* )dst_) = DataTypeConvert<sint64,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint64:  *((uint64* )dst_) = DataTypeConvert<uint64,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint8N:  *((sint8N* )dst_) = DataTypeConvert<sint8N,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint8N:  *((uint8N* )dst_) = DataTypeConvert<uint8N,  _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint16N: *((sint16N*)dst_) = DataTypeConvert<sint16N, _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint16N: *((uint16N*)dst_) = DataTypeConvert<uint16N, _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint32N: *((sint32N*)dst_) = DataTypeConvert<sint32N, _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint32N: *((uint32N*)dst_) = DataTypeConvert<uint32N, _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Sint64N: *((sint64N*)dst_) = DataTypeConvert<sint64N, _srcType>(*((const _srcType*)_src)); break; \
					case DataType_Uint64N: *((uint64N*)dst_) = DataTypeConvert<uint64N, _srcType>(*((const _srcType*)_src)); break; \
				case DataType_Float16: \
					*((float16*)dst_) = internal::DataType_FloatToFloat<float16, float32>(DataTypeConvert<float32, _srcType>(*((const _srcType*)_src))); break; \
				case DataType_Float32: \
					if (_srcEnum == DataType_Float16) { \
						*((float32*)dst_) = internal::DataType_FloatToFloat<float32, float16>(*((const _srcType*)_src)); break; \
					} else { \
						*((float32*)dst_) = DataTypeConvert<float32, _srcType>(*((const _srcType*)_src)); \
					} break; \
					case DataType_Float64: *((float64*)dst_) = DataTypeConvert<float64, _srcType>(*((const _srcType*)_src)); break; \
				}; \
				break;			

		for (uint i = 0; i < _count; ++i) {
			switch (_srcType) {
				FRM_DataType_decl(DataType_case_decl)
				default: FRM_ASSERT(false); break;
			};
			_src = (const char*)_src + DataTypeSizeBytes(_srcType);
			dst_ = (char*)dst_ + DataTypeSizeBytes(_dstType);
		}
	}
}

const char* DataTypeString(DataType _type)
{
	#define CASE_ENUM(_enum) \
		case _enum: return #_enum
	switch (_type) {
		CASE_ENUM(DataType_Invalid);
		CASE_ENUM(DataType_Sint8);
		CASE_ENUM(DataType_Uint8);
		CASE_ENUM(DataType_Sint16);
		CASE_ENUM(DataType_Uint16);
		CASE_ENUM(DataType_Sint32);
		CASE_ENUM(DataType_Uint32);
		CASE_ENUM(DataType_Sint64);		
		CASE_ENUM(DataType_Uint64);
		CASE_ENUM(DataType_Sint8N);
		CASE_ENUM(DataType_Uint8N);
		CASE_ENUM(DataType_Sint16N);
		CASE_ENUM(DataType_Uint16N);
		CASE_ENUM(DataType_Sint32N);
		CASE_ENUM(DataType_Uint32N);
		CASE_ENUM(DataType_Sint64N);
		CASE_ENUM(DataType_Uint64N);
		CASE_ENUM(DataType_Float16);
		CASE_ENUM(DataType_Float32);
		CASE_ENUM(DataType_Float64);	
		CASE_ENUM(DataType_Count);
		default: return "Unkown DataType enum";
	}
}

uint32 PackFloat(float _value, int _signBits, int _exponentBits, int _mantissaBits)
{
	FRM_ASSERT(_signBits + _exponentBits + _mantissaBits <= 32);
	if (_signBits == 0) {
		_value = _value < 0.0f ? 0.0f : _value;
	}
	internal::iee754_f32 in;
	in.f = _value;
	const sint32 maxExponent = (1 << _exponentBits) - 1;
	const uint32 bias = maxExponent >> 1;
	const uint32 sign = in.bits.m_sign;
	uint32 mantissa = in.bits.m_mantissa >> (23 - _mantissaBits);
	sint32 exponent;
	switch (in.bits.m_exponent) {
		case 0x00:
			exponent = 0;
			break;
		case 0xff:
			exponent = maxExponent;
			break;
		default:
			exponent = in.bits.m_exponent - 127 + bias;
			if (exponent < 1) {
				exponent = 1;
				mantissa = 0;
			}
			if (exponent > maxExponent - 1) {
				exponent = maxExponent - 1;
				mantissa = (1 << 23) - 1;
			}
			break;
	}
	uint32 ret = 0;
	ret = BitfieldInsert(ret, mantissa, 0, _mantissaBits);
	ret = BitfieldInsert(ret, (uint32)exponent, _mantissaBits, _exponentBits);
	ret = BitfieldInsert(ret, sign, _mantissaBits + _exponentBits, _signBits);
	return ret;
}

float UnpackFloat(uint32 _value, int _signBits, int _exponentBits, int _mantissaBits)
{
	internal::iee754_f32 ret;
	const uint32 maxExponent = (1 << _exponentBits) - 1;
	const uint32 bias        = maxExponent >> 1;
	const uint32 mantissa    = BitfieldExtract(_value, 0, _mantissaBits);
	const uint32 exponent    = BitfieldExtract(_value, _mantissaBits, _exponentBits);
	const uint32 sign        = BitfieldExtract(_value, _mantissaBits + _exponentBits, _signBits);
	ret.bits.m_mantissa      = mantissa << (23 - _mantissaBits);
	ret.bits.m_exponent      = (exponent == 0) ? 0 : (exponent == maxExponent) ? 0xff : exponent - bias + 127;
	ret.bits.m_sign          = sign;
	return ret.f;
}

} // namespace frm
