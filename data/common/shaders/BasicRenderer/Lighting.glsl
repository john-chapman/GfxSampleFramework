#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform writeonly image2D txOut;

void main()
{
	vec2 txSize = vec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) 
	{
		return;
	}
	ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);

	vec3 albedo = GBuffer_ReadAlbedo(iuv);
	vec3 normal = GBuffer_ReadNormal(iuv); // view space
	     normal = normalize(TransformDirection(bfCamera.m_world, normal)); // to world space
	vec3 roughMetalAo = GBuffer_ReadRoughMetalAo(iuv);

	float NdotL = max(0.0, dot(normal, normalize(vec3(1.0))));
	      NdotL = saturate(NdotL + 0.25 * roughMetalAo.z);
	vec3 ret = NdotL * albedo;

	ret += GBuffer_ReadEmissive(iuv);

	imageStore(txOut, iuv, vec4(ret, 1.0));
}
