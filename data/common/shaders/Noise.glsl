#ifndef Noise_glsl
#define Noise_glsl

// https://www.shadertoy.com/view/XdXGW8
vec2 _Noise_Hash2(in vec2 _p)
{
	const vec2 k = vec2(0.3183099, 0.3678794);
	_p = _p * k + k.yx;
	return -1.0 + 2.0 * fract(16.0 * k * fract(_p.x*_p.y*(_p.x + _p.y)));
}
float Noise_Gradient_Iq(in vec2 _p)
{
	vec2 i = floor(_p);
	vec2 f = fract(_p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(dot(_Noise_Hash2(i + vec2(0.0, 0.0) ), f - vec2(0.0, 0.0) ), 
	               dot(_Noise_Hash2(i + vec2(1.0, 0.0) ), f - vec2(1.0, 0.0) ), u.x),
	           mix(dot(_Noise_Hash2(i + vec2(0.0, 1.0) ), f - vec2(0.0, 1.0) ), 
	               dot(_Noise_Hash2(i + vec2(1.0, 1.0) ), f - vec2(1.0, 1.0) ), u.x), u.y);
}

// https://www.shadertoy.com/view/Xsl3Dl
vec3 _Noise_Hash3(in vec3 _p)
{
	_p = vec3(
		dot(_p, vec3(127.1, 311.7, 74.7)),
		dot(_p, vec3(269.5, 183.3, 246.1)),
		dot(_p, vec3(113.5, 271.9, 124.6))
		);
	return -1.0 + 2.0 * fract(sin(_p) * 43758.5453123);
}
float Noise_Gradient_Iq(in vec3 _p)
{
	vec3 i = floor(_p);
	vec3 f = fract(_p);
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

#define Noise_Gradient(_p) Noise_Gradient_Iq(_p)

#endif // Noise_glsl
