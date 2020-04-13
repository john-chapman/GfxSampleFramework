/* 	\todo
	- Gradient framework for normal mapping? http://advances.realtimerendering.com/s2018/Siggraph%202018%20HDRP%20talk_with%20notes.pdf
*/
#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Sampling.glsl"

#if defined(Pass_GBuffer)
	#define GBuffer_OUT
#elif defined(Pass_Scene)
	#define GBuffer_IN
#endif
#include "shaders/BasicRenderer/GBuffer.glsl"

_VERTEX_IN(0, vec3, aPosition);
_VERTEX_IN(1, vec3, aNormal);
_VERTEX_IN(2, vec4, aTangent);
_VERTEX_IN(3, vec2, aTexcoord);
#ifdef Geometry_SkinnedMesh
	_VERTEX_IN(4, vec4,  aBoneWeights);
	_VERTEX_IN(5, uvec4, aBoneIndices);
	
	layout(std430) restrict readonly buffer bfSkinning
	{
		mat4 uSkinning[];
	};
#endif

#if defined(Geometry_Mesh) || defined(Geometry_SkinnedMesh)
	_VARYING(flat, uint, vInstanceId);
	_VARYING(smooth, vec2, vUv);
	#ifdef Pass_GBuffer
		_VARYING(smooth, vec3, vPrevPositionP);
		_VARYING(smooth, vec3, vNormalV);
		_VARYING(smooth, vec3, vTangentV);
		_VARYING(smooth, vec3, vBitangentV);
	#endif
	
	struct DrawInstance
	{
		mat4 world;
		mat4 prevWorld;
		vec4 baseColorAlpha;
		uint materialIndex;
		uint submeshIndex;
		uint skinningOffset;
	};
	layout(std430) restrict readonly buffer bfDrawInstances
	{
		DrawInstance uDrawInstances[];
	};
#endif // defined(Geometry_Mesh) || defined(Geometry_SkinnedMesh)

uniform vec2 uTexelSize; // 1/framebuffer size

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

#if defined(Pass_Scene) && defined(FRAGMENT_SHADER)
	
	#include "shaders/BasicRenderer/Lighting.glsl"
	#include "shaders/BasicRenderer/Shadow.glsl"

	layout(std430) restrict readonly buffer bfLights
	{
		Lighting_Light uLights[];
	};
	uniform int uLightCount; // can't use uLights.length since it can be 0

	layout(std430) restrict readonly buffer bfShadowLights
	{
		Lighting_ShadowLight uShadowLights[];
	};
	uniform int uShadowLightCount; // can't use uShadowLights.length since it can be 0
	uniform sampler2DArray txShadowMap;

	// \todo see Component_ImageLight
	uniform int uImageLightCount;
	uniform float uImageLightBrightness;
	uniform samplerCube txImageLight;

	_FRAGMENT_OUT(0, vec4, fResult);
#endif

#ifdef Pass_Wireframe
	_FRAGMENT_OUT(0, vec4, fResult);
