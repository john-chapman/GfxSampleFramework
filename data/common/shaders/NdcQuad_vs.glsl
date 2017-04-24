#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

layout(location=0) in vec2 aPosition;

noperspective out vec2 vUv;
noperspective out vec3 vFrustumRay;
noperspective out vec3 vFrustumRayW;

void main() 
{
	vUv = aPosition.xy * 0.5 + 0.5;
	vFrustumRay = Camera_GetFrustumRay(aPosition.xy);
	vFrustumRayW = mat3(bfCamera.m_world) * vFrustumRay;
	gl_Position = vec4(aPosition.xy, 1.0, 1.0); // draw at the far plane
}
