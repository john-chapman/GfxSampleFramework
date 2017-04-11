#include "shaders/def.glsl"

layout(location=0) in vec2 aPosition;

// \todo need to pass corner rays for VR compatibility
uniform float uCameraTanHalfFov;
uniform float uCameraAspectRatio;
uniform mat4  uCameraWorldMatrix;

noperspective out vec2 vUv;
noperspective out vec3 vViewRayV;
noperspective out vec3 vViewRayW;

void main() 
{
	vUv = aPosition.xy * 0.5 + 0.5;
	vViewRayV = vec3(aPosition.x * uCameraTanHalfFov * uCameraAspectRatio, aPosition.y * uCameraTanHalfFov, -1.0);
	vViewRayW = mat3(uCameraWorldMatrix) * vViewRayV;
	gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
