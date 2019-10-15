#include "Serializer.h"

#include <frm/core/log.h>

#include <cstdarg>

using namespace frm;

// PUBLIC

void Serializer::setError(const char* _msg, ...)
{
	if (_msg) {
		va_list args;
		va_start(args, _msg);
		m_errStr.setfv(_msg, args);
		va_end(args);
	} else {
		m_errStr.clear();
	}
}

template <typename tType, uint kLen>
static bool ValueVecMatImpl(Serializer& _serializer_, tType& _value_, const char* _name)
{
	uint len = kLen; \
	if (_serializer_.beginArray(len, _name)) { \
		bool ret = true;
		if (len != kLen) {
			_serializer_.setError("Error serializing %s '%s': array length was %d, expected %d", Serializer::ValueTypeToStr<tType>(), _name ? _name : "", (int)len, (int)kLen);
			ret = false;
		} else {
			float* v = (float*)&_value_; // access vec/mat as a flat array
			for (int i = 0; i < (int)kLen; ++i) { 
				ret &= _serializer_.value(v[i]);
			}
		}
		_serializer_.endArray();
		return ret;
	}
	return false;
}
bool Serializer::value(vec2& _value_, const char* _name) { return ValueVecMatImpl<vec2,   2>(*this, _value_, _name); }
bool Serializer::value(vec3& _value_, const char* _name) { return ValueVecMatImpl<vec3,   3>(*this, _value_, _name); }
bool Serializer::value(vec4& _value_, const char* _name) { return ValueVecMatImpl<vec4,   4>(*this, _value_, _name); }
bool Serializer::value(mat2& _value_, const char* _name) { return ValueVecMatImpl<mat2, 2*2>(*this, _value_, _name); }
bool Serializer::value(mat3& _value_, const char* _name) { return ValueVecMatImpl<mat3, 3*3>(*this, _value_, _name); }
bool Serializer::value(mat4& _value_, const char* _name) { return ValueVecMatImpl<mat4, 4*4>(*this, _value_, _name); }

// PROTECTED

template <typename tType>
const char* Serializer::ValueTypeToStr()
{
	return "Unknown value type";
}
	template <> const char* Serializer::ValueTypeToStr<bool>()        { return "bool";       }
	template <> const char* Serializer::ValueTypeToStr<sint8>()       { return "sint8";      }
	template <> const char* Serializer::ValueTypeToStr<uint8>()       { return "uint8";      }
	template <> const char* Serializer::ValueTypeToStr<sint16>()      { return "sint16";     }
	template <> const char* Serializer::ValueTypeToStr<uint16>()      { return "uint16";     }
	template <> const char* Serializer::ValueTypeToStr<sint32>()      { return "sint32";     }
	template <> const char* Serializer::ValueTypeToStr<uint32>()      { return "uint32";     }
	template <> const char* Serializer::ValueTypeToStr<sint64>()      { return "sint64";     }
	template <> const char* Serializer::ValueTypeToStr<uint64>()      { return "uint64";     }
	template <> const char* Serializer::ValueTypeToStr<float32>()     { return "float32";    }
	template <> const char* Serializer::ValueTypeToStr<float64>()     { return "float64";    }
	template <> const char* Serializer::ValueTypeToStr<StringBase>()  { return "StringBase"; }
	template <> const char* Serializer::ValueTypeToStr<vec2>()        { return "vec2";       }
	template <> const char* Serializer::ValueTypeToStr<vec3>()        { return "vec3";       }
	template <> const char* Serializer::ValueTypeToStr<vec4>()        { return "vec4";       }
	template <> const char* Serializer::ValueTypeToStr<mat2>()        { return "mat2";       }
	template <> const char* Serializer::ValueTypeToStr<mat3>()        { return "mat3";       }
	template <> const char* Serializer::ValueTypeToStr<mat4>()        { return "mat4";       }


//---

template <typename T>
static bool SerializeImpl(Serializer& _serializer_, T& _value_, const char* _name)
{
	if (!_serializer_.value(_value_, _name)) {
		if (_serializer_.getError()) {
			FRM_LOG_ERR(_serializer_.getError());
		}
		return false;
	}
	return true;
}
bool frm::Serialize(Serializer& _serializer_, bool&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, sint8&      _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, uint8&      _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, sint16&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, uint16&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, sint32&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, uint32&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, sint64&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, uint64&     _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, float32&    _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, float64&    _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, StringBase& _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, vec2&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, vec3&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, vec4&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, mat2&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, mat3&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }
bool frm::Serialize(Serializer& _serializer_, mat4&       _value_, const char* _name) { return SerializeImpl(_serializer_, _value_, _name); }