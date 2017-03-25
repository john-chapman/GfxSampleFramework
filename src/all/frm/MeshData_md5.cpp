#include "MeshData.h"

#include <apt/TextParser.h>
#include <apt/Time.h>

#include <algorithm>
#include <vector>

using namespace frm;
using namespace apt;

struct Md5Joint {
	String<32> m_name;
	long int   m_parentIndex;
	vec3       m_position;
	quat       m_orientation;
};

struct Md5Vert {
	long int   m_index;
	vec2       m_texcoord;
	long int   m_weightStart;
	long int   m_weightCount;
};
struct Md5Tri {
	long int   m_index;
	long int   m_verts[3];
};
struct Md5Weight {
	long int   m_index;
	long int   m_jointIndex;
	float      m_bias;
	vec3       m_position;
};
struct Md5Mesh {
	String<32>             m_name; // from the shader field
	std::vector<Md5Vert>   m_verts;
	std::vector<Md5Tri>    m_tris;
	std::vector<Md5Weight> m_weights;
};


bool MeshData::ReadMd5(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	MeshData_AUTOTIMER("MeshData::ReadMd5");

	#define syntax_error(_msg) \
		APT_LOG_ERR("Md5 syntax error, line %d: '%s'", tp.getLineCount(), _msg); \
		ret = false; \
		goto MeshData_LoadMd5_end

	#define misc_error(_fmt, ...) \
		APT_LOG_ERR("Md5 error: " _fmt, __VA_ARGS__); \
		ret = false; \
		goto MeshData_LoadMd5_end

	long int numJoints = -1;
	std::vector<Md5Joint> joints;
	long int numMeshes = -1;
	std::vector<Md5Mesh> meshes;

	MeshBuilder tmpMesh;
	bool ret = true;

	TextParser tp(_srcData);
	while (!tp.isNull()) {
		tp.skipWhitespace();
		
		if (tp[0] == '/' && tp[1] == '/') { // comment
			tp.skipLine();
			continue;
		}
		if (tp.compareNext("MD5Version")) {
			tp.skipWhitespace();
			long int version;
			if (!tp.readNextInt(version)) {
				syntax_error("MD5Version");
			}
			if (version != 10) {
				misc_error("Unsupported version (%d), only version 10 supported", version);
			}
			continue;
		}
		if (tp.compareNext("commandline")) {
			tp.skipLine();
			continue;
		}
		if (tp.compareNext("numJoints")) {
			tp.skipWhitespace();
			if (!tp.readNextInt(numJoints)) {
				syntax_error("numJoints");
			}
			continue;
		}
		if (tp.compareNext("numMeshes")) {
			tp.skipWhitespace();
			if (!tp.readNextInt(numMeshes)) {
				syntax_error("numMeshes");
			}
			continue;
		}
		if (tp.compareNext("joints")) {
			if (tp.advanceToNext('{') != '{') {
				syntax_error("expected a '{'");
			}
			tp.advance(); // skip {

			while (!tp.isNull() && !(*tp == '}')) {
				joints.push_back(Md5Joint());
				Md5Joint& joint = joints.back();

			 // name
				tp.skipWhitespace();
				if (*tp != '"') {
					syntax_error("expected '\"' (joint name)");
				}
				tp.advance(); // skip "
				const char* beg = tp;
				if (tp.advanceToNext('"') != '"') {
					syntax_error("expected '\"' (joint name)");
				}
				joint.m_name.set(beg, tp - beg);
				tp.advance(); // skip "

			 // parent
				tp.skipWhitespace();
				if (!tp.readNextInt(joint.m_parentIndex)) {
					misc_error("joint '%s' missing parent index", (const char*)joint.m_name);
				}

			 // position
				tp.skipWhitespace();
				if (*tp != '(') { 
					syntax_error("expected '(' (joint position)");
				}
				tp.advance(); // skip (
				for (int i = 0; i < 3; ++i) {
					tp.skipWhitespace();
					double d;
					if (!tp.readNextDouble(d)) {
						syntax_error("expected a number (joint position)");
					}
					joint.m_position[i] = (float)d;
				}
				tp.skipWhitespace();
				if (*tp != ')') { 
					syntax_error("expected ')' (joint position)");
				}
				tp.advance(); // skip )

			 // orientation
				tp.skipWhitespace();
				if (*tp != '(') { 
					syntax_error("expected '(' (joint orientation)");
				}
				tp.advance(); // skip (
				joint.m_orientation = quat(0.0f, 0.0f, 0.0f, 0.0f);
				for (int i = 0; i < 3; ++i) {
					tp.skipWhitespace();
					double d;
					if (!tp.readNextDouble(d)) {
						syntax_error("expected a number (joint orientation)");
					}
					joint.m_orientation[i] = (float)d;
				}
				// recover w
				float t = 1.0f - length2(joint.m_orientation);
				joint.m_orientation.w = t < 0.0f ? 0.0f : -sqrtf(t);

				tp.skipWhitespace();
				if (*tp != ')') { 
					syntax_error("expected ')' (joint orientation)");
				}

				tp.skipLine();
			}

			tp.advanceToNextWhitespace();
			continue;
		}
		if (tp.compareNext("mesh")) {
			long int numVerts   = -1;
			long int numTris    = -1;
			long int numWeights = -1;

			meshes.push_back(Md5Mesh());
			Md5Mesh& mesh = meshes.back();

			if (tp.advanceToNext('{') != '{') {
				syntax_error("expected a '{'");
			}
			tp.advance(); // skip {

			while (!tp.isNull() && !(*tp == '}')) {

			 // shader
				tp.skipWhitespace();
				if (tp.compareNext("shader")) {
					tp.skipWhitespace();
					if (*tp != '"') {
						syntax_error("expected '\"' (mesh name)");
					}
					tp.advance(); // skip "
					const char* beg = tp;
					if (tp.advanceToNext('"') != '"') {
						syntax_error("expected '\"' (mesh name)");
					}
					mesh.m_name.set(beg, tp - beg);

					tp.skipLine();
					continue;
				}

			 // vertex count
				if (tp.compareNext("numverts")) {
					tp.skipWhitespace();
					if (!tp.readNextInt(numVerts)) {
						syntax_error("numverts");
					}
					continue;
				}
			 // triangle count
				if (tp.compareNext("numtris")) {
					tp.skipWhitespace();
					if (!tp.readNextInt(numTris)) {
						syntax_error("numtris");
					}
					continue;
				}
			 // weight count
				if (tp.compareNext("numweights")) {
					tp.skipWhitespace();
					if (!tp.readNextInt(numWeights)) {
						syntax_error("numweights");
					}
					continue;
				}

			 // vertex
				if (tp.compareNext("vert")) {
					mesh.m_verts.push_back(Md5Vert());
					Md5Vert& vert = mesh.m_verts.back();
					
					tp.skipWhitespace();
					if (!tp.readNextInt(vert.m_index)) {
						syntax_error("expected a number (vert index)");
					}

					tp.skipWhitespace();
					if (*tp != '(') {
						syntax_error("expected '(' (vert texcoord)");
					}
					tp.advance(); // skip (
					for (int i = 0; i < 2; ++i) {
						tp.skipWhitespace();
						double d;
						if (!tp.readNextDouble(d)) {
							syntax_error("expected a number (vert texcoord)");
						}
						vert.m_texcoord[i] = (float)d;
					}
					tp.skipWhitespace();
					if (*tp != ')') {
						syntax_error("expected ')' (vert texcoord)");
					}
					tp.advance(); // skip (

					tp.skipWhitespace();
					if (!tp.readNextInt(vert.m_weightStart)) {
						syntax_error("expected a number (vert start weight)");
					}
					tp.skipWhitespace();
					if (!tp.readNextInt(vert.m_weightCount)) {
						syntax_error("expected a number (vert weight count)");
					}

					tp.skipLine();
					continue;
				}

			 // triangle
				if (tp.compareNext("tri")) {
					mesh.m_tris.push_back(Md5Tri());
					Md5Tri& tri = mesh.m_tris.back();

					tp.skipWhitespace();
					if (!tp.readNextInt(tri.m_index)) {
						syntax_error("expected a number (tri index)");
					}
					for (int i = 0; i < 3; ++i) {
						tp.skipWhitespace();
						if (!tp.readNextInt(tri.m_verts[i])) {
							syntax_error("expected a number (vert index)");
						}
					}

					tp.skipLine();
					continue;
				}

			 // weight
				if (tp.compareNext("weight")) {
					mesh.m_weights.push_back(Md5Weight());
					Md5Weight& weight = mesh.m_weights.back();
					double d;

					tp.skipWhitespace();
					if (!tp.readNextInt(weight.m_index)) {
						syntax_error("expected a number (weight index)");
					}
					tp.skipWhitespace();
					if (!tp.readNextInt(weight.m_jointIndex)) {
						syntax_error("expected a number (joint index)");
					}
					tp.skipWhitespace();
					if (!tp.readNextDouble(d)) {
						syntax_error("expected a number (weight bias)");
					}
					weight.m_bias = (float)d;

					tp.skipWhitespace();
					if (*tp != '(') {
						syntax_error("expected '(' (weight position)");
					}
					tp.advance(); // skip (
					for (int i = 0; i < 3; ++i) {
						tp.skipWhitespace();
						if (!tp.readNextDouble(d)) {
							syntax_error("expected a number (weight position)");
						}
						weight.m_position[i] = (float)d;
					}
					tp.skipWhitespace();
					if (*tp != ')') {
						syntax_error("expected ')' (weight position)");
					}


					tp.skipLine();
					continue;
				}
			}

			if ((int)mesh.m_verts.size() != numVerts) {
				misc_error("%s - numVerts (%d) did not match the actual vertex count (%d)", (const char*)mesh.m_name, numVerts, mesh.m_verts.size());
			}
			if ((int)mesh.m_tris.size() != numTris) {
				misc_error("%s - numTris (%d) did not match the actual triangle count (%d)", (const char*)mesh.m_name, numTris, mesh.m_tris.size());
			}
			if ((int)mesh.m_weights.size() != numWeights) {
				misc_error("%s - numWeights (%d) did not match the actual weight count (%d)", (const char*)mesh.m_name, numWeights, mesh.m_weights.size());
			}

			tp.advanceToNextWhitespace();
			tp.skipWhitespace(); // handle whitespace at end of file
			continue;
		}	
	}

	if ((int)joints.size() != numJoints) {
		misc_error("numJoints (%d) did not match the actual joint count (%d)", numJoints, joints.size());
	}
	if ((int)meshes.size() != numMeshes) {
		misc_error("numMeshes (%d) did not match the actual mesh count (%d)", numMeshes, meshes.size());
	}

	#undef syntax_error
	#undef misc_error

	for (auto& mesh : meshes) {
		tmpMesh.beginSubmesh(0);

		uint32 vertOffset = tmpMesh.getVertexCount();
		tmpMesh.setVertexCount(vertOffset + (uint32)mesh.m_verts.size());

		for (auto& src : mesh.m_verts) {
			MeshBuilder::Vertex& dst = tmpMesh.getVertex(vertOffset + src.m_index);
			memset(&dst, 0, sizeof(MeshBuilder::Vertex));
			dst.m_texcoord = src.m_texcoord;

		 // construct a sorted list of weights
			std::vector<Md5Weight> weights;
			for (long int i = 0; i < src.m_weightCount; ++i) {
				weights.push_back(mesh.m_weights[src.m_weightStart + i]);
			}
			std::sort(weights.begin(), weights.end(), [](const Md5Weight& _a, const Md5Weight& _b) { return _b.m_bias < _a.m_bias; });
		 
		 // copy the first 4 weights/indices into the vertex and normalize
			for (int i = 0, n = APT_MIN(4, (int)weights.size()); i < n; ++i) {
				dst.m_boneIndices[i] = weights[i].m_jointIndex;
				dst.m_boneWeights[i] = weights[i].m_bias;
			}
			dst.m_boneWeights = normalize(dst.m_boneWeights);

		 // construct object space vertex position
			dst.m_position = vec3(0.0f);
			for (auto& weight : weights) {
				auto& joint = joints[weight.m_jointIndex];
				vec3 posJ = joint.m_orientation * weight.m_position;
				dst.m_position += (joint.m_position + posJ) * weight.m_bias;
			}
			dst.m_boneWeights = normalize(dst.m_boneWeights);
		}

		uint32 triOffset = tmpMesh.getTriangleCount();
		tmpMesh.setTriangleCount(tmpMesh.getTriangleCount() + (uint32)mesh.m_tris.size());
		for (auto& src : mesh.m_tris) {
			MeshBuilder::Triangle& dst = tmpMesh.getTriangle(triOffset + src.m_index);
			//for (int i = 0; i < 3; ++i) {
			//	dst[i] = src.m_verts[i] + vertOffset;
			//}
		 // \todo winding inverted?
			dst.a = src.m_verts[0] + vertOffset;
			dst.b = src.m_verts[2] + vertOffset;
			dst.c = src.m_verts[1] + vertOffset;
		}

		tmpMesh.endSubmesh();
	}

	tmpMesh.generateNormals();
	tmpMesh.generateTangents();
	tmpMesh.updateBounds();

 // \todo use _mesh desc as a conversion target
	if (ret) {
		MeshDesc retDesc(MeshDesc::Primitive_Triangles);
		retDesc.addVertexAttr(VertexAttr::Semantic_Positions,   3, DataType::Float32);
		retDesc.addVertexAttr(VertexAttr::Semantic_Normals,     3, DataType::Sint8N);
		retDesc.addVertexAttr(VertexAttr::Semantic_Tangents,    3, DataType::Sint8N);
		retDesc.addVertexAttr(VertexAttr::Semantic_Texcoords,   2, DataType::Uint16N);
		retDesc.addVertexAttr(VertexAttr::Semantic_BoneWeights, 4, DataType::Uint16N);
		retDesc.addVertexAttr(VertexAttr::Semantic_BoneIndices, 4, DataType::Uint8);
		MeshData retMesh(retDesc, tmpMesh);
		swap(mesh_, retMesh);
	}

MeshData_LoadMd5_end:

	return ret;
}
