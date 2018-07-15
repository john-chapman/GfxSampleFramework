#include <frm/XForm.h>

#include <frm/interpolation.h>
#include <frm/Scene.h>
#include <frm/Spline.h>

#include <apt/log.h>
#include <apt/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  XForm

*******************************************************************************/

APT_FACTORY_DEFINE(XForm);

eastl::vector<const XForm::Callback*> XForm::s_callbackRegistry;

XForm::Callback::Callback(const char* _name, OnComplete* _callback)
	: m_callback(_callback)
	, m_name(_name)
	, m_nameHash(_name)
{
	APT_ASSERT(FindCallback(m_nameHash) == nullptr);
	if (FindCallback(m_nameHash) != nullptr) {
		APT_LOG_ERR("XForm: Callback '%s' already exists", _name);
		APT_ASSERT(false);
		return;
	}
	s_callbackRegistry.push_back(this);
}

int XForm::GetCallbackCount()
{
	return (int)s_callbackRegistry.size();
}
const XForm::Callback* XForm::GetCallback(int _i)
{
	return s_callbackRegistry[_i];
}
const XForm::Callback* XForm::FindCallback(StringHash _nameHash)
{
	for (auto& ret : s_callbackRegistry) {
		if (ret->m_nameHash == _nameHash) {
			return ret;
		}
	}
	return nullptr;
}
const XForm::Callback* XForm::FindCallback(OnComplete* _callback)
{
	for (auto& ret : s_callbackRegistry) {
		if (ret->m_callback == _callback) {
			return ret;
		}
	}
	return nullptr;
}
bool XForm::SerializeCallback(Serializer& _serializer_, OnComplete*& _callback, const char* _name)
{
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		String<64> cbkName;
		if (!Serialize(_serializer_, cbkName, _name)) {
			return false;
		}

		const Callback* cbk = FindCallback(StringHash((const char*)cbkName));
		if (cbk == nullptr) {
			APT_LOG_ERR("XForm: Invalid callback '%s'", (const char*)cbkName);
			_callback = nullptr;
			return false;
		}
		_callback = cbk->m_callback;
		return true;
	} else {
		String<64> cbkName = FindCallback(_callback)->m_name;
		return Serialize(_serializer_, cbkName, _name);
	}
}


/*******************************************************************************

                        XForm_PositionOrientationScale

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_PositionOrientationScale);

void XForm_PositionOrientationScale::apply(float _dt)
{
	mat4 mat = TransformationMatrix(m_position, m_orientation, m_scale);
	m_node->setWorldMatrix(m_node->getWorldMatrix() * mat);
}

void XForm_PositionOrientationScale::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);
	
	ImGui::DragFloat3("Position", &m_position.x, 0.5f);
	/*vec3 eul = eulerAngles(m_orientation);
	if (ImGui::DragFloat3("Orientation", &eul.x, 0.1f)) {
		m_orientation = quat(eul);
	}
	ImGui::DragFloat3("Scale", &m_scale.x, 0.2f);

	mat3 orientation(m_orientation);
	if (Im3d::Gizmo("XForm_PositionOrientationScale", &m_position.x, (float*)&orientation, &m_scale.x)) {
		m_orientation = quat(orientation);
	}*/

	Im3d::PopId();
	ImGui::PopID();
}

bool XForm_PositionOrientationScale::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_position,    "Position");
	ret &= Serialize(_serializer_, m_orientation, "Orientation");
	ret &= Serialize(_serializer_, m_scale,       "Scale");
	return ret;
}

