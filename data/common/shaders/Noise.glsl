#ifndef Noise_glsl
#define Noise_glsl

// \todo the _Iq variants exhibit a lot of precision artefacts as _P increases, potential fix is to pass the integer and fraction coordinates separately

// https://www.shadertoy.com/view/XdXGW8
vec2 _Noise_Hash2(in vec2 _P)
{
	const vec2 k = vec2(0.3183099, 0.3678794);
	_P = _P * k + k.yx;
	return -1.0 + 2.0 * fract(16.0 * k * fract(_P.x * _P.y * (_P.x + _P.y)));
}
float Noise_Gradient_Iq(in vec2 _P)
{
	vec2 i = floor(_P);
	vec2 f = fract(_P);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(dot(_Noise_Hash2(i + vec2(0.0, 0.0) ), f - vec2(0.0, 0.0) ),
	               dot(_Noise_Hash2(i + vec2(1.0, 0.0) ), f - vec2(1.0, 0.0) ), u.x),
	           mix(dot(_Noise_Hash2(i + vec2(0.0, 1.0) ), f - vec2(0.0, 1.0) ),
	               dot(_Noise_Hash2(i + vec2(1.0, 1.0) ), f - vec2(1.0, 1.0) ), u.x), u.y);
}

// https://www.shadertoy.com/view/Xsl3Dl
vec3 _Noise_Hash3(in vec3 _P)
{
	_P = vec3(
		dot(_P, vec3(127.1, 311.7, 74.7)),
		dot(_P, vec3(269.5, 183.3, 246.1)),
		dot(_P, vec3(113.5, 271.9, 124.6))
		);
	return -1.0 + 2.0 * fract(sin(_P) * 43758.5453123);
}
float Noise_Gradient_Iq(in vec3 _P)
{
	vec3 i = floor(_P);
	vec3 f = fract(_P);
	vec3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(dot(_Noise_Hash3(i + vec3(0.0, 0.0, 0.0)), f - vec3(0.0, 0.0, 0.0)),
                       dot(_Noise_Hash3(i + vec3(1.0, 0.0, 0.0)), f - vec3(1.0, 0.0, 0.0)), u.x),
	               mix(dot(_Noise_Hash3(i + vec3(0.0, 1.0, 0.0)), f - vec3(0.0, 1.0, 0.0)),
	                   dot(_Noise_Hash3(i + vec3(1.0, 1.0, 0.0)), f - vec3(1.0, 1.0, 0.0)), u.x), u.y),
	           mix(mix(dot(_Noise_Hash3(i + vec3(0.0, 0.0, 1.0)), f - vec3(0.0, 0.0, 1.0)),
	                   dot(_Noise_Hash3(i + vec3(1.0, 0.0, 1.0)), f - vec3(1.0, 0.0, 1.0)), u.x),
	               mix(dot(_Noise_Hash3(i + vec3(0.0, 1.0, 1.0)), f - vec3(0.0, 1.0, 1.0)),
	                   dot(_Noise_Hash3(i + vec3(1.0, 1.0, 1.0)), f - vec3(1.0, 1.0, 1.0)), u.x), u.y), u.z);
}

#define Noise_Gradient(_P) Noise_Gradient_Iq(_P)

// https://code.google.com/archive/p/fractalterraingeneration/wikis/Fractional_Brownian_Motion.wiki
// Use _lacunarity = 2, _gain = .5 for the standard 1/f noise.
float Noise_fBm_Base(in vec2 _P, in float _freq, in float _lacunarity, in float _gain, in int _layers) // in [-1,1]
{
	float ret  = 0.0;
	float ampl = 1.0;
	for (int i = 0; i < _layers; ++i) {
		ret   += Noise_Gradient(_P * _freq) * ampl;
		_freq *= _lacunarity;
		ampl  *= _gain;
	}
	return ret;
}
float Noise_fBm(in vec2 _P, in float _freq, in float _lacunarity, in float _gain, in int _layers) // in [0,1]
{
	return Noise_fBm_Base(_P, _freq, _lacunarity, _gain, _layers) * 0.5 + 0.5;
}
float Noise_Turbulence(in vec2 _P, in float _freq, in float _lacunarity, in float _gain, in int _layers) // in [0,1]
{
	return abs(Noise_fBm_Base(_P, _freq, _lacunarity, _gain, _layers));
}
float Noise_Ridge(in vec2 _P, in float _freq, in float _lacunarity, in float _gain, in int _layers) // in [0,1]
{
	return 1.0 - abs(Noise_fBm_Base(_P, _freq, _lacunarity, _gain, _layers));
}

#endif // Noise_glsl
