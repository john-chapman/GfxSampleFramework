#include "shaders/def.glsl"
#include "shaders/Rand.glsl"

noperspective in vec2 vUv;

uniform sampler2D txInput;

uniform uint uTime;

struct Data
{
	float m_exposure;
	float m_contrast;
	float m_saturation;
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


/*******************************************************************************

                                  Tonemapping

*******************************************************************************/

vec3 Tonemap_Reinhard(in vec3 _x)
{
	#if 0
	 // RGB separate
		return _x / (vec3(1.0) + _x);
	#else
	 // max3(RGB) (preserves hue/saturation)
		float peak = max3(_x.r, _x.g, _x.b);
		vec3 ratio = _x / peak;
		peak = peak / (1.0 + peak);
		return ratio * peak;
	#endif
}

// http://32ipi028l5q82yhj72224m8j.wpengine.netdna-cdn.com/wp-content/uploads/2016/03/GdcVdrLottes.pdf
// Generic 4-parameter curve
vec3 Tonemap_Lottes(in vec3 _x)
{
	#define fnc(v) (pow(v, bfData.m_tonemapper.x) / (pow(v, bfData.m_tonemapper.w) * bfData.m_tonemapper.y + bfData.m_tonemapper.z))

	#if 0
	 // RGB separate
		return vec3(fnc(_x.r), fnc(_x.g), fnc(_x.b));
	#else
	 // max3(RGB) (preserves hue/saturation)
		float peak = max3(_x.r, _x.g, _x.b);
		vec3 ratio = _x / peak;
		peak = fnc(peak);
		return ratio * peak;

	#endif

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
	vec3 x = _x * 0.6;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}
// https://github.com/selfshadow/aces-dev
// Closer fit, more even saturation across the range
vec3 Tonemap_ACES_Hill(in vec3 _x)
{
	// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
	const mat3 matIn = mat3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
		);
	// ODT_SAT => XYZ => D60_2_D65 => sRGB
	const mat3 matOut = mat3(
		 1.60475, -0.10208, -0.00327,
		-0.53108,  1.10813, -0.07276,
		-0.07367, -0.00605,  1.07602
		);

	vec3 ret = matIn * _x;
	vec3 a = ret * (ret + 0.0245786) - 0.000090537;
	vec3 b = ret * (0.983729 * ret + 0.4329510) + 0.238081;
	ret = a / b;
	ret = matOut * ret;

	return ret;
}

#define Tonemap(_x) Tonemap_ACES_Hill(_x)


/*******************************************************************************

                                  Exposure

*******************************************************************************/

float LogContrast(in float _x, in float _epsilon, in float _midpoint, in float _contrast)
{
	float logX = log2(_x + _epsilon);
	float adjX = _midpoint + (logX - _midpoint) * _contrast;
	float ret = max(0.0, exp2(adjX) - _epsilon);
	return ret;
}

void main()
{
	vec3 ret = textureLod(txInput, vUv, 0.0).rgb;

	#ifdef AUTO_EXPOSURE
	 // \todo sort of works, need to check the impl (also see \todo in LuminanceMeter_cs)
		float avgLum = textureLod(txLuminance, vUv, 99.0).r;
		avgLum = exp(avgLum);
		float autoEV = log2(avgLum * 100.0 / 12.5);
		//autoEV = clamp(autoEV, -2.0, 2.0);
		ret *= exp2(-autoEV) + bfData.m_exposure;
	#else
		ret *= bfData.m_exposure;
	#endif

 // tint
	ret *= bfData.m_tint;

 // contrast
	//ret = saturate(vec3(0.5) + (ret - vec3(0.5)) * uContrast); // linear contrast
	float logMidpoint = 0.18; // log2(linear midpoint)
	ret.x = LogContrast(ret.x, 1e-7, logMidpoint, bfData.m_contrast);
	ret.y = LogContrast(ret.y, 1e-7, logMidpoint, bfData.m_contrast);
	ret.z = LogContrast(ret.z, 1e-7, logMidpoint, bfData.m_contrast);

 // tonemap
	ret = Tonemap(ret);

 // saturation (apply post tonemap to avoid hue shifts)
	vec3 gray = vec3(dot(ret, vec3(0.25, 0.50, 0.25)));
	ret = gray + bfData.m_saturation * (ret - gray);

 // \todo film grain?
	//float rnd = Rand_FloatMantissa(Rand_Hash(uint(gl_FragCoord.x * 698.0 + gl_FragCoord.y) + Rand_Hash(uTime)));
	//ret -= vec3(rnd * 0.03);

 // display gamma
	ret = pow(ret, vec3(1.0/2.2));

	fResult = ret;
}
