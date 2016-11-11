#pragma once
#ifndef frm_Resource_h
#define frm_Resource_h

#include <frm/def.h>

#include <apt/String.h>

#include <vector>

namespace frm
{

////////////////////////////////////////////////////////////////////////////////
/// \class Resource
/// Manages a global list of instances of the deriving class. Resources have a
/// unique id and an optional name (e.g. for display purposes). By default the
/// id is a hash of the name but the two can be set independently.
/// Deriving classes must:
///   - Add an explicit instantiation to Resource.cpp.
///   - Implement Create(), Destroy(), reload().
///   - Set a unique id and optional name via one of the Resource ctors.
///   - Correctly set the resource state.
////////////////////////////////////////////////////////////////////////////////
template <typename tDerived>
class Resource: private apt::non_copyable<Resource<tDerived> >
{
public:
	typedef tDerived Derived;

	typedef uint64 Id;

	enum class State: uint8
	{
		kError = 0u, //< Resource failed to load.
		kUnloaded,   //< Resource was created but not loaded.
		kLoaded      //< Resource successfully loaded.
	};

	
	/// Increment the reference counter for _inst.
	static void Use(Derived* _inst);
	/// Decrement the reference counter for _inst, set the ptr to 0. Does not
	/// implicitly destroy the resource.
	static void Unuse(Derived*& _inst_);

	/// Search for the resource instance matching _id or _name. Note that names
	/// are not required to be unique.
	/// \return Ptr to the first matching instance, or 0 if no match was found.
	static Derived* Find(Id _id);
	static Derived* Find(const char* _name);

	/// Call reload() on all instances.
	/// \return true if all instances were successfully reloaded, false if any
	///    failed.
	static bool ReloadAll();

	/// \return _ith instance.
	static Derived* GetInstance(int _i) { APT_ASSERT(_i < GetInstanceCount()); return s_instances[_i]; }

	/// \return Number of instances.
	static int GetInstanceCount()       { return (int)s_instances.size(); }

	Id          getId() const                { return m_id; }
	const char* getName() const              { return m_name; }
	State       getState() const             { return m_state; }
	sint64      getRefCount() const          { return m_refs; }

	void        setName(const char* _name)   { setNamef(_name); }
	void        setNamef(const char* _fmt, ...);

protected:
	apt::String<32>  m_name;  //< Optional, doesn't need to be unique.

	static Id GetUniqueId();

	/// Resource id is a hash of _name, which should be unique.
	Resource(const char* _name);
	/// Separate id/name. _id should be unique, _name does not have to be unique.
	Resource(Id _id, const char* _name);
	/// Remove this instance from the global instance list.
	~Resource();

	void setState(State _state) { m_state = _state; }

private:
	static std::vector<Derived*> s_instances;
	State                        m_state;
	Id                           m_id;    //< Unique id, by default a hash of m_name
	sint64                       m_refs;  //< Reference counter.

	/// Common code for ctors.
	void init(Id _id, const char* _name);

}; // class Resource

} // namespace frm

#endif // frm_Resource_h
