#pragma once

#include "Component.h"

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// FreeLookComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(FreeLookComponent)
{
public:

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);

		        
	void        lookAt(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.f, 1.f, 0.f));

private:

	vec3  m_position           = vec3(0.0f);                    // Current position.
	vec3  m_velocity           = vec3(0.0f);                    // Current velocity.
	quat  m_orientation        = quat(0.0f, 0.0f, 0.0f, 1.0f);  // Current orientation.
	float m_accelCount         = 0.0f;                          // Current ramp position in [0,m_accelTime].
	float m_speed              = 0.0f;                          // Current speed.

	vec3  m_pitchYawRoll       = vec3(0.0f);                    // Angular velocity in rads/second.
	float m_maxSpeed           = 10.0f;                         // Limit speed.
	float m_maxSpeedMul        = 5.0f;                          // Multiplies m_speed for boost.
	float m_accelTime          = 0.1f;                          // Acceleration ramp length in seconds.
	float m_rotationInputMul   = 0.1f;                          // Scale rotation inputs. \todo Should be relative to fov/screen size.
	float m_rotationDamping    = 2e-3f;                         // Adhoc rotation damping factor.

	void update(float _dt);
	bool postInitImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return false; }
};

} // namespace frm
