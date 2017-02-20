#include <frm/Camera.h>
#include <frm/Scene.h>

#include <apt/Json.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

// PUBLIC

Camera::Camera(Node* _parent)
	: m_projFlags(ProjFlag_Default)
	, m_projDirty(true)
	, m_up(1.0f)
	, m_down(-1.0f)
	, m_right(1.0f)
	, m_left(-1.0f)
	, m_near(-1.0f)
	, m_far(1.0f)
	, m_parent(_parent)
	, m_world(1.0f)
	, m_proj(0.0f)
{
}

bool Camera::serialize(JsonSerializer& _serializer_)
{
 // note that the parent node doesn't get written here - the scene serializes the camera params *within* a node so it's not required
	_serializer_.value("Up",          m_up);
	_serializer_.value("Down",        m_down);
	_serializer_.value("Right",       m_right);
	_serializer_.value("Left",        m_left);
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
	}
	return true;
}

void Camera::edit()
{
	ImGui::PushID(this);
	Im3d::PushId(this);

	bool updated = false;

	bool  orthographic = getProjFlag(ProjFlag_Orthographic);
	bool  asymmetrical = getProjFlag(ProjFlag_Asymmetrical);
	bool  infinite     = getProjFlag(ProjFlag_Infinite);
	bool  reversed     = getProjFlag(ProjFlag_Reversed);

	ImGui::Text("Projection:");
	if (ImGui::Checkbox("Orthographic", &orthographic)) {
		float a = fabs(m_far);
		if (orthographic) {
		 // perspective -> orthographic
			m_up    = m_up * a;
			m_down  = m_down * a;
			m_right = m_right * a;
			m_left  = m_left * a;
		} else {
		 // orthographic -> perspective
			m_up    = m_up / a;
			m_down  = m_down / a;
			m_right = m_right / a;
			m_left  = m_left / a;		 
		}
		updated = true;
	}
	updated |= ImGui::Checkbox("Asymmetrical", &asymmetrical);
	updated |= ImGui::Checkbox("Infinite", &infinite);
	updated |= ImGui::Checkbox("Reversed", &reversed);

	float up    = orthographic ? m_up    : degrees(atanf(m_up));
	float down  = orthographic ? m_down	 : degrees(atanf(m_down));
	float right = orthographic ? m_right : degrees(atanf(m_right));
	float left  = orthographic ? m_left	 : degrees(atanf(m_left));
	float near  = m_near;
	float far   = m_far;

	
	if (orthographic) {
	} else {
		if (asymmetrical) {
			updated |= ImGui::SliderFloat("Up",    &up,    -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Down",  &down,  -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Right", &right, -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Left",  &left,  -90.0f, 90.0f);
		} else {
			float fovVertical = up * 2.0f;
			float fovHorizontal = right * 2.0f;
			updated |= ImGui::SliderFloat("FOV Vertical",   &fovVertical,   0.0f, 180.0f);
			updated |= ImGui::SliderFloat("FOV Horizontal", &fovHorizontal, 0.0f, 180.0f);
			float aspect = fovHorizontal / fovVertical;
			updated |= ImGui::SliderFloat("Apect Ratio (V/H)", &aspect, 0.0f, 4.0f);
			if (updated) {
				up = fovVertical * 0.5f;
				down = -up;
				right = up * aspect;
				left = -right;
			}
		}
	}
	updated |= ImGui::SliderFloat("Near",  &near,   0.0f,  10.0f);
	updated |= ImGui::SliderFloat("Far",   &far,    0.0f,  100.0f);

	if (ImGui::TreeNode("Raw Params")) {
		
		updated |= ImGui::DragFloat("Up",    &up,    0.5f);
		updated |= ImGui::DragFloat("Down",  &down,  0.5f);
		updated |= ImGui::DragFloat("Right", &right, 0.5f);
		updated |= ImGui::DragFloat("Left",  &left,  0.5f);
		updated |= ImGui::DragFloat("Near",  &near,  0.5f);
		updated |= ImGui::DragFloat("Far",   &far,   0.5f);

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Debug")) {
		static const int kSampleCount = 200;
		static float zrange[2] = { 0.0f, 100.0f };
		ImGui::SliderFloat2("Z Curve", zrange, 0.0f, 200.0f);
		float zvalues[kSampleCount];
		for (int i = 0; i < kSampleCount; ++i) {
			float z = zrange[0] + ((float)i / (float)kSampleCount) * (zrange[1] - zrange[0]);
			vec4 pz = m_proj * vec4(0.0f, 0.0f, -z, 1.0f);
			zvalues[i] = pz.z / pz.w;
			//if (!reversed) {
			//	zvalues[i] = zvalues[i] * 0.5f + 0.5f;
			//}
		}
		ImGui::PlotLines("Z Values", zvalues, kSampleCount, 0, 0, 0.0f, 1.0f, ImVec2(0.0f, 128.0f));

		Frustum dbgFrustum = Frustum(inverse(m_proj));
		Im3d::PushDrawState();
		Im3d::PushMatrix(m_world);
			Im3d::SetSize(2.0f);
			Im3d::SetColor(Im3d::Color_Yellow);
			Im3d::BeginLineLoop();
				Im3d::Vertex(dbgFrustum.m_vertices[0]);
				Im3d::Vertex(dbgFrustum.m_vertices[1]);
				Im3d::Vertex(dbgFrustum.m_vertices[2]);
				Im3d::Vertex(dbgFrustum.m_vertices[3]);
			Im3d::End();
			Im3d::SetColor(Im3d::Color_Magenta);
			Im3d::BeginLineLoop();
				Im3d::Vertex(dbgFrustum.m_vertices[4]);
				Im3d::Vertex(dbgFrustum.m_vertices[5]);
				Im3d::Vertex(dbgFrustum.m_vertices[6]);
				Im3d::Vertex(dbgFrustum.m_vertices[7]);
			Im3d::End();
			Im3d::BeginLines();
				Im3d::Vertex(dbgFrustum.m_vertices[0], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[4], Im3d::Color_Magenta);
				Im3d::Vertex(dbgFrustum.m_vertices[1], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[5], Im3d::Color_Magenta);
				Im3d::Vertex(dbgFrustum.m_vertices[2], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[6], Im3d::Color_Magenta);
				Im3d::Vertex(dbgFrustum.m_vertices[3], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[7], Im3d::Color_Magenta);
			Im3d::End();
		Im3d::PopMatrix();
		Im3d::PopDrawState();

		ImGui::TreePop();
	}

	if (updated) {
		m_up    = orthographic ? up    : tanf(radians(up));
		m_down  = orthographic ? down  : tanf(radians(down));
		m_right = orthographic ? right : tanf(radians(right));
		m_left  = orthographic ? left  : tanf(radians(left));
		m_near  = near;
		m_far   = far;
		setProjFlag(ProjFlag_Orthographic, orthographic);
		setProjFlag(ProjFlag_Asymmetrical, asymmetrical);
		setProjFlag(ProjFlag_Infinite, infinite);
		setProjFlag(ProjFlag_Reversed, reversed);
	}


	Im3d::PopId();
	ImGui::PopID();
}


void Camera::setProj(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags)
{
	m_projFlags = _flags;
	m_projDirty = true;

	if (getProjFlag(ProjFlag_Orthographic)) {
	 // ortho proj, params are ±offsets from the centre of the view plane
		m_up    = _up;
		m_down  = _down;
		m_right = _right;
		m_left  = _left;
		m_near  = _near;
		m_far   = _far;

	} else {
	 // perspective proj, params are ±tan(angle from the view axis)
		m_up    = tanf(_up);
		m_down  = tanf(_down);
		m_right = tanf(_right);
		m_left  = tanf(_left);
		m_near  = tanf(_near);
		m_far   = tanf(_far);
	}
	
	bool asymmetrical = fabs(fabs(_up) - fabs(_down)) > FLT_EPSILON || fabs(fabs(_right) - fabs(_left)) > FLT_EPSILON;
	setProjFlag(ProjFlag_Asymmetrical, asymmetrical);
}

void Camera::setProj(const mat4& _projMatrix, uint32 _flags)
{
 // \todo recovering frustum and params from an infinite or reversed proj matrix may not work
	m_proj = _projMatrix; 
	m_localFrustum = Frustum(inverse(_projMatrix));
	const vec3* frustum = m_localFrustum.m_vertices;
	m_up    = frustum[0].y;
	m_down  = frustum[3].y;
	m_left  = frustum[3].x;
	m_right = frustum[1].x;
	m_near  = frustum[0].z;
	m_far   = frustum[4].z;
	if (!getProjFlag(ProjFlag_Orthographic)) {
		m_up    /= m_near;
		m_down  /= m_near;
		m_left  /= m_near;
		m_right /= m_near;
	}
	m_projDirty = false;
}

void Camera::setPerspective(float _fovVertical, float _aspect, float _near, float _far, uint32 _flags)
{
	float halfFovVertical   = _fovVertical * 0.5f;
	float halfFovHorizontal = halfFovVertical * _aspect;
	setProj(
		halfFovVertical,   -halfFovHorizontal,
		halfFovHorizontal, -halfFovHorizontal,
		_near, _far,
		_flags
		);
	APT_ASSERT(!getProjFlag(ProjFlag_Orthographic)); // _flags were invalid
}

void Camera::setPerspective(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags)
{
	setProj(_up, _down, _right, _left, _near, _far, _flags);
	APT_ASSERT(!getProjFlag(ProjFlag_Orthographic)); // _flags were invalid 
}

void Camera::setAspect(float _aspect)
{
	setProjFlag(ProjFlag_Asymmetrical, false);
	float horizontal = _aspect * (fabs(m_up) + fabs(m_down));
	m_left = horizontal * 0.5f;
	m_right = -m_left;
	m_projDirty = true;
}

void Camera::update()
{
	updateView();
	if (m_projDirty) {
		updateProj();	
	}
}

void Camera::updateView()
{
	if (m_parent) {
		m_world = m_parent->getWorldMatrix();
	}
	m_view = inverse(m_world); // \todo cheaper than inverse?
	m_viewProj = m_proj * m_view;
	m_worldFrustum = m_localFrustum;
	m_worldFrustum.transform(m_world);
}

void Camera::updateProj()
{
	m_localFrustum = Frustum(m_up, m_down, m_left, m_right, m_near, m_far, getProjFlag(ProjFlag_Orthographic));
	bool infinite = getProjFlag(ProjFlag_Infinite);
	bool reversed = getProjFlag(ProjFlag_Reversed);

	if (getProjFlag(ProjFlag_Orthographic)) {
		m_proj[0][0] = 2.0f / (m_right - m_left);
		m_proj[1][1] = 2.0f / (m_up - m_down);
		m_proj[2][2] = -2.0f / (m_far - m_near);
		m_proj[0][3] = -((m_right + m_left) / (m_right - m_left));
		m_proj[1][3] = -((m_up + m_down) / (m_up - m_down));
		m_proj[2][3] = -((m_far + m_near) / (m_far - m_near));
		m_proj[3][3] = 1.0f;

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
		m_proj[3][3] = 0.0f;

		if (infinite && reversed) {
			m_proj[2][2] = 0.0f;
			m_proj[3][2] = n;

		} else if (infinite) {
			m_proj[2][2] = -1.0f;
			m_proj[3][2] = -2.0f * n;

		} else if (reversed) {
		 // \todo finite reverse projection broken?
			m_proj[2][2] = n / (n - f);
			m_proj[3][2] = m_proj[2][2] == 0.0f ? n : (f * n / (f - n));

		} else {			
			m_proj[2][2] = (n + f) / (n - f);
			m_proj[3][2] = (2.0f * n * f) / (n - f);

		}
	}

	if (infinite) {
		m_localFrustum.m_planes[Frustum::kFar] = m_localFrustum.m_planes[Frustum::kNear];
	}

	m_projDirty = false;
}

// --
/*
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
*/
