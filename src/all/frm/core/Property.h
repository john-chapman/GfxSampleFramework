#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

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
	typedef apt::StringBase StringBase;
	typedef apt::StringHash StringHash;

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

	bool        edit();
	void        setDefault();
	friend bool Serialize(apt::SerializerJson& _serializer_, Property& _prop_);

	bool*       asBool()               { APT_ASSERT(getType() == Type_Bool);   return (bool*)getData();       }
	int*        asInt()                { APT_ASSERT(getType() == Type_Int);    return (int*)getData();        }
	ivec2*      asInt2()               { APT_ASSERT(getType() == Type_Int);    return (ivec2*)getData();      }
	ivec3*      asInt3()               { APT_ASSERT(getType() == Type_Int);    return (ivec3*)getData();      }
	ivec4*      asInt4()               { APT_ASSERT(getType() == Type_Int);    return (ivec4*)getData();      }
	float*      asFloat()              { APT_ASSERT(getType() == Type_Float);  return (float*)getData();      }
	vec2*       asFloat2()             { APT_ASSERT(getType() == Type_Float);  return (vec2*)getData();       }
	vec3*       asFloat3()             { APT_ASSERT(getType() == Type_Float);  return (vec3*)getData();       }
	vec4*       asFloat4()             { APT_ASSERT(getType() == Type_Float);  return (vec4*)getData();       }
	vec3*       asRgb()                { APT_ASSERT(getType() == Type_Float);  return (vec3*)getData();       }
	vec4*       asRgba()               { APT_ASSERT(getType() == Type_Float);  return (vec4*)getData();       }
	StringBase* asString()             { APT_ASSERT(getType() == Type_String); return (StringBase*)getData(); }
	StringBase* asPath()               { APT_ASSERT(getType() == Type_String); return (StringBase*)getData(); }

	void*       getData() const        { return m_data; }
	Type        getType() const        { return m_type; }
	int         getCount() const       { return m_count; }
	const char* getName() const        { return (const char*)m_name; }
	const char* getDisplayName() const { return (const char*)m_displayName; }

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

};


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

	Property*   find(StringHash _nameHash);
	Property*   find(const char* _name) { return find(StringHash(_name)); }

	const char* getName() const { return (const char*)m_name; }

	bool        edit(bool _showHidden = false);
	friend bool Serialize(apt::SerializerJson& _serializer_, PropertyGroup& _prop_);

private:
	apt::String<32> m_name;
	eastl::vector_map<apt::StringHash, Property*> m_props;

};

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

	Property*      findProperty(const char* _name)   { return findProperty(StringHash(_name)); }
	Property*      findProperty(StringHash _nameHash);	

	PropertyGroup* findGroup(StringHash _nameHash);	
	PropertyGroup* findGroup(const char* _name) { return findGroup(StringHash(_name)); }

	bool           edit(bool _showHidden = false);
	friend bool    Serialize(apt::SerializerJson& _serializer_, Properties& _props_);

private:
	eastl::vector_map<apt::StringHash, PropertyGroup*> m_groups;

};


} // namespace frm
