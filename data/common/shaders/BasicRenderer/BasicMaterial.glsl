_VERTEX_IN(POSITIONS,    vec3, aPosition);
_VERTEX_IN(NORMALS,      vec3, aNormal);
_VERTEX_IN(TANGENTS,     vec4, aTangent);
_VERTEX_IN(MATERIAL_UVS, vec2, aMaterialUV);
_VERTEX_IN(COLORS,       vec4, aColor);
#ifdef Geometry_SkinnedMesh
	_VERTEX_IN(BONE_INDICES, vec4,  aBoneWeights);
	_VERTEX_IN(BONE_WEIGHTS, uvec4, aBoneIndices);

	layout(std430) restrict readonly buffer bfSkinning
	{
		mat4 uSkinning[];
	};
#endif

struct BasicMaterial_Instance
{
	vec4  baseColorAlpha;
	vec4  emissiveColor;
	float metallic;
	float roughness;
	float reflectance;
	float height;
};
layout(std430) restrict readonly buffer bfBasicMaterial_Instances
{
	BasicMaterial_Instance uBasicMaterial_Instances[];
};

#define BasicMaterial_Map_BaseColor     0
#define BasicMaterial_Map_Metallic      1
#define BasicMaterial_Map_Roughness     2
#define BasicMaterial_Map_Reflectance   3
#define BasicMaterial_Map_Occlusion     4
#define BasicMaterial_Map_Normal        5
#define BasicMaterial_Map_Height        6
#define BasicMaterial_Map_Emissive      7
#define BasicMaterial_Map_Alpha         8
#define BasicMaterial_Map_Translucency  9
#define BasicMaterial_Map_Count         10
uniform sampler2D uBasicMaterial_Maps[BasicMaterial_Map_Count];

vec3 _SampleNormalMap(in sampler2D _tx, in vec2 _uv)
{
	#ifdef BasicMaterial_Flag_NormalMapBC5
		vec3 ret;
		ret.xy = texture(_tx, _uv).xy * 2.0 - 1.0;
		ret.z  = sqrt(1.0 - saturate(length2(ret.xy)));
		return ret;
	#else
		return texture(_tx, _uv).xyz * 2.0 - 1.0;
	#endif
}

#define BasicMaterial_Sample(_map, _uv) (texture(uBasicMaterial_Maps[CONCATENATE_TOKENS(BasicMaterial_Map_, _map)], _uv))
#define BasicMaterial_SampleLod(_map, _uv, _lod) (textureLod(uBasicMaterial_Maps[CONCATENATE_TOKENS(BasicMaterial_Map_, _map)], _uv, _lod))
#define BasicMaterial_SampleNormalMap(_map, _uv) (_SampleNormalMap(uBasicMaterial_Maps[CONCATENATE_TOKENS(BasicMaterial_Map_, _map)], _uv))

float _Noise_InterleavedGradient(in vec2 _seed)
{
	return fract(52.9829189 * fract(dot(_seed, vec2(0.06711056, 0.00583715))));
}

float BasicMaterial_ApplyAlphaTest(in vec2 _materialUV, in uint _materialIndex, in float _instanceAlpha)
{
	const float materialAlpha = uBasicMaterial_Instances[_materialIndex].baseColorAlpha.a;
	float ret = _instanceAlpha;

	#ifdef BasicMaterial_Flag_AlphaTest
	{
		if (BasicMaterial_Sample(Alpha, _materialUV).x < materialAlpha)
		{
			#ifdef FRAGMENT_SHADER
				discard;
			#else
				return 0.0;
			#endif
		}
	}
	#else
	{
		ret *= materialAlpha; // combine material/base alpha only for non alpha-tested materials
	}
	#endif

	#if defined(BasicMaterial_Flag_AlphaDither) && defined(FRAGMENT_SHADER)
	{
		const uvec2 seed = uvec2(gl_FragCoord.xy);
		if (ret
			//< Bayer_2x2(seed)
			//< Bayer_4x4(seed)
			< _Noise_InterleavedGradient(seed)
			)
		{
			discard;
		}
	}
	#endif

	return ret;
}


