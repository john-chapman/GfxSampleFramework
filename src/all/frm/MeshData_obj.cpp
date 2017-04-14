#include <frm/MeshData.h>

#include <apt/log.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace frm;
using namespace apt;

bool MeshData::ReadObj(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	using std::istream;
	using std::map;
	using std::string;
	using std::vector;
	
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string err;
	
 // \todo use _mesh desc as a conversion target
	MeshDesc retDesc(MeshDesc::Primitive_Triangles);
	VertexAttr* positionAttr = retDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType::Float32, 3);
	VertexAttr* normalAttr   = retDesc.addVertexAttr(VertexAttr::Semantic_Normals,   DataType::Sint8N,  3);
	VertexAttr* tangentAttr  = retDesc.addVertexAttr(VertexAttr::Semantic_Tangents,  DataType::Sint8N,  3);
	VertexAttr* texcoordAttr = retDesc.addVertexAttr(VertexAttr::Semantic_Texcoords, DataType::Uint16N, 2);
	
	MeshBuilder tmpMesh; // append vertices/indices here

	struct mem_streambuf: std::streambuf {
		mem_streambuf(const char* _data, uint _dataSize) {
			char* d = const_cast<char*>(_data);
			setg(d, d, d + _dataSize);
		};
	};
	struct DummyMatReader: public tinyobj::MaterialReader {
		DummyMatReader() {}
		virtual ~DummyMatReader() {}	
		virtual bool operator()(const string& matId, vector<tinyobj::material_t>& materials, map<string, int>& matMap, string& err) {
			return true;
		}
	} matreader;

	mem_streambuf dbuf(_srcData, _srcDataSize);
	istream dstream(&dbuf);

	bool ret = tinyobj::LoadObj(shapes, materials, err, dstream, matreader);
	if (!ret) {
		goto MeshData_ReadObj_end;
	}

	bool hasNormals = true;
	bool hasTexcoords = true;
	uint32 voffset = 0;
	for (auto shape = shapes.begin(); shape != shapes.end(); ++shape) {
		tinyobj::mesh_t& m = shape->mesh;

		uint pcount = m.positions.size() / 3;
		uint tcount = m.texcoords.size() / 2;
		hasTexcoords &= tcount != 0;
		uint ncount = m.normals.size() / 3;
		hasNormals &= ncount != 0;
		/*if (!((pcount == ncount) && (tcount ? pcount == tcount : true) && (ncount ? pcount == ncount : true))) {
			ret = false;
			err = "Mesh data error (position/normal/texcoord counts didn't match)";
			APT_ASSERT(false);
			goto Mesh_LoadObj_end;
		}*/
		if (pcount > std::numeric_limits<uint32>::max()) {
			ret = false;
			err = "Too many vertices";
			goto MeshData_ReadObj_end;
		}

	 // vertex data
		for (auto i = 0; i < pcount; ++i) {
			MeshBuilder::Vertex vtx;
			
			vtx.m_position.x = m.positions[i * 3 + 0];
			vtx.m_position.y = m.positions[i * 3 + 1];
			vtx.m_position.z = m.positions[i * 3 + 2];

			if (ncount) {
				vtx.m_normal.x = m.normals[i * 3 + 0];
				vtx.m_normal.y = m.normals[i * 3 + 1];
				vtx.m_normal.z = m.normals[i * 3 + 2];
			}

			if (tcount) {
				vtx.m_texcoord.x = m.texcoords[i * 2 + 0];
				vtx.m_texcoord.y = m.texcoords[i * 2 + 1];
			}

			tmpMesh.addVertex(vtx);
		}

	 // submeshes - each unique material ID maps to a submesh, which is a range of indices
		vector<vector<uint32> > submeshIndices;
		vector<int> submeshMaterialMap; // map material IDs to Submesh indices
		for (auto face = 0; face < shape->mesh.material_ids.size(); ++face) {
			if (m.num_vertices[face] != 3) {
				ret = false;
				err = "Invalid face (only triangles supported";
				goto MeshData_ReadObj_end;
			}

		 // find the relevant Submesh list for the mat index, or push a new one
			int matIndex = -1;
			for (auto i = 0; i < submeshMaterialMap.size(); ++i) {
				if (submeshMaterialMap[i] == m.material_ids[face]) {
					matIndex = (int)i;
					break;
				}
			}
			if (matIndex == -1) {
				matIndex = (int)submeshMaterialMap.size();
				submeshMaterialMap.push_back(m.material_ids[face]);
				submeshIndices.push_back(vector<unsigned int>());
			}

		 // add face indices to the appropriate index list
			submeshIndices[matIndex].push_back(m.indices[face * 3 + 0] + voffset);
			submeshIndices[matIndex].push_back(m.indices[face * 3 + 1] + voffset);
			submeshIndices[matIndex].push_back(m.indices[face * 3 + 2] + voffset);
		}
	 
		for (auto submesh = 0; submesh < submeshIndices.size(); ++submesh) {
			for (auto i = 0; i < submeshIndices[submesh].size(); i += 3) {
				tmpMesh.addTriangle(
					submeshIndices[submesh][i + 0],
					submeshIndices[submesh][i + 1],
					submeshIndices[submesh][i + 2]
					);
			}
			
			/*retMesh.beginSubmesh(submesh); // \todo material id is implicit in this case?
			retMesh.m_submeshes[submesh].m_vertexOffset = 0;
			retMesh.m_submeshes[submesh].m_vertexCount = retMesh.getVertexCount();
			retMesh.m_submeshes[submesh].m_indexCount = submeshIndices[submesh].size();
			retMesh.m_submeshes[submesh].m_indexOffset = 
				submesh == 0 ? 0 : submeshIndices[submesh - 1].size();
			
			retMesh.endSubmesh();*/
		}

		voffset += (uint32)pcount;
	}

	
	if (normalAttr != 0 && !hasNormals) {
		tmpMesh.generateNormals();
	}
	if (tangentAttr != 0) {
		tmpMesh.generateTangents();
	}
	tmpMesh.updateBounds();

MeshData_ReadObj_end:
	if (!ret) {
		APT_LOG_ERR("obj error:\n\t'%s'", err.c_str());
		return false;
	}
	
	MeshData retMesh(retDesc, tmpMesh);
	swap(mesh_, retMesh);

	return true;
}
