#ifndef Noise_glsl
#define Noise_glsl

#include "shaders/webgl-noise.glsl"

// Aperiodic gradient noise.
#define Noise_Gradient(_P) _webglnoise_snoise(_P)

// (P)eriodic gradient noise with analytical (D)erivatives (R)otating gradients.
// _period = integer X, even integer Y.
// _rotation = [0,1[.
// Return x,y,z = value,dX,dY.
#define Noise_Gradient_PDR(_P, _period, _rotation) _webglnoise_psrdnoise(_P, _period, _rotation)
#define Noise_Gradient_PR (_P, _period, _rotation) _webglnoise_psrnoise(_P, _period, _rotation)
#define Noise_Gradient_DR (_P, _rotation)          _webglnoise_srdnoise(_P, _rotation)


// https://code.google.com/archive/p/fractalterraingeneration/wikis/Fractional_Brownian_Motion.wiki
// Use _lacunarity = 2, _gain = .5 for the standard 1/f noise.

// 2d fBm
float Noise_fBm(in vec2 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	float ret  = 0.0;
	float ampl = _gain;
	for (int i = 0; i < _layers; ++i)
	{
		ret += Noise_Gradient(_P * _frequency) * ampl;
		_frequency *= _lacunarity;
		ampl  *= _gain;
	}
	return ret * 0.5 + 0.5;
}

// 2d turbulence
float Noise_Turbulence(in vec2 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	float ret  = 0.0;
	float ampl = _gain;
	for (int i = 0; i < _layers; ++i)
	{
		ret += abs(Noise_Gradient(_P * _frequency)) * ampl;
		_frequency *= _lacunarity;
		ampl  *= _gain;
	}
	return ret;
}

// 2d ridge
float Noise_Ridge(in vec2 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	return 1.0 - Noise_Turbulence(_P, _frequency, _lacunarity, _gain, _layers);
}

// 3d fBm
float Noise_fBm(in vec3 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	float ret  = 0.0;
	float ampl = _gain;
	for (int i = 0; i < _layers; ++i)
	{
		ret += Noise_Gradient(_P * _frequency) * ampl;
		_frequency *= _lacunarity;
		ampl  *= _gain;
	}
	return ret * 0.5 + 0.5;
}

// 3d turbulence
float Noise_Turbulence(in vec3 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	float ret  = 0.0;
	float ampl = _gain;
	for (int i = 0; i < _layers; ++i)
	{
		ret += abs(Noise_Gradient(_P * _frequency)) * ampl;
		_frequency *= _lacunarity;
		ampl  *= _gain;
	}
	return ret;
}

// 3d ridge
float Noise_Ridge(in vec3 _P, in float _frequency, in float _lacunarity, in float _gain, in int _layers)
{
	return 1.0 - Noise_Turbulence(_P, _frequency, _lacunarity, _gain, _layers);
}

// Interleaved gradient noise.
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float Noise_InterleavedGradient(in vec2 _seed)
{
	return fract(52.9829189 * fract(dot(_seed, vec2(0.06711056, 0.00583715))));
}

#endif // Noise_glsl
