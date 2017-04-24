#ifndef Camera_glsl
#define Camera_glsl

#include "shaders/def.glsl"

#define Camera_ProjFlag_Perspective  (0)
#define Camera_ProjFlag_Orthographic (1)
#define Camera_ProjFlag_Asymmetrical (2)
#define Camera_ProjFlag_Infinite     (4)
#define Camera_ProjFlag_Reversed     (8)

struct Camera_GpuBuffer
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
	uint   m_projFlags;
};
layout(std140) uniform _bfCamera
{
	Camera_GpuBuffer bfCamera;
};

// Calls the appropriate LinearizeDepth_* function based on bfCamera.m_projFlags.
float Camera_LinearizeDepth(in float _depth)
{
	uint projFlags = bfCamera.m_projFlags;
	if        ((projFlags & (Camera_ProjFlag_Infinite | Camera_ProjFlag_Reversed)) != 0) {
		return LinearizeDepth_InfiniteReversed(_depth, bfCamera.m_near);
	} else if ((projFlags & Camera_ProjFlag_Infinite) != 0) {
		return LinearizeDepth_Infinite(_depth, bfCamera.m_near);
	} else if ((projFlags & Camera_ProjFlag_Reversed) != 0) {
		return LinearizeDepth_Reversed(_depth, bfCamera.m_near, bfCamera.m_far);
	} else {
		return LinearizeDepth_Reversed(_depth, bfCamera.m_near, bfCamera.m_far);
	}
}

// Recover a frustum ray from _ndc (in [-1,1]). Ray * linear depth = view space position.
vec3 Camera_GetFrustumRay(in vec2 _ndc)
{
	uint projFlags = bfCamera.m_projFlags;
	if ((projFlags & Camera_ProjFlag_Asymmetrical) != 0) {
	 // \todo interpolate between frustum edges in XY
		return vec3(0.0, 0.0, 0.0);
	} else {
		return vec3(_ndc.x * bfCamera.m_up * bfCamera.m_aspectRatio, _ndc.y * bfCamera.m_up, -1.0);
	}
}

#endif // Camera_glsl
