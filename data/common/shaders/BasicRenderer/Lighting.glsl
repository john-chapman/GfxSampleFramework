#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#define GBuffer_IN
#include "shaders/BasicRenderer/GBuffer.glsl"

uniform writeonly image2D txOut;

#define Type_Direct 0
#define Type_Point  1
#define Type_Spot   2
#define Type_Count  3

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
	in vec3 _positionW,
	in vec3 _normalW,
	inout Reflectance _reflectance_
)
{
	const int type = int(floor(_light.m_position.a));

	switch (type)
	{
		case Type_Direct:
		{
			float NdotL = dot(_light.m_direction.xyz, _normalW);
			_reflectance_.m_diffuse += _light.m_color.rgb * max(0.0, NdotL);
			break;
		}
		case Type_Point:
		{
			vec3 L = _light.m_position.xyz - _positionW;
			float D = length(L);
			L /= D;
			float NdotL = dot(L, _normalW);
			NdotL *= 1.0 - smoothstep(_light.m_attenuation.x, _light.m_attenuation.y, D);
			_reflectance_.m_diffuse += _light.m_color.rgb * max(0.0, NdotL);
			break;
		}
		case Type_Spot:
		{	
			float NdotL = dot(_light.m_direction.xyz, _normalW);
			if (NdotL > 0.0)
			{
				vec3 L = _positionW - _light.m_position.xyz;
				float D = length(L);
				L /= D;
				NdotL *= smoothstep(_light.m_attenuation.z, _light.m_attenuation.w, 1.0 - dot(_light.m_direction.xyz, -L));
				NdotL *= 1.0 - smoothstep(_light.m_attenuation.x, _light.m_attenuation.y, D);
				_reflectance_.m_diffuse += _light.m_color.rgb * max(0.0, NdotL);
			}
			break;
		}
	};
}

void main()
{
	vec2 txSize = vec2(imageSize(txOut).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) 
	{
		return;
	}
	ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);

	vec3 positionW = Camera_GetPosition() + Camera_GetFrustumRayW(vec2(iuv) / txSize * 2.0 - 1.0) * abs(Camera_GetDepthV(GBuffer_ReadDepth(iuv)));
	vec3 normalW = GBuffer_ReadNormal(iuv); // view space
	     normalW = normalize(TransformDirection(bfCamera.m_world, normalW)); // to world space
	vec3 roughMetalAo = GBuffer_ReadRoughMetalAo(iuv);


	Reflectance reflectance = { vec3(0.0), vec3(0.0), vec3(0.0) };
	for (int i = 0; i < uLightCount; ++i)
	{
		AccumulateLight(uLights[i], positionW, normalW, reflectance);
	}

	vec3 albedo = GBuffer_ReadAlbedo(iuv);
	vec3 ret = reflectance.m_ambient * albedo + reflectance.m_diffuse * albedo + reflectance.m_specular;
	ret += GBuffer_ReadEmissive(iuv);

	imageStore(txOut, iuv, vec4(ret, 1.0));
}
