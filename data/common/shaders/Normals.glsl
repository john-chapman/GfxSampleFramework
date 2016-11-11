#ifndef Normals_glsl
#define Normals_glsl

vec2 Normals_EncodeLambertAzimuthal(in vec3 _normalV)
{
	float p = sqrt(_normalV.z * 8.0 + 8.0);
	return _normalV.xy / p + 0.5;
}

vec3 Normals_DecodeLambertAzimuthal(in vec2 _normal)
{
	vec2 fenc = _normal.xy * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(max(1.0 - f / 4.0, 0.0));
	vec3 ret;
	ret.xy = fenc * g;
	ret.z = 1.0 - f / 2.0;
	return ret;
}

#endif // Normals_glsl
