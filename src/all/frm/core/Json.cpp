#include "Json.h"

#include <frm/core/log.h>
#include <frm/core/math.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/String.h>
#include <frm/core/Time.h>

#include <EASTL/vector.h>

#define RAPIDJSON_ASSERT(x) FRM_ASSERT(x)
#define RAPIDJSON_PARSE_DEFAULT_FLAGS (rapidjson::kParseFullPrecisionFlag | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag)
#include <rapidjson/error/en.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#define JSON_ERR_TYPE(_call, _name, _type, _expected, _onFail) \
	if (_type != _expected) { \
		FRM_LOG_ERR("Json: (%s) %s has type %s, expected %s", _call, _name, GetValueTypeString(_type), GetValueTypeString(_expected)); \
		_onFail; \
	}
#define JSON_ERR_SIZE(_call, _name, _size, _expected, _onFail) \
	if (_size != _expected) { \
		FRM_LOG_ERR("Json: (%s) %s has size %d, expected %d", _call, _name, _size, _expected); \
		_onFail; \
	}
#define JSON_ERR_ARRAY_SIZE(_call, _name, _index, _arraySize, _onFail) \
	if (_index >= _arraySize) { \
		FRM_LOG_ERR("Json: (%s) %s index out of bounds, %d/%d", _call, _name, _index, _arraySize - 1); \
		_onFail; \
	}

using namespace frm;

static Json::ValueType GetValueType(rapidjson::Type _type)
{
	switch (_type)
	{
		case rapidjson::kNullType:   return Json::ValueType_Null;
		case rapidjson::kObjectType: return Json::ValueType_Object;
		case rapidjson::kArrayType:  return Json::ValueType_Array;
		case rapidjson::kFalseType:
		case rapidjson::kTrueType:   return Json::ValueType_Bool;
		case rapidjson::kNumberType: return Json::ValueType_Number;
		case rapidjson::kStringType: return Json::ValueType_String;
		default: FRM_ASSERT(false); break;
	};

	return Json::ValueType_Count;
}

static const char* GetValueTypeString(Json::ValueType _type)
{
	switch (_type)
	{
		case Json::ValueType_Null:    return "Null";
		case Json::ValueType_Object:  return "Object";
		case Json::ValueType_Array:   return "Array";
		case Json::ValueType_Bool:    return "Bool";
		case Json::ValueType_Number:  return "Number";
		case Json::ValueType_String:  return "String";
		default:                      return "Unknown";
	};
}

/*******************************************************************************

                                   Json

*******************************************************************************/

/*	Traversal of the dom:
		- Stack is for container objects only, maintain a separate Value instance for the
		  position *within* the current container. Need to support json whose root isn't an
		  object?
		- find() moves within the current container - takes either a name or an index. ONLY
		  after calling find() can you call enter/leave, get/set etc.

	\todo		
	- "call stack" for errors
	- Matrices should be row major
*/
struct Json::Impl
{
	rapidjson::Document m_dom;

	struct Value
	{
		rapidjson::Value* m_value  = nullptr;
		const char*       m_name   = "";       // value name
		int               m_index  = -1;       // value index in the parent container

		ValueType getType()
		{
			return GetValueType(m_value->GetType()); 
		}

		int getSize() 
		{ 
			auto type = getType();
			if (type == ValueType_Array) {
				return (int)m_value->Size();
			} else if (type == ValueType_Object) {
				return (int)m_value->MemberCount();
			}
			return -1;
		}
	};
	eastl::vector<Value> m_containerStack; // for traversal of containers (arrays, objects)
	Value                m_currentValue;   // current element, index may be -1 if the previous operation was enter() or begin()

	void reset()
	{
		m_containerStack.clear();
		//m_containerStack.push_back({ &m_dom, "", -1 }); // doesn't compile VS2015
		m_containerStack.push_back();
			m_containerStack.back().m_value = &m_dom;
			m_containerStack.back().m_name = "";
			m_containerStack.back().m_index = -1;
		m_currentValue = Value();
	}

	bool find(int _i)
	{
		auto& container = m_containerStack.back();
		JSON_ERR_ARRAY_SIZE("find()", container.m_name, _i, container.getSize(), return false);
		auto containerType = container.getType();
		if (containerType == ValueType_Array) {
			m_currentValue.m_value = container.m_value->Begin() + _i;
			m_currentValue.m_name  = "";
			m_currentValue.m_index = _i;
			return true;

		} else if (containerType == ValueType_Object) {
			auto it = container.m_value->MemberBegin() + _i;
			m_currentValue.m_value = &it->value;
			m_currentValue.m_name  = it->name.GetString();
			m_currentValue.m_index = _i;
			return true;

		}
		return false;
	}

