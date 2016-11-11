#include "shaders/def.glsl"
#include "shaders/Im3d.glsl"

layout(location=0) in vec4 aPositionSize;
layout(location=1) in vec4 aColor;

uniform mat4 uViewProjMatrix;

out VertexData vData;

void main() 
{
	vData.m_color = aColor;
	vData.m_color.a *= smoothstep(0.0, 1.0, aPositionSize.w / kAntialiasing);
	gl_PointSize = max(aPositionSize.w, kAntialiasing);
	vData.m_size = gl_PointSize;
	gl_Position = uViewProjMatrix * vec4(aPositionSize.xyz, 1.0);
}
