#include "FreeLookComponent.h"

#include <frm/core/Input.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>

namespace frm {

FRM_COMPONENT_DEFINE(FreeLookComponent, 0);

// PUBLIC

void FreeLookComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("FreeLookComponent::Update");

	if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		FreeLookComponent* component = (FreeLookComponent*)*_from;
		component->update(_dt);
	}
}

void FreeLookComponent::lookAt(const vec3& _from, const vec3& _to, const vec3& _up)
{
	mat4 m = LookAt(_to, _from, _up); // \hack \todo Camera Z projection is flipped, need to invert _to, _from.
	m_position = _from;
	m_orientation = RotationQuaternion(mat3(m));
}

// PRIVATE

void FreeLookComponent::update(float _dt)
{
	const mat4& localMatrix = getParentNode()->getLocal();

	const Gamepad*  gamePad = Input::GetGamepad();
	const Keyboard* keyboard = Input::GetKeyboard();
	if (keyboard && keyboard->isDown(Keyboard::Key_LCtrl)) // disable keyboard input on lctrl
	{
		keyboard = nullptr;
	}

	bool isAccel = false;
	vec3 dir = vec3(0.0);		
	if (gamePad)
	{			
		float x = gamePad->getAxisState(Gamepad::Axis_LeftStickX);
		float y = gamePad->getAxisState(Gamepad::Axis_LeftStickY);
		float z = gamePad->isDown(Gamepad::Button_Right1) ? 1.0f : (gamePad->isDown(Gamepad::Button_Left1) ? -1.0f : 0.0f);
		dir += localMatrix[0].xyz() * x;
		dir += localMatrix[2].xyz() * y;
		dir += localMatrix[1].xyz() * z;
		isAccel = abs(x + y + z) > 0.0f;
	}
	if (keyboard)
	{
		if (keyboard->isDown(Keyboard::Key_W))
		{
			dir -= localMatrix[2].xyz();
			isAccel = true;
		}
		if (keyboard->isDown(Keyboard::Key_A))
		{
			dir -= localMatrix[0].xyz();
			isAccel = true;
		}
		if (keyboard->isDown(Keyboard::Key_S))
		{
			dir += localMatrix[2].xyz();
			isAccel = true;
		}
		if (keyboard->isDown(Keyboard::Key_D))
		{
			dir += localMatrix[0].xyz();
			isAccel = true;
		}
		if (keyboard->isDown(Keyboard::Key_Q))
		{
			dir -= localMatrix[1].xyz();
			isAccel = true;
		}
		if (keyboard->isDown(Keyboard::Key_E))
		{
			dir += localMatrix[1].xyz();
			isAccel = true;
		}
	}
	if (isAccel)
	{
	 // if accelerating, zero the velocity here to allow instantaneous direction changes
		m_velocity = vec3(0.0f);
	}
	m_velocity += dir;
			
	m_accelCount += isAccel ? _dt : -_dt;
	m_accelCount = FRM_CLAMP(m_accelCount, 0.0f, m_accelTime);
	m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
	if (gamePad)
	{
		m_speed *= 1.0f + m_maxSpeedMul * gamePad->getAxisState(Gamepad::Axis_RightTrigger);
	}
	if (keyboard && keyboard->isDown(Keyboard::Key_LShift))
	{
		m_speed *= m_maxSpeedMul;
	}
	float len2 = Length2(m_velocity);
	if (len2 > 0.0f)
	{
		m_velocity = (m_velocity / sqrt(len2)) * m_speed;
	}		
	m_position += m_velocity * _dt;


	Mouse* mouse = Input::GetMouse();
	if (gamePad)
	{
		m_pitchYawRoll.x -= gamePad->getAxisState(Gamepad::Axis_RightStickY) * 16.0f * _dt;//* m_rotationInputMul * 6.0f; // \todo setter for this?
		m_pitchYawRoll.y -= gamePad->getAxisState(Gamepad::Axis_RightStickX) * 16.0f * _dt;//* m_rotationInputMul * 6.0f;
	}
	if (mouse->isDown(Mouse::Button_Right))
	{
		m_pitchYawRoll.x -= mouse->getAxisState(Mouse::Axis_Y) * m_rotationInputMul;
		m_pitchYawRoll.y -= mouse->getAxisState(Mouse::Axis_X) * m_rotationInputMul;
	}
	quat qpitch     = RotationQuaternion(localMatrix[0].xyz(),   m_pitchYawRoll.x * _dt);
	quat qyaw       = RotationQuaternion(vec3(0.0f, 1.0f, 0.0f), m_pitchYawRoll.y * _dt);
	quat qroll      = RotationQuaternion(localMatrix[2].xyz(),   m_pitchYawRoll.z * _dt);
	m_orientation   = qmul(qmul(qmul(qyaw, qpitch), qroll), m_orientation);
	m_pitchYawRoll *= powf(m_rotationDamping, _dt);

	getParentNode()->setLocal(TransformationMatrix(m_position, m_orientation));
}

bool FreeLookComponent::editImpl()
{
	return false;
}

bool FreeLookComponent::serializeImpl(Serializer& _serializer_) 
{
	bool ret = SerializeAndValidateClass(_serializer_);

	ret &= Serialize(_serializer_, m_position,          "m_position");
	ret &= Serialize(_serializer_, m_orientation,       "m_orientation");
	ret &= Serialize(_serializer_, m_maxSpeed,          "m_maxSpeed");
	ret &= Serialize(_serializer_, m_maxSpeedMul,       "m_maxSpeedMul");
	ret &= Serialize(_serializer_, m_accelTime,         "m_accelTime");
	ret &= Serialize(_serializer_, m_rotationInputMul,  "m_rotationInputMul");
	ret &= Serialize(_serializer_, m_rotationDamping,   "m_rotationDamping");

	return ret;
}


} // namespace frm