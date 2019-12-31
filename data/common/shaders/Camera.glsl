#ifndef Camera_glsl
#define Camera_glsl

#include "shaders/def.glsl"

#define Camera_ProjFlag_Perspective  (1)
#define Camera_ProjFlag_Orthographic (2)
#define Camera_ProjFlag_Asymmetrical (4)
#define Camera_ProjFlag_Infinite     (8)
#define Camera_ProjFlag_Reversed     (16)

struct Camera
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
	Camera uCamera;
};
#define bfCamera uCamera // legacy name

vec3 Camera_GetPosition()
{
	return bfCamera.m_world[3].xyz;
}

float Camera_GetDepthRange()
{
	return bfCamera.m_far - bfCamera.m_near;
}

bool Camera_GetProjFlag(in uint _flag)
{
	return (bfCamera.m_projFlags & _flag) != 0;
}

// Recover view space depth from a depth buffer value.
// This may return INF for infinite perspective projections -- this was causing a lot of hard-to-track issues so it's now handled by returning m_far.
float Camera_GetDepthV(in float _depth)
{
	float ret = 0.0;
	if (Camera_GetProjFlag(Camera_ProjFlag_Perspective))
	{
		ret = GetDepthV_Perspective(_depth, bfCamera.m_proj);
	}
	else
	{
		ret = GetDepthV_Orthographic(_depth, bfCamera.m_proj);
	}
	return isinf(ret) ? uCamera.m_far : ret; // inf = far plane for infinite perspective projections
}

// Recover a frustum ray from _ndc (in [-1,1]). Ray * linear depth = view space position.
vec3 Camera_GetFrustumRay(in vec2 _ndc)
{
	if (Camera_GetProjFlag(Camera_ProjFlag_Asymmetrical)) 
	{
	 // \todo interpolate between frustum edges in XY
		return vec3(0.0, 0.0, 0.0);
	}
	else
	{
		return vec3(_ndc.x * bfCamera.m_up * bfCamera.m_aspectRatio, _ndc.y * bfCamera.m_up, -1.0);
	}
}
// World space frustum ray.
vec3 Camera_GetFrustumRayW(in vec2 _ndc) 
{
	return TransformDirection(bfCamera.m_world, Camera_GetFrustumRay(_ndc));
}

// View ray is a normalized frustum ray.
vec3 Camera_GetViewRay(in vec2 _ndc)
{
	return normalize(Camera_GetFrustumRay(_ndc));
}
// World space view ray.
vec3 Camera_GetViewRayW(in vec2 _ndc)
{
	return normalize(Camera_GetFrustumRayW(_ndc));
}

#endif // Camera_glsl
