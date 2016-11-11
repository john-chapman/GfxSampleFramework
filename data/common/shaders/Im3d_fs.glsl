#include "shaders/def.glsl"
#include "shaders/Im3d.glsl"

in VertexData vData;


layout(location=0) out vec4 fResult;

void main() 
{
	fResult = vData.m_color;
	
	#ifdef LINES
		float t = vData.m_size - kAntialiasing;
		float d = abs(vData.m_edgeDistance) - t;
		d /= kAntialiasing;
		
		if (d >= 0.0) {
			fResult.a *= exp(-d * d);
		}
	#endif
	
}
