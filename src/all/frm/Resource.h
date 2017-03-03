#pragma once
#ifndef frm_Resource_h
#define frm_Resource_h

#include <frm/def.h>
#include <apt/String.h>
#include <vector>

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
//   - Correctly set the resource state during load()/reload().
////////////////////////////////////////////////////////////////////////////////
template <typename tDerived>
class Resource: private apt::non_copyable<Resource<tDerived> >
{
public:
	typedef tDerived Derived;
	typedef uint64   Id;

	enum State
	{
		State_Error,      // failed to load
		State_Unloaded,   // created but not loaded
		State_Loaded      // successfully loaded
	};
	
	// Increment the reference count for _inst, load if 1.
	static void     Use(Derived* _inst_);
	// Decrement the reference count for _inst_, destroy if 0.
	static void     Release(Derived*& _inst_);

	// Call reload() on all instances. Return true if *all* instances were successfully reloaded, false if any failed.
	static bool     ReloadAll();

	static Derived* Find(Id _id);
	static Derived* Find(const char* _name);
	static int      GetInstanceCount()               { return (int)s_instances.size(); }
	static Derived* GetInstance(int _i)              { APT_ASSERT(_i < GetInstanceCount()); return s_instances[_i]; }

	Id              getId() const                    { return m_id; }
	const char*     getName() const                  { return m_name; }
	State           getState() const                 { return m_state; }
	sint64          getRefCount() const              { return m_refs; }

	void            setName(const char* _name)       { setNamef(_name); }
	void            setNamef(const char* _fmt, ...);

protected:
	static Id       GetUniqueId();
	static Id       GetHashId(const char* _str);

	apt::String<32> m_name;

	Resource(const char* _name);
	Resource(Id _id, const char* _name);
	~Resource();

	void setState(State _state) { m_state = _state; }

private:
	static uint32                s_nextUniqueId;
	static std::vector<Derived*> s_instances;
	State                        m_state;
	Id                           m_id;
	sint64                       m_refs;

	void init(Id _id, const char* _name);

}; // class Resource

} // namespace frm

#endif // frm_Resource_h
