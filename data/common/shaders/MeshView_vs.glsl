#include "shaders/def.glsl"
#include "shaders/MeshView.glsl"

layout(location=0) in vec3  aPosition;
layout(location=1) in vec3  aNormal;
layout(location=2) in vec3  aTangent;
layout(location=3) in vec2  aTexcoord;
#ifdef SKINNING
	layout(location=4) in vec4  aBoneWeights;
	layout(location=5) in uvec4 aBoneIndices;
	
	layout(std430) restrict readonly buffer _bfSkinning
	{
		mat4 bfSkinning[];
	};
#endif

uniform mat4 uWorldMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

#if   defined(SHADED)
	smooth out vec2 vUv;
	smooth out vec3 vNormalV;
	smooth out vec3 vBoneWeights;
#elif defined(LINES)
	out VertexData vData;
#endif

void main() 
{
	#ifdef SKINNING
		vec4 boneWeights = aBoneWeights;
		uvec4 boneIndices = aBoneIndices;
		mat4 boneMatrix = 
			bfSkinning[boneIndices.x] * boneWeights.x +
			bfSkinning[boneIndices.y] * boneWeights.y +
			bfSkinning[boneIndices.z] * boneWeights.z +
			bfSkinning[boneIndices.w] * boneWeights.w
			;
		vec3 posM = (boneMatrix * vec4(aPosition.xyz, 1.0)).xyz;
		vec3 nrmM = mat3(boneMatrix) * aNormal.xyz;
		vec3 tngM = mat3(boneMatrix) * aTangent.xyz;
	#else
		#define posM aPosition.xyz
		#define nrmM aNormal.xyz
		#define tngM aTangent.xyz
	#endif
	vec3 posV = (uViewMatrix * (uWorldMatrix * vec4(posM, 1.0))).xyz;

	#if    defined(SHADED)
		vUv = aTexcoord;
		vNormalV = mat3(uViewMatrix) * (mat3(uWorldMatrix) * nrmM);
		#ifdef SKINNING
			vBoneWeights = mix(aBoneWeights.xyz, vec3(1.0, 0.0, 1.0), aBoneWeights.w);
		#endif
	#elif  defined(LINES)
		vData.m_normalM = nrmM;
		vData.m_tangentM = tngM;
		vData.m_positionV = posV;
	#endif

	gl_Position = uProjMatrix * vec4(posV, 1.0);
}
