#include "shaders/def.glsl"

layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aTexcoord;
layout(location=2) in vec4 aColor;

uniform mat4 uProjMatrix;

noperspective out vec2 vUv;
noperspective out vec4 vColor;

void main() 
{
	vUv         = aTexcoord;
	vColor      = aColor;
	gl_Position = uProjMatrix * vec4(aPosition.xy, 0.0, 1.0);
}
