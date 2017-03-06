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
	, m_near(1.0f)
	, m_far(1000.0f)
	, m_parent(_parent)
	, m_world(1.0f)
	, m_proj(0.0f)
	, m_aspectRatio(1.0f)
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

	if (_serializer_.getMode() == JsonSerializer::Mode_Read) {
		setProjFlag(ProjFlag_Orthographic, orthographic);
		setProjFlag(ProjFlag_Asymmetrical, asymmetrical);
		setProjFlag(ProjFlag_Infinite,     infinite);
		setProjFlag(ProjFlag_Reversed,     reversed);
		
		m_aspectRatio = fabs(m_right - m_left) / fabs(m_up - m_down);
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
	if (!orthographic) {
		updated |= ImGui::Checkbox("Infinite", &infinite);
	}
	updated |= ImGui::Checkbox("Reversed", &reversed);

	float up    = orthographic ? m_up    : degrees(atanf(m_up));
	float down  = orthographic ? m_down	 : degrees(atanf(m_down));
	float right = orthographic ? m_right : degrees(atanf(m_right));
	float left  = orthographic ? m_left	 : degrees(atanf(m_left));
	float near  = m_near;
	float far   = m_far;

	
	if (orthographic) {
		if (asymmetrical) {
			updated |= ImGui::DragFloat("Up",    &up,    0.5f);
			updated |= ImGui::DragFloat("Down",  &down,  0.5f);
			updated |= ImGui::DragFloat("Right", &right, 0.5f);
			updated |= ImGui::DragFloat("Left",  &left,  0.5f);
		} else {
			float height = up * 2.0f;
			float width = right * 2.0f;
			updated |= ImGui::SliderFloat("Height", &height, 0.0f, 100.0f);
			updated |= ImGui::SliderFloat("Width",  &width,  0.0f, 100.0f);
			float aspect = width / height;
			updated |= ImGui::SliderFloat("Apect Ratio", &aspect, 0.0f, 4.0f);
			if (updated) {
				up = height * 0.5f;
				down = -up;
				right = up * aspect;
				left = -right;
			}
		}

	} else {
		if (asymmetrical) {
			updated |= ImGui::SliderFloat("Up",    &up,    -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Down",  &down,  -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Right", &right, -90.0f, 90.0f);
			updated |= ImGui::SliderFloat("Left",  &left,  -90.0f, 90.0f);
		} else {
			float fovVertical = up * 2.0f;
			if (ImGui::SliderFloat("FOV Vertical", &fovVertical, 0.0f, 180.0f)) {
				setPerspective(radians(fovVertical), m_aspectRatio, m_near, m_far, m_projFlags);
			}
			if (ImGui::SliderFloat("Apect Ratio", &m_aspectRatio, 0.0f, 2.0f)) {
				setAspectRatio(m_aspectRatio);
			}
			
		}
	}
	updated |= ImGui::SliderFloat("Near",  &near,   0.0f,  10.0f);
	updated |= ImGui::SliderFloat("Far",   &far,    0.0f,  1000.0f);

	if (ImGui::TreeNode("Raw Params")) {
		
		updated |= ImGui::DragFloat("Up",    &up,    0.5f);
		updated |= ImGui::DragFloat("Down",  &down,  0.5f);
		updated |= ImGui::DragFloat("Right", &right, 0.5f);
		updated |= ImGui::DragFloat("Left",  &left,  0.5f);
		updated |= ImGui::DragFloat("Near",  &near,  0.5f);
		updated |= ImGui::DragFloat("Far",   &far,   0.5f);

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("DEBUG")) {
		static const int kSampleCount = 256;
		static float zrange[2] = { -10.0f, fabs(m_far) + 1.0f };
		ImGui::SliderFloat2("Z Range", zrange, 0.0f, 100.0f);
		float zvalues[kSampleCount];
		for (int i = 0; i < kSampleCount; ++i) {
			float z = zrange[0] + ((float)i / (float)kSampleCount) * (zrange[1] - zrange[0]);
			vec4 pz = m_proj * vec4(0.0f, 0.0f, -z, 1.0f);
			zvalues[i] = pz.z / pz.w;
		}
		ImGui::PlotLines("Z", zvalues, kSampleCount, 0, 0, -1.0f, 1.0f, ImVec2(0.0f, 64.0f));
		
		Camera dbgCam;
		dbgCam.m_far = 10.0f;
		dbgCam.setProj(m_proj, m_projFlags);
		dbgCam.updateView();
		Frustum& dbgFrustum = dbgCam.m_worldFrustum;

		Im3d::PushDrawState();
		Im3d::PushMatrix(m_world);
			Im3d::SetSize(2.0f);
			Im3d::BeginLineLoop();
				Im3d::Vertex(dbgFrustum.m_vertices[0], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[1], Im3d::Color_Green);
				Im3d::Vertex(dbgFrustum.m_vertices[2], Im3d::Color_Green);
				Im3d::Vertex(dbgFrustum.m_vertices[3], Im3d::Color_Yellow);
			Im3d::End();
			Im3d::SetColor(Im3d::Color_Magenta);
			Im3d::BeginLineLoop();
				Im3d::Vertex(dbgFrustum.m_vertices[4], Im3d::Color_Magenta);
				Im3d::Vertex(dbgFrustum.m_vertices[5], Im3d::Color_Red);
				Im3d::Vertex(dbgFrustum.m_vertices[6], Im3d::Color_Red);
				Im3d::Vertex(dbgFrustum.m_vertices[7], Im3d::Color_Magenta);
			Im3d::End();
			Im3d::BeginLines();
				Im3d::Vertex(dbgFrustum.m_vertices[0], Im3d::Color_Yellow);
				Im3d::Vertex(dbgFrustum.m_vertices[4], Im3d::Color_Magenta);
				Im3d::Vertex(dbgFrustum.m_vertices[1], Im3d::Color_Green);
				Im3d::Vertex(dbgFrustum.m_vertices[5], Im3d::Color_Red);
				Im3d::Vertex(dbgFrustum.m_vertices[2], Im3d::Color_Green);
				Im3d::Vertex(dbgFrustum.m_vertices[6], Im3d::Color_Red);
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
	} else {
	 // perspective proj, params are ±tan(angle from the view axis)
		m_up    = tanf(_up);
		m_down  = tanf(_down);
		m_right = tanf(_right);
		m_left  = tanf(_left);
	}
	m_near  = _near;
	m_far   = _far;

	m_aspectRatio = fabs(_right - _left) / fabs(_up - _down);
	
	bool asymmetrical = fabs(fabs(_up) - fabs(_down)) > FLT_EPSILON || fabs(fabs(_right) - fabs(_left)) > FLT_EPSILON;
	setProjFlag(ProjFlag_Asymmetrical, asymmetrical);
}

