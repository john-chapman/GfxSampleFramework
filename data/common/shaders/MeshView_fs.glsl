#include "shaders/def.glsl"
#include "shaders/MeshView.glsl"

#if   defined(SHADED)
	smooth in vec2 vUv;
	smooth in vec3 vNormalV;
	smooth in vec3 vBoneWeights;

	uniform vec4 uColor         = vec4(0.4, 0.4, 0.4, 1.0);
	uniform int  uTexcoords     = 0;
	uniform int  uBoneWeights   = 0;

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
			float c = 0.1 + dot(normalize(vNormalV), vec3(0.0, 0.0, 1.0));
			fResult = vec4(c * uColor.rgb, uColor.a);
		}
	#elif defined(LINES)
		fResult = vData.m_color;
	#endif
}
