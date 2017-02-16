#include <frm/Camera.h>

#include <frm/Scene.h>

#include <apt/Json.h>

using namespace frm;
using namespace apt;

// PUBLIC

Camera::Camera(
	float _aspect,
	float _fovVertical,
	float _clipNear,
	float _clipFar
	)
	: m_node(nullptr)
	, m_up(tan(_fovVertical * 0.5f))
	, m_down(m_up)
	, m_left(m_up * _aspect)
	, m_right(m_up * _aspect)
	, m_clipNear(_clipNear)
	, m_clipFar(_clipFar)
	, m_isOrtho(false)
	, m_isSymmetric(true)
	, m_projDirty(true)
	, m_world(1.0f)
	, m_localFrustum(m_up, m_down, m_left, m_right, m_clipNear, m_clipFar, m_isOrtho)
{
}

Camera::Camera(
	float _up,
	float _down,
	float _left,
	float _right,
	float _clipNear,
	float _clipFar,
	bool  _isOrtho
	)
	: m_node(nullptr)
	, m_up(_isOrtho ? _up : tan(_up))
	, m_down(_isOrtho ? _down : tan(_down))
	, m_left(_isOrtho ? _left : tan(_left))
	, m_right(_isOrtho ? _right : tan(_right))
	, m_clipNear(_clipNear)
	, m_clipFar(_clipFar)
	, m_isOrtho(_isOrtho)
	, m_isSymmetric(fabs(m_up) == fabs(m_down) && fabs(m_right) == fabs(m_left))
	, m_projDirty(true)
	, m_world(1.0f)
	, m_localFrustum(m_up, m_down, m_left, m_right, m_clipNear, m_clipFar, m_isOrtho)
{
}

void Camera::setIsOrtho(bool _ortho)
{
	if (m_isOrtho != _ortho) {
		APT_ASSERT(false); // \todo implement
	}
	m_isOrtho = _ortho;
	m_projDirty = true;
}

void Camera::setIsSymmetric(bool _symmetric)
{
	if (_symmetric) {
		float h = (m_up + m_down) * 0.5f;
		m_up = m_down = h;
		float w = (m_left + m_right) * 0.5f;
		m_left = m_right = w;
	}
	m_isSymmetric = _symmetric;
	m_projDirty = true;
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
	m_isSymmetric = true;
	float horizontal = _aspect * (m_up + m_down);
	m_left = horizontal * 0.5f;
	m_right = m_left;
	m_projDirty = true;
}

void Camera::setProjMatrix(const mat4& _proj)
{
	m_proj = _proj; 
	m_localFrustum = Frustum(inverse(_proj));

	vec3* frustum = m_localFrustum.m_vertices;
	m_clipNear = -frustum[0].z;
	m_clipFar  = -frustum[4].z;
	m_isOrtho  =  frustum[0].x == frustum[4].x;

	m_up    =  frustum[0].y;
	m_down  = -frustum[3].y;
	m_left  = -frustum[3].x;
	m_right =  frustum[1].x;
	if (!m_isOrtho) {
		m_up    /= m_clipNear;
		m_down  /= m_clipNear;
		m_left  /= m_clipNear;
		m_right /= m_clipNear;
	}
	m_projDirty = false;
}

void Camera::build()
{
/*
	'Projection Matrix Tricks' (infinite projection, oblique near plane):
	http://www.terathon.com/gdc07_lengyel.pdf
*/
//#define INFINITE_PROJ
	if (m_projDirty) {
		m_localFrustum = Frustum(m_up, m_down, m_left, m_right, m_clipNear, m_clipFar, m_isOrtho);
		
		if (isOrtho()) {
			m_proj = apt::ortho(m_left, m_right, m_down, m_up, m_clipNear, m_clipFar);
		} else {
			float t = m_localFrustum.m_vertices[0].y;
			float b = m_localFrustum.m_vertices[3].y;
			float l = m_localFrustum.m_vertices[0].x;
			float r = m_localFrustum.m_vertices[1].x;
			float n = m_clipNear;
			float f = m_clipFar;
			#ifdef INFINITE_PROJ
				m_proj = mat4(
					(2.0f*n)/(r-l),     0.0f,          0.0f,         0.0f,
					     0.0f,      (2.0f*n)/(t-b),    0.0f,         0.0f,
					  (r+l)/(r-l),    (t+b)/(t-b),     -1.0f,       -1.0f,
					     0.0f,          0.0f,          -2.0f*n,      0.0f
					);
				m_localFrustum.m_planes[Frustum::kFar].m_normal = -m_localFrustum.m_planes[Frustum::kFar].m_normal;
			#else
				m_proj = mat4(
					(2.0f*n)/(r-l),     0.0f,          0.0f,         0.0f,
					     0.0f,      (2.0f*n)/(t-b),    0.0f,         0.0f,
					  (r+l)/(r-l),    (t+b)/(t-b), (n+f)/(n-f),     -1.0f,
					     0.0f,          0.0f,      (2.0f*n*f)/(n-f), 0.0f
					);
			#endif
		}
		m_projDirty = false;
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
	_serializer_.value("IsOrtho",     m_isOrtho);
	_serializer_.value("IsSymmetric", m_isSymmetric);
	_serializer_.value("Up",          m_up);
	_serializer_.value("Down",        m_down);
	_serializer_.value("Left",        m_left);
	_serializer_.value("Right",       m_right);
	_serializer_.value("ClipNear",    m_clipNear);
	_serializer_.value("ClipFar",     m_clipFar);
	_serializer_.value("WorldMatrix", m_world);

	if (_serializer_.getMode() == JsonSerializer::kRead) {
		m_projDirty = true;
		build();
	}

	return true;
}