/*******************************************************************************

                                XForm_FreeCamera

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_FreeCamera);

void XForm_FreeCamera::apply(float _dt)
{
 	if (!m_node->isSelected()) {
		return;
	}

	const mat4& localMatrix = m_node->getLocalMatrix();

	const Gamepad* gpad = Input::GetGamepad();
	const Keyboard* keyb = Input::GetKeyboard();
	if (keyb && keyb->isDown(Keyboard::Key_LCtrl)) {
		keyb = nullptr; // disable keyboard input on lctrl
	}

	bool isAccel = false;
	vec3 dir = vec3(0.0);		
	if (gpad) {			
		float x = gpad->getAxisState(Gamepad::Axis_LeftStickX);
		float y = gpad->getAxisState(Gamepad::Axis_LeftStickY);
		float z = gpad->isDown(Gamepad::Button_Right1) ? 1.0f : (gpad->isDown(Gamepad::Button_Left1) ? -1.0f : 0.0f);
		dir += localMatrix[0].xyz() * x;
		dir += localMatrix[2].xyz() * y;
		dir += localMatrix[1].xyz() * z;
		isAccel = abs(x + y + z) > 0.0f;
	}
	if (keyb) {
		if (keyb->isDown(Keyboard::Key_W)) {
			dir -= localMatrix[2].xyz();
			isAccel = true;
		}
		if (keyb->isDown(Keyboard::Key_A)) {
			dir -= localMatrix[0].xyz();
			isAccel = true;
		}
		if (keyb->isDown(Keyboard::Key_S)) {
			dir += localMatrix[2].xyz();
			isAccel = true;
		}
		if (keyb->isDown(Keyboard::Key_D)) {
			dir += localMatrix[0].xyz();
			isAccel = true;
		}
		if (keyb->isDown(Keyboard::Key_Q)) {
			dir -= localMatrix[1].xyz();
			isAccel = true;
		}
		if (keyb->isDown(Keyboard::Key_E)) {
			dir += localMatrix[1].xyz();
			isAccel = true;
		}
	}
	if (isAccel) {
	 // if accelerating, zero the velocity here to allow instantaneous direction changes
		m_velocity = vec3(0.0f);
	}
	m_velocity += dir;
			
	m_accelCount += isAccel ? _dt : -_dt;
	m_accelCount = APT_CLAMP(m_accelCount, 0.0f, m_accelTime);
	m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
	if (gpad) {
		m_speed *= 1.0f + m_maxSpeedMul * gpad->getAxisState(Gamepad::Axis_RightTrigger);
	}
	if (keyb && keyb->isDown(Keyboard::Key_LShift)) {
		m_speed *= m_maxSpeedMul;
	}
	float len2 = apt::Length2(m_velocity);
	if (len2 > 0.0f) {
		m_velocity = (m_velocity / sqrt(len2)) * m_speed;
	}		
	m_position += m_velocity * _dt;


	Mouse* mouse = Input::GetMouse();
	if (gpad) {
		m_pitchYawRoll.x -= gpad->getAxisState(Gamepad::Axis_RightStickY) * 16.0f * _dt;//* m_rotationInputMul * 6.0f; // \todo setter for this?
		m_pitchYawRoll.y -= gpad->getAxisState(Gamepad::Axis_RightStickX) * 16.0f * _dt;//* m_rotationInputMul * 6.0f;
	}
	if (mouse->isDown(Mouse::Button_Right)) {
		m_pitchYawRoll.x -= mouse->getAxisState(Mouse::Axis_Y) * m_rotationInputMul;
		m_pitchYawRoll.y -= mouse->getAxisState(Mouse::Axis_X) * m_rotationInputMul;
	}
	quat qpitch     = RotationQuaternion(localMatrix[0].xyz(),   m_pitchYawRoll.x * _dt);
	quat qyaw       = RotationQuaternion(vec3(0.0f, 1.0f, 0.0f), m_pitchYawRoll.y * _dt);
	quat qroll      = RotationQuaternion(localMatrix[2].xyz(),   m_pitchYawRoll.z * _dt);
	m_orientation   = qmul(qmul(qmul(qyaw, qpitch), qroll), m_orientation);
	m_pitchYawRoll *= powf(m_rotationDamp, _dt);

	m_node->setLocalMatrix(TransformationMatrix(m_position, m_orientation));
	
}

void XForm_FreeCamera::edit()
{
	ImGui::PushID(this);
	ImGui::SliderFloat3("Position", &m_position.x, -1000.0f, 1000.0f);
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
	ImGui::PopID();
}

bool XForm_FreeCamera::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_position,         "Position");
	ret &= Serialize(_serializer_, m_orientation,      "Orientation");
	ret &= Serialize(_serializer_, m_maxSpeed,         "MaxSpeed");
	ret &= Serialize(_serializer_, m_maxSpeedMul,      "MaxSpeedMultiplier");
	ret &= Serialize(_serializer_, m_accelTime,        "AccelerationTime");
	ret &= Serialize(_serializer_, m_rotationInputMul, "RotationInputMultiplier");
	ret &= Serialize(_serializer_, m_rotationDamp,     "RotationDamping");
	return ret;
}

/*******************************************************************************

                                 XForm_LookAt

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_LookAt);

void XForm_LookAt::apply(float _dt)
{
	vec3 posW = GetTranslation(m_node->getWorldMatrix());
	vec3 targetW = m_offset;
	if_unlikely (m_targetId != Node::kInvalidId && m_target == nullptr) {
		m_target = Scene::GetCurrent()->findNode(m_targetId);
	}
	if (m_target) {
		targetW += GetTranslation(m_target->getWorldMatrix());
	}
	m_node->setWorldMatrix(LookAt(posW, targetW));
}

void XForm_LookAt::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);
	Scene& scene = *Scene::GetCurrent();
	if (ImGui::Button("Target Node")) {
		scene.beginSelectNode();
	}
	m_target = scene.selectNode(m_target);
	if (m_target) {
		ImGui::SameLine();
		ImGui::Text(m_target->getName());
		m_targetId = m_target->getId();
	}
	//ImGui::DragFloat3("Offset", &m_offset.x, 0.5f);
	Im3d::GizmoTranslation("XForm_LookAt", &m_offset.x);	

	Im3d::PopId();
	ImGui::PopID();
}

bool XForm_LookAt::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_offset,   "Offset");
	ret &= Serialize(_serializer_, m_targetId, "TargetId");
	return ret;
}

/*******************************************************************************

                                XForm_Spin

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_Spin);

void XForm_Spin::apply(float _dt)
{
	m_rotation += m_rate * _dt;
	m_node->setWorldMatrix(m_node->getWorldMatrix() * RotationMatrix(m_axis, m_rotation));
}

void XForm_Spin::edit()
{
	ImGui::SliderFloat("Rate (radians/s)", &m_rate, -8.0f, 8.0f);
	ImGui::SliderFloat3("Axis", &m_axis.x, -1.0f, 1.0f);
	m_axis = normalize(m_axis);

	Im3d::PushDrawState();
		Im3d::SetColor(Im3d::Color_Yellow);
		Im3d::SetAlpha(1.0f);
		Im3d::SetSize(2.0f);
		Im3d::BeginLines();
			vec3 p = GetTranslation(m_node->getWorldMatrix());
			Im3d::Vertex(p - m_axis * 9999.0f);
			Im3d::Vertex(p + m_axis * 9999.0f);
		Im3d::End();
	Im3d::PopDrawState();
}

bool XForm_Spin::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_axis, "Axis");
	ret &= Serialize(_serializer_, m_rate, "Rate");
	return ret;
}

/*******************************************************************************

                              XForm_PositionTarget

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_PositionTarget);

void XForm_PositionTarget::apply(float _dt)
{
	m_currentTime = APT_MIN(m_currentTime + _dt, m_duration);
	if (m_onComplete && m_currentTime >= m_duration) {
		m_onComplete(this);
	}
	m_currentPosition = smooth(m_start, m_end, m_currentTime / m_duration);
	m_node->setWorldPosition(m_node->getWorldPosition() + m_currentPosition);
}

void XForm_PositionTarget::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);

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

	Im3d::GizmoTranslation("XForm_PositionTarget::m_start", &m_start.x);
	Im3d::GizmoTranslation("XForm_PositionTarget::m_end",   &m_end.x);
	Im3d::PushDrawState();
		Im3d::SetColor(Im3d::Color_Yellow);
		Im3d::SetSize(2.0f);
		Im3d::BeginLines();
			Im3d::SetAlpha(0.2f);
			Im3d::Vertex(m_start);
			Im3d::SetAlpha(1.0f);
			Im3d::Vertex(m_end);
		Im3d::End();
	Im3d::PopDrawState();

	Im3d::PopId();
	ImGui::PopID();
}

bool XForm_PositionTarget::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_start,    "Start");
	ret &= Serialize(_serializer_, m_end,      "End");
	ret &= Serialize(_serializer_, m_duration, "Duration");
	ret &= SerializeCallback(_serializer_, m_onComplete, "OnComplete");
	return ret;
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
	eastl::swap(m_start, m_end);
	m_currentTime = APT_MAX(m_duration - m_currentTime, 0.0f);
}

/*******************************************************************************

                              XForm_SplinePath

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_SplinePath);

void XForm_SplinePath::apply(float _dt)
{
	m_currentTime = APT_MIN(m_currentTime + _dt, m_duration);
	if (m_onComplete && m_currentTime >= m_duration) {
		m_onComplete(this);
	}
	vec3 position;
	position = m_path->sample(m_currentTime / m_duration, &m_pathHint);
	m_node->setWorldPosition(m_node->getWorldPosition() + position);
}

void XForm_SplinePath::edit()
{
	ImGui::PushID(this);
	ImGui::DragFloat("Duration (s)", &m_duration, 0.1f);
	m_duration = APT_MAX(m_duration, 0.0f);
	m_currentTime = APT_MIN(m_currentTime, m_duration);
	if (ImGui::Button("Reset")) {
		reset();
	}
	ImGui::Text("Current Time: %.3fs", m_currentTime);
	ImGui::Text("Path hint:    %d",    m_pathHint);
	ImGui::PopID();
}

bool XForm_SplinePath::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_duration, "Duration");
	ret &= SerializeCallback(_serializer_, m_onComplete, "OnComplete");
	return ret;
}

void XForm_SplinePath::reset()
{
	m_currentTime = 0.0f;
	m_pathHint = 0;
}

void XForm_SplinePath::reverse()
{
	APT_ASSERT(false); // \todo
}

/*******************************************************************************

                              XForm_OrbitalPath

*******************************************************************************/
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_OrbitalPath);

