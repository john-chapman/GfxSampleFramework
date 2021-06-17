#include "EnvironmentProbeComponent.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializer.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(EnvironmentProbeComponent, 0);

// PUBLIC

void EnvironmentProbeComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("EnvironmentProbeComponent::Update");

	if (_phase != World::UpdatePhase::PostPhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		EnvironmentProbeComponent* component = (EnvironmentProbeComponent*)*_from;
	}
}

eastl::span<EnvironmentProbeComponent*> EnvironmentProbeComponent::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("EnvironmentProbeComponent"));
	return eastl::span<EnvironmentProbeComponent*>(*((eastl::vector<EnvironmentProbeComponent*>*)&activeList));
}

// PROTECTED

bool EnvironmentProbeComponent::initImpl()
{
	return true;
}

void EnvironmentProbeComponent::shutdownImpl()
{
}

bool EnvironmentProbeComponent::editImpl()
{
	bool ret = false;

	int isBox = m_radius == 0.0f ? 1 : 0;
	if (ImGui::Combo("Type", &isBox, "Sphere\0Box\0", 2))
	{
		if (isBox == 0)
		{
			m_radius = Max(1.0f, Max(m_boxExtents.x, Max(m_boxExtents.y, m_boxExtents.z)));
		}
		else
		{
			m_boxExtents = vec3(Max(1.0f, m_radius));
			m_radius = 0.0f;
		}

		ret = true;
	}

	Im3d::PushDrawState();
	Im3d::PushEnableSorting();
	Im3d::SetColor(Im3d::Color_Cyan);
	Im3d::SetSize(3.0f);

	if (isBox == 0)
	{
		ret |= ImGui::DragFloat("Radius", &m_radius, 0.1f, 0.1f);
		m_radius = Max(m_radius, 0.1f);

		Im3d::SetAlpha(0.2f);
		Im3d::DrawSphereFilled(m_origin, m_radius);
		Im3d::SetAlpha(1.0f);
		Im3d::DrawSphere(m_origin, m_radius);
	}
	else
	{
		ret |= ImGui::DragFloat3("Box Extents", &m_boxExtents.x, 0.1f, 0.1f);
		m_boxExtents = Max(m_boxExtents, vec3(0.1f));

		const vec3 halfBoxExtents = m_boxExtents / 2.0f;
		Im3d::SetAlpha(0.2f);
		Im3d::DrawAlignedBoxFilled(m_origin - halfBoxExtents, m_origin + halfBoxExtents);
		Im3d::SetAlpha(1.0f);
		Im3d::DrawAlignedBox(m_origin - halfBoxExtents, m_origin + halfBoxExtents);
	}

	Im3d::PopEnableSorting();
	Im3d::PopDrawState();

	ret |= Im3d::GizmoTranslation("EnvironmentProbeComponent::m_origin", &m_origin.x);
	ret |= ImGui::InputFloat3("Origin", &m_origin.x);

	if (ImGui::Button("Force Dirty"))
	{
		m_dirty = true;
	}

	ImGui::Text("Probe Index: %d", m_probeIndex);

	if (ret)
	{
		m_dirty = true;
	}

	return ret;
}

bool EnvironmentProbeComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	Serialize(_serializer_, m_origin,     "m_origin");
	Serialize(_serializer_, m_radius,     "m_radius");
	Serialize(_serializer_, m_boxExtents, "m_boxExtents");

	return _serializer_.getError() == nullptr;
}

} // namespace frm