#pragma once
#ifndef frm_Camera_h
#define frm_Camera_h

#include <frm/core/def.h>
#include <frm/core/geom.h>
#include <frm/core/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Camera
// Projection is defined either by 4 angles (±radians) from the view axis for
// perspective projections, or 4 offsets (±world units) from the view origin for
// parallel projections, plus a near/far clipping plane.
//
// Projection flags control how the projection matrix is set up - this must be 
// congruent with the graphics API clip control settings and depth test, as
// well as any shader operations which might be affected (depth linearization,
// shadow tests, etc.).
//
// ProjFlag_Reversed will give better precision when using a floating point depth
// depth buffer, in which case FRM_NDC_Z_ZERO_TO_ONE should be defined.
// 
// ProjFlag_Infinite does not affect the frustum far plane, so m_far should be
// be set to a distance appropriate for culling.
////////////////////////////////////////////////////////////////////////////////
class Camera
{
public:
	enum ProjFlag
	{
		ProjFlag_Perspective  = 1 << 0,
		ProjFlag_Orthographic = 1 << 1,
		ProjFlag_Asymmetrical = 1 << 2,
		ProjFlag_Infinite     = 1 << 3, // only for perspective projections
		ProjFlag_Reversed     = 1 << 4,

		ProjFlag_Default      = ProjFlag_Infinite // symmetrical infinite perspective projection
	};

	Camera(Node* _parent = nullptr);
	~Camera();
	
	Camera(const Camera& _rhs);
	Camera(Camera&& _rhs);
	Camera& operator=(Camera&& _rhs);
	friend void swap(Camera& _a_, Camera& _b_);


	friend bool Serialize(apt::Serializer& _serializer_, Camera& _camera_);
	void edit();
	
	// Set projection params. For a perspective projection _up/_down/_right/_near are ­±radians from the view
	// axis, for an orthographic projection they are offsets from the center of the projection plane.
	void setProj(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags = ProjFlag_Default);
	// Recover params directly from a projection matrix. 
	void setProj(const mat4& _projMatrix, uint32 _flags = ProjFlag_Default);
	// Set a symmetrical perspective projection.
	void setPerspective(float _fovVertical, float _aspect, float _near, float _far, uint32 _flags = ProjFlag_Default);
	// Set an asymmetrical (oblique) perspective projection.
	void setPerspective(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags = ProjFlag_Default | ProjFlag_Asymmetrical);
	// Set an asymmetrical orthographic projection. _up/_down/_left/_right are ­±world units from the view plane origin.
	void setOrtho(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags = ProjFlag_Orthographic | ProjFlag_Asymmetrical);
	// Force a symmetrical projection with the specified aspect ratio.
	void setAspectRatio(float _aspectRatio);

	// Set a world matrix with position = _from and -Z = (_to - _from).
	void lookAt(const vec3& _from, const vec3& _to, const vec3& _up = vec3(0.0f, 1.0f, 0.0f));

	
	// Update the derived members (view matrix + world frustum, proj matrix + local frustum if dirty).
	// Update m_gpuBuffer if != null (else call updateGpuBuffer()).
	void update();
	// Update the view matrix + world frustum. Called by update().
	void updateView();
	// Update the projection matrix + local frustum. Called by update().
	void updateProj();
	// Fill _buffer_ with camera members, else alloc/update m_gpuBuffer.
	void updateGpuBuffer(Buffer* _buffer_ = nullptr);
	

	// Proj flag helpers.
	bool getProjFlag(ProjFlag _flag) const        { return (m_projFlags & _flag) != 0; }
	void setProjFlag(ProjFlag _flag, bool _value) { m_projFlags = _value ? (m_projFlags | _flag) : (m_projFlags & ~_flag); m_projDirty = true; }
	
	// Extract position from world matrix.
	vec3 getPosition() const                      { return m_world[3].xyz();  }
	// Extract view direction from world matrix. Projection is along -z, hence the negation.
	vec3 getViewVector() const                    { return -m_world[2].xyz(); }

	// Recover view space depth from a depth buffer value.
	// This may return INF for infinite perspective projections.
	float getDepthV(float _depth) const;

	uint32  m_projFlags;      // Combination of ProjFlag_ enums.
	bool    m_projDirty;      // Whether to rebuild the projection matrix/local frustum during update().
	
	float   m_up;             // Projection params are interpreted depending on the projection flags;
	float   m_down;           //  for a perspective projection they are ±tan(angle from the view axis),
	float   m_right;          //  for ortho projections they are ­±offset from the center of the projection
	float   m_left;           //  plane.
	float   m_near;
	float   m_far;		

	Node*   m_parent;         // Overrides world matrix if set.
	mat4    m_world;
	mat4    m_view;
	mat4    m_proj;
	mat4    m_viewProj;
	mat4    m_inverseProj;
	float   m_aspectRatio;    // Derived from the projection parameters.

	Frustum m_localFrustum;   // Derived from the projection parameters.
	Frustum m_worldFrustum;   // World space frustum (use for culling).

	struct GpuBuffer
	{
		mat4   m_world;
		mat4   m_view;
		mat4   m_proj;
		mat4   m_viewProj;
		mat4   m_inverseProj;
		mat4   m_inverseViewProj;
		float  m_up;
		float  m_down;
		float  m_right;
		float  m_left;
		float  m_near;
		float  m_far;
		float  m_aspectRatio;
		uint32 m_projFlags;
	};
	Buffer* m_gpuBuffer;

private:
	void defaultInit();

}; // class Camera

} // namespace frm

#endif // frm_Camera_h