void XForm_OrbitalPath::apply(float _dt)
{
	m_theta = Fract(m_theta + m_speed * _dt);

	float a = m_azimuth;
	float b = -m_elevation;
	float t = m_theta * kTwoPi;
	float ca = cosf(a);
	float sa = sinf(a);
	float cb = cosf(b);
	float sb = sinf(b);
	float ct = cosf(t);
	float st = sinf(t);
	mat3 tmat = transpose(mat3(
		vec3(st,     0.0f,  ct),
		vec3(0.0f,   1.0f,  0.0f),
		vec3(-ct,    0.0f,  st)
		));
	mat3 amat = transpose(mat3(
		vec3(ca,     0.0f,  sa),
		vec3(0.0f,   1.0f,  0.0f),
		vec3(-sa,    0.0f,  ca)
		));
	mat3 bmat = transpose(mat3(
		vec3(1.0f,   0.0f,  0.0f),
		vec3(0.0f,   cb,    -sb),
		vec3(0.0f,   sb,     cb)
		));
	m_direction = amat * bmat * tmat * vec3(1.0f, 0.0f, 0.0f);
	m_normal = amat * bmat * vec3(0.0f, 1.0f, 0.0f);
	if (m_node) {
		m_node->setWorldPosition(m_node->getWorldPosition() + m_direction * m_radius);
	}
}

