#include "shaders/def.glsl"

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform int uSrcLevel;

float Luminance(in vec3 _x)
{
	return dot(_x, vec3(0.25, 0.50, 0.25));
}

void main()
{
	const ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
	{
		return;
	}
	vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(txSize);

	const vec2 srcTexelSize = 1.0 / vec2(textureSize(txSrc, uSrcLevel).xy);
	vec4 ret = vec4(0.0);

// \todo
// - Use hbox 4x4 for low quality where perf is crucial (it's a lot cheaper than jorge and nearly as good).
// - Gaussian is much worse than jorge, it weights the centre samples too high.

	#if 0
		// Low quality, fast = 4 bilinear samples.
		ret.rgb += textureLod(txSrc, uv + vec2(-1, -1) * srcTexelSize, uSrcLevel).rgb;
		ret.rgb += textureLod(txSrc, uv + vec2( 1, -1) * srcTexelSize, uSrcLevel).rgb;
		ret.rgb += textureLod(txSrc, uv + vec2(-1,  1) * srcTexelSize, uSrcLevel).rgb;
		ret.rgb += textureLod(txSrc, uv + vec2( 1,  1) * srcTexelSize, uSrcLevel).rgb;
		ret.rgb *= 0.25;

		// \todo reduce fireflies?
		//if (uSrcLevel == 0)
		//{
		//	ret.rgb *= (1.0 / (1.0 + Luminance(ret.rgb)));
		//}
	#else
		// High quality, slow = 13 bilinear samples (Jimenez).
		// \todo partial karis requires sum of individual hbox 4x4 regions for luma, need to cache texture fetches and sum individually.

		ret.rgb += (
			  textureLod(txSrc, uv + vec2(-1, -1) * srcTexelSize, uSrcLevel).rgb
		    + textureLod(txSrc, uv + vec2( 1, -1) * srcTexelSize, uSrcLevel).rgb
		    + textureLod(txSrc, uv + vec2(-1,  1) * srcTexelSize, uSrcLevel).rgb
		    + textureLod(txSrc, uv + vec2( 1,  1) * srcTexelSize, uSrcLevel).rgb
			) * 0.125;

		ret.rgb += (
			  textureLod(txSrc, uv + vec2(-2, -2) * srcTexelSize, uSrcLevel).rgb
			+ textureLod(txSrc, uv + vec2( 2, -2) * srcTexelSize, uSrcLevel).rgb
			+ textureLod(txSrc, uv + vec2(-2,  2) * srcTexelSize, uSrcLevel).rgb
			+ textureLod(txSrc, uv + vec2( 2,  2) * srcTexelSize, uSrcLevel).rgb
			) * 0.03125;

		ret.rgb += (
			  textureLod(txSrc, uv + vec2( 0, -2) * srcTexelSize, uSrcLevel).rgb
		    + textureLod(txSrc, uv + vec2(-2,  0) * srcTexelSize, uSrcLevel).rgb
			+ textureLod(txSrc, uv + vec2( 2,  0) * srcTexelSize, uSrcLevel).rgb
			+ textureLod(txSrc, uv + vec2( 0,  2) * srcTexelSize, uSrcLevel).rgb
			) * 0.0625;

		ret.rgb += textureLod(txSrc, uv, uSrcLevel).rgb * 0.125;
	#endif

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
