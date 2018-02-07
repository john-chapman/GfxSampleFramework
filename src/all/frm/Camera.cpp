#include <frm/Camera.h>
#include <frm/Buffer.h>
#include <frm/Scene.h>

#include <apt/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;
using namespace apt;

// PUBLIC

Camera::Camera(Node* _parent)
{
	defaultInit();
	m_parent = _parent;
}

Camera::~Camera()
{
	if (m_gpuBuffer) {
		Buffer::Destroy(m_gpuBuffer);
	}
}

Camera::Camera(const Camera& _rhs)
{
	memcpy(this, &_rhs, sizeof(Camera));
	if (_rhs.m_gpuBuffer) {
		m_gpuBuffer = nullptr;
		updateGpuBuffer();
	}
}
Camera::Camera(Camera&& _rhs)
{
	if (this != &_rhs) {
		memcpy(this, &_rhs, sizeof(Camera));
		_rhs.defaultInit();
	}
}
Camera& Camera::operator=(Camera&& _rhs)
{
	if (&_rhs != this) {
		swap(*this, _rhs);
	}
	return *this;
}

void frm::swap(Camera& _a_, Camera& _b_)
{
	using eastl::swap;
	
	swap(_a_.m_projFlags,    _b_.m_projFlags);
	swap(_a_.m_projDirty,    _b_.m_projDirty);
	swap(_a_.m_up,           _b_.m_up);
	swap(_a_.m_down,         _b_.m_down);
	swap(_a_.m_right,        _b_.m_right);
	swap(_a_.m_left,         _b_.m_left);
	swap(_a_.m_near,         _b_.m_near);
	swap(_a_.m_far,          _b_.m_far);
	swap(_a_.m_parent,       _b_.m_parent);
	swap(_a_.m_world,        _b_.m_world);
	swap(_a_.m_view,         _b_.m_view);
	swap(_a_.m_proj,         _b_.m_proj);
	swap(_a_.m_viewProj,     _b_.m_viewProj);
	swap(_a_.m_inverseProj,  _b_.m_inverseProj);
	swap(_a_.m_aspectRatio,  _b_.m_aspectRatio);
	swap(_a_.m_localFrustum, _b_.m_localFrustum);
	swap(_a_.m_worldFrustum, _b_.m_worldFrustum);
	swap(_a_.m_gpuBuffer,    _b_.m_gpuBuffer);
}

