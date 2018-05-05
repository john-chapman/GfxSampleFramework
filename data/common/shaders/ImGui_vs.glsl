#include "shaders/def.glsl"

layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aTexcoord;
layout(location=2) in uint aColor;

uniform mat4 uProjMatrix;

noperspective out vec2 vUv;
noperspective out vec4 vColor;

vec4 UintToRgba(uint _u)
{
	vec4 ret = vec4(0.0);
	ret.r = float((_u & 0x000000ffu) >> 0u)  / 255.0;
	ret.g = float((_u & 0x0000ff00u) >> 8u)  / 255.0;
	ret.b = float((_u & 0x00ff0000u) >> 16u) / 255.0;
	ret.a = float((_u & 0xff000000u) >> 24u) / 255.0;
	return ret;
}

void main() 
{
	vUv         = aTexcoord;
	vColor      = UintToRgba(aColor);
	gl_Position = uProjMatrix * vec4(aPosition.xy, 0.0, 1.0);
}
