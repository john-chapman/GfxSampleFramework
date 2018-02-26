#include "shaders/def.glsl"

#define Average_Arithmetic 0
#define Average_Geometric  1
#ifndef AVERAGE_MODE
	#define AVERAGE_MODE Average_Geometric
#endif
#define Weight_Average 0
#ifndef WEIGHT_MODE
	#define WEIGHT_MODE Weight_Average
#endif

struct Data
{
	float m_rate;
};
layout(std140) uniform _bfData
{
	Data bfData;
};
uniform float uDeltaTime;

uniform sampler2D txSrc;
uniform sampler2D txSrcPrev;
uniform writeonly image2D txDst;

uniform int uSrcLevel;
uniform int uMaxLevel;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) {
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	vec2 ret = vec2(0.0);
	if (uSrcLevel == -1) {
		ret.x = dot(textureLod(txSrc, uv, 0.0).rgb, vec3(0.25, 0.50, 0.25));
		#if (AVERAGE_MODE == Average_Geometric)
			ret = log(max(ret, 1e-7)); // use exp(avg) to get the geometric mean when reading the texture
		#endif
		ret.y = ret.x;

	} else {
		#if (WEIGHT_MODE == Weight_Average)
			float w = 1.0;
		#endif

	 // average
		ret.x = textureLod(txSrc, uv, uSrcLevel).x * w;

	 // max
		vec2 offset = 0.25 / vec2(txSize);
		vec4 s;
		s.x = textureLod(txSrc, uv + vec2(-offset.x, -offset.y), uSrcLevel).x;
		s.y = textureLod(txSrc, uv + vec2( offset.x, -offset.y), uSrcLevel).x;
		s.z = textureLod(txSrc, uv + vec2( offset.x,  offset.y), uSrcLevel).x;
		s.w = textureLod(txSrc, uv + vec2(-offset.x,  offset.y), uSrcLevel).x;
		ret.y = max(s.x, max(s.y, max(s.z, s.w)));

		if (uSrcLevel == uMaxLevel - 1) {
			vec2 prev = textureLod(txSrcPrev, uv, 99.0).xy;
			ret = prev + (ret - prev) * (1.0 - exp(uDeltaTime * -bfData.m_rate));
		}
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), vec4(ret.xyxy));
}
