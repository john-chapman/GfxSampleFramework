#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform sampler2D txScene;
uniform writeonly image2D txFinal;

layout(std140) uniform bfPostProcessData
{
	float uMotionBlurScale;
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

void main()
{
	const ivec2 txSize = ivec2(imageSize(txFinal).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
    {
		return;
	}
	const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
    const ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);

	vec3 ret = vec3(0.0);
    
    ret = textureLod(txScene, uv, 0.0).rgb;

    #if 1 // motion blur
		const vec2 velocity = GBuffer_ReadVelocity(iuv) * uMotionBlurScale
			* mix(0.5, 1.5, Noise_InterleavedGradient(vec2(iuv))) // add some noise to reduce banding
			;

        const int kSampleCount = 8;
        for (int i = 1; i < kSampleCount; ++i)
        {
            const vec2 offset = velocity * (float(i) / float(kSampleCount - 1) - 0.5);
            ret += textureLod(txScene, uv + offset, 0.0).rgb;
        }
        ret /= float(kSampleCount);
    #endif

	ret = Tonemap_ACES_Narkowicz(ret);

	float luminance = Luminance(ret); // write luminance to alpha, useful for e.g. FXAA

	imageStore(txFinal, iuv, vec4(ret, luminance));
}