void XForm_OrbitalPath::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);

	ImGui::SliderFloat("Theta",     &m_theta,      0.0f, 1.0f);
	ImGui::Spacing();		
	ImGui::SliderAngle("Azimuth",   &m_azimuth,   -180.0f, 180.0f);
	ImGui::SliderAngle("Elevation", &m_elevation, -180.0f, 180.0f);
	ImGui::DragFloat  ("Radius",    &m_radius,     0.1f);
	ImGui::DragFloat  ("Speed",     &m_speed,      0.01f);
	
	Im3d::PushAlpha(0.5f);
	Im3d::PushSize(2.0f);
	Im3d::SetColor(Im3d::Color(m_displayColor));
		Im3d::DrawCircle(vec3(0.0f), m_normal, m_radius);
	Im3d::PopAlpha();
	Im3d::PopSize();
	Im3d::DrawPoint(m_direction * m_radius, 8.0f, Im3d::Color(m_displayColor));

	Im3d::PopId();
	ImGui::PopID();
}

bool XForm_OrbitalPath::serialize(Serializer& _serializer_)
{
	bool ret = true;
	ret &= Serialize(_serializer_, m_azimuth,   "Azimuth");
	ret &= Serialize(_serializer_, m_elevation, "Elevation");
	ret &= Serialize(_serializer_, m_theta,     "Theta");
	ret &= Serialize(_serializer_, m_radius,    "Radius");
	ret &= Serialize(_serializer_, m_speed,     "Speed");
	return ret;
}

