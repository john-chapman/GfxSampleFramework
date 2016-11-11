#include "shaders/def.glsl"

noperspective in vec2 vUv;
noperspective in vec4 vColor;

uniform sampler2D txTexture;

layout(location=0) out vec4 fResult;

void main() 
{
	fResult = vColor;
	fResult.a *= texture(txTexture, vUv).r;
}
