#include "Component.h"

#include <frm/core/interpolation.h>
#include <frm/core/Input.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializer.h>
#include <frm/core/Serializable.inl>
#include <frm/core/world/WorldEditor.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

#include <EASTL/algorithm.h>

namespace frm {

FRM_FACTORY_DEFINE(Component);

// PUBLIC

Component* Component::Create(const ClassRef& _cref, SceneID _id)
{
	Component* ret = Factory<Component>::Create(&_cref);
	if (ret)
	{
		ret->m_id = _id;
	}

	return ret;
}

Component* Component::Create(StringHash _name, SceneID _id)
{
	Component* ret = Factory<Component>::Create(_name);
	if (ret)
	{
		ret->m_id = _id;
	}

	return ret;
}

void Component::Update(float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("Component::Update");

	for (auto& it : s_activeComponents)
	{
		if (it.second.empty())
		{
			continue;
		}

		(*s_updateFuncs)[it.first](it.second.begin(), it.second.end(), _dt, _phase);
	}
}

Component::RegisterUpdateFunc::RegisterUpdateFunc(UpdateFunc* _func, const char* _className)
{
	if_unlikely (!s_updateFuncs)
	{
		s_updateFuncs = new eastl::map<StringHash, Component::UpdateFunc*>();
	}

	classNameHash = StringHash(_className);
	FRM_ASSERT(s_updateFuncs->find(classNameHash) == s_updateFuncs->end()); // double registration?
	(*s_updateFuncs)[classNameHash] = _func;
}

Component::RegisterUpdateFunc::~RegisterUpdateFunc()
{
	auto it = s_updateFuncs->find(classNameHash);
	FRM_ASSERT(it != s_updateFuncs->end()); // not registered?
	s_updateFuncs->erase(it);

	if_unlikely (s_updateFuncs->empty())
	{
		delete s_updateFuncs;
		s_updateFuncs = nullptr;
	}
}

void Component::ClearActiveComponents()
{
	for (auto& it : s_activeComponents)
	{
		it.second.clear();
	}
}

const Component::ComponentList& Component::GetActiveComponents(StringHash _classNameHash)
{
	return s_activeComponents[_classNameHash];
}

bool Component::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);
	bool ret = editImpl();
	ImGui::PopID();
	Im3d::PopId();
	return ret;
}

bool Component::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= m_id.serialize(_serializer_);
	ret &= serializeImpl(_serializer_);
	return ret;
}

// PROTECTED

Component::~Component()
{
	FRM_ASSERT(m_state == World::State::Shutdown);
	m_state = World::State::Deleted;
}

// PRIVATE

void Component::setActive()
{
	const ClassRef* cref = getClassRef();
	const StringHash classNameHash = cref->getNameHash();

	ComponentList& activeComponents = s_activeComponents[classNameHash];
	FRM_STRICT_ASSERT(eastl::find(activeComponents.begin(), activeComponents.end(), this) == activeComponents.end()); // Double activation during an update?
	activeComponents.push_back(this);
}

eastl::map<StringHash, Component::ComponentList> Component::s_activeComponents;
eastl::map<StringHash, Component::UpdateFunc*>*  Component::s_updateFuncs;

} // namespace frm
