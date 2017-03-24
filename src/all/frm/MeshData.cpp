#include <frm/MeshData.h>

#include <apt/log.h>
#include <apt/hash.h>
#include <apt/FileSystem.h>
#include <apt/TextParser.h>
#include <apt/Time.h>

#include <algorithm> // swap
#include <cstdarg>
#include <cstdlib>
#include <cstring>

using namespace frm;
using namespace apt;

static const uint8 kVertexAttrAlignment = 4;

static const char* VertexSemanticToStr(VertexAttr::Semantic _semantic)
{
	static const char* kSemanticStr[] =
	{
		"Position",
		"Texcoord",
		"Normal",
		"Tangent",
		"Color",
		"BoneWeights",
		"BoneIndices",
		"Padding",
	};
	APT_STATIC_ASSERT(sizeof(kSemanticStr) / sizeof(const char*) == VertexAttr::Semantic_Count);
	return kSemanticStr[_semantic];
};

static inline DataType GetIndexDataType(uint _vertexCount)
{
	if (_vertexCount >= UINT16_MAX) {
		return DataType::Uint32;
	}
	return DataType::Uint16;
}

/*******************************************************************************

                                   VertexAttr

*******************************************************************************/

bool VertexAttr::operator==(const VertexAttr& _rhs) const
{
	return m_semantic == _rhs.m_semantic 
		&& m_dataType == _rhs.m_dataType
		&& m_count    == _rhs.m_count 
		&& m_offset   == _rhs.m_offset
		; 
}

/*******************************************************************************

                                   MeshDesc

*******************************************************************************/

VertexAttr* MeshDesc::addVertexAttr(
	VertexAttr::Semantic _semantic, 
	uint8                _count, 
	DataType             _dataType
	)
{
	APT_ASSERT_MSG(findVertexAttr(_semantic) == 0, "MeshDesc: Semantic '%s' already exists", VertexSemanticToStr(_semantic));
	APT_ASSERT_MSG(m_vertexAttrCount < kMaxVertexAttrCount, "MeshDesc: Too many vertex attributes (added %d, max is %d)", m_vertexAttrCount + 1, kMaxVertexAttrCount);
	
 // roll back padding if present
	uint8 offset = m_vertexSize;
	if (m_vertexAttrCount > 0) {
		if (m_vertexDesc[m_vertexAttrCount - 1].getSemantic() == VertexAttr::Semantic_Padding) {
			--m_vertexAttrCount;
			m_vertexSize -= m_vertexDesc[m_vertexAttrCount].getSize();
		}
	 // modify offset to add implicit padding
		offset = m_vertexSize;
		if (offset % kVertexAttrAlignment != 0) {
			offset += kVertexAttrAlignment - (offset % kVertexAttrAlignment); 
		}
	}

	VertexAttr* ret = &m_vertexDesc[m_vertexAttrCount];

 // set the attr
	ret->setOffset(offset);
	ret->setSemantic(_semantic);
	ret->setCount(_count);
	ret->setDataType(_dataType);
	
 // update vertex size, add padding if required
	m_vertexSize = ret->getOffset() + ret->getSize();
	if (m_vertexSize % kVertexAttrAlignment != 0) {
		++m_vertexAttrCount;
		m_vertexDesc[m_vertexAttrCount].setOffset(m_vertexSize);
		m_vertexDesc[m_vertexAttrCount].setSemantic(VertexAttr::Semantic_Padding);
		m_vertexDesc[m_vertexAttrCount].setCount(kVertexAttrAlignment - (m_vertexSize % kVertexAttrAlignment));
		m_vertexDesc[m_vertexAttrCount].setDataType(DataType::Uint8);
		m_vertexSize += m_vertexDesc[m_vertexAttrCount].getSize();
	}
	++m_vertexAttrCount;

	return ret;
}

VertexAttr* MeshDesc::addVertexAttr(VertexAttr& _attr)
{
	APT_ASSERT_MSG(findVertexAttr(_attr.getSemantic()) == 0, "MeshDesc: Semantic '%s' already exists", VertexSemanticToStr(_attr.getSemantic()));
	APT_ASSERT_MSG(m_vertexAttrCount < kMaxVertexAttrCount, "MeshDesc: Too many vertex attributes (added %d, max is %d)", m_vertexAttrCount + 1, kMaxVertexAttrCount);
	VertexAttr* ret = &m_vertexDesc[m_vertexAttrCount++];
	*ret = _attr;
	m_vertexSize += _attr.getSize();
	return ret;
}

