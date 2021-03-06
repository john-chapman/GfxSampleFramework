#pragma once

#include <frm/core/frm.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Resource
// Manages a global list of instances of the deriving class. Resources have a
// unique id and an optional name (e.g. for display purposes). By default the
// id is a hash of the name but the two can be set independently.
//
// Resources are refcounted; calling Use() implicitly calls load() when the
// refcount is 1. Calling Release() implicitly calls Destroy() when the 
// refcount is 0.
//
// Deriving classes must:
//   - Add an explicit instantiation to Resource.cpp.
//   - Implement Create(), Destroy(), load(), reload().
//   - Set a unique id and optional name via one of the Resource ctors.
//   - Correctly set the resource state during load(), reload().
//
// \todo
// - Use of paths to generate resource IDs is generally flawed: the *resolved*
//   file path should be used, not the relative path passed to the Create() 
//   function.
// - Refactor: 
//   - Use dynamic binding rather than CRTP.
//   - Specific resource base class for resources backed by a disk file, with
//     extra helpers for doing UI etc.
////////////////////////////////////////////////////////////////////////////////
template <typename tDerived>
class Resource: private frm::non_copyable<Resource<tDerived> >
{
public:

	typedef tDerived Derived;
	typedef uint64   Id;

	enum State_
	{
		State_Error,       // Failed to load.
		State_Unloaded,    // Created but not loaded.
		State_Loaded,      // Successfully loaded.

		State_Count
	};
	typedef int State;
	
	// Increment the reference count for _inst, load if 1.
	static void     Use(Derived* _inst_);
	// Decrement the reference count for _inst_, destroy if 0.
	static void     Release(Derived*& _inst_);

	// Call reload() on all instances. Return true if *all* instances were successfully reloaded, false if any failed.
	static bool     ReloadAll();

	static bool     Load(Derived* _inst_)            { return _inst_->load(); }
	static bool     Reload(Derived* _inst_)          { return _inst_->reload(); }

	// \hack \todo Resource ptrs should ideally be const everywhere.
	static void     Use(const Derived* _inst_)       { Use(const_cast<Derived*>(_inst_)); }
	static void     Release(const Derived*& _inst_)  { Release(const_cast<Derived*&>(_inst_)); }
	static bool     Load(const Derived* _inst_)      { return Load(const_cast<Derived*>(_inst_)); }
	static bool     Reload(const Derived* _inst_)    { return Reload(const_cast<Derived*>(_inst_)); }

	static Derived* Find(Id _id);
	static Derived* Find(const char* _name);
	static int      GetInstanceCount()               { return (int)s_instances.size(); }
	static Derived* GetInstance(int _index)          { FRM_ASSERT(_index < GetInstanceCount()); return s_instances[_index]; }
	static const char* GetClassName()                { return s_className; }

	static bool     Select(Derived*& _resource_, const char* _buttonLabel, std::initializer_list<const char*> _fileExtensions);

	int             getIndex() const                 { return m_index; }
	Id              getId() const                    { return m_id; }
	const char*     getName() const                  { return (const char*)m_name; }
	State           getState() const                 { return m_state; }
	sint64          getRefCount() const              { return m_refs; }

	void            setName(const char* _name)       { setNamef(_name); }
	void            setNamef(const char* _fmt, ...);

	// \hack Dummy method Select().
	const char*     getPath() const                  { FRM_ASSERT(false); return ""; }

protected:

	static Id       GetUniqueId();
	static Id       GetHashId(const char* _str);
	
	typedef frm::String<32> NameStr;
	NameStr m_name;

	Resource(const char* _name);
	Resource(Id _id, const char* _name);
	~Resource();

	void setState(State _state) { m_state = _state; }

private:

	struct InstanceList: public eastl::vector<Derived*>
	{
		typedef eastl::vector<Derived*> BaseType;
		InstanceList(): BaseType() {}
		~InstanceList(); // dtor used to check resources were correctly released
	};
	static const char*  s_className;
	static InstanceList s_instances;
	static uint32       s_nextUniqueId;
	State               m_state;
	int                 m_index;
	Id                  m_id;
	sint64              m_refs;

	void init(Id _id, const char* _name);

}; // class Resource

template <typename tResourceType>
bool CheckResource(const tResourceType* _resource)
{
	return _resource && _resource->getState() != tResourceType::State_Error;
}

void ShowResourceViewer(bool* _open_);

} // namespace frm
