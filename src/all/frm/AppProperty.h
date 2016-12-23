#pragma once
#ifndef frm_AppProperty_h
#define frm_AppProperty_h

#include <frm/def.h>
#include <frm/math.h>

#include <apt/PersistentVector.h>
#include <apt/String.h>
#include <apt/StringHash.h>

namespace apt {
	class IniFile;
	class ArgList;
}

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class AppProperty
/// \note Data is conservatively aligned on 16-byte boundaries to permit 
///   directly casting to/from SIMD types (vec4, etc.).
/// \note String properties are implemented as char[kMaxStringLength].
/// \todo Validate names.
////////////////////////////////////////////////////////////////////////////////
class AppProperty: private apt::non_copyable<AppProperty>
{
	friend class AppPropertyGroup;
public:
	typedef apt::String<32> String;
	static const int kMaxStringLength = 1024;

	typedef bool (Edit)(AppProperty& _prop);

	enum Type
	{
		kBool,
		kInt,
		kFloat,
		kString,

		kTypeCount
	};

	AppProperty(AppProperty&& _rhs);
	AppProperty& operator=(AppProperty&& _rhs);
	friend void swap(AppProperty& _a, AppProperty& _b);

	~AppProperty();

	template <typename tType>
	tType& getValue();

	template <typename tType>
	tType& getDefault();

	template <typename tType>
	tType& getMin();

	template <typename tType>
	tType& getMax();

	/// Copy default -> value.
	void reset();

	const char* getName() const        { return m_name; }
	const char* getDisplayName() const { return m_displayName; }
	Type        getType() const        { return (Type)m_type; }
	uint8       getCount() const       { return m_count; }
	uint8       getSize() const        { return m_size; }
	bool        isHidden() const       { return m_isHidden; }

	/// \return true if the value was changed.
	bool edit();

private:
	String   m_name;        //< Identifier (must be unique within a group).
	String   m_displayName; //< User-friendly name.
	uint8    m_type;        //< Type enum.
	uint8    m_count;       //< 1,2,3 or 4 for vector types.
	uint8    m_size;        //< Bytes, size of the base type * m_count.
	bool     m_isHidden;    //< Don't render in the UI.
	char*    m_data;        //< Property value, default value + optional min/max values.
	Edit*    m_pfEdit;

	AppProperty(
		const char*       _name,
		const char*       _displayName,
		AppProperty::Type _type, 
		uint8             _count,
		bool              _isHidden,
		const void*       _value, 
		const void*       _default, 
		const void*       _min, 
		const void*       _max,
		Edit*             _pfEdit
		);

	void allocData( 
		const void* _value, 
		const void* _default, 
		const void* _min, 
		const void* _max
		);

	void freeData();

}; // class AppProperty

////////////////////////////////////////////////////////////////////////////////
/// \class AppPropertyGroup
/// Sub container for AppProperty instances.
/// \note Ptrs returnd by operator[] are guaranteed to be persistent.
////////////////////////////////////////////////////////////////////////////////
class AppPropertyGroup: private apt::non_copyable<AppPropertyGroup>
{
public:

	AppPropertyGroup(const char* _name)
		: m_name(_name)
		, m_propKeys(16)
		, m_props(16)
	{
	}

	AppPropertyGroup(AppPropertyGroup&& _rhs);
	AppPropertyGroup& operator=(AppPropertyGroup&& _rhs);
	friend void swap(AppPropertyGroup& _a, AppPropertyGroup& _b);

