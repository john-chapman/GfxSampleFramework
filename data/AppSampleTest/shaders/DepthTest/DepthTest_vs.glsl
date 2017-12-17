#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

layout(location=0) in vec3  aPosition;
layout(location=1) in vec3  aNormal;
layout(location=2) in vec3  aTangent;
layout(location=3) in vec2  aTexcoord;
#ifdef DEPTH_ERROR
	smooth out vec3 vPositionV;
#endif

layout(std430) buffer _bfInstances
{
	mat4 bfInstances[];
};

void main() 
{
	vec3 posV = TransformPosition(bfCamera.m_view, TransformPosition(bfInstances[gl_InstanceID], aPosition.xyz));
	#ifdef DEPTH_ERROR
		vPositionV = posV;
	#endif
	gl_Position = bfCamera.m_proj * vec4(posV, 1.0);
}
