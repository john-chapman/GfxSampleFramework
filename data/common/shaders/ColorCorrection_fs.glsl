#include "shaders/def.glsl"
#include "shaders/Rand.glsl"

noperspective in vec2 vUv;

uniform sampler2D txInput;

uniform uint uTime;

struct Data
{
	float m_saturation;
	float m_contrast;
	float m_exposureCompensation;
	float m_aperture;
	float m_shutterSpeed;
	float m_iso;
	vec3  m_tint;
};
layout(std140) uniform _bfData
{
	Data bfData;
};

#define APERTURE_MIN 1.8
#define APERTURE_MAX 22.0
#define ISO_MIN      100.0
#define ISO_MAX      6400.0

#ifdef AUTO_EXPOSURE
	uniform sampler2D txAvgLogLuminance;
#endif

layout(location=0) out vec3 fResult;

// Given an aperture, shutter speed, and exposure value compute the required ISO value
float ComputeISO(in float aperture, in float shutterSpeed, in float ev)
{
	return (sqrt(aperture) * 100.0) / (shutterSpeed * exp2(ev));
}
// Given the camera settings compute the current exposure value
float ComputeEV(in float aperture, in float shutterSpeed, in float iso)
{
	return log2((sqrt(aperture) * 100.0) / (shutterSpeed * iso));
}
// Using the light metering equation compute the target exposure value
float ComputeTargetEV(in float averageLuminance)
{
	return log2(averageLuminance * 100.0 / 12.5);
}

void ApplyShutterPriority(in float targetEV,
                          out float aperture,
                          out float shutterSpeed,
                          out float iso)
{
	// Start with the assumption that we want an aperture of 4.0
	aperture = 4.0;
 
	// Compute the resulting ISO if we left the aperture here
	iso = clamp(ComputeISO(aperture, shutterSpeed, targetEV), ISO_MIN, ISO_MAX);
 
	// Figure out how far we were from the target exposure value
	float evDiff = targetEV - ComputeEV(aperture, shutterSpeed, iso);
 
	// Compute the final aperture
	aperture = clamp(aperture * pow(sqrt(2.0), evDiff), APERTURE_MIN, APERTURE_MAX);
}

float getStandardOutputBasedExposure(float aperture,
                                     float shutterSpeed,
                                     float iso,
                                     float middleGrey = 0.18)
{
    float l_avg = (1000.0 / 65.0) * sqrt(aperture) / (iso * shutterSpeed);
    return middleGrey / l_avg;
}
float GetExposureFromEV100(float EV100)
{
// Compute the maximum luminance possible with H_sbs sensitivity
// maxLum = 78 / ( S * q ) * N^2 / t
// = 78 / ( S * q ) * 2^ EV_100
// = 78 / (100 * 0.65) * 2^ EV_100
// = 1.2 * 2^ EV
// Reference : http :// en. wikipedia . org / wiki / Film_speed
	float maxLuminance = 1.2 * exp2(EV100);
	return 1.0 / maxLuminance;
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
	#ifdef AUTO_EXPOSURE
		float avgLogLum = exp(textureLod(txAvgLogLuminance, vUv, 99.0).r);
		float targetEV = ComputeTargetEV(avgLogLum);
		float EV100 =  ComputeEV(bfData.m_aperture, bfData.m_shutterSpeed, bfData.m_iso);
		ret *= GetExposureFromEV100(EV100 - targetEV - bfData.m_exposureCompensation);
	#else
		ret *= bfData.m_exposureCompensation;
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
	ret = Tonemap_ACES_Hill(ret);

 // \todo film grain
	//float rnd = Rand_FloatMantissa(Rand_Hash(uint(gl_FragCoord.x * 698.0 + gl_FragCoord.y) + Rand_Hash(uTime)));
	//ret -= vec3(rnd * 0.03);

 // saturation/grain
	vec3 gray = vec3(dot(ret, vec3(0.25, 0.50, 0.25)));
	ret = gray + bfData.m_saturation * (ret - gray);
	
 // display gamma
	ret = pow(ret, vec3(1.0/2.2));
	
	fResult = ret;
}
