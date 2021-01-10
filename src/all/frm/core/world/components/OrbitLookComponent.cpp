#include "OrbitLookComponent.h"

#include <frm/core/Input.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>

namespace frm {

FRM_COMPONENT_DEFINE(OrbitLookComponent, 0);

// PUBLIC

void OrbitLookComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("OrbitLookComponent::Update");

	if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		OrbitLookComponent* component = (OrbitLookComponent*)*_from;
		component->update(_dt);
	}
}

// PRIVATE

void OrbitLookComponent::update(float _dt)
{
	auto SetTargetOnXZPlane = [this](const vec3& _target)
	{
		Ray ray(m_position, Normalize(_target - m_position));
		Plane plane(vec3(0.0f, 1.0f, 0.0f), m_target);
		float t0;
		if (Intersect(ray, plane, t0))
		{
			m_target = ray.m_origin + ray.m_direction * t0;
		}
	};

	const mat4& localMatrix = getParentNode()->getLocal();

	const Keyboard* keyboard = Input::GetKeyboard();
	const Mouse*    mouse    = Input::GetMouse();
	const Gamepad*  gamepad  = Input::GetGamepad();
	if (keyboard && keyboard->isDown(Keyboard::Key_LCtrl)) // Disable keyboard input on lctrl.
	{
		keyboard = nullptr;
	}

	if (keyboard && mouse)
	{
		const vec2  mouseXY    = vec2(mouse->getAxisState(Mouse::Axis_X), mouse->getAxisState(Mouse::Axis_Y));
		const float mouseWheel = mouse->getAxisState(Mouse::Axis_Wheel);
		
		if (mouse->isDown(Mouse::Button_Right))
		{
			m_azimuth   += mouseXY.x * -m_orbitRate;
			m_elevation += mouseXY.y * -m_orbitRate;
			m_elevation  = Clamp(m_elevation, 1e-2f, 180.0f - 1e-2f); // Epsilon prevents the orientation popping at the poles.
		}

		if (Abs(mouseWheel) > 1e-2f)
		{
			#if 1
			// Zoom towards/away from m_target.
				m_radius += mouseWheel * -m_translateRate; 
				m_radius  = Max(m_radius, 1e-2f);
			#else
			// Zoom along the cursor ray: need to keep m_target in the center of the view, therefore compute the desired camera position, then move m_target to the nearest 
			// point on the ray formed by the view vector and the new position, then recompute the spherical coordinates.
				vec3 newPosition = m_position + _cursorRayW.m_direction * mouseWheel;
				Ray centerRay(newPosition, m_parentNode->getForward());
				SetTargetOnXZPlane(Nearest(centerRay, m_target));

				vec3 offset      = newPosition - m_target;
				vec3 spherical   = CartesianToSpherical(offset);
				m_radius         = spherical.x;
				m_azimuth        = spherical.y;
				m_elevation      = spherical.z;
			#endif
		}

		if (mouse->isDown(Mouse::Button_Middle))
		{
			const mat4& world = m_parentNode->getWorld();
			#if 0
			// For some use cases translating on the camera-aligned plane is confusing.
				SetTargetOnXZPlane(m_target
					+ vec3(world[0].xyz()) * -mouseXY.x * m_translateRate
					+ vec3(world[1].xyz()) *  mouseXY.y * m_translateRate
					);
			#else
				m_target += world[0].xyz() * -mouseXY.x * m_translateRate;
				m_target += world[1].xyz() *  mouseXY.y * m_translateRate;
			#endif
		}
	}

	if (gamepad)
	{
		// \todo
	}

	vec3 offset = SphericalToCartesian(m_radius, Radians(m_azimuth), Radians(m_elevation));
	m_position = m_target + offset;

	mat4 m = AlignZ(Normalize(offset));
	SetTranslation(m, m_position);

	getParentNode()->setLocal(m);
}

bool OrbitLookComponent::editImpl()
{
	bool ret = false;

	ret |= ImGui::DragFloat("Radius",    &m_radius,    1.0f, 1e-2f, 1000.0f);
	ret |= ImGui::DragFloat("Azimuth",   &m_azimuth,   1.0f, 0.0f,  360.0f);
	ret |= ImGui::DragFloat("Elevation", &m_elevation, 1.0f, 0.0f,  180.0f);

	ImGui::Spacing();
	
	ret |= ImGui::DragFloat3("Target", &m_target.x, 1.0f);

	ImGui::Spacing();

	ret |= ImGui::DragFloat("Orbit Rate",     &m_orbitRate,     0.1f, 1e-2f, 10.0f);
	ret |= ImGui::DragFloat("Translate Rate", &m_translateRate, 0.1f, 1e-2f, 10.0f);

	return ret;
}

bool OrbitLookComponent::serializeImpl(Serializer& _serializer_) 
{
	bool ret = SerializeAndValidateClass(_serializer_);

	ret &= Serialize(_serializer_, m_target,        "m_target");
	ret &= Serialize(_serializer_, m_azimuth,       "m_azimuth");
	ret &= Serialize(_serializer_, m_elevation,     "m_elevation");
	ret &= Serialize(_serializer_, m_radius,        "m_radius");
	ret &= Serialize(_serializer_, m_orbitRate,     "m_orbitRate");
	ret &= Serialize(_serializer_, m_translateRate, "m_translateRate");

	return ret;
}

} // namespace frm