bool frm::Serialize(apt::Serializer& _serializer_, Camera& _camera_)
{
 // note that the parent node doesn't get written here - the scene serializes the camera params *within* a node so it's not required
	_serializer_.value(_camera_.m_up,    "Up");
	_serializer_.value(_camera_.m_down,  "Down");
	_serializer_.value(_camera_.m_right, "Right");
	_serializer_.value(_camera_.m_left,  "Left");
	_serializer_.value(_camera_.m_near,  "Near");
	_serializer_.value(_camera_.m_far,   "Far");
	_serializer_.value(_camera_.m_world, "WorldMatrix");

	bool orthographic = _camera_.getProjFlag(Camera::ProjFlag_Orthographic);
	bool asymmetrical = _camera_.getProjFlag(Camera::ProjFlag_Asymmetrical);
	bool infinite     = _camera_.getProjFlag(Camera::ProjFlag_Infinite);
	bool reversed     = _camera_.getProjFlag(Camera::ProjFlag_Reversed);
	_serializer_.value(orthographic, "Orthographic");
	_serializer_.value(asymmetrical, "Asymmetrical");
	_serializer_.value(infinite,     "Infinite");
	_serializer_.value(reversed,     "Reversed");

	bool hasGpuBuffer = _camera_.m_gpuBuffer != nullptr;
	_serializer_.value(hasGpuBuffer, "HasGpuBuffer");

	if (_serializer_.getMode() == apt::Serializer::Mode_Read) {
		_camera_.setProjFlag(Camera::ProjFlag_Perspective,  !orthographic);
		_camera_.setProjFlag(Camera::ProjFlag_Orthographic, orthographic);
		_camera_.setProjFlag(Camera::ProjFlag_Asymmetrical, asymmetrical);
		_camera_.setProjFlag(Camera::ProjFlag_Infinite,     orthographic ? false : infinite);
		_camera_.setProjFlag(Camera::ProjFlag_Reversed,     reversed);
		
		_camera_.m_aspectRatio = abs(_camera_.m_right - _camera_.m_left) / abs(_camera_.m_up - _camera_.m_down);
		_camera_.m_projDirty = true;

		if (hasGpuBuffer) {
			_camera_.updateGpuBuffer();
		}
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

	float up    = orthographic ? m_up    : Degrees(atan(m_up));
	float down  = orthographic ? m_down	 : Degrees(atan(m_down));
	float right = orthographic ? m_right : Degrees(atan(m_right));
	float left  = orthographic ? m_left	 : Degrees(atan(m_left));
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
				setPerspective(Radians(fovVertical), m_aspectRatio, m_near, m_far, m_projFlags);
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
		m_up    = orthographic ? up    : tanf(Radians(up));
		m_down  = orthographic ? down  : tanf(Radians(down));
		m_right = orthographic ? right : tanf(Radians(right));
		m_left  = orthographic ? left  : tanf(Radians(left));
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
		#if FRM_NDC_Z_ZERO_TO_ONE
			vec4(-1.0f,  1.0f, 0.0f,  1.0f),
			vec4( 1.0f,  1.0f, 0.0f,  1.0f),
			vec4( 1.0f, -1.0f, 0.0f,  1.0f),
			vec4(-1.0f, -1.0f, 0.0f,  1.0f),
		#else
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
		lvt[i] = v.xyz();
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
	m_projFlags   = _flags | ProjFlag_Perspective;
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
	setProj(_up, _down, _right, _left, _near, _far, _flags | ProjFlag_Perspective);
	APT_ASSERT(!getProjFlag(ProjFlag_Orthographic)); // invalid _flags
}

void Camera::setOrtho(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags)
{
	_flags &= ~ProjFlag_Infinite; // disallow infinite ortho projection
	setProj(_up, _down, _right, _left, _near, _far, _flags | ProjFlag_Orthographic);
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

void Camera::lookAt(const vec3& _from, const vec3& _to, const vec3& _up)
{
	m_world = AlignZ(normalize(_from - _to), _up); // swap to/from, align -Z
	m_world[3] = vec4(_from, 1.0f);
}

void Camera::update()
{
	if (m_projDirty) {
		updateProj();	
	}
	updateView();
	if (m_gpuBuffer) {
		updateGpuBuffer();
	}
}

void Camera::updateView()
{
	if (m_parent) {
		m_world = m_parent->getWorldMatrix();
	}
	m_view = AffineInverse(m_world);
	m_viewProj = m_proj * m_view;
	m_worldFrustum = m_localFrustum;
	m_worldFrustum.transform(m_world);
}

void Camera::updateProj()
{
 	m_localFrustum = Frustum(m_up, m_down, m_right, m_left, m_near, m_far, getProjFlag(ProjFlag_Orthographic));
	bool infinite = getProjFlag(ProjFlag_Infinite) && !getProjFlag(ProjFlag_Orthographic); // infinite ortho projection not supported
	bool reversed = getProjFlag(ProjFlag_Reversed);

	float t = m_localFrustum.m_vertices[0].y;
	float b = m_localFrustum.m_vertices[3].y;
	float l = m_localFrustum.m_vertices[0].x;
	float r = m_localFrustum.m_vertices[1].x;
	float n = m_near;
	float f = m_far;

	float scale = -1.0f; // view space z direction \todo expose? need to also modify getViewVector(), lookAt(), frustum ctors, shader code

	if (getProjFlag(ProjFlag_Orthographic)) {
		m_proj = mat4(
			2.0f / (r - l), 0.0f,           0.0f,    scale * (l + r) / (r - l),
			0.0f,           2.0f / (t - b), 0.0f,    scale * (b + t) / (t - b),
			0.0f,           0.0f,           0.0f,    0.0f,
			0.0f,           0.0f,           0.0f,    1.0f
			);

	 	if (reversed) {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = -scale * 1.0f / (f - n);
				m_proj[3][2] = f / (f - n);
			#else
				m_proj[2][2] = -scale * 2.0f / (f - n);
				m_proj[3][2] = (f + n) / (f - n);
			#endif
		} else {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = scale * 1.0f / (f - n);
				m_proj[3][2] = n / (n - f);
			#else
				m_proj[2][2] = scale * 2.0f / (f - n);
				m_proj[3][2] = (n + f) / (n - f);
			#endif
		}

	} else {
	 // oblique perspective
		m_proj = mat4(
			(2.0f * n) / (r - l), 0.0f,                 -scale * (l + r) / (r - l), 0.0f,
			0.0f,                 (2.0f * n) / (t - b), -scale * (b + t) / (t - b), 0.0f,
			0.0f,                 0.0f,                  0.0f,                      0.0f,
			0.0f,                 0.0f,                  scale,                     0.0f
			);
		if (infinite && reversed) {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = 0.0;
				m_proj[3][2] = n;
			#else
				m_proj[2][2] = -scale;
				m_proj[3][2] = 2.0f * n;
			#endif
			//m_proj[2][2] = ndcGl ? -scale : 0.0f;
			//m_proj[3][2] = (n + offset);

		} else if (infinite) {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = scale;
				m_proj[3][2] = -n;
			#else
				m_proj[2][2] = scale;
				m_proj[3][2] = -2.0f * n;
			#endif
			//m_proj[2][2] = scale;
			//m_proj[3][2] = -(n + offset);

		} else if (reversed) {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = -scale * n / (f - n);
				m_proj[3][2] = n * f / (f - n);
			#else
				m_proj[2][2] = -scale * (f + n) / (f - n);
				m_proj[3][2] = 2.0f * n * f / (f - n);
			#endif
			//m_proj[2][2] = -scale * (ndcGl? (f + offset) : n) / (f - n);
			//m_proj[3][2] = (n + offset) * f / (f - n);

		} else {
			#if FRM_NDC_Z_ZERO_TO_ONE
				m_proj[2][2] = scale * f / (f - n);
				m_proj[3][2] = n * f / (n - f);
			#else
				m_proj[2][2] = scale * (f + n) / (f - n);
				m_proj[3][2] = 2.0f * n * f / (n - f);
			#endif
			//m_proj[2][2] = scale * (f + offset) / (f - n);
			//m_proj[3][2] = -(n + offset) * f / (f - n);

		}
	}

	m_inverseProj = inverse(m_proj); // \todo avoid this, compute the inverse directly
	m_projDirty = false;
}

void Camera::updateGpuBuffer(Buffer* _buffer_)
{
	Buffer*	buf = m_gpuBuffer;
	if (_buffer_) {
		APT_ASSERT(_buffer_->getSize() >= sizeof(GpuBuffer));
		buf = _buffer_;
	} else if (!buf) {
		m_gpuBuffer = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(GpuBuffer), GL_DYNAMIC_STORAGE_BIT);
		m_gpuBuffer->setName("_bfCamera");
		buf = m_gpuBuffer;
	}
	GpuBuffer data;
	data.m_world           = m_world;
	data.m_view            = m_view;
	data.m_proj            = m_proj;
	data.m_viewProj        = m_viewProj;
	data.m_inverseProj     = m_inverseProj;
	data.m_inverseViewProj = m_world * m_inverseProj;
	data.m_up              = m_up;
	data.m_down            = m_down;
	data.m_right           = m_right;
	data.m_left            = m_left;
	data.m_near            = m_near;
	data.m_far             = m_far;
	data.m_aspectRatio     = m_aspectRatio;
	data.m_projFlags       = m_projFlags;
	buf->setData(sizeof(GpuBuffer), &data);
}

float Camera::getDepthV(float _depth) const
{	
	#if FRM_NDC_Z_ZERO_TO_ONE
		float zndc = _depth;
	#else
		float zndc = _depth * 2.0f - 1.0f;
	#endif	
	if (getProjFlag(ProjFlag_Perspective)) {
		return m_proj[3][2] / (m_proj[2][3] * zndc - m_proj[2][2]);
	} else {
		return (zndc - m_proj[3][2]) / m_proj[2][2];
	}
}

void Camera::defaultInit()
{
	m_projFlags          = ProjFlag_Default;
	m_projDirty          = true;
	m_up                 = 1.0f;
	m_down               = -1.0f;
	m_right              = 1.0f;
	m_left               = -1.0f;
	m_near               = 1.0f;
	m_far                = 1000.0f;
	m_parent             = nullptr;
	m_world              = mat4(identity);
	m_proj               = mat4(identity);
	m_aspectRatio        = 1.0f;
	m_gpuBuffer          = nullptr;
}
