#pragma once
#ifndef frm_Camera_h
#define frm_Camera_h

#include <frm/def.h>
#include <frm/geom.h>
#include <frm/math.h>

namespace frm
{

////////////////////////////////////////////////////////////////////////////////
/// \class Camera
/// Projection is defined either by 4 angles (radians) from the view axis for
/// perspective projections, or 4 offsets (world units) from the view origin for
/// parallel projections, plus a near/far clipping plane. 
////////////////////////////////////////////////////////////////////////////////
class Camera
{
public:

	/// Symmetrical perspective projection from a vertical fov + viewport aspect
	/// ratio.
	Camera(
		float _aspect      = 1.0f,
		float _fovVertical = 0.873f, // 60 degrees
		float _clipNear    = 0.1f,
		float _clipFar     = 1000.0f
		)
		: m_up(tan(_fovVertical * 0.5f))
		, m_down(m_up)
		, m_left(m_up * _aspect)
		, m_right(m_up * _aspect)
		, m_clipNear(_clipNear)
		, m_clipFar(_clipFar)
		, m_isOrtho(false)
		, m_isSymmetric(true)
		, m_projDirty(true)
		, m_world(1.0f)
		, m_localFrustum(m_up, m_down, m_left, m_right, m_clipNear, m_clipFar)
	{
	}

	/// General perspective projection from 4 angles (position, radians) relative
	/// to the view axis.
	Camera(
		float _fovUp,
		float _fovDown,
		float _fovLeft,
		float _fovRight,
		float _clipNear,
		float _clipFar
		)
		: m_up(tan(_fovUp))
		, m_down(tan(_fovDown))
		, m_left(tan(_fovLeft))
		, m_right(tan(_fovRight))
		, m_clipNear(_clipNear)
		, m_clipFar(_clipFar)
		, m_isOrtho(false)
		, m_projDirty(true)
		, m_world(1.0f)
		, m_localFrustum(m_up, m_down, m_left, m_right, m_clipNear, m_clipFar)
	{
	}

	bool           isOrtho() const                    { return m_isOrtho; }
	bool           isSymmetric() const                { return m_isSymmetric; }
	void           setIsOrtho(bool _ortho);
	void           setIsSymmetric(bool _symmetric);

	/// Asymmetrical perspective projection properties.
	void           setFovUp(float _radians)           { m_up = tan(_radians); m_isSymmetric = false; m_projDirty = true; }
	void           setFovDown(float _radians)         { m_down = tan(_radians); m_isSymmetric = false; m_projDirty = true; }
	void           setFovLeft(float _radians)         { m_left = tan(_radians); m_isSymmetric = false; m_projDirty = true; }
	void           setFovRight(float _radians)        { m_right = tan(_radians); m_isSymmetric = false; m_projDirty = true; }
	void           setTanFovUp(float _tan)            { m_up = _tan; m_isSymmetric = false; m_projDirty = true; }
	void           setTanFovDown(float _tan)          { m_down = _tan; m_isSymmetric = false; m_projDirty = true; }
	void           setTanFovLeft(float _tan)          { m_left = _tan; m_isSymmetric = false; m_projDirty = true; }
	void           setTanFovRight(float _tan)         { m_right = _tan; m_isSymmetric = false; m_projDirty = true; }
	float          getTanFovUp() const                { return m_up; }
	float          getTanFovDown() const              { return m_down; }
	float          getTanFovLeft() const              { return m_left; }
	float          getTanFovRight() const             { return m_right; }
	
	/// Symmetrical perspective projection properties.
	void           setVerticalFov(float _radians);
	void           setHorizontalFov(float _radians);
	void           setAspect(float _aspect);
	float          getVerticalFov() const             { return atan(m_up + m_down); }
	float          getHorizontalFov() const           { return atan(m_left + m_right); }
	float          getAspect() const                  { return (m_left + m_right) / (m_up + m_down); }

	void           setClipNear(float _near)           { m_clipNear = _near; m_projDirty = true; }
	void           setClipFar(float _far)             { m_clipFar = _far; m_projDirty = true; }
	float          getClipNear() const                { return m_clipNear; }
	float          getClipFar() const                 { return m_clipFar; }

	void           setWorldMatrix(const mat4& _world) { m_world = _world; }
	void           setProjMatrix(const mat4& _proj);

	void           setNode(Node* _node)               { m_node = _node; }
	Node*          getNode()                          { return m_node; }
	const Frustum& getLocalFrustum() const            { return m_localFrustum; }
	const Frustum& getWorldFrustum() const            { return m_worldFrustum; }
	const mat4&    getWorldMatrix() const             { return m_world; }
	const mat4&    getViewMatrix() const              { return m_view; }
	const mat4&    getProjMatrix() const              { return m_proj; }
	const mat4&    getViewProjMatrix() const          { return m_viewProj; }

	/// Extract position from world matrix.
	vec3           getPosition() const                { return vec3(apt::column(m_world, 3)); }
	/// Extract view direction from world matrix.
	/// \note Projection is along -z, hence the negation.
	vec3           getViewVector() const              { return -vec3(apt::column(m_world, 2)); }

	/// Construct the view matrix, view-projection matrix and world space frustum,
	/// plus the projection matrix (if dirty).
	void           build();

private:
	Node*   m_node;         //< Parent node, copy world matrix if non-null.
	mat4    m_world;
	mat4    m_view;
	mat4    m_proj;
	mat4    m_viewProj;

	float   m_up;           //< Tangent of the fov angles for perspective projections, offsets on the view rectangle for orthographic projections.
	float   m_down;
	float   m_left;
	float   m_right;
	float   m_clipNear;     //< Clip plane distances (positive).
	float   m_clipFar;

	bool    m_isOrtho;      //< If projection is orthographic.
	bool    m_isSymmetric;  //< If projection is symmetrical.
	bool    m_projDirty;    //< If projection matrix/local frustum is dirty.

	Frustum m_localFrustum; //< Untransformed frustum from the projection settings.
	Frustum m_worldFrustum; //< World space frustum (world matrix * local frustum)

}; // class Camera

} // namespace frm

#endif // frm_Camera_h
