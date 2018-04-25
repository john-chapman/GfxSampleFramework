#include "shaders/def.glsl"

uniform int               uLevel; // level to read from
uniform sampler2D         txIn;
uniform writeonly image2D txOut;

void main()
{
	vec2 txSize = vec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) {
		return;
	}


#if 0
/* Fractional offsets from the texel center
	+---+---+    +-------+
	| X | X |    |       |
	+---+---+ <===== X   |
	| X | X |    |       |
	+---+---+    +-------+

	uv is texel center, so add 1/4 texels offset to sample the footprint.

 */
 	ivec2 iuv  = ivec2(gl_GlobalInvocationID.xy);
	vec2  uv   = vec2(iuv) / txSize + 0.5 / txSize;

	vec2 offset = 0.25 / txSize;
	vec2 ret;
	if (uLevel == 0) {
		vec2 s;
	 	s = textureLod(txIn, uv + vec2(-offset.x, -offset.y), uLevel).xx;
		ret = s;
		s = textureLod(txIn, uv + vec2( offset.x, -offset.y), uLevel).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = textureLod(txIn, uv + vec2( offset.x,  offset.y), uLevel).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = textureLod(txIn, uv + vec2(-offset.x,  offset.y), uLevel).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
	} else {
		vec2 s;
	 	s = textureLod(txIn, uv + vec2(-offset.x, -offset.y), uLevel).xy;
		ret = s;
		s = textureLod(txIn, uv + vec2( offset.x, -offset.y), uLevel).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = textureLod(txIn, uv + vec2( offset.x,  offset.y), uLevel).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = textureLod(txIn, uv + vec2(-offset.x,  offset.y), uLevel).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
	}
	imageStore(txOut, iuv, vec4(ret, 0.0, 0.0));

#else
	ivec2 iuv = ivec2(gl_GlobalInvocationID.x << 1, gl_GlobalInvocationID.y << 1);
	vec2 ret;
	if (uLevel == 0) {
		vec2 s;
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(0, 0)).xx;
		ret = s;
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(1, 0)).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(1, 1)).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(0, 1)).xx;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
	} else {
		vec2 s;
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(0, 0)).xy;
		ret = s;
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(1, 0)).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(1, 1)).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
		s = texelFetchOffset(txIn, iuv, uLevel, ivec2(0, 1)).xy;
	   	ret.x = min(ret.x, s.x);
		ret.y = max(ret.y, s.y);
	}
	imageStore(txOut, ivec2(gl_GlobalInvocationID.xy), vec4(ret, 0.0, 0.0));
#endif
}