const VertexAttr* MeshDesc::findVertexAttr(VertexAttr::Semantic _semantic) const
{
	for (int i = 0; i < kMaxVertexAttrCount; ++i) {
		if (m_vertexDesc[i].getSemantic() == _semantic) {
			return &m_vertexDesc[i];
		}
	}
	return 0;
}

uint64 MeshDesc::getHash() const
{
	uint64 ret = Hash<uint64>(m_vertexDesc, sizeof(VertexAttr) * m_vertexAttrCount);
	ret = Hash<uint64>(&m_primitive, 1, ret);
	return ret;
}

bool MeshDesc::operator==(const MeshDesc& _rhs) const
{
	if (m_vertexAttrCount != _rhs.m_vertexAttrCount) {
		return false;
	}
	for (uint8 i = 0; i < m_vertexAttrCount; ++i) {
		if (m_vertexDesc[i] != _rhs.m_vertexDesc[i]) {
			return false;
		}
	}
	return m_vertexSize == _rhs.m_vertexSize && m_primitive == _rhs.m_primitive;
}

/*******************************************************************************

                                   MeshData

*******************************************************************************/

// PUBLIC

MeshData::Submesh::Submesh()
	: m_indexCount(0)
	, m_indexOffset(0)
	, m_vertexCount(0)
	, m_vertexOffset(0)
	, m_materialId(0)
{
}

MeshData* MeshData::Create(const char* _path)
{
	
	File f;
	if (!FileSystem::Read(f, _path)) {
		return nullptr;
	}
	MeshData* ret = new MeshData();
	ret->m_path.set(_path);

	const char* ext = FileSystem::GetExtension(_path);

	if (strcmp(ext, "obj") == 0) {
		if (!ReadObj(*ret, f.getData(), f.getDataSize())) {
			goto MeshData_Create_error;
		}
	} else if (strcmp(ext, "md5mesh") == 0) {
		if (!ReadMD5(*ret, f.getData(), f.getDataSize())) {
			goto MeshData_Create_error;
		}
	} else {
		APT_ASSERT(false); // unsupported format
		goto MeshData_Create_error;
	}
	
	return ret;

MeshData_Create_error:
	delete ret;
	return nullptr;
}

MeshData* MeshData::Create(
	const MeshDesc& _desc, 
	uint            _vertexCount, 
	uint            _indexCount, 
	const void*     _vertexData,
	const void*     _indexData
	)
{
	MeshData* ret = new MeshData(_desc);
	
	ret->m_vertexData = (char*)malloc(_desc.getVertexSize() * _vertexCount);
	ret->m_submeshes[0].m_vertexCount = _vertexCount;
	if (_vertexData) {
		ret->setVertexData(_vertexData);
	}

	if (_indexCount > 0) {
		ret->m_indexDataType = GetIndexDataType(_vertexCount);
		ret->m_indexData = (char*)malloc(DataType::GetSizeBytes(ret->m_indexDataType) * _indexCount);
		ret->m_submeshes[0].m_indexCount = _indexCount;
		if (_indexData) {
			ret->setIndexData(_indexData);
		}
	}

	return ret;
}

MeshData* MeshData::Create(
	const MeshDesc& _desc, 
	const MeshBuilder& _meshBuilder
	)
{
	MeshData* ret = new MeshData(_desc, _meshBuilder);
	return ret;
}