	bool find(const char* _name)
	{
		auto& container = m_containerStack.back();
		//JSON_ERR_TYPE("find()", container.m_name, ValueType_Object, container.getType(), return false);
		auto it = container.m_value->FindMember(_name);
		if (it == container.m_value->MemberEnd()) {
			return false;
		}
		m_currentValue.m_value = &it->value;
		m_currentValue.m_name  = it->name.GetString();
		m_currentValue.m_index = (int)(it - container.m_value->MemberBegin());
		return true;
	}

	void enter()
	{
		auto& container = m_containerStack.back();
		auto containerType = container.getType();
		FRM_ASSERT(containerType == ValueType_Object || containerType == ValueType_Array);
		m_containerStack.push_back(m_currentValue);
		m_currentValue = Value();

	 // In some cases we enter an object/array *before* we can get the name, for example when using `while (beginObject)` from the serializer class.
	 // To work around this we copy the container name into the dummy current value.
		m_currentValue.m_name = m_containerStack.back().m_name;
	}

	void leave()
	{
		m_currentValue = m_containerStack.back();
		m_containerStack.pop_back();
	}
	
	void addNew(const char* _name)
	{
		auto& container = m_containerStack.back();
		auto containerType = container.getType();
		FRM_ASSERT(containerType == ValueType_Object);
		m_currentValue.m_index = (int)container.m_value->MemberCount();
		container.m_value->AddMember(
			rapidjson::StringRef(_name),
			rapidjson::Value().Move(), 
			m_dom.GetAllocator()
		);
		auto it = container.m_value->MemberEnd() - 1;
		m_currentValue.m_name  = it->name.GetString();
		m_currentValue.m_value = &it->value;
	}

	void pushNew()
	{
		auto& container = m_containerStack.back();
		auto containerType = container.getType();
		FRM_ASSERT(containerType == ValueType_Array);
		m_currentValue.m_index = (int)container.m_value->Size();
		container.m_value->PushBack(rapidjson::Value().Move(), m_dom.GetAllocator());
		m_currentValue.m_name  = "";
		m_currentValue.m_value = container.m_value->End() - 1;
	}

 // findGet*() optionally moves m_currentValue to the specified member/array element and returns the value

	rapidjson::Value* findGet(const char* _name , int _i, ValueType _expectedType)
	{
		if (_name) {
			find(_name);
		} else if (_i > -1) {
			find(_i);
		}
		FRM_ASSERT(m_currentValue.m_value);
		JSON_ERR_TYPE("get()", m_currentValue.m_name, m_currentValue.getType(), _expectedType, ;);
		return m_currentValue.m_value;
	}

	bool findGetBool(const char * _name, int _i)
	{
		return findGet(_name, _i, ValueType_Bool)->GetBool();
	}

	const char* findGetString(const char * _name, int _i)
	{
		return findGet(_name, _i, ValueType_String)->GetString();
	}

	template <typename T>
	T findGetNumber(const char* _name, int _i)
	{
		switch (FRM_DATA_TYPE_TO_ENUM(T)) {
			default:
			case DataType_Uint8:
			case DataType_Uint8N:
			case DataType_Uint16:
			case DataType_Uint16N:
			case DataType_Uint32:
			case DataType_Uint32N:
				return (T)findGet(_name, _i, ValueType_Number)->GetUint();
			case DataType_Uint64:
			case DataType_Uint64N:
				return (T)findGet(_name, _i, ValueType_Number)->GetUint64();
			case DataType_Sint8:
			case DataType_Sint8N:
			case DataType_Sint16:
			case DataType_Sint16N:
			case DataType_Sint32:
			case DataType_Sint32N:
				return (T)findGet(_name, _i, ValueType_Number)->GetInt();
			case DataType_Sint64:
			case DataType_Sint64N:
				return (T)findGet(_name, _i, ValueType_Number)->GetUint();
			case DataType_Float16: 
				return (T)PackFloat16(findGet(_name, _i, ValueType_Number)->GetFloat());
			case DataType_Float32:
				return (T)findGet(_name, _i, ValueType_Number)->GetFloat();
			case DataType_Float64:
				return (T)findGet(_name, _i, ValueType_Number)->GetDouble();
		};
	}

