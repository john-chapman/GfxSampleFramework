#include "shaders/def.glsl"

smooth in vec2 vUv;

uniform sampler2D txVuiScreen;

layout(location=0) out vec4 fResult;

void main() 
{
	fResult = texture(txVuiScreen, vUv);
}
