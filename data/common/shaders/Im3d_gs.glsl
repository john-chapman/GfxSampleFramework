#include "shaders/def.glsl"
#include "shaders/Im3d.glsl"

layout(lines) in;
layout(triangle_strip,max_vertices=4) out;

in  VertexData vData[];
out VertexData vDataOut;

uniform vec2 uViewport;

void main() 
{
	vec2 pos0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	vec2 pos1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
	
	vec2 dir = normalize(pos0 - pos1);
	vec2 tng0 = vec2(-dir.y, dir.x);
	vec2 tng1 = tng0 * vData[1].m_size / uViewport;
	tng0 = tng0 * vData[0].m_size / uViewport;
	
	gl_Position = vec4((pos0 - tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw); 
	vDataOut.m_edgeDistance = -vData[0].m_size;
	vDataOut.m_size = vData[0].m_size;
	vDataOut.m_color = vData[0].m_color;
	EmitVertex();
	
	gl_Position = vec4((pos0 + tng0) * gl_in[0].gl_Position.w, gl_in[0].gl_Position.zw);
	vDataOut.m_color = vData[0].m_color;
	vDataOut.m_edgeDistance = vData[0].m_size;
	vDataOut.m_size = vData[0].m_size;
	EmitVertex();
	
	gl_Position = vec4((pos1 - tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
	vDataOut.m_edgeDistance = -vData[1].m_size;
	vDataOut.m_size = vData[1].m_size;
	vDataOut.m_color = vData[1].m_color;
	EmitVertex();
	
	gl_Position = vec4((pos1 + tng1) * gl_in[1].gl_Position.w, gl_in[1].gl_Position.zw);
	vDataOut.m_color = vData[1].m_color;
	vDataOut.m_size = vData[1].m_size;
	vDataOut.m_edgeDistance = vData[1].m_size;
	EmitVertex();
}