	template <typename T>
	T findGetVector(const char* _name, int _i)
	{
	 // vectors are arrays of numbers
		T ret = T(FRM_TRAITS_BASE_TYPE(T)(0));
		auto val = findGet(_name, _i, ValueType_Array);
		auto arr = val->GetArray();
		JSON_ERR_SIZE("getVector", m_currentValue.m_name, (int)arr.Size(), FRM_TRAITS_COUNT(T), return ret);
		JSON_ERR_TYPE("getVector", m_currentValue.m_name, GetValueType(arr[0].GetType()), ValueType_Number, return ret);

		typedef FRM_TRAITS_BASE_TYPE(T) BaseType;
		if (DataTypeIsFloat(FRM_DATA_TYPE_TO_ENUM(BaseType))) {
			for (int i = 0; i < FRM_TRAITS_COUNT(T); ++i) {
				ret[i] = (BaseType)arr[i].GetFloat();
			}
		} else {
			if (DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(BaseType))) {
				for (int i = 0; i < FRM_TRAITS_COUNT(T); ++i) {
					ret[i] = (BaseType)arr[i].GetInt();
				}
			} else {
				for (int i = 0; i < FRM_TRAITS_COUNT(T); ++i) {
					ret[i] = (BaseType)arr[i].GetUint();
				}
			}
		}		
		return ret;
	}

	template <typename T, int kCount> // \hack FRM_TRAITS_COUNT(T) returns the # of floats in the matrix, hence kCount for the # of vectors
	T findGetMatrix(const char* _name, int _i)
	{
	 // matrices are arrays of vectors
		auto val = findGet(_name, _i, ValueType_Array);
		auto rows = val->GetArray();
		JSON_ERR_SIZE("getMatrix", m_currentValue.m_name, (int)rows.Size(), kCount, return identity);
		JSON_ERR_TYPE("getMatrix", m_currentValue.m_name, GetValueType(rows[0].GetType()), ValueType_Array, return identity);

		T ret;
		for (int i = 0; i < kCount; ++i) {
			for (int j = 0; j < kCount; ++j) { // only square matrices supported
				auto& row = rows[i];
				JSON_ERR_SIZE("getMatrix", m_currentValue.m_name, (int)row.Size(), kCount, return identity);
				ret[i][j] = row[j].GetFloat();
			}
		}		
		return ret;
	}

 // findAdd*() optionally moves m_currentValue to the specified member/array element and returns the value

	rapidjson::Value* findAdd(const char* _name , int _i)
	{
		if (_name) {
			if (!find(_name)) {
				addNew(_name);
			}
		} else if (_i > -1) {
			if (!find(_i)) {
				FRM_ASSERT(false); // \todo what to do in this case?
			}
		}
		FRM_ASSERT(m_currentValue.m_value);
		return m_currentValue.m_value;
	}

	void findAddBool(const char* _name, int _i, bool _value)
	{
		findAdd(_name, _i)->SetBool(_value);
	}

	void findAddString(const char* _name, int _i, const char* _value)
	{
		findAdd(_name, _i)->SetString(_value, m_dom.GetAllocator());
	}

	template <typename T>
	void findAddNumber(const char* _name, int _i, T _value)
	{
		switch (FRM_DATA_TYPE_TO_ENUM(T)) {
			default:
			case DataType_Uint8:
			case DataType_Uint8N:
			case DataType_Uint16:
			case DataType_Uint16N:
			case DataType_Uint32:
			case DataType_Uint32N:
				findAdd(_name, _i)->SetUint((uint32)_value);
				break;
			case DataType_Uint64:
			case DataType_Uint64N:
				findAdd(_name, _i)->SetUint64((uint64)_value);
				break;
			case DataType_Sint8:
			case DataType_Sint8N:
			case DataType_Sint16:
			case DataType_Sint16N:
			case DataType_Sint32:
			case DataType_Sint32N:
				findAdd(_name, _i)->SetInt((sint32)_value);
				break;
			case DataType_Sint64:
			case DataType_Sint64N:
				findAdd(_name, _i)->SetInt64((sint64)_value);
				break;
			case DataType_Float16: 
				findAdd(_name, _i)->SetFloat(UnpackFloat16((uint16)_value));
				break;
			case DataType_Float32:
				findAdd(_name, _i)->SetFloat((float)_value);
				break;
			case DataType_Float64:
				findAdd(_name, _i)->SetDouble((double)_value);
				break;
		};
	}

	template <typename T>
	void findAddVector(const char* _name, int _i, const T& _value)
	{
		int n = FRM_TRAITS_COUNT(T);
		auto vec = findAdd(_name, _i);
		vec->SetArray();
		for (int i = 0; i < n; ++i) {
			vec->PushBack(_value[i], m_dom.GetAllocator());
		}
	}

	template <typename T, int kCount> // \hack FRM_TRAITS_COUNT(T) returns the # of floats in the matrix, hence kCount for the # of vectors
	void findAddMatrix(const char* _name, int _i, const T& _value)
	{
		auto mat = findAdd(_name, _i);
		mat->SetArray();
		for (int i = 0; i < kCount; ++i) {
			mat->PushBack(rapidjson::Value().SetArray().Move(), m_dom.GetAllocator());
			auto& row = *(mat->End() - 1);
			for (int j = 0; j < kCount; ++j) {
				row.PushBack(_value[i][j], m_dom.GetAllocator());
			}
		}
	}
};