	bool*  addBool  (const char* _name, const char* _displayName, bool         _default,                         bool _isHidden = false);
	int*   addInt   (const char* _name, const char* _displayName, int          _default, int _min,   int _max,   bool _isHidden = false);
	float* addFloat (const char* _name, const char* _displayName, float        _default, float _min, float _max, bool _isHidden = false);
	vec2*  addVec2  (const char* _name, const char* _displayName, const vec2&  _default, float _min, float _max, bool _isHidden = false);
	vec3*  addVec3  (const char* _name, const char* _displayName, const vec3&  _default, float _min, float _max, bool _isHidden = false);
	vec4*  addVec4  (const char* _name, const char* _displayName, const vec4&  _default, float _min, float _max, bool _isHidden = false);
	ivec2* addVec2i (const char* _name, const char* _displayName, const ivec2& _default, int _min,   int _max,   bool _isHidden = false);
	ivec3* addVec3i (const char* _name, const char* _displayName, const ivec3& _default, int _min,   int _max,   bool _isHidden = false);
	ivec4* addVec4i (const char* _name, const char* _displayName, const ivec4& _default, int _min,   int _max,   bool _isHidden = false);
	vec3*  addRgb   (const char* _name, const char* _displayName, const vec3&  _default,                         bool _isHidden = false);
	vec4*  addRgba  (const char* _name, const char* _displayName, const vec4&  _default,                         bool _isHidden = false);
	char*  addString(const char* _name, const char* _displayName, const char*  _default,                         bool _isHidden = false);
	char*  addPath  (const char* _name, const char* _displayName, const char*  _default,                         bool _isHidden = false);

	AppProperty&       operator[](const char* _name);
	const AppProperty& operator[](const char* _name) const;

	AppProperty&       operator[](int _i)                   { APT_ASSERT(_i <= getSize()); return m_props[_i]; }
	const AppProperty& operator[](int _i) const             { APT_ASSERT(_i <= getSize()); return m_props[_i]; }
	
	const char* getName() const                             { return m_name; }
	int         getSize() const                             { return (int)m_props.size(); }

	/// \return true if any value was changed.
	bool edit(bool _showHidden);

private:
	AppProperty::String                    m_name;
	apt::PersistentVector<apt::StringHash> m_propKeys;
	apt::PersistentVector<AppProperty>     m_props;

	AppProperty* add(
		const char*       _name,
		const char*       _displayName,
		AppProperty::Type _type, 
		uint8             _count,
		bool              _isHidden,
		const void*       _value, 
		const void*       _default, 
		const void*       _min, 
		const void*       _max
		);

	AppProperty* get(apt::StringHash _nameHash) const;

}; // class AppPropertyGroup


////////////////////////////////////////////////////////////////////////////////
/// \class AppProperties
////////////////////////////////////////////////////////////////////////////////
class AppProperties: private apt::non_copyable<AppProperties>
{
public:
	
	AppProperties()
		: m_groupKeys(16)
		, m_groups(16)
		, m_editShowHidden(false)
	{
	}

	AppPropertyGroup& addGroup(const char* _name);

	AppPropertyGroup&       operator[](const char* _name);
	const AppPropertyGroup& operator[](const char* _name) const;

	AppPropertyGroup&       operator[](int _i)              { APT_ASSERT(_i <= getSize()); return m_groups[_i]; }
	const AppPropertyGroup& operator[](int _i) const        { APT_ASSERT(_i <= getSize()); return m_groups[_i]; }
	
	int getSize() const                                     { return (int)m_groups.size(); }

	/// \return true if any value was changed.
	bool edit(const char* _title);

	/// Set values from command line arguments. Groups are ignored.
	void setValues(const apt::ArgList& _argList);

	/// Set values from an ini file. Ini sections map to groups; ini properties
	/// in the default section are ignored.
	void setValues(const apt::IniFile& _ini);

	/// Add groups/values to _ini_.
	void appendToIni(apt::IniFile& _ini_) const;

	void        save() const;
	void        load();
	const char* getIniPath() const            { return m_iniPath; }
	void        setIniPath(const char* _path) { m_iniPath.set(_path); }

private:
	AppProperty::String                     m_iniPath;
	apt::PersistentVector<apt::StringHash>  m_groupKeys;
	apt::PersistentVector<AppPropertyGroup> m_groups;
	bool m_editShowHidden;


	AppPropertyGroup* getGroup(apt::StringHash _nameHash) const;

}; // class AppProperties

} // namespace frm

#endif // frm_AppProperty_h
