#pragma once

#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// EnvironmentProbeComponent
//
// \todo
// - Transform relative to parent?
///////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(EnvironmentProbeComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<EnvironmentProbeComponent*> GetActiveComponents();

protected:
	
	vec3  m_origin     = vec3(0.0f);
	float m_radius     = 0.0f;
	vec3  m_boxExtents = vec3(2.0f);
	bool  m_dirty      = true;
	int   m_probeIndex = -1;

	bool initImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }

	friend class BasicRenderer;
};

} // namespace frm