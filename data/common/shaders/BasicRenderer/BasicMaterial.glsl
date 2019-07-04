/* 	\todo
	- Normal mapping http://advances.realtimerendering.com/s2018/Siggraph%202018%20HDRP%20talk_with%20notes.pdf
*/
#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/BasicRenderer/GBuffer.glsl"

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

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

uniform mat4 uWorld;
uniform mat4 uPrevWorld;

smooth out vec2 vUv;
smooth out vec3 vNormalV;
smooth out vec3 vTangentV;
smooth out vec3 vBitangentV;
smooth out vec2 vVelocityP;

void main()
{
	vUv = aTexcoord.xy;

	vec3 positionW = TransformPosition(uWorld, aPosition.xyz);
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

uniform vec4  uColorAlpha;
uniform float uRough;
uniform float uMetal;

uniform sampler2D txAlbedo;
uniform sampler2D txNormal;
uniform sampler2D txRough;
uniform sampler2D txMetal;
uniform sampler2D txCavity;
uniform sampler2D txHeight;
uniform sampler2D txEmissive;

smooth in vec2 vUv;
smooth in vec3 vNormalV;
smooth in vec3 vTangentV;
smooth in vec3 vBitangentV;
smooth in vec2 vVelocityP;

void main()
{
	#ifdef GBuffer_OUT
	{
		vec4 colorAlpha = uColorAlpha;
		colorAlpha *= texture(txAlbedo, vUv);
		GBuffer_WriteAlbedo(colorAlpha.rgb);
		
		float rough = uRough * texture(txRough, vUv).x;
		float metal = uMetal * texture(txMetal, vUv).x;
		float ao    = texture(txCavity, vUv).x;
		GBuffer_WriteRoughMetalAo(rough, uMetal, ao);

		vec3 normalT = normalize(texture(txNormal, vUv).xyz * 2.0 - 1.0);
		vec3 normalV = normalize(vTangentV) * normalT.x + normalize(vBitangentV) * normalT.y + normalize(vNormalV) * normalT.z;
		GBuffer_WriteNormal(normalV);

		vec2 velocity = vVelocityP; // \todo scale to [-127,127] range (pixel space)
		GBuffer_WriteVelocity(velocity);

		vec3 emissive = Gamma_Apply(texture(txEmissive, vUv).rgb);
		GBuffer_WriteEmissive(emissive);
	}
	#endif

	#ifdef Shadow_OUT
	{
	}
	#endif

}

#endif // FRAGMENT_SHADER