#include <frm/XForm.h>

#include <frm/Scene.h>

#include <imgui/imgui.h>
#include <frm/im3d.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  XForm

*******************************************************************************/

APT_FACTORY_DEFINE(XForm);

/*******************************************************************************

                        PositionOrientationScaleXForm

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(XForm, PositionOrientationScaleXForm);

PositionOrientationScaleXForm::PositionOrientationScaleXForm()
: m_position(0.0f)
	, m_orientation(1.0f, 0.0f, 0.0f, 0.0f)
	, m_scale(1.0f)
{
};

void PositionOrientationScaleXForm::apply(Node* _node_, float _dt)
{
	mat4 mat = scale(translate(mat4(1.0f), m_position) * mat4_cast(m_orientation), m_scale);
	_node_->setWorldMatrix(_node_->getWorldMatrix() * mat);
}

void PositionOrientationScaleXForm::edit()
{
	vec3 eul = eulerAngles(m_orientation);
	ImGui::DragFloat3("Position", &m_position.x, 0.5f);
	if (ImGui::DragFloat3("Orientation", &eul.x, 0.1f)) {
		m_orientation = quat(eul);
	}
	ImGui::DragFloat3("Scale", &m_scale.x, 0.2f);
}

/*******************************************************************************

                                FreeCameraXForm

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(XForm, FreeCameraXForm);

FreeCameraXForm::FreeCameraXForm()
	: m_position(0.0f)
	, m_velocity(0.0f)
	, m_accelTime(0.2f)
	, m_accelCount(0.0f)
	, m_maxSpeed(10.0f)
	, m_maxSpeedMul(5.0f)
	, m_orientation(1.0f, 0.0f, 0.0f, 0.0f)
	, m_pitchYawRoll(0.0f)
	, m_rotationInputMul(0.1f)
	, m_rotationDamp(0.0002f)
{
}

void FreeCameraXForm::apply(Node* _node_, float _dt)
{
 	if (!_node_->isSelected()) {
		return;
	}

	const mat4& localMatrix = _node_->getLocalMatrix();

	const Gamepad* gpad = Input::GetGamepad();
	const Keyboard* keyb = Input::GetKeyboard();

	bool isAccel = false;		
	vec3 dir = vec3(0.0);		
	if (gpad) {			
		float x = gpad->getAxisState(Gamepad::kLeftStickX);
		dir += vec3(column(localMatrix, 0)) * x;
		float y = gpad->getAxisState(Gamepad::kLeftStickY);
		dir += vec3(column(localMatrix, 2)) * y;
		if (gpad->isDown(Gamepad::kLeft1)) {
			dir -= vec3(column(localMatrix, 1));
		}
		if (gpad->isDown(Gamepad::kRight1)) {
			dir += vec3(column(localMatrix, 1));
		}
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kW)) {
		dir -= vec3(column(localMatrix, 2));
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kA)) {
		dir -= vec3(column(localMatrix, 0));
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kS)) {
		dir += vec3(column(localMatrix, 2));
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kD)) {
		dir += vec3(column(localMatrix, 0));
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kQ)) {
		dir -= vec3(column(localMatrix, 1));
		isAccel = true;
	}
	if (keyb->isDown(Keyboard::kE)) {
		dir += vec3(column(localMatrix, 1));
		isAccel = true;
	}
	if (isAccel) {
	 // if we're accelerating, zero the velocity here to allow instantaneous direction changes
		m_velocity = vec3(0.0f);
	}
	m_velocity += dir;
			
	m_accelCount += isAccel ? _dt : -_dt;
	m_accelCount = apt::clamp(m_accelCount, 0.0f, m_accelTime);
	m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
	if (gpad) {
		m_speed *= 1.0f + m_maxSpeedMul * gpad->getAxisState(Gamepad::kRightTrigger);
	}
	if (keyb->isDown(Keyboard::kLShift)) {
		m_speed *= m_maxSpeedMul;
	}
	float len = apt::length(m_velocity);
	if (len > 0.0f) {
		m_velocity = (m_velocity / len) * m_speed;
	}		
	m_position += m_velocity * _dt;


	Mouse* mouse = Input::GetMouse();
	if (gpad) {
		m_pitchYawRoll.x -= gpad->getAxisState(Gamepad::kRightStickY) * 16.0f * _dt;//* m_rotationInputMul * 6.0f; // \todo setter for this?
		m_pitchYawRoll.y -= gpad->getAxisState(Gamepad::kRightStickX) * 16.0f * _dt;//* m_rotationInputMul * 6.0f;
	}
	if (mouse->isDown(Mouse::kRight)) {
		m_pitchYawRoll.x -= mouse->getAxisState(Mouse::kY) * m_rotationInputMul;
		m_pitchYawRoll.y -= mouse->getAxisState(Mouse::kX) * m_rotationInputMul;
	}
	quat qpitch     = angleAxis(m_pitchYawRoll.x * _dt, vec3(column(localMatrix, 0)));
	quat qyaw       = angleAxis(m_pitchYawRoll.y * _dt, vec3(0.0f, 1.0f, 0.0f));
	quat qroll      = angleAxis(m_pitchYawRoll.z * _dt, vec3(column(localMatrix, 2)));
	m_orientation   = qyaw * qpitch * qroll * m_orientation;
	m_pitchYawRoll *= powf(m_rotationDamp, _dt);

	_node_->setLocalMatrix(translate(mat4(1.0f), m_position) * mat4_cast(m_orientation));
	
}

void FreeCameraXForm::edit()
{
	ImGui::Text("Position:       %1.3f,%1.3f,%1.3f",       m_position.x, m_position.y, m_position.z);
	ImGui::Text("Speed:          %1.3f",                   m_speed);
	ImGui::Text("Accel:          %1.3f",                   m_accelCount);
	ImGui::Text("Velocity:       %1.3f,%1.3f,%1.3f",       m_velocity.x, m_velocity.y, m_velocity.z);
	ImGui::Spacing();
	ImGui::Text("Orientation:    %1.3f,%1.3f,%1.3f,%1.3f", m_orientation.w, m_orientation.x, m_orientation.y, m_orientation.z);
	ImGui::Text("Pitch/Yaw/Roll: %1.3f,%1.3f,%1.3f",       m_pitchYawRoll.x, m_pitchYawRoll.y, m_pitchYawRoll.z);
	ImGui::Spacing();
	ImGui::SliderFloat("Max Speed",           &m_maxSpeed,         0.0f,  500.0f);
	ImGui::SliderFloat("Max Speed Mul",       &m_maxSpeedMul,      0.0f,  100.0f);
	ImGui::SliderFloat("Accel Ramp",          &m_accelTime,        1e-4f, 2.0f);
	ImGui::Spacing();
	ImGui::SliderFloat("Rotation Input Mul",  &m_rotationInputMul, 1e-4f, 0.2f, "%1.5f");
	ImGui::SliderFloat("Rotation Damp",       &m_rotationDamp,     1e-4f, 0.2f, "%1.5f");
}


/*******************************************************************************

                                 LookAtXForm

*******************************************************************************/

APT_FACTORY_REGISTER_DEFAULT(XForm, LookAtXForm);

LookAtXForm::LookAtXForm()
	: m_target(nullptr)
	, m_offset(0.0f)
{
}

void LookAtXForm::apply(Node* _node_, float _dt)
{
	vec3 posW = GetTranslation(_node_->getWorldMatrix());
	vec3 targetW = m_offset;
	if (m_target) {
		targetW += GetTranslation(_node_->getWorldMatrix());
	}
	_node_->setWorldMatrix(GetLookAtMatrix(posW, targetW));
}

void LookAtXForm::edit()
{
	ImGui::DragFloat3("Offset", &m_offset.x, 0.5f);
}