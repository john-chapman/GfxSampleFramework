#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform writeonly image2D txOut;

struct LightInstance
{
 // \todo pack
	vec4 m_position;
	vec4 m_direction;
	vec4 m_color;       // RGB = color * brightness, A = brightness
	vec4 m_attenuation; // X,Y = linear attenuation start,stop, Z,W = radial attenuation start,stop
};
layout(std430) restrict readonly buffer bfLights
{
	LightInstance uLights[];
};
uniform int uLightCount;

struct Reflectance
{
	vec3 m_ambient;
	vec3 m_diffuse;
	vec3 m_specular;
};

void AccumulateLight(
	in LightInstance _light,
	in vec3 _normalW,
	in vec3 _positionW,
	inout Reflectance _reflectance_
)
{
}

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