void Camera::setProj(const mat4& _projMatrix, uint32 _flags)
{
	m_proj = _projMatrix; 
	m_projFlags = _flags;
	
 // transform an NDC box by the inverse matrix
	mat4 invProj = inverse(_projMatrix);
	static const vec4 lv[8] = {
		#if   defined(Camera_ClipD3D)
			vec4(-1.0f,  1.0f, 0.0f,  1.0f),
			vec4( 1.0f,  1.0f, 0.0f,  1.0f),
			vec4( 1.0f, -1.0f, 0.0f,  1.0f),
			vec4(-1.0f, -1.0f, 0.0f,  1.0f),
		#elif defined(Camera_ClipOGL)
			vec4(-1.0f,  1.0f, -1.0f,  1.0f),
			vec4( 1.0f,  1.0f, -1.0f,  1.0f),
			vec4( 1.0f, -1.0f, -1.0f,  1.0f),
			vec4(-1.0f, -1.0f, -1.0f,  1.0f),
		#endif
		vec4(-1.0f,  1.0f,  1.0f,  1.0f),
		vec4( 1.0f,  1.0f,  1.0f,  1.0f),
		vec4( 1.0f, -1.0f,  1.0f,  1.0f),
		vec4(-1.0f, -1.0f,  1.0f,  1.0f)
	};
	vec3 lvt[8];
	for (int i = 0; i < 8; ++i) {
		vec4 v = invProj * lv[i];
		if (!getProjFlag(ProjFlag_Orthographic)) {
			v /= v.w;
		}
		lvt[i] = vec3(v);
	}
 // replace far plane in the case of an infinite projection
	if (getProjFlag(ProjFlag_Infinite)) {
		for (int i = 4; i < 8; ++i) {
			lvt[i] = lvt[i - 4] * (m_far / -lvt[0].z);
		}
	}
	m_localFrustum.setVertices(lvt);

	const vec3* frustum = m_localFrustum.m_vertices;
	m_up    = frustum[0].y;
	m_down  = frustum[3].y;
	m_left  = frustum[3].x;
	m_right = frustum[1].x;
	if (!getProjFlag(ProjFlag_Orthographic)) {
		m_up    /= m_near;
		m_down  /= m_near;
		m_left  /= m_near;
		m_right /= m_near;
	}
	m_near  = frustum[0].z;
	m_far   = frustum[4].z;
	m_aspectRatio = fabs(m_right - m_left) / fabs(m_up - m_down);
	m_projDirty = false;
}

void Camera::setPerspective(float _fovVertical, float _aspect, float _near, float _far, uint32 _flags)
{
	m_projFlags   = _flags;
	m_aspectRatio = _aspect;
	m_up          = tanf(_fovVertical * 0.5f);
	m_down        = -m_up;
	m_right       = _aspect * m_up;
	m_left        = -m_right;
	m_near        = _near;
	m_far         = _far;
	m_projDirty   = true;
	setProjFlag(ProjFlag_Asymmetrical, false);
	APT_ASSERT(!getProjFlag(ProjFlag_Orthographic)); // invalid _flags
}

