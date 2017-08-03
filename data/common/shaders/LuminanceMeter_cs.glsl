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
uniform image2D writeonly txDst;

uniform int uSrcLevel;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) {
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	vec2 ret = vec2(0.0);
	if (uSrcLevel == -1) {
		ret.y = dot(textureLod(txSrc, uv, 0.0).rgb, vec3(0.25, 0.50, 0.25));
		#if (AVERAGE_MODE == Average_Geometric)
			ret.y = log(max(ret.y, 1e-7)); // use exp(avg) to get the geometric mean when reading the texture
		#endif

		float prev = textureLod(txSrcPrev, uv, 0.0).x;
		ret.x = prev + (ret.y - prev) * (1.0 - exp(uDeltaTime * -bfData.m_rate));
	} else {
		#if (WEIGHT_MODE == Weight_Average)
			float w = 1.0;
		#endif
		ret.xy = textureLod(txSrc, uv, uSrcLevel).xy * w;
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), vec4(ret.xyxy));
}