bool Json::Read(Json& json_, const File& _file)
{
	json_.m_impl->m_dom.Parse(_file.getData());
	if (json_.m_impl->m_dom.HasParseError()) {
		FRM_LOG_ERR("Json: %s\n\t'%s'", _file.getPath(), rapidjson::GetParseError_En(json_.m_impl->m_dom.GetParseError()));
		return false;
	}
	return true;
}

bool Json::Read(Json& json_, const char* _path, int _root)
{
	FRM_AUTOTIMER("Json::Read(%s)", _path);
	File f;
	if (!FileSystem::ReadIfExists(f, _path, _root)) {
		return false;
	}
	return Read(json_, f);
}

bool Json::Write(const Json& _json, File& file_)
{
	rapidjson::StringBuffer buf;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(buf);
	wr.SetIndent('\t', 1);
	wr.SetFormatOptions(rapidjson::kFormatSingleLineArray);
	_json.m_impl->m_dom.Accept(wr);
	file_.setData(buf.GetString(), buf.GetSize());
	return true;
}

bool Json::Write(const Json& _json, const char* _path, int _root)
{
	FRM_AUTOTIMER("Json::Write(%s)", _path);
	File f;
	if (Write(_json, f)) {
		return FileSystem::Write(f, _path, _root);
	}
	return false;
}

Json::Json(const char* _path, int _root)
	: m_impl(nullptr)
{
	m_impl = FRM_NEW(Impl);
	m_impl->m_dom.SetObject();	
	m_impl->reset();
	if (_path) {
		Json::Read(*this, _path, _root);
	}
}

Json::~Json()
{
	if (m_impl) {
		FRM_DELETE(m_impl);
	}
}

bool Json::find(const char* _name)
{
	return m_impl->find(_name);
}
	
bool Json::next()
{
	auto& container = m_impl->m_containerStack.back();
	auto& current   = m_impl->m_currentValue;

	int i = current.m_index + 1;
	if (i >= container.getSize()) {
		return false;
	}
	current.m_index = i;

	auto containerType = container.getType();
	if (containerType == ValueType_Object) {
		auto it = (container.m_value->MemberBegin() + i);
		current.m_value = &it->value;
		current.m_name  = it->name.GetString();
	} else if (containerType == ValueType_Array) {
		current.m_value = container.m_value->Begin() + i;
	} else {
		FRM_ASSERT(false);
		return false;
	}
	return true;
}

bool Json::enterObject()
{
	JSON_ERR_TYPE("enterObject", getName(), getType(), ValueType_Object, return false);
	m_impl->enter();
	return true;
}

void Json::leaveObject()
{
	m_impl->leave();
	FRM_ASSERT(getType() == ValueType_Object);
}

bool Json::enterArray()
{
	JSON_ERR_TYPE("enterArray", getName(), getType(), ValueType_Array, return false);
	m_impl->enter();
	return true;
}

void Json::leaveArray()
{
	m_impl->leave();
	FRM_ASSERT(getType() == ValueType_Array);
}

void Json::reset()
{
	m_impl->reset();
}

Json::ValueType Json::getType() const
{
	return m_impl->m_currentValue.getType();
}

const char* Json::getName() const
{
	return m_impl->m_currentValue.m_name;
}

int Json::getIndex() const
{
	return m_impl->m_currentValue.m_index;
}

int Json::getArrayLength() const
{
	if (m_impl->m_containerStack.back().getType() == ValueType_Array) {
		return m_impl->m_containerStack.back().getSize();
	}
	return -1;
}

