#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

_VERTEX_IN(POSITIONS,    vec3,  aPosition);
_VERTEX_IN(NORMALS,      vec3,  aNormal);
_VERTEX_IN(TANGENTS,     vec4,  aTangent);
_VERTEX_IN(COLORS,       vec4,  aColor);
_VERTEX_IN(MATERIAL_UVS, vec2,  aMaterialUV);
_VERTEX_IN(LIGHTMAP_UVS, vec2,  aLightMapUV);
_VERTEX_IN(BONE_WEIGHTS, vec4,  aBoneWeights);
_VERTEX_IN(BONE_INDICES, uvec4, aBoneIndices);

_VARYING(smooth, vec3, vNormalW);
_VARYING(smooth, vec3, vTangentW);
_VARYING(smooth, vec4, vColor);
_VARYING(smooth, vec2, vMaterialUV);
_VARYING(smooth, vec2, vLightMapUV);
_VARYING(smooth, vec4, vBoneWeights);

_FRAGMENT_OUT(0, vec4, fResult);

uniform mat4 uWorld;
uniform float uAlpha;
uniform sampler2D txDepth;


#define Mode_None          0
#define Mode_Normals       1
#define Mode_Tangents      2
#define Mode_Colors        3
#define Mode_MaterialUVs   4
#define Mode_LightmapUVs   5
#define Mode_BoneWeights   6
#define Mode_BoneIndices   7
uniform int uMode;

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main()
{
	vec3 positionW = TransformPosition(uWorld, aPosition.xyz);
	gl_Position = uCamera.m_viewProj * vec4(positionW, 1.0);

	#ifndef WIREFRAME
	{
		vNormalW     = TransformDirection(uWorld, aNormal.xyz);
		vTangentW    = TransformDirection(uWorld, aTangent.xyz * aTangent.w);
		vMaterialUV  = aMaterialUV;
		vLightMapUV  = aLightMapUV;
		vColor       = aColor;
		vBoneWeights = aBoneWeights;
	}
	#endif
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

bool GridMask(in vec2 _uv, in float _scale)
{
	const ivec2 gridUv = ivec2(_uv * _scale);
	return ((gridUv.x + gridUv.y) & 1) == 0;
}

void main()
{
	float sceneDepth = texelFetch(txDepth, ivec2(gl_FragCoord.xy), 0).x;

	#ifdef WIREFRAME
	{
		if (sceneDepth < gl_FragCoord.z)
		{
			fResult = vec4(1.0, 1.0, 1.0, 0.025 * uAlpha);
		}
		else
		{
			fResult = vec4(0.0, 0.0, 0.0, 0.25 * uAlpha);
		}
	}
	#else
	{
		if (sceneDepth < gl_FragCoord.z)
		{
			discard;
		}
		fResult.a = uAlpha;
		switch (uMode)
		{
			default:
			case Mode_None:
				discard;
			case Mode_Normals:
				fResult.rgb = normalize(vNormalW) * 0.5 + 0.5;
				break;
			case Mode_Tangents:
				fResult.rgb = normalize(vTangentW) * 0.5 + 0.5;
				break;
			case Mode_Colors:
				fResult.rgb = vColor.rgb; 
				fResult.a *= vColor.a;
				break;
			case Mode_MaterialUVs:
			{
				const float gridAlpha = GridMask(vMaterialUV, 128.0) ? 0.75 : 1.0;
				fResult.rgb = vec3(fract(vMaterialUV.xy), 0.0) * gridAlpha;
				break;
			}
			case Mode_LightmapUVs:
				const float gridAlpha = GridMask(vLightMapUV, 128.0) ? 0.75 : 1.0;
				fResult.rgb = vec3(vLightMapUV.xy, 0.0) * gridAlpha;
				break;
		};
	}
	#endif
}

#endif // FRAGMENT_SHADER