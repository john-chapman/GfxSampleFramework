#include "shaders/def.glsl"

uniform float uClearDepth;
#if DEBUG
	_FRAGMENT_OUT(0, vec4, fColor);
#endif

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

_VERTEX_IN(POSITIONS, vec2, aPosition);

void main() 
{
	gl_Position = vec4(aPosition.xy, uClearDepth, 1.0);
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER ////////////////////////////////////////////////////////

void main() 
{
	//gl_FragDepth = uClearDepth;
	#if DEBUG
		fColor = vec4(1, 0, 1, 1);
	#endif
}

#endif // FRAGMENT_SHADER