#endif

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main()
{
	#if defined(Geometry_Mesh) || defined(Geometry_SkinnedMesh)
	{
		vInstanceId = gl_InstanceID;

		vUv = aTexcoord.xy;
		vUv.y = 1.0 - vUv.y; // \todo

		vec3 positionW = aPosition.xyz;
		vec3 prevPositionW = aPosition.xyz;
		vec3 normalW = aNormal.xyz;
		vec3 tangentW = aTangent.xyz * aTangent.w;
		#ifdef Geometry_SkinnedMesh
		{
			const uint offset = uDrawInstances[gl_InstanceID].skinningOffset;
			const mat4 boneMatrix = 
				uSkinning[aBoneIndices.x * 2 + offset] * aBoneWeights.x +
				uSkinning[aBoneIndices.y * 2 + offset] * aBoneWeights.y +
				uSkinning[aBoneIndices.z * 2 + offset] * aBoneWeights.z +
				uSkinning[aBoneIndices.w * 2 + offset] * aBoneWeights.w
				;
			positionW = TransformPosition(boneMatrix, positionW);
			#ifdef Pass_GBuffer
				const mat4 prevBoneMatrix = 
					uSkinning[aBoneIndices.x * 2 + offset + 1] * aBoneWeights.x +
					uSkinning[aBoneIndices.y * 2 + offset + 1] * aBoneWeights.y +
					uSkinning[aBoneIndices.z * 2 + offset + 1] * aBoneWeights.z +
					uSkinning[aBoneIndices.w * 2 + offset + 1] * aBoneWeights.w
					;
				prevPositionW = TransformPosition(prevBoneMatrix, aPosition.xyz);
				normalW  = TransformDirection(boneMatrix, normalW);
				tangentW = TransformDirection(boneMatrix, tangentW);
			#endif
		}
		#endif
		
		const mat4 world = uDrawInstances[gl_InstanceID].world;
		positionW = TransformPosition(world, positionW);
		normalW = TransformDirection(world, normalW);
		tangentW  = TransformDirection(world, tangentW);
		gl_Position = uCamera.m_viewProj * vec4(positionW, 1.0);				
		#ifdef Pass_GBuffer
		{
			const mat4 prevWorld = uDrawInstances[gl_InstanceID].prevWorld;
			//vPositionP = gl_Position.xyw; // save the interpolant, use gl_FragCoord.xy / uTexelSize instead
			vPrevPositionP = (uCamera.m_prevViewProj * (prevWorld * vec4(prevPositionW, 1.0))).xyw;

			vNormalV = TransformDirection(uCamera.m_view, normalW);
			vTangentV = TransformDirection(uCamera.m_view, tangentW);
			vBitangentV = cross(vNormalV, vTangentV);
		}
		#endif
		

		#ifdef Pass_Shadow
		{
		 // project clipped vertices onto the near plane
		 	if (Camera_GetProjFlag(Camera_ProjFlag_Orthographic)) // only valid for ortho projections
			{
		 		#if FRM_NDC_Z_ZERO_TO_ONE
					float depthNear = 0.0;
				#else
					float depthNear = -1.0;
				#endif
				gl_Position.z = max(gl_Position.z, depthNear);
			}
		}
		#endif
	}
	#endif
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

#define Map_BaseColor     0
#define Map_Metallic      1
#define Map_Roughness     2
#define Map_Reflectance   3
#define Map_Occlusion     4
#define Map_Normal        5
#define Map_Height        6
#define Map_Emissive      7
#define Map_Alpha         8
#define Map_Count         9
uniform sampler2D uMaps[Map_Count];
float Noise_InterleavedGradient(in vec2 _seed)
{
	return fract(52.9829189 * fract(dot(_seed, vec2(0.06711056, 0.00583715))));
}
float BasicMaterial_ApplyAlphaTest()
{
	const uint materialIndex = uDrawInstances[vInstanceId].materialIndex;
	const float materialAlpha = uMaterials[materialIndex].baseColorAlpha.a;
	float instanceAlpha = uDrawInstances[vInstanceId].baseColorAlpha.a;

	#ifdef Material_AlphaTest
	{
		if (texture(uMaps[Map_Alpha], vUv).x < materialAlpha)
		{
			discard;
		}
	}
	#else
	{
		instanceAlpha *= materialAlpha; // combine material/base alpha only for non alpha-tested materials
	}
	#endif

	//#ifdef Material_AlphaDither
	#if 1
	{
		uvec2 seed = uvec2(gl_FragCoord.xy);
		if (instanceAlpha
			//< Bayer_2x2(seed)
			//< Bayer_4x4(seed)
			< Noise_InterleavedGradient(seed)
			)
		{
			discard;
		}
	}
	#endif

	return instanceAlpha;
}

void main()
{
	#ifdef Pass_GBuffer
	{
		float alpha = BasicMaterial_ApplyAlphaTest();

		vec3 normalT = normalize(texture(uMaps[Map_Normal], vUv).xyz * 2.0 - 1.0);
		vec3 normalV = normalize(vTangentV) * normalT.x + normalize(vBitangentV) * normalT.y + normalize(vNormalV) * normalT.z;
		GBuffer_WriteNormal(normalV);

		vec2 positionP = gl_FragCoord.xy * uTexelSize;
		vec2 prevPositionP = vPrevPositionP.xy / vPrevPositionP.z * 0.5 + 0.5;
		vec2 velocity = positionP - prevPositionP;
		
		// Correct for any jitter in the projection matrices.
    	// Here we need to account for the current and previous jitter since both positionP and prevPositionP have jitter applied.
		vec2 jitterVelocity = (uCamera.m_prevProj[2].xy - uCamera.m_proj[2].xy) * 0.5;
    	velocity -= jitterVelocity;
    	
		velocity *= alpha; // \hack scale velocity with alpha to reduce ghosting artefacts.

		GBuffer_WriteVelocity(velocity);
	}
	#endif

	#ifdef Pass_Scene
	{
		const vec2 iuv = gl_FragCoord.xy;

		vec3  V = Camera_GetFrustumRayW(iuv * uTexelSize * 2.0 - 1.0);
		float D = abs(Camera_GetDepthV(GBuffer_ReadDepth(ivec2(iuv))));
		vec3  P = Camera_GetPosition() + V * D;
              V = normalize(-V);	
		vec3  N = GBuffer_ReadNormal(ivec2(iuv)); // view space
		      N = normalize(TransformDirection(uCamera.m_world, N)); // world space
		fResult.a = D;

		const uint materialIndex = uDrawInstances[vInstanceId].materialIndex;
		Lighting_In lightingIn;
		vec3  baseColor   = texture(uMaps[Map_BaseColor], vUv).rgb * uMaterials[materialIndex].baseColorAlpha.rgb * uDrawInstances[vInstanceId].baseColorAlpha.rgb;
		      baseColor   = Gamma_Apply(baseColor);
		float metallic    = texture(uMaps[Map_Metallic],    vUv).x   * uMaterials[materialIndex].metallic;
		float roughness   = texture(uMaps[Map_Roughness],   vUv).x   * uMaterials[materialIndex].roughness;
		float reflectance = texture(uMaps[Map_Reflectance], vUv).x   * uMaterials[materialIndex].reflectance;
		Lighting_Init(lightingIn, N, V, roughness, baseColor, metallic, reflectance);
		vec3 ret = vec3(0.0);
		
		// Lights.
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
					ret += Lighting_Spot(lightingIn, uLights[i], P, N, V);
					break;
				}
			};
		}

		// Shadow lights.
		const float shadowTexelSize = 1.0 / float(textureSize(txShadowMap, 0).x);
		for (int i = 0; i < uShadowLightCount; ++i)
		{
			const int type = int(floor(uShadowLights[i].light.position.a));

			switch (type)
			{
				default:
				case LightType_Direct:
				{
					vec3 radiance = Lighting_Direct(lightingIn, uShadowLights[i].light, N, V);
					if (!bool(Shadow_PREDICATE_NoL) || lightingIn.NoL > Lighting_EPSILON)
					{
						vec3 shadowCoord = Shadow_Project(P, uShadowLights[i].worldToShadow, shadowTexelSize, uShadowLights[i].uvScale, uShadowLights[i].uvBias);
						radiance *= Shadow_FetchBilinear(txShadowMap, shadowCoord, uShadowLights[i].arrayIndex, shadowTexelSize);
					}
					ret += radiance;
					break;
				}
				case LightType_Point:
				{
					vec3 radiance = Lighting_Point(lightingIn, uShadowLights[i].light, P, N, V);
					if (!bool(Shadow_PREDICATE_NoL) || lightingIn.NoL > Lighting_EPSILON)
					{
						// \todo
					}
					ret += radiance;
					break;
				}
				case LightType_Spot:
				{	
					vec3 radiance = Lighting_Spot(lightingIn, uShadowLights[i].light, P, N, V);
					if (!bool(Shadow_PREDICATE_NoL) || lightingIn.NoL > Lighting_EPSILON)
					{
						vec3 shadowCoord = Shadow_Project(P, uShadowLights[i].worldToShadow, shadowTexelSize, uShadowLights[i].uvScale, uShadowLights[i].uvBias);
						radiance *= Shadow_FetchBilinear(txShadowMap, shadowCoord, uShadowLights[i].arrayIndex, shadowTexelSize);
					}
					ret += radiance;
					break;
				}
			};
		}

		// Image lights.
		if (uImageLightCount > 0)
		{
		 // \todo rewrite this as per Filament
			float maxLevel = 6.0; // depends on envmap size, limit to 8x8 face to prevent filtering artefacts
			ret += (textureLod(txImageLight, N, maxLevel).rgb * uImageLightBrightness) * lightingIn.diffuse;

			vec3 R = reflect(-V, N);
			ret += lightingIn.f0 * textureLod(txImageLight, R, sqrt(roughness) * maxLevel).rgb * uImageLightBrightness;
		}

		fResult.rgb = ret;
	}
	#endif

	#ifdef Pass_Wireframe
		const uint materialIndex = uDrawInstances[vInstanceId].materialIndex;
		fResult = uDrawInstances[vInstanceId].baseColorAlpha * uMaterials[materialIndex].baseColorAlpha;
	#endif

	#ifdef Pass_Shadow
	{
		BasicMaterial_ApplyAlphaTest();
	}
	#endif

}

#endif // FRAGMENT_SHADER