#include "shaders/def.glsl"
#include "shaders/MeshView.glsl"

layout(triangles) in;
layout(line_strip, max_vertices = 24) out;

in  VertexData vData[];
out VertexData vDataOut;

uniform mat4  uWorldMatrix;
uniform mat4  uViewMatrix;
uniform mat4  uProjMatrix;
uniform int   uWireframe;
uniform int   uNormals;
uniform int   uTangents;
uniform float uVectorLength;

void main() 
{
	vec3 faceNormal;
	vec3 ab = vData[1].m_positionV - vData[0].m_positionV;
	vec3 bc = vData[2].m_positionV - vData[0].m_positionV;
	faceNormal = normalize(cross(ab, bc));

	float alpha = max(faceNormal.z, 0.0);


	if (bool(uWireframe)) {
		float c = 0.7;
		gl_Position = gl_in[0].gl_Position;
		vDataOut.m_color = vec4(vec3(c), alpha);
		EmitVertex();
	
		gl_Position = gl_in[1].gl_Position;
		vDataOut.m_color = vec4(vec3(c), alpha);
		EmitVertex();
	
		gl_Position = gl_in[2].gl_Position;
		vDataOut.m_color = vec4(vec3(c), alpha);
		EmitVertex();
	
		gl_Position = gl_in[0].gl_Position;
		vDataOut.m_color = vec4(vec3(c), alpha);
		EmitVertex();

		EndPrimitive();
	}

	if (bool(uNormals)) {
		for (int i = 0; i < gl_in.length(); ++i) {		
			vec3 nrmV = mat3(uViewMatrix) * (mat3(uWorldMatrix) * vData[i].m_normalM);
			vec3 color = vData[i].m_normalM * 0.5 + 0.5;

			gl_Position = gl_in[i].gl_Position;
			vDataOut.m_color = vec4(color * 0.1, 1.0);
			EmitVertex();
	
			gl_Position = uProjMatrix * vec4(vData[i].m_positionV + nrmV * uVectorLength, 1.0);
			vDataOut.m_color = vec4(color, 1.0);
			EmitVertex();
		
			EndPrimitive();
		}
	}

	if (bool(uTangents)) {
		for (int i = 0; i < gl_in.length(); ++i) {		
			vec3 tngV = mat3(uViewMatrix) * (mat3(uWorldMatrix) * vData[i].m_tangentM);
			vec3 color = vData[i].m_tangentM * 0.5 + 0.5;

			gl_Position = gl_in[i].gl_Position;
			vDataOut.m_color = vec4(color * 0.1, 1.0);
			EmitVertex();
	
			gl_Position = uProjMatrix * vec4(vData[i].m_positionV + tngV * uVectorLength, 1.0);
			vDataOut.m_color = vec4(color, 1.0);
			EmitVertex();
		
			EndPrimitive();
		}
	}
}