template <> bool Json::getValue<bool>(int _i) const
{
	return m_impl->findGetBool(nullptr, _i);
}

template <> const char* Json::getValue<const char*>(int _i) const
{
	return m_impl->findGetString(nullptr, _i);
}

// use the FRM_DataType_decl macro to instantiate get<>() for all the number types
#define Json_getValue_Number(_type, _enum) \
	template <> _type Json::getValue<_type>(int _i) const { \
		return m_impl->findGetNumber<_type>(nullptr, _i); \
	}
FRM_DataType_decl(Json_getValue_Number)

// manually instantiate vector types
#define Json_getValue_Vector(_type) \
	template <> _type Json::getValue<_type>(int _i) const { \
		 return m_impl->findGetVector<_type>(nullptr, _i); \
	}
Json_getValue_Vector(vec2)
Json_getValue_Vector(vec3)
Json_getValue_Vector(vec4)
Json_getValue_Vector(ivec2)
Json_getValue_Vector(ivec3)
Json_getValue_Vector(ivec4)
Json_getValue_Vector(uvec2)
Json_getValue_Vector(uvec3)
Json_getValue_Vector(uvec4)

// manually instantiate matrix types
#define Json_getValue_Matrix(_type, _count) \
	template <> _type Json::getValue<_type>(int _i) const { \
		 return m_impl->findGetMatrix<_type, _count>(nullptr, _i); \
	}
Json_getValue_Matrix(mat2, 2)
Json_getValue_Matrix(mat3, 3)
Json_getValue_Matrix(mat4, 4)


template <> void Json::setValue<bool>(bool _value, int _i)
{
	m_impl->findAddBool(nullptr, _i, _value);
}
template <> void Json::setValue<bool>(bool _value, const char* _name)
{
	m_impl->findAddBool(_name, -1, _value);
}
template <> void Json::setValue<const char*>(const char* _value, int _i)
{
	m_impl->findAddString(nullptr, _i, _value);
}
template <> void Json::setValue<const char*>(const char* _value, const char* _name)
{
	m_impl->findAddString(_name, -1, _value);
}
#define Json_setValue_Number(_type, _enum) \
	template <> void Json::setValue<_type>(_type _value, int _i) { \
		m_impl->findAddNumber<_type>(nullptr, _i, _value); \
	} \
	template <> void Json::setValue<_type>(_type _value, const char* _name) { \
		m_impl->findAddNumber<_type>(_name, -1, _value); \
	}
FRM_DataType_decl(Json_setValue_Number)

#define Json_setValue_Vector(_type) \
	template <> void Json::setValue<_type>(_type _value, int _i) { \
		m_impl->findAddVector<_type>(nullptr, _i, _value); \
	} \
	template <> void Json::setValue<_type>(_type _value, const char* _name) { \
		m_impl->findAddVector<_type>(_name, -1, _value); \
	}
Json_setValue_Vector(vec2)
Json_setValue_Vector(vec3)
Json_setValue_Vector(vec4)
Json_setValue_Vector(ivec2)
Json_setValue_Vector(ivec3)
Json_setValue_Vector(ivec4)
Json_setValue_Vector(uvec2)
Json_setValue_Vector(uvec3)
Json_setValue_Vector(uvec4)


#define Json_setValue_Matrix(_type, _count) \
	template <> void Json::setValue<_type>(_type _value, int _i) { \
		m_impl->findAddMatrix<_type, _count>(nullptr, _i, _value); \
	} \
	template <> void Json::setValue<_type>(_type _value, const char* _name) { \
		m_impl->findAddMatrix<_type, _count>(_name, -1, _value); \
	}
Json_setValue_Matrix(mat2, 2)
Json_setValue_Matrix(mat3, 3)
Json_setValue_Matrix(mat4, 4)

#define Json_pushValue(_type, _enum) \
	template <> void Json::pushValue<_type>(_type _value) { \
		m_impl->pushNew(); \
		setValue(_value); \
	}
Json_pushValue(bool, 0)
Json_pushValue(const char*, 0)
FRM_DataType_decl(Json_pushValue)
Json_pushValue(vec2, 0)
Json_pushValue(vec3, 0)
Json_pushValue(vec4, 0)
Json_pushValue(ivec2, 0)
Json_pushValue(ivec3, 0)
Json_pushValue(ivec4, 0)
Json_pushValue(uvec2, 0)
Json_pushValue(uvec3, 0)
Json_pushValue(uvec4, 0)
Json_pushValue(mat2, 0)
Json_pushValue(mat3, 0)
Json_pushValue(mat4, 0)

