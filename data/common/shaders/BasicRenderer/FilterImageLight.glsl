#include "shaders/def.glsl"
#include "shaders/Envmap.glsl"
#include "shaders/Sampling.glsl"
#include "shaders/Rand.glsl"

#ifndef CUBEMAP_ARRAY
	#define CUBEMAP_ARRAY 0
#endif

// Normal distribution function (should match BasicRenderer/Lighting.glsl).
float Lighting_SpecularD_GGX(in float NoH, in float alpha)
{
	float a = NoH * alpha;
	float k = alpha / (1.0 - NoH * NoH + a * a);
	return k * k * (1.0 / kPi);
}

#if CUBEMAP_ARRAY
	uniform samplerCubeArray txSrc;
	uniform writeonly imageCubeArray txDst;
	uniform int uArrayIndex;

	#define SampleSrc(uvw, lod) (textureLod(txSrc, vec4(uvw, float(uArrayIndex)), lod))
#else
	uniform samplerCube txSrc;
	uniform writeonly imageCube txDst;

	#define SampleSrc(uvw, lod) (textureLod(txSrc, uvw, lod))
#endif

uniform int uLevel;
uniform int uMaxLevel;
uniform int uSrcIsGamma;

#define kMaxSampleCount 512
shared vec3  s_sampleL[kMaxSampleCount];
shared float s_sampleMips[kMaxSampleCount];
shared float s_sampleWeights[kMaxSampleCount];
shared int   s_sampleCount;
shared float s_invSampleWeightSum;

void main()
{
	const ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize)))
	{
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
	const vec3 uvw = normalize(Envmap_GetCubeFaceUvw(uv, int(gl_WorkGroupID.z)));

	if (uLevel == 0)
	{
		// Copy src -> dst.
		vec4 ret = SampleSrc(uvw, 0.0);
		if (bool(uSrcIsGamma))
		{
			ret.rgb = pow(ret.rgb, vec3(2.2));
		}

		#if CUBEMAP_ARRAY
			imageStore(txDst, ivec4(gl_GlobalInvocationID.xy, gl_WorkGroupID.z, uArrayIndex), ret);
		#else
			imageStore(txDst, ivec3(gl_GlobalInvocationID.xy, gl_WorkGroupID.z), ret);
		#endif
	}
	else
	{
		// Precompute sample directions and weights.
		float roughness = float(uLevel) / float(uMaxLevel); // \todo uMaxLevel - 1?
		if (gl_LocalInvocationIndex == 0)
		{
			s_sampleCount = 0;
			s_invSampleWeightSum = 0.0;
			for (int i = 0; i < kMaxSampleCount; ++i)
			{
				// Generate sample direction around +Z.
				float alpha = clamp(roughness * roughness, 1e-4, 1.0); // Remap perceptual roughness.
				float rn    = 1.0 / float(kMaxSampleCount);
				vec2  xi    = Sampling_Hammersley2d(i, rn);
				vec3  H     = Sampling_Hemisphere(xi, alpha);

				vec3 L = reflect(-vec3(0.0, 0.0, 1.0), H);
				if (L.z > 0.0)
				{
					s_sampleL[s_sampleCount] = L;
					s_sampleWeights[s_sampleCount] = L.z;
					s_invSampleWeightSum += L.z;

					// Compute mip level.
					float srcSize = float(textureSize(txSrc, 0).x);
					float NoH     = L.z;
					float pdf     = Lighting_SpecularD_GGX(NoH, alpha);
					float omegaS  = 1.0 / (float(kMaxSampleCount) * pdf); // Solid angle this sample.
					float omegaP  = 4.0 * kPi / (6.0 * srcSize * srcSize); // Solid angle covered by 1 pixel with 6 faces that are srcSize X srcSize.
					float bias    = 1.0; // Bias the result to improve smoothness.
					float mip     = max(0.5 * log2(omegaS / omegaP) + bias, 0.0);
					s_sampleMips[s_sampleCount] = mip;

					++s_sampleCount;
				}
			}
			s_invSampleWeightSum = 1.0 / s_invSampleWeightSum;
		}
		memoryBarrierShared();
		barrier();

		// Filter.
		vec3 up = abs(uvw.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
		vec3 tx = normalize(cross(up, uvw));
		vec3 ty = cross(uvw, tx);
		mat3 basis = mat3(tx, ty, uvw);
		vec3 ret = vec3(0.0);
		for (int i = 0; i < s_sampleCount; ++i)
		{
			vec3 L = basis * s_sampleL[i];
			ret += SampleSrc(L, s_sampleMips[i]).rgb * s_sampleWeights[i];
		}
		ret *= s_invSampleWeightSum;
		
		#if CUBEMAP_ARRAY
			imageStore(txDst, ivec4(gl_GlobalInvocationID.xy, gl_WorkGroupID.z, uArrayIndex), vec4(ret, 1.0));
		#else
			imageStore(txDst, ivec3(gl_GlobalInvocationID.xy, gl_WorkGroupID.z), vec4(ret, 1.0));
		#endif
	}
}
