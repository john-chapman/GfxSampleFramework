#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform sampler2D txScene;
uniform sampler2D txVelocityTileNeighborMax;
uniform writeonly image2D txOut;

layout(std140) uniform bfPostProcessData
{
	float uMotionBlurScale;
	uint  uFrameIndex;
};

vec3 Gamma(in vec3 _x)
{
	return pow(_x, vec3(1.0 / 2.2));
}

float Luminance(in vec3 _x)
{
	return dot(_x, vec3(0.25, 0.50, 0.25));
}

// Interleaved gradient noise.
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float Noise_InterleavedGradient(in vec2 _seed)
{
	return fract(52.9829189 * fract(dot(_seed, vec2(0.06711056, 0.00583715))));
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

float Cone(in vec2 _x, in vec2 _y, in vec2 _v)
{
	return saturate(1.0 - length2(_x - _y) / length2(_v));
}

float Cylinder(in vec2 _x, in vec2 _y, in vec2 _v)
{
	const float lengthV = length2(_v);
	return 1.0 - smoothstep(0.95 * lengthV, 1.05 * lengthV, length2(_x - _y));
}

float SoftStep(in float _a, in float _b, in float _radius)
{
	return saturate(1.0 - (_a - _b) / _radius);
}


void main()
{
	const ivec2 txSize = ivec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
    {
		return;
	}
	const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
    const ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
	const vec2 texelSize = 1.0 / vec2(txSize);

	vec3 ret = vec3(0.0);
    
    ret = textureLod(txScene, uv, 0.0).rgb;

    //const int kMotionBlurSampleCount = 11; // \todo quality setting
		const int kMotionBlurSampleCount = 5; // good for VR
    #if 0
	{
	// Basic motion blur.
		const vec2 jitterVelocity = uCamera.m_prevProj[2].xy * vec2(txSize);
		const vec2 velocity = 
			GBuffer_ReadVelocity(iuv) * uMotionBlurScale
			* mix(0.5, 1.0, Noise_InterleavedGradient(vec2(iuv) + jitterVelocity)) // add some noise to reduce banding
			;

        for (int i = 1; i < kMotionBlurSampleCount; ++i)
        {
            const vec2 offset = velocity * (float(i) / float(kMotionBlurSampleCount - 1) - 0.5);
            ret += textureLod(txScene, uv + offset, 0.0).rgb;
        }
        ret /= float(kMotionBlurSampleCount);
	}
	#else
	{
	// https://casual-effects.com/research/McGuire2012Blur/McGuire12Blur.pdf
		const vec2 jitterVelocity = uCamera.m_prevProj[2].xy * vec2(txSize);
		const vec2 neighborMaxV = (textureLod(txVelocityTileNeighborMax, uv, 0.0).xy * 2.0 - 1.0) * uMotionBlurScale
			* mix(0.5, 1.0, Noise_InterleavedGradient(vec2(iuv.x, iuv.y + uFrameIndex) + jitterVelocity)) // add some noise to reduce banding
			;
		if (length2(neighborMaxV) > length2(texelSize)) // only do blur if velocity > .5 texels
		{
			float weight = 1.0;
			vec4 centerSample = textureLod(txScene, uv, 0.0);
			ret = centerSample.rgb * weight;
			vec2 centerVelocity = (textureLod(txGBuffer0, uv, 0.0).zw * 2.0 - 1.0) * uMotionBlurScale;
			for (int i = 1; i < kMotionBlurSampleCount; ++i)
        	{
            	const vec2 offset = uv + neighborMaxV * (float(i) / float(kMotionBlurSampleCount - 1) - 0.5); // \todo round to neareest texel?
            	vec4 offsetSample = textureLod(txScene, offset, 0.0);
				vec2 offsetVelocity = (textureLod(txGBuffer0, offset, 0.0).zw * 2.0 - 1.0) * uMotionBlurScale;
				const float kSoftZExtent = 0.5;
				float foreground = SoftStep(offsetSample.w, centerSample.w, kSoftZExtent);
				float background = SoftStep(centerSample.w, offsetSample.w, kSoftZExtent);
				float offsetSampleWeight = 0.0
					+ foreground * Cone(offset, uv, offsetVelocity) // blurry Y in front of any X
					+ background * Cone(uv, offset, centerVelocity) // any Y behind blurry X (background reconstruction)
					//+ Cylinder(offset, uv, offsetVelocity) * Cylinder(uv, offset, centerVelocity) // \todo causes silhoettes?
					;
				weight += offsetSampleWeight;
				ret += offsetSample.rgb * offsetSampleWeight;
        	}
			ret.rgb /= weight;
		}
	}
    #endif

	ret = Tonemap_ACES_Narkowicz(ret);

	float luminance = Luminance(ret); // write luminance to alpha, useful for e.g. FXAA
	imageStore(txOut, iuv, vec4(ret, luminance));
}