void Json::beginObject(const char* _name)
{
	if (m_impl->m_containerStack.back().getType() == ValueType_Object) {
		FRM_ASSERT(_name);
		if (!m_impl->find(_name)) {
			m_impl->addNew(_name);
			m_impl->m_currentValue.m_value->SetObject();
		}
			
	} else {
		if (_name) {
			FRM_LOG_ERR("Json: beginObject() called inside array, name '%s' will be ignored", _name);
		}
		m_impl->pushNew();
		m_impl->m_currentValue.m_value->SetObject();
	}
	m_impl->enter();
}

void Json::beginArray(const char* _name)
{
	if (m_impl->m_containerStack.back().getType() == ValueType_Object) {
		FRM_ASSERT(_name);
		if (!m_impl->find(_name)) {
			m_impl->addNew(_name);
			m_impl->m_currentValue.m_value->SetArray();
		}

	} else {
		if (_name) {
			FRM_LOG_ERR("Json: beginArray() called inside array, name '%s' will be ignored", _name);
		}
		m_impl->pushNew();
		m_impl->m_currentValue.m_value->SetArray();
	}
	m_impl->enter();
}

static bool VisitRecursive(Json* _json_, Json::OnVisit* _onVisit, int _depth = 0)
{
	bool ret = true;

	while (ret && _json_->next()) {
		auto type  = _json_->getType();
		auto name  = _json_->getName();
		auto index = _json_->getIndex();
		ret = _onVisit(_json_, type, name, index, _depth);
		if (ret) {
			if (type == Json::ValueType_Array) {
				_json_->enterArray();
				ret = VisitRecursive(_json_, _onVisit, _depth + 1);
				_json_->leaveArray();
			} else if (type == Json::ValueType_Object) {
				_json_->enterObject();
				ret = VisitRecursive(_json_, _onVisit, _depth + 1);
				_json_->leaveObject();
			}
		}
	}

	return ret;
}
void Json::visitAll(OnVisit* _onVisit)
{
	FRM_STRICT_ASSERT(_onVisit);
	VisitRecursive(this, _onVisit);
}

/*******************************************************************************

                              SerializerJson

*******************************************************************************/

// PUBLIC

SerializerJson::SerializerJson(Json& _json_, Mode _mode)
	: Serializer(_mode) 
	, m_json(&_json_)
{
}

bool SerializerJson::beginObject(const char* _name)
{
	if (getMode() == SerializerJson::Mode_Read) {
		if (_name) {
			if (!m_json->find(_name)) {
				//setError("Error serializing object: '%s' not found", _name);
				return false;
			}
		} else {
			if (!m_json->next()) {
				return false;
			}
		}
		if (m_json->getType() != Json::ValueType_Object) {
			setError("Error serializing object: '%s' not an object", m_json->getName());
			return false;
		}

		m_json->enterObject();

	} else {
		m_json->beginObject(_name);
	}
	
	return true;
}
void SerializerJson::endObject()
{
	if (m_mode == Mode_Read) {
		m_json->leaveObject();
	} else {
		m_json->endObject();
	}
}

bool SerializerJson::beginArray(uint& _length_, const char* _name)
{
	if (getMode() == SerializerJson::Mode_Read) {
		if (_name) {
			if (!m_json->find(_name)) {
				//setError("Error serializing array: '%s' not found", _name);
				return false;
			}
		} else {
			if (!m_json->next()) {
				return false;
			}
		}
		if (m_json->getType() != Json::ValueType_Array) {
			setError("Error serializing array: '%s' not an array", m_json->getName());
			return false;
		}

		m_json->enterArray();
		_length_ = (uint)m_json->getArrayLength();

	} else {
		m_json->beginArray(_name);
	}

	return true;
}
void SerializerJson::endArray()
{
	if (m_mode == Mode_Read) {
		m_json->leaveArray();
	} else {
		m_json->endArray();
	}
}

const char* SerializerJson::getName() const
{
	return m_json->getName();
}

uint32 SerializerJson::getIndex() const
{
	return (uint32)m_json->getIndex();
}

