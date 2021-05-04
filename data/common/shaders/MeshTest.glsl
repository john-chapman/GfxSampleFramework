#include "shaders/def.glsl"
#include "shaders/Camera.glsl"

_VERTEX_IN(0, vec3, aPosition);
_VERTEX_IN(1, vec3, aNormal);
_VERTEX_IN(2, vec4, aTangent);
_VERTEX_IN(3, vec2, aMaterialUV);

_FRAGMENT_OUT(0, vec4, fResult);

_VARYING(smooth, vec3, vNormalW);
_VARYING(smooth, vec3, vTangentW);
_VARYING(smooth, vec2, vMaterialUV);

uniform mat4 uWorld;
uniform vec4 uColor;

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

void main()
{
	vMaterialUV = aMaterialUV.xy;

	vec3 positionW = aPosition.xyz;
	vNormalW = aNormal.xyz;
	vTangentW = aTangent.xyz * aTangent.w;

	positionW = TransformPosition(uWorld, positionW);
	vNormalW = TransformDirection(uWorld, vNormalW);
	vTangentW  = TransformDirection(uWorld, vTangentW);
	gl_Position = uCamera.m_viewProj * vec4(positionW, 1.0);
}

#endif // VERTEX_SHADER


#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

void main()
{
	fResult = uColor;
	//fResult.rgb = vNormalW * 0.5 + 0.5;
	//fResult.rg = fract(vMaterialUV); fResult.b = 0.0;
}

#endif // FRAGMENT_SHADER