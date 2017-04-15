#pragma once
#ifndef frm_Property_h
#define frm_Property_h

#include <frm/def.h>
#include <frm/math.h>

#include <apt/String.h>
#include <apt/StringHash.h>

#include <EASTL/vector_map.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Property
////////////////////////////////////////////////////////////////////////////////
class Property
{
public:
	typedef bool (Edit)(Property& _prop); // edit func, return true if the value changed

	enum Type : uint8
	{
		Type_Bool,
		Type_Int,
		Type_Float,
		Type_String,

		Type_Count
	};

	static int GetTypeSize(Type _type);

	Property(
		const char* _name,
		Type        _type,
		int         _count,
		const void* _default,
		const void* _min,
		const void* _max,
		void*       storage_,
		const char* _displayName,
		Edit*       _edit
		);

	~Property();

	Property(Property&& _rhs);
	Property& operator=(Property&& _rhs);
	friend void swap(Property& _a_, Property& _b_);

	bool edit();
	bool serialize(apt::JsonSerializer& _serializer_);

	void* getData()               { return m_data; }
	Type getType()                { return m_type; }
	int  getCount()               { return m_count; }
	const char* getName()         { return m_name; }
	const char* getDisplayName()  { return m_displayName; }

private:
	typedef apt::String<32> String;

	char*  m_data;
	char*  m_default;
	char*  m_min;
	char*  m_max;
	Type   m_type;
	uint8  m_count;
	bool   m_ownsData;
	String m_name;
	String m_displayName;
	Edit*  m_pfEdit;

}; // class Property


////////////////////////////////////////////////////////////////////////////////
// PropertyGroup
////////////////////////////////////////////////////////////////////////////////
class PropertyGroup: private apt::non_copyable<PropertyGroup>
{
public:
	typedef apt::StringBase StringBase;
	typedef apt::StringHash StringHash;

	PropertyGroup(const char* _name);
	~PropertyGroup();

	PropertyGroup(PropertyGroup&& _rhs);
	PropertyGroup& operator=(PropertyGroup&& _rhs);
	friend void swap(PropertyGroup& _a_, PropertyGroup& _b_);

	bool*       addBool  (const char* _name, bool         _default,                         bool*       storage_ = nullptr, const char* _displayName = nullptr);
	int*        addInt   (const char* _name, int          _default, int   _min, int   _max, int*        storage_ = nullptr, const char* _displayName = nullptr);
	ivec2*      addInt2  (const char* _name, const ivec2& _default, int   _min, int   _max, ivec2*      storage_ = nullptr, const char* _displayName = nullptr);
	ivec3*      addInt3  (const char* _name, const ivec3& _default, int   _min, int   _max, ivec3*      storage_ = nullptr, const char* _displayName = nullptr);
	ivec4*      addInt4  (const char* _name, const ivec4& _default, int   _min, int   _max, ivec4*      storage_ = nullptr, const char* _displayName = nullptr);
	float*      addFloat (const char* _name, float        _default, float _min, float _max, float*      storage_ = nullptr, const char* _displayName = nullptr);
	vec2*       addFloat2(const char* _name, const vec2&  _default, float _min, float _max, vec2*       storage_ = nullptr, const char* _displayName = nullptr);
	vec3*       addFloat3(const char* _name, const vec3&  _default, float _min, float _max, vec3*       storage_ = nullptr, const char* _displayName = nullptr);
	vec4*       addFloat4(const char* _name, const vec4&  _default, float _min, float _max, vec4*       storage_ = nullptr, const char* _displayName = nullptr);
	vec3*       addRgb   (const char* _name, const vec3&  _default, float _min, float _max, vec3*       storage_ = nullptr, const char* _displayName = nullptr);
	vec4*       addRgba  (const char* _name, const vec4&  _default, float _min, float _max, vec4*       storage_ = nullptr, const char* _displayName = nullptr);
	StringBase* addString(const char* _name, const char*  _default,                         StringBase* storage_ = nullptr, const char* _displayName = nullptr);
	StringBase* addPath  (const char* _name, const char*  _default,                         StringBase* storage_ = nullptr, const char* _displayName = nullptr);

	Property* find(StringHash _nameHash);
	Property* find(const char* _name) { return find(StringHash(_name)); }

	const char* getName() const { return m_name; }

	bool edit(bool _showHidden = false);
	bool serialize(apt::JsonSerializer& _serializer_);

private:
	apt::String<32> m_name;
	eastl::vector_map<apt::StringHash, Property*> m_props;

}; // class PropertyGroup

////////////////////////////////////////////////////////////////////////////////
// Properties
////////////////////////////////////////////////////////////////////////////////
class Properties: private apt::non_copyable<Properties>
{
public:
	typedef apt::StringHash StringHash;

	Properties()  {}
	~Properties();

	PropertyGroup& addGroup(const char* _name);

	Property* findProperty(StringHash _nameHash);	
	Property* findProperty(const char* _name) { return findProperty(StringHash(_name)); }

	PropertyGroup* findGroup(StringHash _nameHash);	
	PropertyGroup* findGroup(const char* _name) { return findGroup(StringHash(_name)); }

	bool edit(bool _showHidden = false);
	bool serialize(apt::JsonSerializer& _serializer_);

private:
	eastl::vector_map<apt::StringHash, PropertyGroup*> m_groups;

}; // class Properties


} // namespace frm

#endif // frm_Property_h