template <typename tType>
static bool ValueImpl(SerializerJson& _serializer_, tType& _value_, const char* _name)
{
	Json* json = _serializer_.getJson();
	//if (!_name && json->getArrayLength() < 0) {
	//	_serializer_.setError("Error serializing %s: name must be specified if not in an array", Serializer::ValueTypeToStr<tType>());
	//	return false;
	//}

	if (_serializer_.getMode() == SerializerJson::Mode_Read) {
		if (_name) {
			if (!json->find(_name)) {
				//_serializer_.setError("Error serializing %s: '%s' not found", Serializer::ValueTypeToStr<tType>(), _name);
				return false;
			}
		} else {
			if (!json->next()) {
				return false;
			}
		}
		_value_ = json->getValue<tType>();
		return true;

	} else {
		if (_name) {
			json->setValue<tType>(_value_, _name);
		} else {
			json->pushValue<tType>(_value_);
		}
		return true; 
	}
}

bool SerializerJson::value(bool&    _value_, const char* _name) { return ValueImpl<bool>   (*this, _value_, _name); }
bool SerializerJson::value(sint8&   _value_, const char* _name) { return ValueImpl<sint8>  (*this, _value_, _name); }
bool SerializerJson::value(uint8&   _value_, const char* _name) { return ValueImpl<uint8>  (*this, _value_, _name); }
bool SerializerJson::value(sint16&  _value_, const char* _name) { return ValueImpl<sint16> (*this, _value_, _name); }
bool SerializerJson::value(uint16&  _value_, const char* _name) { return ValueImpl<uint16> (*this, _value_, _name); }
bool SerializerJson::value(sint32&  _value_, const char* _name) { return ValueImpl<sint32> (*this, _value_, _name); }
bool SerializerJson::value(uint32&  _value_, const char* _name) { return ValueImpl<uint32> (*this, _value_, _name); }
bool SerializerJson::value(sint64&  _value_, const char* _name) { return ValueImpl<sint64> (*this, _value_, _name); }
bool SerializerJson::value(uint64&  _value_, const char* _name) { return ValueImpl<uint64> (*this, _value_, _name); }
bool SerializerJson::value(float32& _value_, const char* _name) { return ValueImpl<float32>(*this, _value_, _name); }
bool SerializerJson::value(float64& _value_, const char* _name) { return ValueImpl<float64>(*this, _value_, _name); }

bool SerializerJson::value(StringBase& _value_, const char* _name) 
{ 
	//if (!_name && m_json->getArrayLength() == -1) {
	//	setError("Error serializing StringBase; name must be specified if not in an array");
	//	return false;
	//}
	if (getMode() == SerializerJson::Mode_Read) {
		if (_name) {
			if (!m_json->find(_name)) {
				//setError("Error serializing StringBase; '%s' not found", _name);
				return false;
			}
		} else {
			if (!m_json->next()) {
				return false;
			}
		}

		if (m_json->getType() == Json::ValueType_String) {
			_value_.set(m_json->getValue<const char*>());
			return true;
		} else {
			setError("Error serializing StringBase; '%s' not a string", _name ? _name : "");
			return false;
		}

	} else {
		if (_name) {
			m_json->setValue<const char*>((const char*)_value_, _name);
		} else {
			m_json->pushValue<const char*>((const char*)_value_);
		}
		return true; 
	}
}


// Base64 encode/decode of binary data, adfrmed from https://github.com/adamvr/arduino-base64
static const char kBase64Alphabet[] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/"
		;
