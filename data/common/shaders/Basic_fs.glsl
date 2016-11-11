#include "shaders/def.glsl"

noperspective in vec2 vUv;

uniform sampler2D txTexture2d;

layout(location=0) out vec4 fResult;

void main() 
{
	fResult = texture(txTexture2d, vUv);
}
