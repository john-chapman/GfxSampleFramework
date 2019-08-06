/* 	\todo
	- Normal mapping http://advances.realtimerendering.com/s2018/Siggraph%202018%20HDRP%20talk_with%20notes.pdf
*/
#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/BasicRenderer/Lighting.glsl"

#ifdef Scene_OUT
	#define GBuffer_IN
#endif
#include "shaders/BasicRenderer/GBuffer.glsl"

_VERTEX_IN(0, vec3, aPosition);
_VERTEX_IN(1, vec3, aNormal);
_VERTEX_IN(2, vec3, aTangent);
_VERTEX_IN(3, vec2, aTexcoord);
#ifdef SKINNING
	_VERTEX_IN(4, vec4,  aBoneWeights);
	_VERTEX_IN(5, uvec4, aBoneIndices);
	
	layout(std430) restrict readonly buffer bfSkinning
	{
		mat4 uSkinning[];
	};
#endif

_VARYING(smooth, vec2, vUv);
#ifdef GBuffer_OUT
	_VARYING(smooth, vec3, vNormalV);
	_VARYING(smooth, vec3, vTangentV);
	_VARYING(smooth, vec3, vBitangentV);
	_VARYING(smooth, vec2, vVelocityP);
#endif

// per-instance uniforms \todo bufferize
uniform mat4  uWorld;
uniform mat4  uPrevWorld;
uniform vec4  uBaseColorAlpha;
uniform int   uMaterialIndex;

struct MaterialInstance
{
	vec4  baseColorAlpha;
	vec4  emissiveColor;
	float metallic;
	float roughness;
	float reflectance;
	float height;
};
layout(std430) restrict readonly buffer bfMaterials
{
	MaterialInstance uMaterials[];
};

#ifdef Scene_OUT
	layout(std430) restrict readonly buffer bfLights
	{
		Lighting_Light uLights[];
	};
	uniform int uLightCount;

	_FRAGMENT_OUT(0, vec4, fResult);
#endif

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main()
{
	vUv = aTexcoord.xy;
	vUv.y = 1.0 - vUv.y;

	vec3 positionW = TransformPosition(uWorld, aPosition.xyz);
	#ifdef SKINNING
	{ // \todo
	}
	#endif
	gl_Position = bfCamera.m_viewProj * vec4(positionW, 1.0);
	#ifdef Shadow_OUT
	{
	 // project clipped vertices onto the near plane
	 	#if FRM_NDC_Z_ZERO_TO_ONE
			float depthNear = 0.0;
		#else
			float depthNear = -1.0;
		#endif
		gl_Position.z = max(gl_Position.z, depthNear);
	}
	#endif

	#ifdef GBuffer_OUT
	{
		vec3 prevPositionW = TransformPosition(uPrevWorld, aPosition.xyz);
		vec3 prevPositionP = (bfCamera.m_viewProj * vec4(prevPositionW, 1.0)).xyw;
		vec3 currPositionP = gl_Position.xyw; // \todo already interpolated by default?
		vVelocityP = (currPositionP.xy / currPositionP.z) - (prevPositionP.xy / prevPositionP.z); // \todo interpolation may be incorrect for large tris

		vNormalV = TransformDirection(bfCamera.m_view, TransformDirection(uWorld, aNormal.xyz));
		vTangentV = TransformDirection(bfCamera.m_view, TransformDirection(uWorld, aTangent.xyz));
		vBitangentV = cross(vNormalV, vTangentV);
	}
	#endif
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

uniform sampler2D txBaseColor;
uniform sampler2D txMetallic;
uniform sampler2D txRoughness;
uniform sampler2D txReflectance;
uniform sampler2D txOcclusion;
uniform sampler2D txNormal;
uniform sampler2D txHeight;
uniform sampler2D txEmissive;

void main()
{
	#ifdef GBuffer_OUT
	{
		vec3 normalT = normalize(texture(txNormal, vUv).xyz * 2.0 - 1.0);
		vec3 normalV = normalize(vTangentV) * normalT.x + normalize(vBitangentV) * normalT.y + normalize(vNormalV) * normalT.z;
		GBuffer_WriteNormal(normalV);

		vec2 velocity = vVelocityP; // \todo scale to [-127,127] range (pixel space)
		GBuffer_WriteVelocity(velocity);
	}
	#endif

	#ifdef Scene_OUT
	{
		const vec2 txSize = vec2(textureSize(txGBufferDepth, 0)); // \todo more efficient to pass as a constant?
		const vec2 iuv = gl_FragCoord.xy;

		vec3 V = Camera_GetFrustumRayW(iuv / txSize * 2.0 - 1.0);
		vec3 P = Camera_GetPosition() + V * abs(Camera_GetDepthV(GBuffer_ReadDepth(ivec2(iuv))));
             V = normalize(-V);	
		vec3 N = GBuffer_ReadNormal(ivec2(iuv)); // view space
		     N = normalize(TransformDirection(bfCamera.m_world, N)); // world space

		Lighting_In lightingIn;
		vec3  baseColor   = texture(txBaseColor,   vUv).rgb * uMaterials[uMaterialIndex].baseColorAlpha.rgb * uBaseColorAlpha.rgb;
		      baseColor   = Gamma_Apply(baseColor);
		float metallic    = texture(txMetallic,    vUv).x   * uMaterials[uMaterialIndex].metallic;
		float roughness   = texture(txRoughness,   vUv).x   * uMaterials[uMaterialIndex].roughness;
		float reflectance = texture(txReflectance, vUv).x   * uMaterials[uMaterialIndex].reflectance;
		Lighting_Init(lightingIn, N, V, roughness, baseColor, metallic, reflectance);
		vec3 ret = vec3(0.0);
		
		for (int i = 0; i < uLightCount; ++i)
		{
			const int type = int(floor(uLights[i].position.a));

			switch (type)
			{
				default:
				case LightType_Direct:
				{
					ret += Lighting_Direct(lightingIn, uLights[i], N, V);
					break;
				}
				case LightType_Point:
				{
					ret += Lighting_Point(lightingIn, uLights[i], P, N, V);
					break;
				}
				case LightType_Spot:
				{	
					/*float NdotL = dot(_light.m_direction.xyz, _normalW);
					if (NdotL > 0.0)
					{
						vec3 L = _light.m_position.xyz - _positionW;
						float D = length(L);
						L /= D;
						NdotL *= 1.0 - smoothstep(_light.m_attenuation.z, _light.m_attenuation.w, 1.0 - dot(_light.m_direction.xyz, L));
						NdotL *= 1.0 - smoothstep(_light.m_attenuation.x, _light.m_attenuation.y, D);
						_reflectance_.m_diffuse += _light.m_color.rgb * max(0.0, NdotL);
					}
					break;*/
				}
			};
		}

		fResult = vec4(ret, 1.0);
	}
	#endif

	#ifdef Shadow_OUT
	{
	}
	#endif

}

#endif // FRAGMENT_SHADER