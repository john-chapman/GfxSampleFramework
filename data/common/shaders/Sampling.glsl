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
	float ct = sqrt((1.0 - _uv.x) / (1.0 + (_a * _a - 1.0) * _uv.x));
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


// WIP - note that Bicubic4 exhibits artefacts in areas of high contrast (lower precision in the bilinear filtering HW?)

vec4 Quintic(in sampler2D _tx, in vec2 _uv, in int _lod)
{
	vec2 uv = _uv * vec2(textureSize(_tx, _lod)) + 0.5;
	vec2 iuv = floor(uv);
	vec2 fuv = uv - iuv;
	fuv = fuv * fuv * fuv * (fuv * (fuv * 6.0 - 15.0) + 10.0);
	uv = (iuv + fuv - 0.5) / vec2(textureSize(_tx, _lod));
	return  textureLod(_tx, uv, float(_lod));
}

vec4 CubicWeights_BSpline(in float _d)
{
	float d1 = _d;
	float d2 = d1 * d1;
	float d3 = d2 * d1;
	
	vec4 ret;
	
 	ret.x = -d3 + 3.0 * d2 - 3.0 * d1 + 1.0;
    ret.y = 3.0 * d3 - 6.0 * d2 + 4.0;
    ret.z = -3.0 * d3 + 3.0 * d2 + 3.0 * d1 + 1.0;
    ret.w = d3;
	
	return ret / 6.0;
}
#define CubicWeights(_d) CubicWeights_BSpline(_d)

vec4 Bicubic16(in sampler2D _tx, in vec2 _uv, in int _lod)
{
	vec2 texelSize = 1.0 / vec2(textureSize(_tx, _lod));
	_uv /= texelSize;
	vec2 iuv = floor(_uv - 0.5) + 0.5; // round to nearest texel center
	vec2 d = _uv - iuv; // offset from the texel center to the sample location
	
	vec4 wX = CubicWeights(d.x);
	vec4 wY = CubicWeights(d.y);
	vec2 w0 = vec2(wX[0], wY[0]);
	vec2 w1 = vec2(wX[1], wY[1]);
	vec2 w2 = vec2(wX[2], wY[2]);
	vec2 w3 = vec2(wX[3], wY[3]);
	
	vec2 s0 = (iuv - 1.0) * texelSize;
	vec2 s1 = (iuv + 0.0) * texelSize;
	vec2 s2 = (iuv + 1.0) * texelSize;
	vec2 s3 = (iuv + 2.0) * texelSize;
	
	return 
		  textureLod(_tx, vec2(s0.x, s0.y), float(_lod)) * (w0.x * w0.y) 
		+ textureLod(_tx, vec2(s1.x, s0.y), float(_lod)) * (w1.x * w0.y) 
		+ textureLod(_tx, vec2(s2.x, s0.y), float(_lod)) * (w2.x * w0.y) 
		+ textureLod(_tx, vec2(s3.x, s0.y), float(_lod)) * (w3.x * w0.y)
		
		+ textureLod(_tx, vec2(s0.x, s1.y), float(_lod)) * (w0.x * w1.y) 
		+ textureLod(_tx, vec2(s1.x, s1.y), float(_lod)) * (w1.x * w1.y) 
		+ textureLod(_tx, vec2(s2.x, s1.y), float(_lod)) * (w2.x * w1.y) 
		+ textureLod(_tx, vec2(s3.x, s1.y), float(_lod)) * (w3.x * w1.y)
		
		+ textureLod(_tx, vec2(s0.x, s2.y), float(_lod)) * (w0.x * w2.y) 
		+ textureLod(_tx, vec2(s1.x, s2.y), float(_lod)) * (w1.x * w2.y) 
		+ textureLod(_tx, vec2(s2.x, s2.y), float(_lod)) * (w2.x * w2.y) 
		+ textureLod(_tx, vec2(s3.x, s2.y), float(_lod)) * (w3.x * w2.y)
		
		+ textureLod(_tx, vec2(s0.x, s3.y), float(_lod)) * (w0.x * w3.y) 
		+ textureLod(_tx, vec2(s1.x, s3.y), float(_lod)) * (w1.x * w3.y) 
		+ textureLod(_tx, vec2(s2.x, s3.y), float(_lod)) * (w2.x * w3.y) 
		+ textureLod(_tx, vec2(s3.x, s3.y), float(_lod)) * (w3.x * w3.y)
		;
}
vec4 Bicubic4(in sampler2D _tx, in vec2 _uv, in int _lod)
{
	vec2 texelSize = 1.0 / vec2(textureSize(_tx, _lod));
	_uv /= texelSize;
	vec2 iuv = floor(_uv - 0.5) + 0.5; // round to nearest texel center
	vec2 d = _uv - iuv; // offset from the texel center to the sample location
	
	vec4 wX = CubicWeights(d.x);
	vec4 wY = CubicWeights(d.y);
	vec2 w0 = vec2(wX[0], wY[0]);
	vec2 w1 = vec2(wX[1], wY[1]);
	vec2 w2 = vec2(wX[2], wY[2]);
	vec2 w3 = vec2(wX[3], wY[3]);
	
	vec2 s0 = w0 + w1;
	vec2 s1 = w2 + w3;
	vec2 f0 = w1 / (w0 + w1);
	vec2 f1 = w3 / (w2 + w3);
	
	vec2 t0 = (iuv - 1.0 + f0) * texelSize;
	vec2 t1 = (iuv + 1.0 + f1) * texelSize;
	
	return 
		  textureLod(_tx, vec2(t0.x, t0.y), float(_lod)) * (s0.x * s0.y) 
		+ textureLod(_tx, vec2(t1.x, t0.y), float(_lod)) * (s1.x * s0.y) 
		+ textureLod(_tx, vec2(t0.x, t1.y), float(_lod)) * (s0.x * s1.y) 
		+ textureLod(_tx, vec2(t1.x, t1.y), float(_lod)) * (s1.x * s1.y)
		;
}


#endif // Sampling_glsl
