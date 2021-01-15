#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>

#include <eastl/vector.h>
#include <eastl/vector_map.h>

namespace frm {

class Property;

///////////////////////////////////////////////////////////////////////////////
// Properties
// Simple property system. Use for application configs etc.
// - Instances of the Properties class are containers of named groups, which
//   are containers of named properties. Properties are therefore uniquely
//   identified by the name and group name.
// - Loading properties is order independent wrt the code which initializes
//   the properties, i.e. don't require the properties to be init before loading
//   or vice-versa. This is achieved by loading *everything* which is in the disk
//   file and then setting the value when the property is added from the code.
// 
// \todo
// - The system potentially does a lot of small allocations, all of which persist
//   for the lifetime of the object. A simple linear allocator would work well.
// - Property paths e.g. "Group0/Group1/Group2/PropertyName".
// - In theory, external storage should be optional, however due to the limits of
//   the Json implementation int and float types are stored as doubles in the
//   internal storage.
///////////////////////////////////////////////////////////////////////////////
class Properties: private non_copyable<Properties>
{
public:

	typedef bool (EditFunc)(Property& _prop_);  // return true if the value changed
	typedef void (DisplayFunc)(const Property& _prop);
		
	enum Type_
	{
		Type_Bool,
		Type_Int,
		Type_Float,
		Type_String,

		Type_Count
	};
	typedef int Type;

	static const char* GetTypeStr(Type _type);
	static int         GetTypeSizeBytes(Type _type);
	template <typename T>
	static Type        GetType();
	template <typename T>
	static int         GetCount();

	static bool        DefaultEditFunc(Property& _prop_);
	static bool        ColorEditFunc(Property& _prop_);
	static bool        PathEditFunc(Property& _prop_);
	static void        DefaultDisplayFunc(const Property& _prop);
	static void        ColorDisplayFunc(const Property& _prop);

	static Properties* GetDefault();
	static Properties* GetCurrent();

	// Add a new property to the current group. If _storage is 0, memory is allocated internally. If the property already exists it is updated with the new metadata. 
	template <typename T>
	static Property*   Add(const char* _name, const T& _default, const T& _min, const T& _max, T* _storage = nullptr, const char* _displayName = nullptr) { return GetCurrent()->add<T>(_name, _default, _min, _max, _storage, _displayName); }
	template <typename T>
	static Property*   Add(const char* _name, const T& _default, T* _storage = nullptr, const char* _displayName = nullptr)                               { return GetCurrent()->add<T>(_name, _default, _storage, _displayName); }
	static Property*   AddColor(const char* _name, const vec3& _default, vec3* _storage = nullptr, const char* _displayName = nullptr);
	static Property*   AddColor(const char* _name, const vec4& _default, vec4* _storage = nullptr, const char* _displayName = nullptr);
	static Property*   AddPath(const char* _name, const PathStr& _default, PathStr* _storage = nullptr, const char* _displayName = nullptr);

	// Find an existing property. If _groupName is 0, search the current group first.
	static Property*   Find(const char* _propName, const char* _groupName = nullptr);

	// Push/pop the current group. If _group doesn't exist, a new empty group is created.
	static Properties* PushGroup(const char* _groupName);
	static void        PopGroup(int _count = 1);

	// Find a property as per Find(), invalidate the external storage ptr. Call this e.g. in the dtor of a class which owns the storage, this is important to allow properties to be correctly serialized.
	static void        InvalidateStorage(const char* _propName, const char* _groupName = nullptr);

	// Call InvalidateStorage() for all members of a group.
	static void        InvalidateGroup(const char* _groupName);
	
	static Properties* Create(const char* _groupName);
	static void        Destroy(Properties*& _properties_);

	friend bool        Serialize(SerializerJson& _serializer_, Properties& _group_);
	
	// Return true if any property or subgroup was modified.
	bool               edit(const char* _filter = nullptr);

	void               display(const char* _filter = nullptr);

	void               invalidate();

private:
	static eastl::vector<Properties*> s_groupStack;

	String<32>                                 m_name = "";
	eastl::vector_map<StringHash, Properties*> m_subGroups;
	eastl::vector_map<StringHash, Property*>   m_properties;
	
	Properties(const char* _name);
	~Properties();

	static StringHash GetStringHash(const char* _str);
	
	Property* findOrAdd(const char* _name, Type _type, int _count);

	Property* add(const char* _name, Type _type, int _count, const void* _default, const void* _min, const void* _max, void* _storage, const char* _displayName);