MeshData* MeshData::CreatePlane(
	const MeshDesc& _desc, 
	float           _sizeX, 
	float           _sizeZ, 
	int             _segsX, 
	int             _segsZ
	)
{
	APT_ASSERT(_segsX == _segsZ); // \todo broken if the tesselation factors differ in X and Z
	MeshBuilder mesh;

	for (int x = 0; x <= _segsX; ++x) {
		for (int z = 0; z <= _segsZ; ++z) {
			MeshBuilder::Vertex vert;
			vert.m_position = vec3(
				_sizeX * -0.5f + (_sizeX / (float)_segsX) * (float)x,
				0.0f,
				_sizeZ * -0.5f + (_sizeZ / (float)_segsZ) * (float)z
				);
			vert.m_texcoord = vec2(
				(float)x / (float)_segsX,
				1.0f - (float)z / (float)_segsZ
				);
			vert.m_normal = vec3(0.0f, 1.0f, 0.0f);
			vert.m_tangent = vec4(1.0f, 0.0f, 0.0f, 1.0f);

			mesh.addVertex(vert);
		}
	}
	
	uint32 j = 0;
	for (uint32 i = 0, n = _segsX * _segsZ * 2; i < n; ++i, ++j) {
		uint32 a, b, c;

		a = j + 1;
		b = j + _segsX + 1;
		c = j;
		mesh.addTriangle(a, b, c);

		a = j + _segsX + 2;
		b = j + _segsX + 1;
		c = j + 1;
		mesh.addTriangle(a, b, c);

		++i;
		if ((j + 2) % (_segsX + 1) == 0) {
			++j;
		}
	}

	mesh.m_boundingBox.m_min = mesh.m_vertices.front().m_position;
	mesh.m_boundingBox.m_max = mesh.m_vertices.back().m_position;
	mesh.m_boundingSphere = Sphere(mesh.m_boundingBox);

	return Create(_desc, mesh);
}

void MeshData::Destroy(MeshData*& _meshData_)
{
	APT_ASSERT(_meshData_);
	delete _meshData_;
	_meshData_ = nullptr;
}

void frm::swap(MeshData& _a, MeshData& _b)
{
	using std::swap;
	swap(_a.m_desc,           _b.m_desc);
	swap(_a.m_vertexData,     _b.m_vertexData);
	swap(_a.m_indexData,      _b.m_indexData);
	swap(_a.m_indexDataType,  _b.m_indexDataType);
	swap(_a.m_submeshes,      _b.m_submeshes);
}

void MeshData::setVertexData(const void* _src)
{
	APT_ASSERT(_src);
	APT_ASSERT(m_vertexData);
	memcpy(m_vertexData, _src, m_desc.getVertexSize() * getVertexCount());
}

void MeshData::setVertexData(VertexAttr::Semantic _semantic, DataType _srcType, uint _srcCount, const void* _src)
{
	APT_ASSERT(_src);
	APT_ASSERT(m_vertexData);
	APT_ASSERT(_srcCount <= 4);
	
	const VertexAttr* attr = m_desc.findVertexAttr(_semantic);
	APT_ASSERT(attr);
	APT_ASSERT(attr->getCount() == _srcCount); // \todo implement count conversion (trim or pad with 0s)

	const char* src = (const char*)_src;
	char* dst = (char*)m_vertexData;
	dst += attr->getOffset();
	if (_srcType == attr->getDataType()) {
	 // type match, copy directly
		for (auto i = 0; i < getVertexCount(); ++i) {
			memcpy(dst, src, DataType::GetSizeBytes(_srcType) * attr->getCount());
			src += DataType::GetSizeBytes(_srcType) * _srcCount;
			dst += m_desc.getVertexSize();
		}

	} else {
	 // type mismatch, convert
		for (auto i = 0; i < getVertexCount(); ++i) {
			DataType::Convert(_srcType, attr->getDataType(), src, dst, attr->getCount());
			src += DataType::GetSizeBytes(_srcType) * _srcCount;
			dst += m_desc.getVertexSize();
		}

	}
}

void MeshData::setIndexData(const void* _src)
{
	APT_ASSERT(_src);
	APT_ASSERT(m_indexData);
	memcpy(m_indexData, _src, DataType::GetSizeBytes(m_indexDataType) * getIndexCount());
}

void MeshData::setIndexData(DataType _srcType, const void* _src)
{
	APT_ASSERT(_src);
	APT_ASSERT(m_indexData);
	if (_srcType == m_indexDataType) {
		setIndexData(_src);

	} else {
		const char* src = (char*)_src;
		char* dst = (char*)m_indexData;
		for (auto i = 0; i < getIndexCount(); ++i) {
			DataType::Convert(_srcType,	m_indexDataType, src, dst);
			src += DataType::GetSizeBytes(_srcType);
			dst += DataType::GetSizeBytes(m_indexDataType);
		}

	}
}

