#include "shaders/def.glsl"

_VARYING(noperspective, vec2, vUv);
_VARYING(noperspective, vec4, vColor);

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

_VERTEX_IN(POSITIONS,    vec2, aPosition);
_VERTEX_IN(MATERIAL_UVS, vec2, aTexcoord);
_VERTEX_IN(COLORS,       uint, aColor);

uniform mat4 uProjMatrix;

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

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER ////////////////////////////////////////////////////////

_FRAGMENT_OUT(0, vec4, fColor);

uniform sampler2D txTexture;

void main() 
{
	fColor = vColor;
	fColor.a *= texture(txTexture, vUv).x;
}

#endif // FRAGMENT_SHADER
