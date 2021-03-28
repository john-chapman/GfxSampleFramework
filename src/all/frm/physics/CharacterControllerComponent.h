#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/world/components/Component.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// CharacterControllerComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(CharacterControllerComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);

private:

	float m_heading  = 0.0f;
	float m_radius   = 0.5f;
	float m_height   = 1.5f;
	void* m_impl     = nullptr;

	void update(float _dt);

	bool initImpl() override;
	bool postInitImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }
};

} // namespace frm
