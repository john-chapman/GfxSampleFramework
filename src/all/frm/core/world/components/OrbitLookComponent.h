#pragma once

#include "Component.h"

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// OrbitLookComponent
//
// \todo 
// - Smooth motion.
// - Different modes for translation (XZ plane vs. view plane).
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(OrbitLookComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);

private:

	vec3  m_target         = vec3(0.0f);
	float m_azimuth        = 0.0f;
	float m_elevation      = 45.0f;
	float m_radius         = 10.0f;
	float m_orbitRate      = 1.0f;  // Degrees/pixel.
	float m_translateRate  = 0.01f; // Meters/pixel.

	vec3  m_position       = vec3(0.0f);

	void update(float _dt);
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
};

} // namespace frm
