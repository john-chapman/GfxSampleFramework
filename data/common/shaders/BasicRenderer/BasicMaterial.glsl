#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

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

smooth out vec2 vUv;

void main()
{
	vUv = aTexcoord.xy;

	vec3 positionW = TransformPosition(uWorld, aPosition.xyz);
	gl_Position = bfCamera.m_viewProj * vec4(positionW, 1.0);
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

uniform vec4  uColorAlpha;
uniform float uRough;

uniform sampler2D txAlbedo;
uniform sampler2D txNormal;
uniform sampler2D txRough;
uniform sampler2D txCavity;
uniform sampler2D txHeight;
uniform sampler2D txEmissive;

#ifdef GBUFFER
	layout(location=0) out vec4 fGbuffer0;
	layout(location=1) out vec4 fGbuffer1;
	layout(location=2) out vec4 fGbuffer2;
#endif

smooth in vec2 vUv;

void main()
{
	#ifdef GBUFFER
	{
		fGbuffer0 = texture(txAlbedo, vUv);
		fGbuffer1 = vec4(1.0, 0.0, 1.0, 1.0);
		fGbuffer1 = vec4(0.0, 1.0, 0.0, 1.0);
	}
	#endif

	#ifdef SHADOW
	{
	}
	#endif

}

#endif // FRAGMENT_SHADER