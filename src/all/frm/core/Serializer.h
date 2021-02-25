#pragma once

#include <frm/core/frm.h>
#include <frm/core/compress.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/BitFlags.h>
#include <frm/core/String.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Serializer
// Base class for serialization handlers. A set of Serialize() free functions
// are also provided which implicitly log an error if the serialization fails.
////////////////////////////////////////////////////////////////////////////////
class Serializer
{
public:
	enum Mode
	{
		Mode_Read,
		Mode_Write
	};

	Mode                getMode() const                              { return m_mode;  }
	void                setMode(Mode _mode)                          { onModeChange(_mode); m_mode = _mode; }
	const char*         getError() const                             { return m_errStr.isEmpty() ? nullptr : (const char*)m_errStr; }
	void                setError(const char* _msg, ...);

	// Return false if _name is not found, or if the end of the current object is reached. If in an object/array and _name is not specified,
	// advance to the next element.
	virtual bool        beginObject(const char* _name = nullptr) = 0;
	virtual void        endObject() = 0;

	// Return false if _name is not found, or if the end of the current array is reached. If in an object/array and _name is not specified,
	// advance to the next element.
	virtual bool        beginArray(uint& _length_, const char* _name = nullptr) = 0;
	virtual void        endArray() = 0;

	// Variant for fixed-sized arrays or cases where _length_ isn't required.
	bool                beginArray(const char* _name = nullptr)      { uint n = 0; return beginArray(n, _name); }

	// Get the name of the current value. Return "" if the current value is an array member.
	virtual const char* getName() const = 0;

	// Get the index of the current value.
	virtual uint32      getIndex() const = 0;

	// Serialize a value. If in an object/array and _name is not specified, advance to the next element.
	virtual bool        value(bool&       _value_, const char* _name = nullptr) = 0;
	virtual bool        value(sint8&      _value_, const char* _name = nullptr) = 0;
	virtual bool        value(uint8&      _value_, const char* _name = nullptr) = 0;
	virtual bool        value(sint16&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(uint16&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(sint32&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(uint32&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(sint64&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(uint64&     _value_, const char* _name = nullptr) = 0;
	virtual bool        value(float32&    _value_, const char* _name = nullptr) = 0;
	virtual bool        value(float64&    _value_, const char* _name = nullptr) = 0;
	virtual bool        value(StringBase& _value_, const char* _name = nullptr) = 0;
	
	// vec* and mat* variants are implemented in terms of beginArray/endArray and value(float&).
	bool                value(vec2& _value_, const char* _name = nullptr);
	bool                value(vec3& _value_, const char* _name = nullptr);
	bool                value(vec4& _value_, const char* _name = nullptr);
	bool                value(mat2& _value_, const char* _name = nullptr);
	bool                value(mat3& _value_, const char* _name = nullptr);
	bool                value(mat4& _value_, const char* _name = nullptr);

	// Helper to avoid explicit cast to StringBase&.
	template <uint kCapacity>
	bool                value(String<kCapacity>& _value_, const char* _name = nullptr)
	{
		 return value((StringBase&)_value_, _name); 
	}

	// Directly serialize bytes of binary data with optional compression. When reading, _data_ will be allocated by the function if null, and 
	// should subsequently be released via FRM_FREE().
	virtual bool        binary(
		void*&           _data_, 
		uint&            _sizeBytes_, 
		const char*      _name = nullptr, 
		CompressionFlags _compressionFlags = CompressionFlags_None
		) = 0;

	// Return tType as a string.
	template <typename tType>
	static const char* ValueTypeToStr();

protected:
	Mode m_mode;

	Serializer(Mode _mode)
		: m_mode(_mode)
	{
	}
	virtual ~Serializer()
	{
	}

	virtual void onModeChange(Mode _mode)
	{
	}

private:
	String<64> m_errStr;

}; // class Serializer

// Serialize* variants implicitly log an error if _serializer_.value() returns false.
bool Serialize(Serializer& _serializer_, bool&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, sint8&       _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, uint8&       _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, sint16&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, uint16&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, sint32&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, uint32&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, sint64&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, uint64&      _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, float32&     _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, float64&     _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, StringBase&  _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, vec2&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, vec3&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, vec4&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, mat2&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, mat3&        _value_, const char* _name = nullptr);
bool Serialize(Serializer& _serializer_, mat4&        _value_, const char* _name = nullptr);
template <uint kCapacity>
bool Serialize(Serializer& _serializer_, String<kCapacity>& _value_, const char* _name = nullptr)
{
	return Serialize(_serializer_, (StringBase&)_value_, _name); 
}

// Helper for the common case of serializing an enum to/from a const char*.
template <typename tType, int kCount>
bool SerializeEnum(Serializer& _serializer_, tType& _value_, const char* (&_strList)[kCount], const char* _name = nullptr)
{
	String<32> tmp;
	if (_serializer_.getMode() == Serializer::Mode_Write) {
		FRM_ASSERT((int)_value_ < kCount);
		tmp = _strList[(int)_value_];
	}
	bool ret = Serialize(_serializer_, (StringBase&)tmp, _name);
	if (ret && _serializer_.getMode() == Serializer::Mode_Read) {
		int i = 0;
		for (; i < kCount; ++i) {
			if (tmp == _strList[i]) {
				_value_ = (tType)i;
				break;
			}
		}
		if (i == kCount) {
			_serializer_.setError("Error serializing enum; '%s' not valid", (const char*)tmp);
			ret = false;
		}
	}
	return ret;
}

// Helper for serializing bitflags to individual bools.
template <typename tEnumType>
bool Serialize(Serializer& _serializer_, BitFlags<tEnumType>& _bitFlags_, const char* (&_strList)[BitFlags<tEnumType>::kCount], const char* _name = nullptr)
{
	bool ret = true;

	if (_serializer_.beginObject(_name))
	{
		ret = true;

		for (int i = 0; i < BitFlags<tEnumType>::kCount; ++i)
		{
			bool flag = _bitFlags_.get((tEnumType)i);
			Serialize(_serializer_, flag, _strList[i]); // Allow flags to be optional (don't modify ret).
			_bitFlags_.set((tEnumType)i, flag);
		}

		_serializer_.endObject();
	}
	else
	{
		ret = false;
	}

	return ret;
}

} // namespace frm
