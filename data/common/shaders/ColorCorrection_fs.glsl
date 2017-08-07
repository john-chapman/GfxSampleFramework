#include "shaders/def.glsl"
#include "shaders/Rand.glsl"

noperspective in vec2 vUv;

uniform sampler2D txInput;

uniform uint uTime;

struct Data
{
	float m_exposure;
	float m_autoExposureClamp;
	float m_contrast;
	float m_saturation;
	float m_shadows;
	float m_highlights;
	vec4  m_tonemapper; // a,b,c,d (see Tonemap_Lottes below)
	vec3  m_tint;
};
layout(std140) uniform _bfData
{
	Data bfData;
};


/*******************************************************************************

                                  Exposure

*******************************************************************************/

#ifdef AUTO_EXPOSURE
	uniform sampler2D txLuminance;
#endif

layout(location=0) out vec3 fResult;


float Luminance(in vec3 _x)
{
	return dot(_x, vec3(0.25, 0.50, 0.25));
}

float LuminanceToEV100(in float _lum)
{
	return log2(_lum * 100.0 / 12.5);
}

float EV100ToExposure(in float _ev100)
{
	float maxLum = (1.2 * exp2(_ev100));
	return 1.0 / maxLum;
}

//vec3 ShadowsHighlights(in vec3 _x, in float _lumMid)
//{
//}


/*******************************************************************************

                                  Saturation

*******************************************************************************/

vec3 Saturation(in vec3 _x)
{
	vec3 gray = vec3(Luminance(_x));
	return gray + bfData.m_saturation * (_x - gray);
}

/*******************************************************************************

                                  Tonemapping/Gamma

*******************************************************************************/

vec3 Gamma(in vec3 _x)
{
	return pow(_x, vec3(1.0 / 2.2));
}

vec3 Tonemap_Reinhard(in vec3 _x)
{
	#if 1
	 // RGB separate
		vec3 ret = _x / (vec3(1.0) + _x);
	#else
	 // max3(RGB) (preserves hue/saturation but high exposures look oversaturated)
		float peak = max3(_x.r, _x.g, _x.b);
		vec3 ratio = _x / peak;
		peak = peak / (1.0 + peak);
		vec3 ret = ratio * peak;
	#endif

	return Gamma(ret);
}

// http://32ipi028l5q82yhj72224m8j.wpengine.netdna-cdn.com/wp-content/uploads/2016/03/GdcVdrLottes.pdf
// Generic 4-parameter curve
vec3 Tonemap_Lottes(in vec3 _x)
{
	#define fnc(v) (pow(v, bfData.m_tonemapper.x) / (pow(v, bfData.m_tonemapper.w) * bfData.m_tonemapper.y + bfData.m_tonemapper.z))

	#if 0
	 // RGB separate
		vec3 ret = vec3(fnc(_x.r), fnc(_x.g), fnc(_x.b));
	#else
	 // max3(RGB) (preserves hue/saturation, doesn't have the same issues as Reinhard)
		float peak = max3(_x.r, _x.g, _x.b);
		vec3 ratio = _x / peak;
		peak = fnc(peak);
		vec3 ret = ratio * peak;

	#endif

	return Gamma(ret);

	#undef fnc
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// Cheap, luminance-only fit; over saturates the brights
vec3 Tonemap_ACES_Narkowicz(in vec3 _x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	#define fnc(v) saturate((v * (a * v + b)) / (v * (c * v + d) + e));
	#if 1
		vec3 x = _x * 0.6;
		vec3 ret = fnc(x);
	#else
		// max3(RGB) (preserves hue/saturation but high exposures look oversaturated)
		float peak = max3(_x.r, _x.g, _x.b);
		vec3 ratio = _x / peak;
		peak = fnc(peak);
		vec3 ret = ratio * peak;
	#endif

 	return Gamma(ret);

	#undef fnc
}
// https://github.com/selfshadow/aces-dev
// Closer fit, more even saturation across the range
// \todo Contrast/saturation issues with this?
//vec3 Tonemap_ACES_Hill(in vec3 _x)
//{
// // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
//	const mat3 matIn = mat3(
//		0.59719, 0.07600, 0.02840,
//		0.35458, 0.90834, 0.13383,
//		0.04823, 0.01566, 0.83777
//		);
// // ODT_SAT => XYZ => D60_2_D65 => sRGB
//	const mat3 matOut = mat3(
//		 1.60475, -0.10208, -0.00327,
//		-0.53108,  1.10813, -0.07276,
//		-0.07367, -0.00605,  1.07602
//		);
//
//	vec3 ret = matIn * _x;
//	vec3 a = ret * (ret + 0.0245786) - 0.000090537;
//	vec3 b = ret * (0.983729 * ret + 0.4329510) + 0.238081;
//	ret = a / b;
//	ret = matOut * ret;
//
//	return Gamma(ret);
//}

#define Tonemap(_x) Tonemap_ACES_Narkowicz(_x)


/*******************************************************************************

                                  Contrast

*******************************************************************************/

vec3 Contrast_Linear(in vec3 _x)
{
	return saturate(vec3(0.5) + (_x - vec3(0.5)) * bfData.m_contrast);
}

vec3 Contrast_Log(in vec3 _rgb)
{
	const float kEpsilon = 1e-7;
	const float kLogMidpoint = 0.18;

	#define fnc(v) max(0.0, exp2(kLogMidpoint + (log2(v + kEpsilon) - kLogMidpoint) * bfData.m_contrast) - kEpsilon)

	return vec3(fnc(_rgb.x), fnc(_rgb.y), fnc(_rgb.z));

	#undef fnc
}

#define Contrast(_x) Contrast_Log(_x)

/*******************************************************************************/

void main()
{
	vec3 ret = textureLod(txInput, vUv, 0.0).rgb;

	#ifdef AUTO_EXPOSURE
	 // auto exposure based on average luminance: in this case, bfData.m_exposure is an offset (in EV) from the automatically chosen value
		float avgLum = textureLod(txLuminance, vUv, 99.0).x;
		avgLum = exp(avgLum); // if geometric
		float autoEV = LuminanceToEV100(avgLum);
		autoEV = clamp(autoEV, -bfData.m_autoExposureClamp, bfData.m_autoExposureClamp);

		#if 1
	 	 // localized exposure (shadows/highlights)
			float lum = Luminance(ret);
			float mask = lum / (1.0 + lum); // tonemap the luminance as a base for the shadows/highlights mask
			lum = LuminanceToEV100(lum);
			avgLum = avgLum / (1.0 + avgLum);
			float highlightsMask = smoothstep(avgLum, 1.0, mask) * bfData.m_highlights;
			float shadowsMask = smoothstep(avgLum, 0.0, mask) * bfData.m_shadows;
			autoEV += lum * shadowsMask + lum * highlightsMask;
		#endif

		float ev = autoEV - bfData.m_exposure;
		ret *= EV100ToExposure(ev);
	#else
		ret *= EV100ToExposure(bfData.m_exposure);
	#endif

 	ret = Saturation(ret);
 	ret = Contrast(ret);
 	ret = Tonemap(ret);
 	ret *= bfData.m_tint;

 // \todo film grain?
	//float rnd = Rand_FloatMantissa(Rand_Hash(uint(gl_FragCoord.x * 698.0 + gl_FragCoord.y) + Rand_Hash(uTime)));
	//ret -= vec3(rnd * 0.08);

	fResult = ret;
}
