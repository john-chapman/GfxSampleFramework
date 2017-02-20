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

		ProjFlag_Default      = 0        // symmetrical perspective projection
	};

	Camera(Node* _parent = nullptr);

	bool serialize(apt::JsonSerializer& _serializer_);
	void edit();
	
	void setProj(float _up, float _down, float _right, float _left, float _near, float _far, uint32 _flags = ProjFlag_Default);
	void setProj(const mat4& _projMatrix, uint32 _flags = ProjFlag_Default);
	
	void setPerspective(float _fovVertical, float _aspect, float _near, float _far, uint32 _flags = ProjFlag_Default);
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


	uint32  m_projFlags;      // Combination of ProjFlag enums.
	bool    m_projDirty;      // Whether to rebuild the projection matrix/local frustum during update().
	
	float   m_up;             // Projection params are interpreted depending on the projection flags;
	float   m_down;           //  for a perspective projections they are ±tan(angle from the view axis),
	float   m_right;          //  for ortho projections they are ­±offset from the projection plane.
	float   m_left;
	float   m_near;
	float   m_far;		

	Node*   m_parent;         // Overrides world matrix if set.
	mat4    m_world;
	mat4    m_view;
	mat4    m_proj;
	mat4    m_viewProj;

	Frustum m_localFrustum;   // Derived from the projection parameters.
	Frustum m_worldFrustum;   // World space frustum (use for culling).

// ---
/*public:

	// Symmetrical perspective projection from a vertical fov + viewport aspect ratio.
	Camera(
		float _aspect      = 1.0f,
		float _fovVertical = 0.873f, // 60 degrees
		float _near        = 0.1f,
		float _far         = 1000.0f
		);

	// General perspective/ortho projection from 4 angles/positions (positive, relative to the view axis).
	Camera(
		float _up,
		float _down,
		float _left,
		float _right,
		float _near,
		float _far,
		bool  _isOrtho = false
		);

	bool           getProjFlag(ProjFlag _flag) const        { return (m_projFlags & _flag) != 0; }
	void           setProjFlag(ProjFlag _flag, bool _value) { m_projFlags = _value ? (m_projFlags | _flag) : (m_projFlags & ~_flag); m_projDirty = true; }
	void           setProjFlags(uint32 _flags)              { m_projFlags = _flags; m_projDirty = true; }

	// Asymmetrical perspective projection properties.
	void           setFovUp(float _radians)                 { m_up = tan(_radians); m_projDirty = true;    }
	void           setFovDown(float _radians)               { m_down = tan(_radians); m_projDirty = true;  }
	void           setFovLeft(float _radians)               { m_left = tan(_radians); m_projDirty = true;  }
	void           setFovRight(float _radians)              { m_right = tan(_radians); m_projDirty = true; }
	void           setTanFovUp(float _tan)                  { m_up = _tan; m_projDirty = true;             }
	void           setTanFovDown(float _tan)                { m_down = _tan; m_projDirty = true;           }
	void           setTanFovLeft(float _tan)                { m_left = _tan; m_projDirty = true;           }
	void           setTanFovRight(float _tan)               { m_right = _tan; m_projDirty = true;          }
	float          getTanFovUp() const                      { return m_up;    }
	float          getTanFovDown() const                    { return m_down;  }
	float          getTanFovLeft() const                    { return m_left;  }
	float          getTanFovRight() const                   { return m_right; }
	
	// Symmetrical perspective projection properties.
	void           setVerticalFov(float _radians);
	void           setHorizontalFov(float _radians);
	void           setAspect(float _aspect);
	float          getVerticalFov() const                   { return atan(m_up + m_down); }
	float          getHorizontalFov() const                 { return atan(m_left + m_right); }
	float          getAspect() const                        { return (m_left + m_right) / (m_up + m_down); }

	void           setNear(float _near)                     { m_near = _near; m_projDirty = true; }
	void           setFar(float _far)                       { m_far = _far; m_projDirty = true; }
	float          getNear() const                          { return m_near; }
	float          getFar() const                           { return m_far; }

	void           setWorldMatrix(const mat4& _world)       { m_world = _world; }
	void           setProjMatrix(const mat4& _proj, uint32 _flags = ProjFlag_Default);

	void           setNode(Node* _node)                     { m_node = _node;        }
	Node*          getNode()                                { return m_node;         }
	const Frustum& getLocalFrustum() const                  { return m_localFrustum; }
	const Frustum& getWorldFrustum() const                  { return m_worldFrustum; }
	const mat4&    getWorldMatrix() const                   { return m_world;        }
	const mat4&    getViewMatrix() const                    { return m_view;         }
	const mat4&    getProjMatrix() const                    { return m_proj;         }
	const mat4&    getViewProjMatrix() const                { return m_viewProj;     }

	// Extract position from world matrix.
	vec3           getPosition() const                { return vec3(apt::column(m_world, 3)); }
	// Extract view direction from world matrix. Projection is along -z, hence the negation.
	vec3           getViewVector() const              { return -vec3(apt::column(m_world, 2)); }

	// Construct the view matrix, view-projection matrix and world space frustum, plus the projection matrix (if dirty).
	void           build();

	bool           serialize(apt::JsonSerializer& _serializer_);

private:
	Node*   m_node;         // Parent node, copy world matrix if non-null.

	float   m_up;           // Tangent of the fov angles for perspective projections, offsets on the view rectangle for orthographic projections.
	float   m_down;
	float   m_left;
	float   m_right;
	float   m_near;
	float   m_far;

	mat4    m_world;        // Can be overwritten by the parent matrix if m_node != null.
	mat4    m_view;
	mat4    m_proj;
	mat4    m_viewProj;

	uint32  m_projFlags;    // Combination of ProjFlags, controls how the projection matrix is constructed.
	bool    m_projDirty;    // If projection matrix/local frustum is dirty.

	Frustum m_localFrustum; // Untransformed frustum from the projection settings.
	Frustum m_worldFrustum; // World space frustum (world matrix * local frustum)

	void buildProj();
*/
}; // class Camera

} // namespace frm

#endif // frm_Camera_h
