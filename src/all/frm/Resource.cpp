#include <frm/Resource.h>

#include <apt/String.h>
#include <apt/Time.h>
#include <apt/hash.h>

#include <cstdarg> // va_list
#include <utility>

using namespace frm;
using namespace apt;

// PUBLIC

template <typename tDerived>
void Resource<tDerived>::Use(Derived* _inst_)
{
	if (_inst_) {
		++(_inst_->m_refs);
		if (_inst_->m_refs == 1) {
			_inst_->m_state = kError;
			if (_inst_->load()) {
				_inst_->m_state = kLoaded;
			}
		}
	}
}

template <typename tDerived>
void Resource<tDerived>::Release(Derived*& _inst_)
{
	if (_inst_) {
		--(_inst_->m_refs);
		APT_ASSERT(_inst_->m_refs >= 0);
		if (_inst_->m_refs == 0) {
			Derived::Destroy(_inst_);
		}
		_inst_ = nullptr;
	}
}

template <typename tDerived>
bool Resource<tDerived>::ReloadAll()
{
	bool ret = true;
	for (auto it = s_instances.begin(); it != s_instances.end(); ++it) {
		ret &= (*it)->reload();
	}
	return ret;
}

template <typename tDerived>
tDerived* Resource<tDerived>::Find(Id _id)
{
	for (auto it = s_instances.begin(); it != s_instances.end(); ++it) {
		if ((*it)->m_id == _id) {
			return *it;
		}
	}
	return nullptr;
}

template <typename tDerived>
tDerived* Resource<tDerived>::Find(const char* _name)
{
	for (auto it = s_instances.begin(); it != s_instances.end(); ++it) {
		if ((*it)->m_name == _name) {
			return *it;
		}
	}
	return nullptr;
}


// PROTECTED

template <typename tDerived>
typename Resource<tDerived>::Id Resource<tDerived>::GetUniqueId()
{
	Id ret = s_nextUniqueId++;
	APT_ASSERT(!Find(ret));
	return ret;
}

template <typename tDerived>
typename Resource<tDerived>::Id Resource<tDerived>::GetHashId(const char* _str)
{
	return (Id)HashString<uint32>(_str) << 32;
}

template <typename tDerived>
Resource<tDerived>::Resource(const char* _name)
{
	init(GetHashId(_name), _name);
}

template <typename tDerived>
Resource<tDerived>::Resource(Id _id, const char* _name)
{
	init(_id, _name);
}

template <typename tDerived>
Resource<tDerived>::~Resource()
{
	APT_ASSERT(m_refs == 0); // resource still in use
	auto it = std::find(s_instances.begin(), s_instances.end(), (Derived*)this);
	APT_ASSERT(it != s_instances.end()); 
	std::swap(*it, s_instances.back());
	s_instances.pop_back();
}

template <typename tDerived>
void Resource<tDerived>::setNamef(const char* _fmt, ...)
{	
	va_list args;
	va_start(args, _fmt);
	m_name.setfv(_fmt, args);
	va_end(args);
}


// PRIVATE

template <typename tDerived>  uint32 Resource<tDerived>::s_nextUniqueId;
template <typename tDerived>  std::vector<tDerived*> Resource<tDerived>::s_instances;

template <typename tDerived>
void Resource<tDerived>::init(Id _id, const char* _name)
{
 // at this point an id collision is an error; reusing existing resources must happen
 // prior to calling the Resource ctor
	APT_ASSERT_MSG(Find(_id) == 0, "Resource '%s' already exists", _name);

	m_state = State::kUnloaded;
	m_id = _id;
	m_name.set(_name);
	m_refs = 0;
	s_instances.push_back((Derived*)this);
}



// Explicit template instantiations
#include <frm/Mesh.h>
template class Resource<Mesh>;
#include <frm/Shader.h>
template class Resource<Shader>;
#include <frm/Texture.h>
template class Resource<Texture>;