void MeshData::beginSubmesh(uint _materialId)
{
	Submesh submesh;
	submesh.m_materialId = _materialId;
	if (!m_submeshes.empty()) {
		const Submesh& prevSubmesh = m_submeshes.back();
		submesh.m_indexOffset = prevSubmesh.m_indexOffset + prevSubmesh.m_indexCount * DataType::GetSizeBytes(m_indexDataType);
		submesh.m_vertexOffset = prevSubmesh.m_vertexOffset + prevSubmesh.m_vertexCount * m_desc.getVertexSize();
	}

	m_submeshes.push_back(submesh);
}

void MeshData::addSubmeshVertexData(const void* _src, uint _vertexCount)
{
	APT_ASSERT(!m_submeshes.empty());
	APT_ASSERT(_src && _vertexCount > 0);
	uint vertexSize = m_desc.getVertexSize();
	m_submeshes[0].m_vertexCount += _vertexCount;
	m_vertexData = (char*)realloc(m_vertexData, vertexSize * m_submeshes[0].m_vertexCount);
	memcpy(m_vertexData + m_submeshes.back().m_vertexOffset, _src, _vertexCount * vertexSize);
	m_submeshes.back().m_vertexCount += _vertexCount;
}

void MeshData::addSubmeshIndexData(const void* _src, uint _indexCount)
{
	APT_ASSERT(!m_submeshes.empty());
	APT_ASSERT(_src && _indexCount > 0);
	uint indexSize = DataType::GetSizeBytes(m_indexDataType);
	m_submeshes[0].m_indexCount += _indexCount;
	m_indexData = (char*)realloc(m_indexData, indexSize * m_submeshes[0].m_indexCount);
	memcpy(m_indexData + m_submeshes.back().m_indexOffset, _src, _indexCount * indexSize);
	m_submeshes.back().m_indexCount += _indexCount;
}

void MeshData::endSubmesh()
{
	updateSubmeshBounds(m_submeshes.back());
 // \todo need to grow the bounds for the whole mesh (submesh 0)
}

uint64 MeshData::getHash() const
{
	if (!m_path.isEmpty()) {
		return HashString<uint64>(m_path);
	} else {
		uint64 ret = m_desc.getHash();
		if (m_vertexData) {
			ret = Hash<uint64>(m_vertexData, m_desc.getVertexSize() * getVertexCount(), ret);
		}
		if (m_indexData) {
			ret = Hash<uint64>(m_indexData, DataType::GetSizeBytes(m_indexDataType) * getIndexCount(), ret);
		}
		return ret;
	}
}

// PRIVATE

MeshData::MeshData()
	: m_vertexData(0)
	, m_indexData(0)
{
}

MeshData::MeshData(const MeshDesc& _desc)
	: m_desc(_desc)
	, m_vertexData(0)
	, m_indexData(0)
{
	m_submeshes.push_back(Submesh());
}

