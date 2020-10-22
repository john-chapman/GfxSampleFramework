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

vec2 GBuffer_EncodeNormal(in vec3 _normal)
{
	return Normals_EncodeLambertAzimuthal(_normal);
}
vec3 GBuffer_DecodeNormal(in vec2 _encodedNormal)
{
	return Normals_DecodeLambertAzimuthal(_encodedNormal);
}

vec2 GBuffer_EncodeVelocity(in vec2 _velocity)
{
	return _velocity * 0.5 + 0.5;
}
vec2 GBuffer_DecodeVelocity(in vec2 _encodedVelocity)
{
	return _encodedVelocity * 2.0 - 1.0;
}


#ifdef GBuffer_IN //////////////////////////////////////////////////////////////

uniform sampler2D txGBuffer0;
uniform sampler2D txGBufferDepthStencil;

vec3 GBuffer_ReadNormal(in ivec2 _iuv)
{
	return GBuffer_DecodeNormal(texelFetch(txGBuffer0, _iuv, 0).xy);
}

vec2 GBuffer_ReadVelocity(in ivec2 _iuv)
{
	return GBuffer_DecodeVelocity(texelFetch(txGBuffer0, _iuv, 0).zw);
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
	GBuffer0.xy = GBuffer_EncodeNormal(_normal);
}

void GBuffer_WriteVelocity(in vec2 _velocity)
{
	GBuffer0.zw = GBuffer_EncodeVelocity(_velocity);
}

#endif // FRAGMENT_SHADER

#endif // GBuffer_OUT


#endif // GBuffer_glsl