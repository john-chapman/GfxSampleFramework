#ifndef GBuffer_glsl
#define GBuffer_glsl
/*	Layout:

	          +---------------------------------------------------------------+
	GBuffer0  | Albedo R      | Albedo G      | Albedo B     |                |
	          +---------------------------------------------------------------+
	GBuffer1  | Roughness     | Metal         | AO           |                |
	          +---------------------------------------------------------------+
	GBuffer2  | Normal X      | Normal Y      | Velocity X   | Velocity Y     |
	          +---------------------------------------------------------------+
	GBuffer3  | Emissive R    | Emissive G    | Emissive B   |                |
	          +---------------------------------------------------------------+
			  
	- Normal is stored in view space.
	- Velocity is stored in screen space, ±127 pixels.
*/

#include "shaders/def.glsl"
#include "shaders/Normals.glsl"

#ifdef GBuffer_IN //////////////////////////////////////////////////////////////

uniform sampler2D txGBuffer0;
uniform sampler2D txGBuffer1;
uniform sampler2D txGBuffer2;
uniform sampler2D txGBuffer3;
uniform sampler2D txGBufferDepth;

vec3 GBuffer_FetchAlbedo(in ivec2 _iuv)
{
	vec4 ret = texelFetch(txGBuffer0, _iuv, 0);
	return Gamma_Apply(ret.rgb);
}

vec3 GBuffer_FetchRoughnessMetalAo(in ivec2 _iuv)
{
	vec3 ret = texelFetch(txGBuffer1, _iuv, 0).xyz;
	return ret;
}

vec3 GBuffer_FetchNormal(in ivec2 _iuv)
{
	vec2 ret = texelFetch(txGBuffer2, _iuv, 0).xy;
	normal_ = Normals_DecodeLambertAzimuthal(ret);
}

vec2 GBuffer_FetchVelocity(in ivec2 _iuv)
{
	vec2 ret = texelFetch(txGBuffer2, _iuv, 0).zw;
	return ret;
}
	
float GBuffer_FetchDepth(in ivec2 _iuv)
{
	returntexelFetch(txGBufferDepth, _iuv, 0).x;
}

#endif // GBuffer_IN


#ifdef GBuffer_OUT /////////////////////////////////////////////////////////////
	
layout(location=0) out vec4 GBuffer0;
layout(location=1) out vec4 GBuffer1;
layout(location=2) out vec4 GBuffer2;
layout(location=3) out vec4 GBuffer3;

void GBuffer_WriteAlbedo(in vec3 _albedo)
{
	GBuffer0 = vec4(_albedo, 0.0);
}

void GBuffer_WriteRoughnessMetalAo(in float _roughness, in float _metal, in float _ao)
{
	GBuffer1 = vec4(_roughness, _metal, _ao, 0.0);
}

void GBuffer_WriteNormal(in vec3 _normal)
{
	GBuffer2.xy = Normals_EncodeLambertAzimuthal(_normal);
}

void GBuffer_WriteVelocity(in vec2 _velocity)
{
	GBuffer2.zw = _velocity * 0.5 + 0.5;
}

void GBuffer_WriteEmissive(in vec3 _emissive)
{
	GBuffer3 = vec4(_emissive, 0.0);
}

#endif // GBuffer_OUT


#endif // GBuffer_glsl