MeshData::MeshData(const MeshDesc& _desc, const MeshBuilder& _meshBuilder)
	: m_desc(_desc)
	, m_vertexData(0)
	, m_indexData(0)
{
	const VertexAttr* positionsAttr   = m_desc.findVertexAttr(VertexAttr::Semantic_Positions);
	const VertexAttr* texcoordsAttr   = m_desc.findVertexAttr(VertexAttr::Semantic_Texcoords);
	const VertexAttr* normalsAttr     = m_desc.findVertexAttr(VertexAttr::Semantic_Normals);
	const VertexAttr* tangentsAttr    = m_desc.findVertexAttr(VertexAttr::Semantic_Tangents);
	const VertexAttr* colorsAttr      = m_desc.findVertexAttr(VertexAttr::Semantic_Colors);
	const VertexAttr* boneWeightsAttr = m_desc.findVertexAttr(VertexAttr::Semantic_BoneWeights);
	const VertexAttr* boneIndicesAttr = m_desc.findVertexAttr(VertexAttr::Semantic_BoneIndices);
	m_vertexData = (char*)malloc(m_desc.getVertexSize() * _meshBuilder.getVertexCount());
	for (uint32 i = 0, n = _meshBuilder.getVertexCount(); i < n; ++i) {
		char* dst = m_vertexData + i * m_desc.getVertexSize();
		const MeshBuilder::Vertex& src = _meshBuilder.getVertex(i);
		if (positionsAttr) {
			DataType::Convert(DataType::Float32, positionsAttr->getDataType(), &src.m_position, dst + positionsAttr->getOffset(), APT_MIN(3, (int)positionsAttr->getCount()));
		}
		if (texcoordsAttr) {
			DataType::Convert(DataType::Float32, texcoordsAttr->getDataType(), &src.m_texcoord, dst + texcoordsAttr->getOffset(), APT_MIN(2, (int)positionsAttr->getCount()));
		}
		if (normalsAttr) {
			DataType::Convert(DataType::Float32, normalsAttr->getDataType(), &src.m_normal, dst + normalsAttr->getOffset(), APT_MIN(3, (int)normalsAttr->getCount()));
		}
		if (tangentsAttr) {
			DataType::Convert(DataType::Float32, tangentsAttr->getDataType(), &src.m_tangent, dst + tangentsAttr->getOffset(), APT_MIN(3, (int)tangentsAttr->getCount()));
		}
		if (boneWeightsAttr) {
			DataType::Convert(DataType::Float32, boneWeightsAttr->getDataType(), &src.m_boneWeights, dst + boneWeightsAttr->getOffset(), APT_MIN(4, (int)boneWeightsAttr->getCount()));
		}
		if (boneIndicesAttr) {
			DataType::Convert(DataType::Float32, boneIndicesAttr->getDataType(), &src.m_boneIndices, dst + boneIndicesAttr->getOffset(), APT_MIN(4, (int)boneIndicesAttr->getCount()));
		}
	}

	m_indexDataType = DataType::Uint16;
	if (_meshBuilder.getVertexCount() > (uint32)std::numeric_limits<uint16>::max) {
		m_indexDataType = DataType::Uint32;
	}
	m_indexData = (char*)malloc(_meshBuilder.getIndexCount() * DataType::GetSizeBytes(m_indexDataType));
	DataType::Convert(DataType::Uint32, m_indexDataType, _meshBuilder.m_triangles.data(), m_indexData, _meshBuilder.getIndexCount());

 // submesh 0 represents the whole mesh
	m_submeshes.push_back(Submesh());
	m_submeshes.back().m_vertexCount    = _meshBuilder.getVertexCount();
	m_submeshes.back().m_indexCount     = _meshBuilder.getIndexCount();
	m_submeshes.back().m_boundingBox    = _meshBuilder.getBoundingBox();
	m_submeshes.back().m_boundingSphere = _meshBuilder.getBoundingSphere();

	for (auto& submesh : _meshBuilder.m_submeshes) {
		m_submeshes.push_back(submesh);

	 // convert MeshBuilder offsets to bytes
		m_submeshes.back().m_vertexOffset *= _desc.getVertexSize();
		m_submeshes.back().m_indexOffset  *= DataType::GetSizeBytes(m_indexDataType);
	}
}

MeshData::~MeshData()
{
	free(m_vertexData);
	free(m_indexData);
}

void MeshData::updateSubmeshBounds(Submesh& _submesh)
{
	const VertexAttr* posAttr = m_desc.findVertexAttr(VertexAttr::Semantic_Positions);
	APT_ASSERT(posAttr); // no positions
	
	const char* data = m_vertexData + posAttr->getOffset() + _submesh.m_vertexOffset;
	_submesh.m_boundingBox.m_min = vec3(FLT_MAX);
	_submesh.m_boundingBox.m_max = vec3(-FLT_MAX);
	for (auto i = 0; i < _submesh.m_vertexCount; ++i) {
		vec3 v;
		for (auto j = 0; j < APT_MIN(posAttr->getCount(), (uint8)3); ++j) {
			DataType::Convert(
				posAttr->getDataType(),
				DataType::Float32,
				data + j * DataType::GetSizeBytes(posAttr->getDataType()) * j,
				&v[j]
				);
		}
		_submesh.m_boundingBox.m_min = min(_submesh.m_boundingBox.m_min, v);
		_submesh.m_boundingBox.m_max = max(_submesh.m_boundingBox.m_max, v);
		data += m_desc.getVertexSize();
	}
	_submesh.m_boundingSphere = Sphere(_submesh.m_boundingBox);
}


#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

