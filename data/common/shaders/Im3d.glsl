#if !defined(POINTS) && !defined(LINES) && !defined(TRIANGLES)
	#error No primitive type defined
#endif
#if !defined(VERTEX_SHADER) && !defined(GEOMETRY_SHADER) && !defined(FRAGMENT_SHADER)
	#error No shader stage defined
#endif

#include "shaders/def.glsl"

#define VertexData \
	VertexData { \
		noperspective float m_edgeDistance; \
		noperspective float m_size; \
		smooth vec4 m_color; \
	}

#define kAntialiasing 2.0

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

uniform mat4 uViewProjMatrix;
	
_VERTEX_IN(0, vec4, aPositionSize);
_VERTEX_IN(1, vec4, aColor);
out VertexData vData;

void main() 
{
	vData.m_color = aColor.abgr; // swizzle to correct endianness

	#if !defined(TRIANGLES)
	{
		vData.m_color.a *= smoothstep(0.0, 1.0, aPositionSize.w / kAntialiasing);
	}
	#endif

	vData.m_size = max(aPositionSize.w, kAntialiasing);
	gl_Position = uViewProjMatrix * vec4(aPositionSize.xyz, 1.0);

	#if defined(POINTS)
	{
		gl_PointSize = vData.m_size;
	}
	#endif
}

#endif // VERTEX_SHADER


#ifdef GEOMETRY_SHADER ////////////////////////////////////////////////////////

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2 uViewport;

in  VertexData vData[];
out VertexData vDataOut;

void main() 
{
	vec2 pos0 = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	vec2 pos1 = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
	
	vec2 dir = pos0 - pos1;
	dir = normalize(vec2(dir.x, dir.y * uViewport.y / uViewport.x)); // correct for aspect ratio
	vec2 tng0 = vec2(-dir.y, dir.x);
	vec2 tng1 = tng0 * vData[1].m_size / uViewport;
	tng0 = tng0 * vData[0].m_size / uViewport;
	
 // line start
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
	
 // line end
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

#endif // GEOMETRY_SHADER


#ifdef FRAGMENT_SHADER ////////////////////////////////////////////////////////

in VertexData vData;
_FRAGMENT_OUT(0, vec4, fColor);

#if defined(DEPTH)
	uniform sampler2D txDepth;
#endif	

void main() 
{
	fColor = vData.m_color;
	
	#if defined(LINES)
	{
		float d = abs(vData.m_edgeDistance) / vData.m_size;
		d = smoothstep(1.0, 1.0 - (kAntialiasing / vData.m_size), d);
		fColor.a *= d;
	}
	#elif defined(POINTS)
	{
		float d = length(gl_PointCoord.xy - vec2(0.5));
		d = smoothstep(0.5, 0.5 - (kAntialiasing / vData.m_size), d);
		fColor.a *= d;
	}	
	#endif

	#if defined(DEPTH)
	{
		float depth = texelFetch(txDepth, ivec2(gl_FragCoord.xy), 0).x;
		fColor.a *= depth < gl_FragCoord.z ? 0.1 : 1.0;
	}
	#endif
}

#endif // FRAGMENT_SHADER
