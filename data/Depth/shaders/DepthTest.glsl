#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

_VERTEX_IN(0, vec3, aPosition);
_VERTEX_IN(1, vec3, aNormal);
_VERTEX_IN(2, vec3, aTangent);
_VERTEX_IN(3, vec2, aTexcoord);

layout(std430) buffer _bfInstances
{
	mat4 bfInstances[];
};

#ifdef DEPTH_ERROR
	uniform sampler2D txDepth;
	uniform sampler2D txRadar;
	uniform float     uMaxError;
	uniform int       uReconstructPosition;

	_VARYING(smooth, vec3, vPositionV);
	_FRAGMENT_OUT(0, vec3, fResult);
#endif


#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main() 
{
	vec3 posV = TransformPosition(bfCamera.m_view, TransformPosition(bfInstances[gl_InstanceID], aPosition.xyz));
	#ifdef DEPTH_ERROR
		vPositionV = posV;
	#endif
	gl_Position = bfCamera.m_proj * vec4(posV, 1.0);
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

void main()
{
	#ifdef DEPTH_ERROR
		float depth = texelFetch(txDepth, ivec2(gl_FragCoord.xy), 0).r;
		depth = Camera_GetDepthV(depth);
		
		float err = abs(depth - vPositionV.z);
		if (bool(uReconstructPosition))
		{
			vec2 ndc = gl_FragCoord.xy / vec2(textureSize(txDepth, 0));
			ndc = ndc * 2.0 - 1.0;
			vec3 posV = Camera_GetFrustumRay(ndc) * -depth;
			posV = abs(posV - vPositionV);
			fResult = posV / uMaxError;
		}
		else
		{
			fResult = textureLod(txRadar, vec2(err / uMaxError, 0.5), 0.0).rgb;
		}
	#endif
}

#endif // FRAGMENT_SHADER
