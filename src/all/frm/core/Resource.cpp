#include <frm/Resource.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/String.h>
#include <apt/Time.h>

#include <cstdarg> // va_list
#include <EASTL/algorithm.h>

using namespace frm;
using namespace apt;

// PUBLIC

template <typename tDerived>
void Resource<tDerived>::Use(Derived* _inst_)
{
	if (_inst_) {
		++(_inst_->m_refs);
		if (_inst_->m_refs == 1 && _inst_->m_state != State_Loaded) {
			_inst_->m_state = State_Error;
			if (_inst_->load()) {
				_inst_->m_state = State_Loaded;
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
	for (auto& inst : s_instances) {
		ret &= inst->reload();
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
	auto it = eastl::find(s_instances.begin(), s_instances.end(), (Derived*)this);
	s_instances.erase_unsorted(it);
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

template <typename tDerived> uint32 Resource<tDerived>::s_nextUniqueId;
template <typename tDerived> typename Resource<tDerived>::InstanceList Resource<tDerived>::s_instances;

template <typename tDerived>
void Resource<tDerived>::init(Id _id, const char* _name)
{
 // at this point an id collision is an error; reusing existing resources must happen prior to calling the Resource ctor
	APT_ASSERT_MSG(Find(_id) == 0, "Resource '%s' already exists", _name);

	m_state = State_Unloaded;
	m_id = _id;
	m_name.set(_name);
	m_refs = 0;
	s_instances.push_back((Derived*)this);
}

template <typename tDerived>
Resource<tDerived>::InstanceList::~InstanceList()
{
	if (size() != 0) {
		String<256> list;
		for (auto inst : (*this)) {
			list.appendf("\n\t'%s' -- %d refs", inst->getName(), inst->getRefCount());
		}
		list.append("\n");
		APT_LOG_ERR("Warning: %d %s instances were not released:%s", (int)size(), tDerived::s_className, (const char*)list);
	}
}


// Explicit template instantiations, resource release check
#define DECL_RESOURCE(_name) \
	template class Resource<_name>; \
	const char* Resource<_name>::s_className = #_name;

#include <frm/Mesh.h>
DECL_RESOURCE(Mesh);
#include <frm/SkeletonAnimation.h>
DECL_RESOURCE(SkeletonAnimation);
#include <frm/Shader.h>
DECL_RESOURCE(Shader);
#include <frm/Texture.h>
DECL_RESOURCE(Texture);
