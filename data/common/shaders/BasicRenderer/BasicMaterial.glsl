/* 	\todo
	- Gradient framework for normal mapping? http://advances.realtimerendering.com/s2018/Siggraph%202018%20HDRP%20talk_with%20notes.pdf
*/
#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Sampling.glsl"
#include "shaders/BasicRenderer/Lighting.glsl"

#ifdef Scene_OUT
	#define GBuffer_IN
#endif
#include "shaders/BasicRenderer/GBuffer.glsl"

_VERTEX_IN(0, vec3, aPosition);
_VERTEX_IN(1, vec3, aNormal);
_VERTEX_IN(2, vec4, aTangent);
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
	_VARYING(smooth, vec3, vPrevPositionP);
	_VARYING(smooth, vec3, vNormalV);
	_VARYING(smooth, vec3, vTangentV);
	_VARYING(smooth, vec3, vBitangentV);
#endif

// per-instance uniforms \todo bufferize
uniform mat4  uWorld;
uniform mat4  uPrevWorld;
uniform vec4  uBaseColorAlpha;
uniform int   uMaterialIndex;

uniform vec2 uTexelSize; // 1/framebuffer size

struct MaterialInstance
{
	vec4  baseColorAlpha;
	vec4  emissiveColor;
	float metallic;
	float roughness;
	float reflectance;
	float height;
	uint  flags; // see Flag_ below
};
layout(std430) restrict readonly buffer bfMaterials
{
	MaterialInstance uMaterials[];
};

#define Flag_AlphaTest    0
#define Flag_Count        1

bool BasicMaterial_CheckFlag(in uint _flag)
{
	return (uMaterials[uMaterialIndex].flags & (1 << _flag)) == 1;
}

#ifdef Scene_OUT
	layout(std430) restrict readonly buffer bfLights
	{
		Lighting_Light uLights[];
	};
	uniform int uLightCount;

	// \todo see Component_ImageLight
	uniform int uImageLightCount;
	uniform float uImageLightBrightness;
	uniform samplerCube txImageLight;

	_FRAGMENT_OUT(0, vec4, fResult);
#endif

#ifdef Wireframe_OUT
	_FRAGMENT_OUT(0, vec4, fResult);
#endif

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main()
{
	vUv = aTexcoord.xy;
	//vUv.y = 1.0 - vUv.y; // \todo

	vec3 positionW = TransformPosition(uWorld, aPosition.xyz);
	#ifdef SKINNING
	{ // \todo
	}
	#endif
	gl_Position = uCamera.m_viewProj * vec4(positionW, 1.0);
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
		//vPositionP = gl_Position.xyw; // save the interpolant, use gl_FragCoord.xy / uTexelSize instead
		vPrevPositionP = (uCamera.m_prevViewProj * (uPrevWorld * vec4(aPosition.xyz, 1.0))).xyw;

		vNormalV = TransformDirection(uCamera.m_view, TransformDirection(uWorld, aNormal.xyz));
		vTangentV = TransformDirection(uCamera.m_view, TransformDirection(uWorld, aTangent.xyz * aTangent.w));
		vBitangentV = cross(vNormalV, vTangentV);
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
void BasicMaterial_ApplyAlphaTest()
{
	if (BasicMaterial_CheckFlag(Flag_AlphaTest))
	{
		float alphaThreshold = uMaterials[uMaterialIndex].baseColorAlpha.a;
		if (texture(uMaps[Map_Alpha], vUv).x < 0.3)
		{
			discard;
		}
	}

	uvec2 seed = uvec2(gl_FragCoord.xy);
	if (uBaseColorAlpha.a 
		//< Bayer_2x2(seed)
		//< Bayer_4x4(seed)
		< Noise_InterleavedGradient(seed)
		)
	{
		discard;
	}
}

void main()
{
	#ifdef GBuffer_OUT
	{
		BasicMaterial_ApplyAlphaTest();

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
    	
		velocity *= uBaseColorAlpha.a; // \hack scale velocity with alpha to reduce ghosting artefacts.

		GBuffer_WriteVelocity(velocity);
	}
	#endif

	#ifdef Scene_OUT
	{
		const vec2 iuv = gl_FragCoord.xy;

		vec3  V = Camera_GetFrustumRayW(iuv * uTexelSize * 2.0 - 1.0);
		float D = abs(Camera_GetDepthV(GBuffer_ReadDepth(ivec2(iuv))));
		vec3  P = Camera_GetPosition() + V * D;
              V = normalize(-V);	
		vec3  N = GBuffer_ReadNormal(ivec2(iuv)); // view space
		      N = normalize(TransformDirection(uCamera.m_world, N)); // world space
		fResult.a = D;

		Lighting_In lightingIn;
		vec3  baseColor   = texture(uMaps[Map_BaseColor], vUv).rgb * uMaterials[uMaterialIndex].baseColorAlpha.rgb * uBaseColorAlpha.rgb;
		      baseColor   = Gamma_Apply(baseColor);
		float metallic    = texture(uMaps[Map_Metallic],    vUv).x   * uMaterials[uMaterialIndex].metallic;
		float roughness   = texture(uMaps[Map_Roughness],   vUv).x   * uMaterials[uMaterialIndex].roughness;
		float reflectance = texture(uMaps[Map_Reflectance], vUv).x   * uMaterials[uMaterialIndex].reflectance;
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
					ret += Lighting_Spot(lightingIn, uLights[i], P, N, V);
					break;
				}
			};
		}

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

	#ifdef Wireframe_OUT
		fResult = uBaseColorAlpha;
	#endif

	#ifdef Shadow_OUT
	{
		BasicMaterial_ApplyAlphaTest();
	}
	#endif

}

#endif // FRAGMENT_SHADER