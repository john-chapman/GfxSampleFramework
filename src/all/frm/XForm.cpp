#include <frm/XForm.h>

#include <frm/interpolation.h>
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

                        XForm_PositionOrientationScale

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_PositionOrientationScale);

XForm_PositionOrientationScale::XForm_PositionOrientationScale()
	: m_position(0.0f)
	, m_orientation(1.0f, 0.0f, 0.0f, 0.0f)
	, m_scale(1.0f)
{
};

void XForm_PositionOrientationScale::apply(float _dt)
{
	mat4 mat = scale(translate(mat4(1.0f), m_position) * mat4_cast(m_orientation), m_scale);
	m_node->setWorldMatrix(m_node->getWorldMatrix() * mat);
}

void XForm_PositionOrientationScale::edit()
{
	vec3 eul = eulerAngles(m_orientation);
	ImGui::DragFloat3("Position", &m_position.x, 0.5f);
	if (ImGui::DragFloat3("Orientation", &eul.x, 0.1f)) {
		m_orientation = quat(eul);
	}
	ImGui::DragFloat3("Scale", &m_scale.x, 0.2f);

	Im3d::Gizmo("XForm_PositionOrientationScale", &m_position, &m_orientation, &m_scale);
}

/*******************************************************************************

                                XForm_FreeCamera

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_FreeCamera);

XForm_FreeCamera::XForm_FreeCamera()
	: m_position(0.0f)
	, m_velocity(0.0f)
	, m_speed(0.0f)
	, m_maxSpeed(10.0f)
	, m_maxSpeedMul(5.0f)
	, m_accelTime(0.1f)
	, m_accelCount(0.0f)
	, m_orientation(1.0f, 0.0f, 0.0f, 0.0f)
	, m_pitchYawRoll(0.0f)
	, m_rotationInputMul(0.1f)
	, m_rotationDamp(0.0002f)
{
}

void XForm_FreeCamera::apply(float _dt)
{
 	if (!m_node->isSelected()) {
		return;
	}

	const mat4& localMatrix = m_node->getLocalMatrix();

	const Gamepad* gpad = Input::GetGamepad();
	const Keyboard* keyb = Input::GetKeyboard();

	bool isAccel = false;
	vec3 dir = vec3(0.0);		
	if (gpad) {			
		float x = gpad->getAxisState(Gamepad::kLeftStickX);
		float y = gpad->getAxisState(Gamepad::kLeftStickY);
		float z = gpad->isDown(Gamepad::kRight1) ? 1.0f : (gpad->isDown(Gamepad::kLeft1) ? -1.0f : 0.0f);
		dir += vec3(column(localMatrix, 0)) * x;
		dir += vec3(column(localMatrix, 2)) * y;
		dir += vec3(column(localMatrix, 1)) * z;
		isAccel = abs(x + y + z) > 0.0f;
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
	 // if accelerating, zero the velocity here to allow instantaneous direction changes
		m_velocity = vec3(0.0f);
	}
	m_velocity += dir;
			
	m_accelCount += isAccel ? _dt : -_dt;
	m_accelCount = clamp(m_accelCount, 0.0f, m_accelTime);
	m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
	if (gpad) {
		m_speed *= 1.0f + m_maxSpeedMul * gpad->getAxisState(Gamepad::kRightTrigger);
	}
	if (keyb->isDown(Keyboard::kLShift)) {
		m_speed *= m_maxSpeedMul;
	}
	float len2 = apt::length2(m_velocity);
	if (len2 > 0.0f) {
		m_velocity = (m_velocity / sqrt(len2)) * m_speed;
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

	m_node->setLocalMatrix(translate(mat4(1.0f), m_position) * mat4_cast(m_orientation));
	
}

void XForm_FreeCamera::edit()
{
	ImGui::Text("Position:       %1.3f,%1.3f,%1.3f",       m_position.x, m_position.y, m_position.z);
	ImGui::Text("Speed:          %1.3f",                   m_speed);
	ImGui::Text("Accel:          %1.3f",                   m_accelCount);
	ImGui::Text("Velocity:       %1.3f,%1.3f,%1.3f",       m_velocity.x, m_velocity.y, m_velocity.z);
	ImGui::Spacing();
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

                                 XForm_LookAt

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_LookAt);

XForm_LookAt::XForm_LookAt()
	: m_target(nullptr)
	, m_offset(0.0f)
{
}

void XForm_LookAt::apply(float _dt)
{
	vec3 posW = GetTranslation(m_node->getWorldMatrix());
	vec3 targetW = m_offset;
	if (m_target) {
		targetW += GetTranslation(m_node->getWorldMatrix());
	}
	m_node->setWorldMatrix(GetLookAtMatrix(posW, targetW));
}

void XForm_LookAt::edit()
{
	ImGui::DragFloat3("Offset", &m_offset.x, 0.5f);
}


/*******************************************************************************

                                XForm_Spin

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_Spin);

XForm_Spin::XForm_Spin()
	: m_axis(0.0f, 0.0f, 1.0f)
	, m_rate(pi<float>())
	, m_rotation(0.0f)
{
}

XForm_Spin::~XForm_Spin()
{
}

void XForm_Spin::apply(float _dt)
{
	m_rotation += m_rate * _dt;
	m_node->setWorldMatrix(rotate(m_node->getWorldMatrix(), m_rotation, m_axis));
}

void XForm_Spin::edit()
{
	ImGui::SliderFloat("Rate (radians/s)", &m_rate, -8.0f, 8.0f);
	ImGui::SliderFloat3("Axis", &m_axis.x, -1.0f, 1.0f);
	m_axis = normalize(m_axis);

	Im3d::PushDrawState();
		Im3d::SetColor(Im3d::kColorYellow);
		Im3d::SetAlpha(1.0f);
		Im3d::SetSize(2.0f);
		Im3d::BeginLines();
			vec3 p = GetTranslation(m_node->getWorldMatrix());
			Im3d::Vertex(p - m_axis * 9999.0f);
			Im3d::Vertex(p + m_axis * 9999.0f);
		Im3d::End();
	Im3d::PopDrawState();
}


/*******************************************************************************

                              XForm_PositionTarget

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_PositionTarget);

XForm_PositionTarget::XForm_PositionTarget()
	: m_start(0.0f, 0.0f, 1.0f)
	, m_end(0.0f, 1.0f, 0.0f)
	, m_duration(1.0f)
	, m_currentTime(0.0f)
{
}

XForm_PositionTarget::~XForm_PositionTarget()
{
}

void XForm_PositionTarget::apply(float _dt)
{
	m_currentTime = APT_MIN(m_currentTime + _dt, m_duration);
	if (m_onComplete && m_currentTime > m_duration) {
		m_onComplete(this);
	}
	m_currentPosition = smooth(m_start, m_end, m_currentTime / m_duration);
	m_node->setWorldMatrix(translate(m_node->getWorldMatrix(), m_currentPosition));
}

void XForm_PositionTarget::edit()
{
	ImGui::SliderFloat("Duration (s)", &m_duration, 0.0f, 10.0f);
	if (ImGui::Button("Reset")) {
		reset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Relative Reset")) {
		relativeReset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reverse")) {
		reverse();
	}

	Im3d::GizmoPosition("XForm_PositionTarget::m_start", &m_start);
	Im3d::GizmoPosition("XForm_PositionTarget::m_end",   &m_end);
	Im3d::PushDrawState();
		Im3d::SetColor(Im3d::kColorYellow);
		Im3d::SetSize(2.0f);
		Im3d::BeginLines();
			Im3d::SetAlpha(0.2f);
			Im3d::Vertex(m_start);
			Im3d::SetAlpha(1.0f);
			Im3d::Vertex(m_end);
		Im3d::End();
	Im3d::PopDrawState();
}

void XForm_PositionTarget::reset()
{
	m_currentTime = 0.0f;
}

void XForm_PositionTarget::relativeReset()
{
	m_end = m_currentPosition +	(m_end - m_start);
	m_start = m_currentPosition;
	m_currentTime = 0.0f;
}

void XForm_PositionTarget::reverse()
{
	std::swap(m_start, m_end);
	m_currentTime = APT_MAX(m_duration - m_currentTime, 0.0f);
}


/*******************************************************************************

                                XForm_VRGamepad

*******************************************************************************/


