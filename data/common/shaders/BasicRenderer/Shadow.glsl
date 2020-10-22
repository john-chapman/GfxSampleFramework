#ifndef Shadow_glsl
#define Shadow_glsl

#include "shaders/Rand.glsl"

#define Shadow_PREDICATE_NoL 1 // Skip shadow lookup if NoL <= Lighting_EPSILON.

#ifndef Shadow_CONSTANT_BIAS
	#define Shadow_CONSTANT_BIAS 1e-4
#endif

mat2 Shadow_RandRotation(in vec2 _seed)
{
	float t = k2Pi * Rand_Float(_seed);
	float c = cos(t);
	float s = sin(t);
	return mat2(
		c, -s,
		s,  c
		);
}

vec3 Shadow_Project(
	in vec3  _P,
	in mat4  _worldToShadow,
	in float _shadowTexelSize,
	in float _scale,
	in vec2  _bias
	)
{
	vec4 ret     = _worldToShadow * vec4(_P, 1.0);
		 ret.xyz = ret.xyz / ret.w;
		 ret.xy  = ret.xy * 0.5 + 0.5;
		 ret.xy  = clamp(ret.xy, _shadowTexelSize, 1.0 - _shadowTexelSize); // clamp to border texels \todo need to account for the filter width here
		 ret.z   = saturate(ret.z); // required for directional lights where P projects outside the frustum
		 ret.xy  = ret.xy * _scale + _bias;
		 ret.z   = ret.z - Shadow_CONSTANT_BIAS;

	return ret.xyz;
}

float Shadow_Fetch(
	in sampler2DArray _txShadowMap,
	in vec3           _shadowCoord,
	in float          _arrayIndex
	)
{
	return step(_shadowCoord.z, textureLod(_txShadowMap, vec3(_shadowCoord.xy, _arrayIndex), 0.0).x);
}

float Shadow_FetchBilinear(
	in sampler2DArray _txShadowMap,
	in vec3           _shadowCoord,
	in float          _arrayIndex,
	in float          _shadowTexelSize
	)
{
	vec4  shadow4  = textureGather(_txShadowMap, vec3(_shadowCoord.xy, _arrayIndex));
	      shadow4  = step(_shadowCoord.zzzz, shadow4);
	vec2  weights  = fract((_shadowCoord.xy - _shadowTexelSize * 0.5) / _shadowTexelSize);
	float shadowXY = mix(shadow4.x, shadow4.y, weights.x);
	float shadowZW = mix(shadow4.w, shadow4.z, weights.x);

	return mix(shadowZW, shadowXY, weights.y);
}

float Shadow_FetchQuincunx(
	in sampler2DArray _txShadowMap,
	in vec3           _shadowCoord,
	in float          _arrayIndex,
	in float          _shadowTexelSize
	)
{
	const vec2 kQuincunx[5] =
	{
		vec2( 0.0,  0.0),
		vec2(-1.0, -1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0),
		vec2(-1.0,  1.0)
	};

	const mat2 rmat = Shadow_RandRotation(_shadowCoord.xy);
	float ret = 0.0;
	for (uint i = 0; i < 5; ++i)
	{
		const vec2 offset = rmat * kQuincunx[i] * _shadowTexelSize * 1.0;
		ret += Shadow_FetchBilinear(_txShadowMap, vec3(_shadowCoord.xy + offset, _shadowCoord.z), _arrayIndex, _shadowTexelSize);
	}
	return ret / 5.0;
}

#endif // Shadow_glsl