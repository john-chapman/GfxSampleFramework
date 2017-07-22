#ifndef Sampling_glsl
#define Sampling_glsl

#include "shaders/def.glsl"

// Van der Corput sequence.
float Sampling_RadicalInverse(in uint _seed)
{
	uint s = _seed;
	#if 0
		s = (s << 16u) | (s >> 16u);
		s = ((s & 0x55555555u) << 1u) | ((s & 0xaaaaaaaau) >> 1u);
		s = ((s & 0x33333333u) << 2u) | ((s & 0xccccccccu) >> 2u);
		s = ((s & 0x0f0f0f0fu) << 4u) | ((s & 0xf0f0f0f0u) >> 4u);
		s = ((s & 0x00ff00ffu) << 8u) | ((s & 0xff00ff00u) >> 8u);
	#else
		s = bitfieldReverse(s);
	#endif
	return float(s) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sequence at _i given 1/N (N is the number of points in the sequence).
vec2 Sampling_Hammersley2d(in uint _i, in float _rn)
{
	return vec2(float(_i) * _rn, Sampling_RadicalInverse(_i));
}

// Return a uniformly-distributed point on the unit hemisphere oriented along +Z given a random _uv (e.g. the result of Sampling_Hammersley2d).
vec3 Sampling_Hemisphere_Uniform(in vec2 _uv) 
{
	float phi = _uv.y * 2.0 * kPi;
	float ct = 1.0 - _uv.x;
	float st = sqrt(1.0 - ct * ct);
	return vec3(cos(phi) * st, sin(phi) * st, ct);
}

// Return a cosine-distributed point on the unit hemisphere oriented along +Z given a random _uv (e.g. the result of Sampling_Hammersley2d).
vec3 Sampling_Hemisphere_Cosine(in vec2 _uv)
{
	float phi = _uv.y * 2.0 * kPi;
	float ct = sqrt(1.0 - _uv.x);
	float st = sqrt(1.0 - ct * ct);
	return vec3(cos(phi) * st, sin(phi) * st, ct);
}

// Return a point on the unit hemisphere oriented along +Z given a random _uv, skewed by _a (0 = +Z, 1 = uniform).
vec3 Sampling_Hemisphere(in vec2 _uv, in float _a) 
{
	float phi = _uv.y * 2.0 * kPi;
	float ct = sqrt((1.0 - _uv.y) / (1.0 + (_a * _a - 1.0) * _uv.y ));
	float st = sqrt(1.0 - ct * ct);
	return vec3(cos(phi) * st, sin(phi) * st, ct);
}


// \todo ALU only versions?
float Bayer_2x2(in uvec2 _seed)
{
	_seed.x = _seed.x % 2;
	_seed.y = _seed.y % 2;
	uint i = _seed.x + _seed.y * 2;
	const float kBayer[] = { 0.25, 0.75, 1.0, 0.5 };
	return kBayer[i];
}
float Bayer_4x4(in uvec2 _seed)
{
	_seed.x = _seed.x % 4;
	_seed.y = _seed.y % 4;
	uint i = _seed.x + _seed.y * 4;
	const float kBayer[] = { 0.0625, 0.5625, 0.1875, 0.6875, 0.8125, 0.3125, 0.9375, 0.4375, 0.25, 0.75, 0.125, 0.625, 1.0, 0.5, 0.875, 0.375 };
	return kBayer[i];
}

#endif // Sampling_glsl
