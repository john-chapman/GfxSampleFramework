#include "shaders/def.glsl"

layout(location=0) in vec2 aPosition;

noperspective out vec2 vUv;

void main() 
{
	vUv = aPosition.xy * 0.5 + 0.5;
	gl_Position = vec4(aPosition.xy, 0.0, 1.0);
}