bool MeshData::ReadObj(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	using std::istream;
	using std::map;
	using std::string;
	using std::vector;

	APT_AUTOTIMER_DBG("MeshData::ReadObj");
	
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string err;
	
 // \todo use _mesh desc as a conversion target
	MeshDesc retDesc(MeshDesc::Primitive_Triangles);
	VertexAttr* positionAttr = retDesc.addVertexAttr(VertexAttr::Semantic_Positions, 3, DataType::Float32);
	VertexAttr* normalAttr   = retDesc.addVertexAttr(VertexAttr::Semantic_Normals,   3, DataType::Sint8N);
	VertexAttr* tangentAttr  = retDesc.addVertexAttr(VertexAttr::Semantic_Tangents,  3, DataType::Sint8N);
	VertexAttr* texcoordAttr = retDesc.addVertexAttr(VertexAttr::Semantic_Texcoords, 2, DataType::Uint16N);
	
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
		goto MeshData_LoadObj_end;
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
			goto MeshData_LoadObj_end;
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
				goto MeshData_LoadObj_end;
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

MeshData_LoadObj_end:
	if (!ret) {
		APT_LOG_ERR("obj error:\n\t'%s'", err.c_str());
		return false;
	}
	
	MeshData retMesh(retDesc, tmpMesh);
	swap(mesh_, retMesh);

	return true;
}


#include <algorithm> // std::sort
#include <vector>

bool MeshData::ReadMD5(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	APT_AUTOTIMER_DBG("MeshData::ReadMD5");

	TextParser tp(_srcData);
	
	MeshBuilder tmpMesh;

	long int numJoints = -1;
	long int numMeshes = -1;

	struct MD5Joint {
		String<32> m_name;
		long int   m_parentIndex;
		vec3       m_position;
		quat       m_orientation;
	};
	std::vector<MD5Joint> joints;

	struct MD5Vert {
		long int   m_index;
		vec2       m_texcoord;
		long int   m_weightStart;
		long int   m_weightCount;
	};
	struct MD5Tri {
		long int   m_index;
		long int   m_verts[3];
	};
	struct MD5Weight {
		long int   m_index;
		long int   m_jointIndex;
		float      m_bias;
		vec3       m_position;
	};
	struct MD5Mesh {
		String<32>             m_name;   // from the shader field
		std::vector<MD5Vert>   m_verts;
		std::vector<MD5Tri>    m_tris;
		std::vector<MD5Weight> m_weights;
	};
	std::vector<MD5Mesh> meshes;

	bool ret = true;
	#define syntax_error(_msg) \
		APT_LOG_ERR("MD5 syntax error, line %d: '%s'", tp.getLineCount(), _msg); \
		ret = false; \
		goto MeshData_LoadMD5_end

	#define misc_error(_fmt, ...) \
		APT_LOG_ERR("MD5 error: " _fmt, __VA_ARGS__); \
		ret = false; \
		goto MeshData_LoadMD5_end
	

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
				joints.push_back(MD5Joint());
				MD5Joint& joint = joints.back();

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

			meshes.push_back(MD5Mesh());
			MD5Mesh& mesh = meshes.back();

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
					mesh.m_verts.push_back(MD5Vert());
					MD5Vert& vert = mesh.m_verts.back();
					
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
					mesh.m_tris.push_back(MD5Tri());
					MD5Tri& tri = mesh.m_tris.back();

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
					mesh.m_weights.push_back(MD5Weight());
					MD5Weight& weight = mesh.m_weights.back();
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
			std::vector<MD5Weight> weights;
			for (long int i = 0; i < src.m_weightCount; ++i) {
				weights.push_back(mesh.m_weights[src.m_weightStart + i]);
			}
			std::sort(weights.begin(), weights.end(), [](const MD5Weight& _a, const MD5Weight& _b) { return _b.m_bias < _a.m_bias; });
		 
		 // copy the first 4 weights/indices into the vertex and normalize
			for (int i = 0, n = APT_MIN(4, (int)weights.size()); i < n; ++i) {
				dst.m_boneIndices[i] = weights[i].m_jointIndex;
				dst.m_boneWeights[i] = weights[i].m_bias;
			}
			dst.m_boneWeights = normalize(dst.m_boneWeights);

		 // construct object space vertex position
			dst.m_position = vec3(0.0f);
			for (auto& weight : weights) {
				const MD5Joint& joint = joints[weight.m_jointIndex];
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

MeshData_LoadMD5_end:
	if (ret) {
	 // \todo use _mesh desc as a conversion target
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

	return ret;
}

/*	Blender file format: http://www.atmind.nl/blender/mystery_ot_blend.html
	
	Blender files begin with a 12 byte header:
		- The chars 'BLENDER'
		- 1 byte pointer size flag; '_' means 32 bits, '-' means 64 bit.
		- 1 byte endianness flag; 'v' means little-endian, 'V' means big-endian.
		- 3 byte version string; '248' means 2.48

	There then follows some number of file blocks:
		- 4-byte aligned, 20 or 24 bytes size depending on the pointer size.
		- 4 byte block type code.
		- 4 byte block size (excluding header).
		- 4/8 byte address of the block when the file was written. Treat this as an ID.
		- 4 byte SDNA index.
		- 4 byte SDNA count.

	Block types:
		- ENDB marks the end of the blend file.
		- DNA1 contains the structure DNA data.

	SDNA:
		- Database of descriptors in a special file block (DNA1).
		- Hundreds of them, only interested in a few: http://www.atmind.nl/blender/blender-sdna.html.
*/
bool MeshData::ReadBlend(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{

	return true;
}


/*******************************************************************************

                                 MeshBuilder

*******************************************************************************/

// PUBLIC

MeshBuilder::MeshBuilder()
{
}

void MeshBuilder::transform(const mat4& _mat)
{
	mat3 nmat = transpose(inverse(mat3(_mat)));

	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_position = vec3(_mat * vec4(vert->m_position, 1.0f));
		vert->m_normal   = normalize(nmat * vert->m_normal);

		vec3 tng = normalize(nmat * vec3(vert->m_tangent));
		vert->m_tangent = vec4(tng, vert->m_tangent.w);
	}
}

void MeshBuilder::transformTexcoords(const mat3& _mat)
{
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_texcoord = vec2(_mat * vec3(vert->m_texcoord, 1.0f));
	}
}

void MeshBuilder::transformColors(const mat4& _mat)
{
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_color = _mat * vert->m_color;
	}
}

