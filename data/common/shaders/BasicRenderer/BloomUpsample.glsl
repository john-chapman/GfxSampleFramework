#include "shaders/def.glsl"
#include "shaders/Noise.glsl"
#include "shaders/Rand.glsl"
#include "shaders/Sampling.glsl"

#define ROTATE_KERNEL 0

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform int uSrcLevel;

void main()
{
	const ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
	{
		return;
	}
	vec2 iuv = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(txSize);

	const vec2 srcTexelSize = 1.0 / vec2(textureSize(txSrc, uSrcLevel).xy);
	vec4 ret = vec4(0.0);

#if 0
	#if ROTATE_KERNEL
		float theta =
			#if 1
				Noise_InterleavedGradient(iuv);
			#else
				Bayer_4x4(gl_GlobalInvocationID.xy);
			#endif
		theta = theta * k2Pi;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		mat2 rotation = mat2(
			 cosTheta, sinTheta,
			-sinTheta, cosTheta
			);
	#endif

	const float kRadius = 1.0;
	const int kSampleCount = 4;
	const float rn = 1.0 / float(kSampleCount);
	for (uint i = 0; i < kSampleCount; ++i)
	{
		vec2 offset = Sampling_Hammersley2d(i, rn) * 2.0 - 1.0;
		#if ROTATE_KERNEL
			offset = rotation * offset;
		#endif
		ret += textureLod(txSrc, uv + offset * kRadius * srcTexelSize, uSrcLevel);
	}
	ret = ret * rn;
#else
	const float kWeights[9] =
	{
		1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
		2.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0,
		1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0
	};
	const vec2 kOffsets[9] =
	{
		vec2(-1, -1), vec2( 0, -1), vec2( 1, -1),
		vec2(-1,  0), vec2( 0,  0), vec2( 1,  0),
		vec2(-1,  1), vec2( 0,  1), vec2( 1,  1)
	};

	const float kRadius = 1.0;// + Noise_InterleavedGradient(iuv);

	for (int i = 0; i < 9; ++i)
	{
		ret.rgb += textureLod(txSrc, uv + kOffsets[i] * srcTexelSize * kRadius, uSrcLevel).rgb * kWeights[i];
	}
#endif

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
