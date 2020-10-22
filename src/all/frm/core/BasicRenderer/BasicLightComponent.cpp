#include "BasicLightComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(BasicLightComponent, 0);

// PUBLIC

void BasicLightComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("BasicLightComponent::Update");

	if (_phase != World::UpdatePhase::PostPhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		BasicLightComponent* component = (BasicLightComponent*)*_from;
	}
}

eastl::span<BasicLightComponent*> BasicLightComponent::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("BasicLightComponent"));
	return eastl::span<BasicLightComponent*>(*((eastl::vector<BasicLightComponent*>*)&activeList));
}

// PROTECTED

bool BasicLightComponent::editImpl()
{
	bool ret = false;

	ret |= ImGui::Combo("Type", &m_type, "Direct\0Point\0Spot\0");
	ret |= ImGui::ColorEdit3("Color", &m_colorBrightness.x);
	ret |= ImGui::DragFloat("Brightness", &m_colorBrightness.w, 0.1f);
	m_colorBrightness.w = Max(m_colorBrightness.w, 0.0f);
	if (m_type == Type_Point || m_type == Type_Spot)
	{
		ret |= ImGui::DragFloat("Radius", &m_radius, 0.25f);
	}
	if (m_type == Type_Spot)
	{
		ret |= ImGui::SliderFloat("Cone Inner Angle", &m_coneInnerAngle, 0.0f, 180.0f);
		ret |= ImGui::SliderFloat("Cone Outer Angle", &m_coneOuterAngle, 0.0f, 180.0f);
	}

	ImGui::Checkbox("Cast Shadows", &m_castShadows);

	// \todo draw light proxy
	Im3d::PushSize(4.0f);
	Im3d::PushMatrix(m_parentNode->getWorld());
		Im3d::DrawXyzAxes();
	Im3d::PopMatrix();
	Im3d::PopSize();

	return ret;
}

bool BasicLightComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	const char* kTypeNames[3] = { "Direct", "Point", "Spot" };
	SerializeEnum(_serializer_, m_type, kTypeNames, "m_type");
	Serialize(_serializer_, m_colorBrightness, "m_colorBrightness");
	Serialize(_serializer_, m_radius,          "m_radius");
	Serialize(_serializer_, m_coneInnerAngle,  "m_coneInnerAngle");
	Serialize(_serializer_, m_coneOuterAngle,  "m_coneOuterAngle");
	Serialize(_serializer_, m_castShadows,     "m_castShadows");
	return _serializer_.getError() == nullptr;
}



} // namespace frm