void MeshBuilder::normalizeBoneWeights()
{
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_boneWeights = normalize(vert->m_boneWeights);
	}
}

void MeshBuilder::generateNormals()
{	APT_AUTOTIMER_DBG("MeshBuilder::generateNormals");

 // zero normals for accumulation
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_normal = vec3(0.0f);
	}

 // generate normals
	for (auto tri = m_triangles.begin(); tri != m_triangles.end(); ++tri) {
		Vertex& va = getVertex(tri->a);
		Vertex& vb = getVertex(tri->b);
		Vertex& vc = getVertex(tri->c);

		vec3 ab = vb.m_position - va.m_position;
		vec3 ac = vc.m_position - va.m_position;
		vec3 n = cross(ab, ac);

		va.m_normal += n;
		vb.m_normal += n;
		vc.m_normal += n;
	}

 // normalize results
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_normal = normalize(vert->m_normal);
	}
}

void MeshBuilder::generateTangents()
{	APT_AUTOTIMER_DBG("MeshBuilder::generateTangents");

 // zero tangents for accumulation
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_tangent = vec4(0.0f);
	}

 // generate normals
	for (auto tri = m_triangles.begin(); tri != m_triangles.end(); ++tri) {
		Vertex& va = getVertex(tri->a);
		Vertex& vb = getVertex(tri->b);
		Vertex& vc = getVertex(tri->c);

		vec3 pab = vb.m_position - va.m_position;
		vec3 pac = vc.m_position - va.m_position;
		vec2 tab = vb.m_texcoord - va.m_texcoord;
		vec2 tac = vc.m_texcoord - va.m_texcoord;
		vec4 t(
			tac.y * pab.x - tab.y * pac.x,
			tac.y * pab.y - tab.y * pac.y,
			tac.y * pab.z - tab.y * pac.z,
			0.0f
			);
		t /= (tab.x * tac.y - tab.y * tac.x);

		va.m_tangent += t;
		vb.m_tangent += t;
		vc.m_tangent += t;
	}

 // normalize results
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_tangent = normalize(vert->m_tangent);
		vert->m_tangent.w = 1.0f;
	}
}

void MeshBuilder::updateBounds()
{	APT_AUTOTIMER_DBG("MeshBuilder::updateBounds");

	if (m_vertices.empty()) {
		return;
	}
	m_boundingBox.m_min = m_boundingBox.m_max = m_vertices[0].m_position;
	for (auto vert = ++m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		m_boundingBox.m_min = min(m_boundingBox.m_min, vert->m_position);
		m_boundingBox.m_max = max(m_boundingBox.m_max, vert->m_position);
	}
	m_boundingSphere = Sphere(m_boundingBox);
}

