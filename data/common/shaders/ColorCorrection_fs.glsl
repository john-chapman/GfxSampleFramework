#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"

noperspective in vec2 vUv;

uniform sampler2D txInput;

struct Data
{
	float m_exposure;
	float m_saturation;
	float m_contrast;
	vec3  m_tint;
};
layout(std140) uniform _bfData
{
	Data bfData;
};

layout(location=0) out vec3 fResult;

#define kLumaWeights vec3(0.25, 0.5, 0.25)

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

 // exposure
	ret *= bfData.m_exposure;

 // tint
	ret *= bfData.m_tint;

 // contrast
	//ret = saturate(vec3(0.5) + (ret - vec3(0.5)) * uContrast); // linear contrast
	float logMidpoint = 0.18; // log2(linear midpoint)
	ret.x = LogContrast(ret.x, 1e-7, logMidpoint, bfData.m_contrast);
	ret.y = LogContrast(ret.y, 1e-7, logMidpoint, bfData.m_contrast);
	ret.z = LogContrast(ret.z, 1e-7, logMidpoint, bfData.m_contrast);
 // tonemap
	ret = Tonemap_ACES_Hill(ret);

 // saturation
	vec3 gray = vec3(dot(kLumaWeights, ret));
	ret = gray + bfData.m_saturation * (ret - gray);	

 // display gamma
	ret = pow(ret, vec3(1.0/2.2));
	
	fResult = ret;
}
