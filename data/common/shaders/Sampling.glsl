#ifndef Sampling_glsl
#define Sampling_glsl

#include "shaders/def.glsl"

// Van der Corput sequence.
float Sampling_RadicalInverseVdC(in uint _seed)
{
	uint s = _seed;
	#if 0
		s = (_seed << 16u) | (s >> 16u);
		s = ((_seed & 0x55555555u) << 1u) | ((s & 0xaaaaaaaau) >> 1u);
		s = ((_seed & 0x33333333u) << 2u) | ((s & 0xccccccccu) >> 2u);
		s = ((_seed & 0x0f0f0f0fu) << 4u) | ((s & 0xf0f0f0f0u) >> 4u);
		s = ((_seed & 0x00ff00ffu) << 8u) | ((s & 0xff00ff00u) >> 8u);
	#else
		s = bitfieldReverse(s);
	#endif
	return float(s) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sequence at _i given 1/N (N is the number of points in the sequence).
vec2 Sampling_Hammersley2d(in uint _i, in float _rn)
{
	return vec2(float(_i) * _rn, Sampling_RadicalInverseVdC(_i));
}

// Return a uniformly-distributed point on the unit hemisphere oriented along +Z given a random _uv (e.g. the result of Sampling_Hammersley2d).
vec3 Sampling_Hemisphere_Uniform(in vec2 _uv) 
{
	float phi = _uv.y * 2.0f * kPi;
	float ct = 1.0f - _uv.x;
	float st = sqrt(1.0f - ct * ct);
	return vec3(cos(phi) * st, sin(phi) * st, ct);
}

// Return a point on the unit hemisphere oriented along +Z given a random _uv, skewed by _a (0 = +Z, 1 = uniform).
vec3 Sampling_Hemisphere(in vec2 _uv, in float _a) 
{
	float phi = _uv.y * 2.0f * kPi;
	float ct = sqrt((1.0 - _uv.y) / (1.0 + (_a * _a - 1.0) * _uv.y ));
	float st = sqrt(1.0f - ct * ct);
	return vec3(cos(phi) * st, sin(phi) * st, ct);
}

#endif // Sampling_glsl