void XForm_OrbitalPath::reset()
{
	m_theta = 0.0f;
}

/*******************************************************************************

                                XForm_VRGamepad

*******************************************************************************/


struct XForm_VRGamepad: public XForm
{
	vec3  m_position          = vec3(0.0f);
	vec3  m_velocity          = vec3(0.0f);
	float m_speed             = 0.0f;
	float m_maxSpeed          = 2.0f;
	float m_maxSpeedMul       = 5.0f;        // Multiplies m_speed for speed 'boost'.
	float m_accelTime         = 0.01f;       // Acceleration ramp length in seconds.
	float m_accelCount        = 0.0f;        // Current ramp position in [0,m_accelTime].
	
	float m_orientation       = 0.0f;
	float m_yaw               = 0.0f;        // Angular velocity in rads/sec.
	float m_rotationInputMul  = 0.1f;        // Scale rotation inputs (should be relative to fov/screen size).
	float m_rotationDamp      = 0.0001f;     // Adhoc damping factor.

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
		float x = gpad->getAxisState(Gamepad::Axis_LeftStickX);
		dir += vec3(cosTheta, 0.0f, -sinTheta) * x;
		float y = gpad->getAxisState(Gamepad::Axis_LeftStickY);
		dir += vec3(sinTheta, 0.0f, cosTheta) * y;
		if (gpad->isDown(Gamepad::Button_Left1)) {
			dir -= vec3(0.0f, 1.0f, 0.0f);
		}
		if (gpad->isDown(Gamepad::Button_Right1)) {
			dir += vec3(0.0f, 1.0f, 0.0f);
		}
		isAccel = true;

		if (isAccel) {
		 // if we're accelerating, zero the velocity here to allow instantaneous direction changes
			m_velocity = vec3(0.0f);
		}
		m_velocity += dir;
				
		m_accelCount += isAccel ? _dt : -_dt;
		m_accelCount = APT_CLAMP(m_accelCount, 0.0f, m_accelTime);
		m_speed = (m_accelCount / m_accelTime) * m_maxSpeed;
		m_speed *= 1.0f + m_maxSpeedMul * gpad->getAxisState(Gamepad::Axis_RightTrigger);
		float len = apt::Length(m_velocity);
		if (len > 0.0f) {
			m_velocity = (m_velocity / len) * m_speed;
		}
		m_position += m_velocity * _dt;
	
		m_yaw -= gpad->getAxisState(Gamepad::Axis_RightStickX) * 0.5f * _dt;//* m_rotationInputMul * 6.0f;
		m_orientation += m_yaw;
		m_yaw *= powf(m_rotationDamp, _dt);

		m_node->setWorldMatrix(TransformationMatrix(m_position, RotationQuaternion(vec3(0.0f, 1.0f, 0.0f), m_orientation)));
	}

	void XForm_VRGamepad::edit()
	{
	}

	bool serialize(Serializer& _serializer_)
	{
		bool ret = true;
		ret &= Serialize(_serializer_, m_position,         "Position");
		ret &= Serialize(_serializer_, m_orientation,      "Orientation");
		ret &= Serialize(_serializer_, m_maxSpeed,         "MaxSpeed");
		ret &= Serialize(_serializer_, m_maxSpeedMul,      "MaxSpeedMultiplier");
		ret &= Serialize(_serializer_, m_accelTime,        "AccelerationTime");
		ret &= Serialize(_serializer_, m_rotationInputMul, "RotationInputMultiplier");
		ret &= Serialize(_serializer_, m_rotationDamp,     "RotationDamping");
		return ret;
	}

}; // struct XForm_VRGamepad
APT_FACTORY_REGISTER_DEFAULT(XForm, XForm_VRGamepad);

XFORM_REGISTER_CALLBACK(XForm::Reset);
XFORM_REGISTER_CALLBACK(XForm::RelativeReset);
XFORM_REGISTER_CALLBACK(XForm::Reverse);
