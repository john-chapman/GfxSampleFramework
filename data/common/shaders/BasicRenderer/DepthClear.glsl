#include "shaders/def.glsl"

uniform float uClearDepth;

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

_VERTEX_IN(0, vec2, aPosition);

void main() 
{
	gl_Position = vec4(aPosition.xy, uClearDepth, 1.0);
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER ////////////////////////////////////////////////////////

void main() 
{
	//gl_FragDepth = uClearDepth;
}

#endif // FRAGMENT_SHADER
