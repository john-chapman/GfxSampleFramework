#pragma once
#ifndef frm_Camera_h
#define frm_Camera_h

#include <frm/def.h>
#include <frm/geom.h>
#include <frm/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Camera
/// Projection is defined either by 4 angles (radians) from the view axis for
/// perspective projections, or 4 offsets (world units) from the view origin for
/// parallel projections, plus a near/far clipping plane.
///
/// Enable ProjFlag_Reversed for better precision when using a floating points
/// depth buffer - in this case the following setup is required for OpenGL:
///		glDepthClear(0.0f);
///		glDepthFunc(GL_GREATER);
///		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
/// 
/// Note than ProjFlag_Infinite does not affect the frustum far plane, so m_far
/// should be set to a distance appropriate for culling.
////////////////////////////////////////////////////////////////////////////////
class Camera
{
public:
	enum ProjFlag
	{
		ProjFlag_Orthographic = 1 << 0,
		ProjFlag_Asymmetrical = 1 << 1,
		ProjFlag_Infinite     = 1 << 2,
		ProjFlag_Reversed     = 1 << 3,

		ProjFlag_Default      = ProjFlag_Infinite // symmetrical infinite perspective projection
	};

	Camera(Node* _parent = nullptr);

	bool serialize(apt::JsonSerializer& _serializer_);
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
	
	float getAspect() const               { return (fabs(m_right) + fabs(m_left)) / (fabs(m_up) + fabs(m_down)); }
	void  setAspect(float _aspect);  // forces a symmetrical projection

	
	// Update the derived members (view matrix + world frustum, proj matrix + local frustum if dirty).
	void update();
	// Update the view matrix + world frustum. Called by update().
	void updateView();
	// Update the projection matrix + local frustum. Called by update().
	void updateProj();
	

	// Proj flag helpers.
	bool getProjFlag(ProjFlag _flag) const        { return (m_projFlags & _flag) != 0; }
	void setProjFlag(ProjFlag _flag, bool _value) { m_projFlags = _value ? (m_projFlags | _flag) : (m_projFlags & ~_flag); m_projDirty = true; }
	
	// Extract position from world matrix.
	vec3 getPosition() const    { return vec3(apt::column(m_world, 3));  }
	// Extract view direction from world matrix. Projection is along -z, hence the negation.
	vec3 getViewVector() const  { return -vec3(apt::column(m_world, 2)); }


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

	Frustum m_localFrustum;   // Derived from the projection parameters.
	Frustum m_worldFrustum;   // World space frustum (use for culling).

}; // class Camera

} // namespace frm

#endif // frm_Camera_h