uint32 MeshBuilder::addTriangle(uint32 _a, uint32 _b, uint32 _c)
{
	return addTriangle(Triangle(_a, _b, _c));
}

uint32 MeshBuilder::addTriangle(const Triangle& _triangle)
{
	APT_ASSERT(_triangle.a < getVertexCount());
	APT_ASSERT(_triangle.b < getVertexCount());
	APT_ASSERT(_triangle.c < getVertexCount());
	uint32 ret = getTriangleCount();
	m_triangles.push_back(_triangle);
	return ret;
}

uint32 MeshBuilder::addVertex(const Vertex& _vertex)
{
	uint32 ret = getVertexCount();
	m_vertices.push_back(_vertex);
	return ret;
}

void MeshBuilder::addVertexData(const MeshDesc& _desc, const void* _data, uint32 _count)
{
	char* src = (char*)_data;
	for (uint32 i = 0; i < _count; ++i) {
		Vertex v;
		for (int j = 0; j < _desc.getVertexAttrCount(); ++j) {
			const VertexAttr& srcAttr = _desc[j];
			switch (srcAttr.getSemantic()) {
				case VertexAttr::Semantic_Positions: 
					APT_ASSERT(srcAttr.getCount() <= 3);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_position.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Texcoords:
					APT_ASSERT(srcAttr.getCount() <= 2);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_texcoord.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Normals:
					APT_ASSERT(srcAttr.getCount() <= 3);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_normal.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Tangents:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_tangent.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Colors:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_color.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_BoneWeights:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataType::Convert(srcAttr.getDataType(), DataType::Float32, src + srcAttr.getOffset(), &v.m_boneWeights.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_BoneIndices:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataType::Convert(srcAttr.getDataType(), DataType::Uint32, src + srcAttr.getOffset(), &v.m_boneIndices.x, srcAttr.getCount());
					break;
				default:
					break;
						
			};
		}
		m_vertices.push_back(v);
		src += _desc.getVertexSize();
	}
}
void MeshBuilder::addIndexData(DataType _type, const void* _data, uint32 _count)
{
 // \todo avoid conversion in case where _type == uint32
	uint32* tmp = new uint32[_count];
	DataType::Convert(_type, DataType::Uint32, _data, tmp, _count);
	for (uint32 i = 0; i < _count; i += 3) {
		m_triangles.push_back(Triangle(tmp[i], tmp[i + 1], tmp[i + 2]));
	}
	delete[] tmp;
}

void MeshBuilder::setVertexCount(uint32 _count)
{
	m_vertices.resize(_count);
}
void MeshBuilder::setTriangleCount(uint32 _count)
{
	m_triangles.resize(_count);
}

MeshData::Submesh& MeshBuilder::beginSubmesh(uint _materialId)
{
	MeshData::Submesh submesh;
	submesh.m_materialId = _materialId;
	if (!m_submeshes.empty()) {
		const MeshData::Submesh& prevSubmesh = m_submeshes.back();
		submesh.m_vertexOffset = prevSubmesh.m_vertexOffset + prevSubmesh.m_vertexCount;
		submesh.m_indexOffset  = prevSubmesh.m_indexOffset  + prevSubmesh.m_indexCount;
	}
	m_submeshes.push_back(submesh);
	return m_submeshes.back();
}
void MeshBuilder::endSubmesh()
{
	m_submeshes.back().m_vertexCount = m_vertices.size() - m_submeshes.back().m_vertexOffset;
	m_submeshes.back().m_indexCount  = m_triangles.size() * 3 - m_submeshes.back().m_indexOffset;
	MeshData::Submesh& submesh = m_submeshes.back();
	if (submesh.m_vertexCount == 0) {
		return;
	}
	submesh.m_boundingBox.m_min = submesh.m_boundingBox.m_max = m_vertices[submesh.m_vertexOffset].m_position;
	for (uint i = submesh.m_vertexOffset + 1, n = submesh.m_vertexOffset + submesh.m_vertexCount; i < n; ++i) {
		submesh.m_boundingBox.m_min = min(submesh.m_boundingBox.m_min, m_vertices[i].m_position);
		submesh.m_boundingBox.m_max = max(submesh.m_boundingBox.m_max, m_vertices[i].m_position);
	}
	submesh.m_boundingSphere = Sphere(submesh.m_boundingBox);
}
