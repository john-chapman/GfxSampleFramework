#ifndef GBuffer_glsl
#define GBuffer_glsl
/*	Layout:
	          +---------------------------------------------------------------+
	GBuffer0  | Normal X      | Normal Y      | Velocity X   | Velocity Y     |
	          +---------------------------------------------------------------+
			  
	- Normal is stored in view space.
	- Velocity is stored in screen space.
*/

#include "shaders/def.glsl"
#include "shaders/Normals.glsl"

#ifdef GBuffer_IN //////////////////////////////////////////////////////////////

uniform sampler2D txGBuffer0;
uniform sampler2D txGBufferDepthStencil;

vec3 GBuffer_ReadNormal(in ivec2 _iuv)
{
	vec2 ret = texelFetch(txGBuffer0, _iuv, 0).xy;
	return Normals_DecodeLambertAzimuthal(ret);
}

vec2 GBuffer_ReadVelocity(in ivec2 _iuv)
{
	vec2 ret = texelFetch(txGBuffer0, _iuv, 0).zw * 2.0 - 1.0;
	return ret;
}

float GBuffer_ReadDepth(in ivec2 _iuv)
{
	return texelFetch(txGBufferDepthStencil, _iuv, 0).x;
}

#endif // GBuffer_IN


#ifdef GBuffer_OUT /////////////////////////////////////////////////////////////

#ifdef FRAGMENT_SHADER

_FRAGMENT_OUT(0, vec4, GBuffer0);

void GBuffer_WriteNormal(in vec3 _normal)
{
	GBuffer0.xy = Normals_EncodeLambertAzimuthal(_normal);
}

void GBuffer_WriteVelocity(in vec2 _velocity)
{
	GBuffer0.zw = _velocity * 0.5 + 0.5;
}

#endif // FRAGMENT_SHADER

#endif // GBuffer_OUT


#endif // GBuffer_glsl