void Camera::setPerspective(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags)
{
	setProj(_up, _down, _right, _left, _near, _far, _flags);
	APT_ASSERT(!getProjFlag(ProjFlag_Orthographic)); // invalid _flags
}

void Camera::setOrtho(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags)
{
	_flags &= ~ProjFlag_Infinite; // disallow infinite ortho projection
	setProj(_up, _down, _right, _left, _near, _far, _flags);
	APT_ASSERT(getProjFlag(ProjFlag_Orthographic)); // invalid _flags 
}

void Camera::setAspectRatio(float _aspect)
{
	setProjFlag(ProjFlag_Asymmetrical, false);
	m_aspectRatio = _aspect;
	float horizontal = _aspect * fabs(m_up - m_down);
	m_right = horizontal * 0.5f;
	m_left = -m_right;
	m_projDirty = true;
}

void Camera::update()
{
	if (m_projDirty) {
		updateProj();	
	}
	updateView();
}

void Camera::updateView()
{
	if (m_parent) {
		m_world = m_parent->getWorldMatrix();
	}
	
	mat3 viewRotation = transpose(mat3(m_world));
	vec3 viewTranslation = -(viewRotation * vec3(column(m_world, 3)));
	m_view[0] = vec4(viewRotation[0], 0.0f);
	m_view[1] = vec4(viewRotation[1], 0.0f);
	m_view[2] = vec4(viewRotation[2], 0.0f);
	m_view[3] = vec4(viewTranslation, 1.0f);

	m_viewProj = m_proj * m_view;
	m_worldFrustum = m_localFrustum;
	m_worldFrustum.transform(m_world);
}

void Camera::updateProj()
{
 	m_proj = mat4(0.0f);
	m_localFrustum = Frustum(m_up, m_down, m_right, m_left, m_near, m_far, getProjFlag(ProjFlag_Orthographic));
	bool infinite = getProjFlag(ProjFlag_Infinite) && !getProjFlag(ProjFlag_Orthographic); // infinite ortho projection not supported
	bool reversed = getProjFlag(ProjFlag_Reversed);

	float t = m_localFrustum.m_vertices[0].y;
	float b = m_localFrustum.m_vertices[3].y;
	float l = m_localFrustum.m_vertices[0].x;
	float r = m_localFrustum.m_vertices[1].x;
	float n = m_near;
	float f = m_far;

 // \note both clip modes below assume a right-handed coordinates system (view axis along -z).
 // \todo infinite ortho projection probably not possible as it involves tampering with the w component
 // \todo generate an inverse projection matrix at the same time

	if (getProjFlag(ProjFlag_Orthographic)) {
		m_proj[0][0] = 2.0f / (r - l);
		m_proj[1][1] = 2.0f / (t - b);
		m_proj[2][0] = 0.0f;
		m_proj[2][1] = 0.0f;
		m_proj[2][3] = 0.0f;
		m_proj[3][0] = -(r + l) / (r - l);
		m_proj[3][1] = -(t + b) / (t - b);
		m_proj[3][3] = 1.0f;

	 	if (infinite && reversed) {
			#if   defined(Camera_ClipD3D)
			#elif defined(Camera_ClipOGL)
			#endif
		} else if (infinite) {
			#if   defined(Camera_ClipD3D)
			#elif defined(Camera_ClipOGL)
			#endif
		} else if (reversed) {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = 1.0f / (f - n);
				m_proj[3][2] = 1.0f - n / (n - f);
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = -2.0f / (n - f);
				m_proj[3][2] = -(n + f) / (n - f);
			#endif
		} else {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = -1.0f / (f - n);
				m_proj[3][2] = n / (n - f);
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = -2.0f / (f - n);
				m_proj[3][2] = -(f + n) / (f - n);
			#endif
		}

	} else {
	 // oblique perspective
		m_proj[0][0] = (2.0f * n) / (r - l);
		m_proj[1][1] = (2.0f * n) / (t - b);
		m_proj[2][0] = (r + l) / (r - l);
		m_proj[2][1] = (t + b) / (t - b);
		m_proj[2][3] = -1.0f;
		m_proj[3][3] = 0.0f;

		if (infinite && reversed) {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = 0.0f;
				m_proj[3][2] = n;
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = 1.0f;
				m_proj[3][2] = 2.0f * n;
			#endif
		} else if (infinite) {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = -1.0f;
				m_proj[3][2] = -n;
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = -1.0f;
				m_proj[3][2] = -2.0f * n;
			#endif
		} else if (reversed) {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = n / (f - n);
				m_proj[3][2] = (f * n) / (f - n);
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = (f + n) / (f - n);
				m_proj[3][2] = (2.0f * n * f) / (f - n);
			#endif
		} else {
			#if   defined(Camera_ClipD3D)
				m_proj[2][2] = f / (n - f);
				m_proj[3][2] = (n * f) / (n - f);
			#elif defined(Camera_ClipOGL)
				m_proj[2][2] = (n + f) / (n - f);
				m_proj[3][2] = (2.0f * n * f) / (n - f);
			#endif
		}
	}

	m_projDirty = false;
}