static inline void Base64A3ToA4(const unsigned char* _a3, unsigned char* a4_) 
{
	a4_[0] = (_a3[0] & 0xfc) >> 2;
	a4_[1] = ((_a3[0] & 0x03) << 4) + ((_a3[1] & 0xf0) >> 4);
	a4_[2] = ((_a3[1] & 0x0f) << 2) + ((_a3[2] & 0xc0) >> 6);
	a4_[3] = (_a3[2] & 0x3f);
}
static inline void Base64A4ToA3(const unsigned char* _a4, unsigned char* a3_) {
	a3_[0] = (_a4[0] << 2) + ((_a4[1] & 0x30) >> 4);
	a3_[1] = ((_a4[1] & 0xf) << 4) + ((_a4[2] & 0x3c) >> 2);
	a3_[2] = ((_a4[2] & 0x3) << 6) + _a4[3];
}
static inline unsigned char Base64Index(char _c)
{
	if (_c >= 'A' && _c <= 'Z') return _c - 'A';
	if (_c >= 'a' && _c <= 'z') return _c - 71;
	if (_c >= '0' && _c <= '9') return _c + 4;
	if (_c == '+') return 62;
	if (_c == '/') return 63;
	return -1;
}
static void Base64Encode(const char* _in, uint _inSizeBytes, char* out_, uint outSizeBytes_)
{
	uint i = 0;
	uint j = 0;
	uint k = 0;
	unsigned char a3[3];
	unsigned char a4[4];
	while (_inSizeBytes--) {
		a3[i++] = *(_in++);
		if (i == 3) {
			Base64A3ToA4(a3, a4);
			for (i = 0; i < 4; i++) {
				out_[k++] = kBase64Alphabet[a4[i]];
			}
			i = 0;
		}
	}
	if (i) {
		for (j = i; j < 3; j++) {
			a3[j] = '\0';
		}
		Base64A3ToA4(a3, a4);
		for (j = 0; j < i + 1; j++) {
			out_[k++] = kBase64Alphabet[a4[j]];
		}
		while ((i++ < 3)) {
			out_[k++] = '=';
		}
	}
	out_[k] = '\0';
	FRM_ASSERT(outSizeBytes_ == k); // overflow
}
static void Base64Decode(const char* _in, uint _inSizeBytes, char* out_, uint outSizeBytes_) {
	uint i = 0;
	uint j = 0;
	uint k = 0;
	unsigned char a3[3];
	unsigned char a4[4];
	while (_inSizeBytes--) {
		if (*_in == '=') {
			break;
		}
		a4[i++] = *(_in++);
		if (i == 4) {
			for (i = 0; i < 4; i++) {
				a4[i] = Base64Index(a4[i]);
			}
			Base64A4ToA3(a4, a3);
			for (i = 0; i < 3; i++) {
				out_[k++] = a3[i];
			}
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++) {
			a4[j] = '\0';
		}
		for (j = 0; j < 4; j++) {
			a4[j] = Base64Index(a4[j]);
		}
		Base64A4ToA3(a4, a3);
		for (j = 0; j < i - 1; j++) {
			out_[k++] = a3[j];
		}
	}
	FRM_ASSERT(outSizeBytes_ == k); // overflow
}
static uint Base64EncSizeBytes(uint _sizeBytes) 
{
	uint n = _sizeBytes;
	return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}
static uint Base64DecSizeBytes(char* _buf, uint _sizeBytes) 
{
	uint padCount = 0;
	for (uint i = _sizeBytes - 1; _buf[i] == '='; i--) {
		padCount++;
	}
	return ((6 * _sizeBytes) / 8) - padCount;
}

bool SerializerJson::binary(void*& _data_, uint& _sizeBytes_, const char* _name, CompressionFlags _compressionFlags)
{
	if (getMode() == Mode_Write) {
		FRM_ASSERT(_data_);
		char* data = (char*)_data_;
		uint sizeBytes = _sizeBytes_;
		if (_compressionFlags != CompressionFlags_None) {
			data = nullptr;
			Compress(_data_, _sizeBytes_, (void*&)data, sizeBytes, _compressionFlags);
		}
		String<0> str;
		str.setLength(Base64EncSizeBytes(sizeBytes) + 1);
		str[0] = _compressionFlags == CompressionFlags_None ? '0' : '1'; // prepend 0, or 1 if compression
		Base64Encode(data, sizeBytes, (char*)str + 1, str.getLength() - 1);
		if (_compressionFlags != CompressionFlags_None) {
			free(data);
		}
		value((StringBase&)str, _name);

	} else {
		String<0> str;
		if (!value((StringBase&)str, _name)) {
			return false;
		}
		bool compressed = str[0] == '1' ? true : false;
		uint binSizeBytes = Base64DecSizeBytes((char*)str + 1, str.getLength() - 1);
		char* bin = (char*)FRM_MALLOC(binSizeBytes);
		Base64Decode((char*)str + 1, str.getLength() - 1, bin, binSizeBytes);

		char* ret = bin;
		uint retSizeBytes = binSizeBytes;
		if (compressed) {
			ret = nullptr; // decompress to allocates the final buffer
			Decompress(bin, binSizeBytes, (void*&)ret, retSizeBytes);
		}
		if (_data_) {
			if (retSizeBytes != _sizeBytes_) {
				setError("Error serializing %s, buffer size was %llu (expected %llu)", _sizeBytes_, retSizeBytes);
				if (compressed) {
					FRM_FREE(ret);
				}
				return false;
			}
			memcpy(_data_, ret, retSizeBytes);
			if (compressed) {
				FRM_FREE(ret);
			}
		} else {
			_data_ = ret;
			_sizeBytes_ = retSizeBytes;
		}
	}
	return true;
}