struct XForm_VRGamepad: public XForm
{
	vec3  m_position;
	vec3  m_velocity;
	float m_speed;
	float m_maxSpeed;
	float m_maxSpeedMul; //< Multiplies m_speed for speed 'boost'.
	float m_accelTime;   //< Acceleration ramp length in seconds.
	float m_accelCount;  //< Current ramp position in [0,m_accelTime].
	
	float m_orientation;        //< 
	float m_yaw;                //< Angular velocity in rads/sec.
	float m_rotationInputMul;   //< Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp;       //< Adhoc damping factor.

	XForm_VRGamepad::XForm_VRGamepad()
		: m_position(0.0f)
		, m_velocity(0.0f)
		, m_accelTime(0.1f)
		, m_accelCount(0.0f)
		, m_maxSpeed(2.0f)
		, m_maxSpeedMul(5.0f)
		, m_orientation(0.0f)
		, m_yaw(0.0f)
		, m_rotationInputMul(0.1f)
		, m_rotationDamp(0.0001f)
	{
	}


	void XForm_VRGamepad::apply(float _dt)
	{
 		if (!m_node->isSelected()) {
			return;
		}
		const Gamepad* gpad = Input::GetGamepad();
		if (!gpad) {
			return;
		}

		float cosTheta = cosf(m_orientation);
		float sinTheta = sinf(m_orientation);
	
		bool isAccel = false;		
		vec3 dir = vec3(0.0);		
		float x = gpad->getAxisState(Gamepad::kLeftStickX);
		dir += vec3(cosTheta, 0.0f, -sinTheta) * x;
		float y = gpad->getAxisState(Gamepad::kLeftStickY);
		dir += vec3(sinTheta, 0.0f, cosTheta) * y;
		if (gpad->isDown(Gamepad::kLeft1)) {
			dir -= vec3(0.0f, 1.0f, 0.0f);
		}
		if (gpad->isDown(Gamepad::kRight1)) {
			dir += vec3(0.0f, 1.0f, 0.0f);
		}
		isAccel = true;

		if (isAccel) {
		 // if we're accelerating, zero the velocity here to allow instantaneous direction changes
			m_velocity = vec3(0.0f);
		}
		m_velocity += dir;
				
		m_accelCount += isAccel ? _dt : -_dt;
		m_accelCount = apt::clamp(m_accelCount, 0.0f, m_accelTime);
		m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
		m_speed *= 1.0f + m_maxSpeedMul * gpad->getAxisState(Gamepad::kRightTrigger);
		float len = apt::length(m_velocity);
		if (len > 0.0f) {
			m_velocity = (m_velocity / len) * m_speed;
		}
		m_position += m_velocity * _dt;
	
		m_yaw -= gpad->getAxisState(Gamepad::kRightStickX) * 0.5f * _dt;//* m_rotationInputMul * 6.0f;
		m_orientation += m_yaw;
		m_yaw *= powf(m_rotationDamp, _dt);

		m_node->setWorldMatrix(translate(m_node->getLocalMatrix(), m_position) * mat4_cast(angleAxis(m_orientation, vec3(0.0f, 1.0f, 0.0f))));
	}

	void XForm_VRGamepad::edit()
	{
	}

}; // struct XForm_VRGamepad
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_VRGamepad);
