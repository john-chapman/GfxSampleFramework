#include <frm/Camera.h>

#include <frm/Scene.h>

#include <apt/Json.h>

using namespace frm;
using namespace apt;

// PUBLIC

Camera::Camera(
	float _aspect,
	float _fovVertical,
	float _near,
	float _far
	)
	: m_node(nullptr)
	, m_up(tan(_fovVertical * 0.5f))
	, m_down(m_up)
	, m_left(m_up * _aspect)
	, m_right(m_up * _aspect)
	, m_near(_near)
	, m_far(_far)
	, m_projFlags(ProjFlag_Default)
	, m_projDirty(true)
	, m_world(1.0f)
	, m_proj(0.0f)
{
	build();
}

Camera::Camera(
	float _up,
	float _down,
	float _left,
	float _right,
	float _near,
	float _far,
	bool  _isOrtho
	)
	: m_node(nullptr)
	, m_up(_isOrtho ? _up : tan(_up))
	, m_down(_isOrtho ? _down : tan(_down))
	, m_left(_isOrtho ? _left : tan(_left))
	, m_right(_isOrtho ? _right : tan(_right))
	, m_near(_near)
	, m_far(_far)
	, m_projFlags(ProjFlag_Default)
	, m_projDirty(true)
	, m_world(1.0f)
	, m_proj(0.0f)
{
	setProjFlag(ProjFlag_Orthographic, _isOrtho);
	setProjFlag(ProjFlag_Asymmetrical, m_up != m_down || m_right != m_left);
	build();
}

void Camera::setVerticalFov(float _radians)
{
	float aspect = (m_left + m_right) / (m_up + m_down);
	m_up = tan(_radians * 0.5f);
	m_down = m_up;
	m_left = m_up * aspect;
	m_right = m_left;
	m_projDirty = true;
}

void Camera::setHorizontalFov(float _radians)
{
	float aspect = (m_up + m_down) / (m_left + m_right);
	m_left = tan(_radians * 0.5f);
	m_right = m_up;
	m_up = m_left * aspect;
	m_down = m_up;
	m_projDirty = true;
}

void Camera::setAspect(float _aspect)
{
	setProjFlag(ProjFlag_Asymmetrical, false);
	float horizontal = _aspect * (m_up + m_down);
	m_left = horizontal * 0.5f;
	m_right = m_left;
	m_projDirty = true;
}

void Camera::setProjMatrix(const mat4& _proj, uint32 _flags)
{
	m_proj = _proj; 
	m_projFlags = _flags;
	m_localFrustum = Frustum(inverse(_proj));

	vec3* frustum = m_localFrustum.m_vertices;
	m_up    =  frustum[0].y;
	m_down  = -frustum[3].y;
	m_left  = -frustum[3].x;
	m_right =  frustum[1].x;
	m_near  = -frustum[0].z;
	m_far   = -frustum[4].z;
	if (!getProjFlag(ProjFlag_Orthographic)) {
		m_up    /= m_near;
		m_down  /= m_near;
		m_left  /= m_near;
		m_right /= m_near;
	}
	m_projDirty = false;
}

void Camera::build()
{
	if (m_projDirty) {
		buildProj();	
	}
	
	if (m_node) {
		m_world = m_node->getWorldMatrix();
	}
	m_view = inverse(m_world);
	m_viewProj = m_proj * m_view;
	m_worldFrustum = m_localFrustum;
	m_worldFrustum.transform(m_world);
}

bool Camera::serialize(JsonSerializer& _serializer_)
{
	_serializer_.value("Up",          m_up);
	_serializer_.value("Down",        m_down);
	_serializer_.value("Left",        m_left);
	_serializer_.value("Right",       m_right);
	_serializer_.value("Near",        m_near);
	_serializer_.value("Far",         m_far);
	_serializer_.value("WorldMatrix", m_world);

	bool orthographic = getProjFlag(ProjFlag_Orthographic);
	bool asymmetrical = getProjFlag(ProjFlag_Asymmetrical);
	bool infinite     = getProjFlag(ProjFlag_Infinite);
	bool reversed     = getProjFlag(ProjFlag_Reversed);
	_serializer_.value("Orthographic", orthographic);
	_serializer_.value("Asymmetrical", asymmetrical);
	_serializer_.value("Infinite",     infinite);
	_serializer_.value("Reversed",     reversed);

	if (_serializer_.getMode() == JsonSerializer::kRead) {
		setProjFlag(ProjFlag_Orthographic, orthographic);
		setProjFlag(ProjFlag_Asymmetrical, asymmetrical);
		setProjFlag(ProjFlag_Infinite,     infinite);
		setProjFlag(ProjFlag_Reversed,     reversed);
		m_projDirty = true;
		build();
	}

	return true;
}

void Camera::buildProj()
{
	m_localFrustum = Frustum(m_up, m_down, m_left, m_right, m_near, m_far, getProjFlag(ProjFlag_Orthographic));
	if (getProjFlag(ProjFlag_Orthographic)) {
	} else {
		float t = m_localFrustum.m_vertices[0].y;
		float b = m_localFrustum.m_vertices[3].y;
		float l = m_localFrustum.m_vertices[0].x;
		float r = m_localFrustum.m_vertices[1].x;
		float n = m_near;
		float f = m_far;

		m_proj[0][0] = (2.0f * n) / (r - l);
		m_proj[1][1] = (2.0f * n) / (t - b);
		m_proj[2][0] = (r + l) / (r - l);
		m_proj[2][1] = (t + b) / (t - b);
		m_proj[2][3] = -1.0f;

		bool infinite = getProjFlag(ProjFlag_Infinite);
		bool reversed = getProjFlag(ProjFlag_Reversed);
		if (infinite && reversed) {
			m_proj[2][2] = 0.0f;
			m_proj[3][2] = n;

		} else if (infinite) {
			m_proj[2][2] = -1.0f;
			m_proj[3][2] = -2.0f * n;

		} else if (reversed) {
			m_proj[2][2] = n / (n - f);
			m_proj[3][2] = m_proj[2][2] == 0.0f ? n : (f * n / (f - n));

		} else {			
			m_proj[2][2] = (n + f) / (n - f);
			m_proj[3][2] = (2.0f * n * f) / (n - f);

		}

	}
	m_projDirty = false;
}
