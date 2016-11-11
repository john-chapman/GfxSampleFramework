#include "shaders/def.glsl"

layout(location=0) in vec2 aPosition;

uniform mat4 uWorldMatrix;
uniform mat4 uViewProjMatrix;
uniform vec2 uScale;

smooth out vec2 vUv;

void main() 
{
	vUv = aPosition.xy * 0.5 + 0.5;
	vec3 posW = vec3(aPosition * uScale, 0.0);
	posW = vec3(uWorldMatrix * vec4(posW, 1.0)).xyz;
	gl_Position = uViewProjMatrix * vec4(posW, 1.0);
}
