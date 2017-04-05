#include "shaders/def.glsl"
#include "shaders/MeshView.glsl"

#if   defined(SHADED)
	smooth in vec2 vUv;
	smooth in vec3 vNormalV;
	smooth in vec3 vBoneWeights;

	uniform int uTexcoords;
	uniform int uBoneWeights;

#elif defined(LINES)
	in VertexData vData;
#endif

layout(location=0) out vec4 fResult;

void main() 
{
	#if   defined(SHADED)
		if (bool(uTexcoords)) {
			fResult = vec4(vUv, 0.0, 1.0);
			vec2 gridUv = fract(vUv * 16.0);
			bool gridAlpha = (gridUv.x < 0.5);
			gridAlpha = (gridUv.y < 0.5) ? gridAlpha : !gridAlpha;
			fResult.rgb *= gridAlpha ? 0.75 : 1.0;

		} else if (bool(uBoneWeights)) {
			fResult = vec4(vBoneWeights, 1.0);
		} else {
			float c = 0.1 + dot(normalize(vNormalV), vec3(0.0, 0.0, 1.0)) * 0.4;
			fResult = vec4(c, c, c, 1.0);
		}
	#elif defined(LINES)
		fResult = vData.m_color;
	#endif
}