	template <typename T>
	Property* add(const char* _name, const T& _default, const T& _min, const T& _max, T* _storage, const char* _displayName)
	{
		return add(_name, GetType<T>(), FRM_TRAITS_COUNT(T), &_default, &_min, &_max, _storage, _displayName);
	}
	template <typename T>
	Property* add(const char* _name, const T& _default, T* _storage, const char* _displayName)
	{
		return add(_name, GetType<T>(), FRM_TRAITS_COUNT(T), &_default, nullptr, nullptr, _storage, _displayName);
	}
		template <>
		Property* add<bool>(const char* _name, const bool& _default, bool* _storage, const char* _displayName)
		{
			return add(_name, Type_Bool, 1, &_default, nullptr, nullptr, _storage, _displayName);
		}

		template <>
		Property* add<StringBase>(const char* _name, const StringBase& _default, StringBase* _storage, const char* _displayName)
		{
			return add(_name, Type_String, 1, &_default, nullptr, nullptr, _storage, _displayName);
		}

	// Search this group for _propName. Return 0 if not found.
	Property*   find(const char* _propName);

	// Recursively search for _groupName. Return 0 if not found.
	Properties* findGroup(const char* _groupName);
};

///////////////////////////////////////////////////////////////////////////////
// Property
// m_storageInternal stores numeric types (Type_Int, TypeFloat) as doubles -
// this is due to the weak numeric typing in Json.
///////////////////////////////////////////////////////////////////////////////
class Property
{
public:
	using Type        = Properties::Type;
	using EditFunc    = Properties::EditFunc;
	using DisplayFunc = Properties::DisplayFunc;

	void          reset();

	template <typename tType>
	tType*        get(int i = 0)                                { /*FRM_ASSERT(sizeof(tType) * m_count == getSizeBytes());*/ FRM_ASSERT(i < m_count); return (tType*)m_storageExternal + i; }

	const char*   getName() const                               { return m_name.c_str(); }
	void          setName(const char* _name)                    { m_name.set(_name); }

	const char*   getDisplayName() const                        { return m_displayName.c_str(); }
	void          setDisplayName(const char* _displayName)      { m_displayName.set(_displayName); }

	EditFunc*     getEditFunc() const                           { return m_editFunc; }
	void          setEditFunc(EditFunc* _editFunc)              { m_editFunc = _editFunc; }

	DisplayFunc*  getDisplayFunc() const                        { return m_displayFunc; }
	void          setDisplayFunc(DisplayFunc* _displayFunc)     { m_displayFunc = _displayFunc; }

	void*         getDefault()                                  { return m_default; }
	void          setDefault(void* _default);
	bool          isDefault() const;

	void*         getMin()                                      { return m_min; }
	void          setMin(void* _min);

	void*         getMax()                                      { return m_min; }
	void          setMax(void* _max);

	Type          getType() const                               { return m_type; }
	int           getCount() const                              { return m_count; }
	int           getSizeBytes() const;
	void*         getInternalStorage() const                    { return m_storageInternal; }

	// Setting the external storage ptr to a none-null value will copy the value from internal -> external.
	// Setting the external storage ptr to null will copy external -> internal (invalidation).
	void*         getExternalStorage() const                    { return m_storageExternal; }
	void          setExternalStorage(void* _storage_);

	// Return external storage ptr if not 0, else internal storage ptr.
	void*         getStorage() const                            { return m_storageExternal ? m_storageExternal : m_storageInternal; }

	// Return true if the property was modified.
	bool          edit();

	void          display();

private:

	Property() = default;
	~Property();

	// Minimal init (called during serialization).
	void init(
		const char* _name,
		Type        _type,
		int         _count,
		void*       _value // copied to internal storage
		);

	// Full init (called by the code).
	void init(
		const char* _name,
		const char* _displayName,
		Type        _type,
		int         _count,
		void*       _storageExternal,
		const void* _default,
		const void* _min,
		const void* _max
		);

	void alloc();

	void shutdown();

	EditFunc*       m_editFunc        = Properties::DefaultEditFunc;
	DisplayFunc*    m_displayFunc     = Properties::DefaultDisplayFunc;

	String<32>      m_name            = "";
	String<32>      m_displayName     = "";
	Type            m_type            = Properties::Type_Count;
	int             m_count           = 0;
	void*           m_storageExternal = nullptr;
	void*           m_storageInternal = nullptr;
	void*           m_default         = nullptr;
	void*           m_min             = nullptr;
	void*           m_max             = nullptr;
	bool            m_setFromCode     = false;   // Whether this property was set from code, i.e. whether it should be written during serialization.
	bool            m_ownsStorage     = false;   // Whether m_storageExternal should be deleted by the property.

	void copy(void* dst_, const void* _src);
	bool compare(const void* _a, const void* _b) const;

	friend class Properties;
	friend bool Serialize(SerializerJson& _serializer_, Properties& _group_);
};

} // namespace frm
