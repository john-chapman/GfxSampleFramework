#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

#ifdef DEPTH_ERROR
	uniform sampler2D txDepth;
	uniform sampler2D txRadar;
	uniform float uMaxError;
	uniform int uReconstructPosition;
	smooth in vec3 vPositionV;
	layout(location=0) out vec3 fResult;
#endif

void main()
{
	#ifdef DEPTH_ERROR
		float depth = texelFetch(txDepth, ivec2(gl_FragCoord.xy), 0).r;
		depth = Camera_GetDepthV(depth);
		
		float err = abs(depth - vPositionV.z);
		if (bool(uReconstructPosition)) {
			vec2 ndc = gl_FragCoord.xy / vec2(textureSize(txDepth, 0));
			ndc = ndc * 2.0 - 1.0;
			vec3 posV = Camera_GetFrustumRay(ndc) * -depth;
			posV = abs(posV - vPositionV);
			fResult = posV / uMaxError;
		} else {
			fResult = textureLod(txRadar, vec2(err / uMaxError, 0.5), 0.0).rgb;
		}
	#endif
}
