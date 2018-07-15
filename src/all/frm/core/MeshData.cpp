#include <frm/MeshData.h>

#include <apt/log.h>
#include <apt/hash.h>
#include <apt/memory.h>
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
	if (_vertexCount >= APT_DATA_TYPE_MAX(uint16)) {
		return DataType_Uint32;
	}
	return DataType_Uint16;
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
	DataType             _dataType,
	uint8                _count
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
		m_vertexDesc[m_vertexAttrCount].setDataType(DataType_Uint8);
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

	if        (FileSystem::CompareExtension("obj", _path)) {
		if (!ReadObj(*ret, f.getData(), f.getDataSize())) {
			goto MeshData_Create_error;
		}
	} else if (FileSystem::CompareExtension("md5mesh", _path)) {
		if (!ReadMd5(*ret, f.getData(), f.getDataSize())) {
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
	
	ret->m_vertexData = (char*)APT_MALLOC(_desc.getVertexSize() * _vertexCount);
	ret->m_submeshes[0].m_vertexCount = _vertexCount;
	if (_vertexData) {
		ret->setVertexData(_vertexData);
	}

	if (_indexCount > 0) {
		ret->m_indexDataType = GetIndexDataType(_vertexCount);
		ret->m_indexData = (char*)APT_MALLOC(DataTypeSizeBytes(ret->m_indexDataType) * _indexCount);
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

static void BuildPlane(MeshBuilder& mesh_, float _sizeX, float _sizeZ, int _segsX, int _segsZ)
{
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

			mesh_.addVertex(vert);
		}
	}
	int zoff = _segsZ + 1;
	for (int x = 0; x < _segsX; ++x) {
		for (int z = 0; z < _segsZ; ++z) {
			uint32 a, b, c;

			a = z + x * zoff;
			b = a + zoff + 1;
			c = a + zoff;
			mesh_.addTriangle(a, b, c);

			b = a + 1;
			c = a + zoff + 1;
			mesh_.addTriangle(a, b, c);
		}
	}
}

MeshData* MeshData::CreatePlane(
	const MeshDesc& _desc, 
	float           _sizeX, 
	float           _sizeZ, 
	int             _segsX, 
	int             _segsZ,
	const mat4&     _transform
	)
{
	MeshBuilder mesh;
	BuildPlane(mesh, _sizeX, _sizeZ, _segsX, _segsZ);
	
	mesh.transform(_transform);
	mesh.m_boundingBox.m_min = mesh.m_vertices.front().m_position;
	mesh.m_boundingBox.m_max = mesh.m_vertices.back().m_position;
	mesh.m_boundingSphere = Sphere(mesh.m_boundingBox);

	return Create(_desc, mesh);
}
MeshData* MeshData::CreateSphere(
	const MeshDesc& _desc, 
	float           _radius, 
	int             _segsLat, 
	int             _segsLong,
	const mat4&     _transform
	)
{
	MeshBuilder mesh;
	BuildPlane(mesh, kTwoPi, kPi, _segsLong, _segsLat);
	for (uint32 i = 0; i < mesh.getVertexCount(); ++i) {
		MeshBuilder::Vertex& v = mesh.getVertex(i);
		float x = sinf(v.m_position.x) * sinf(v.m_position.z + kHalfPi);
		float y = cosf(v.m_position.x) * sinf(v.m_position.z + kHalfPi);
		float z = v.m_position.z;
		v.m_normal = normalize(vec3(x, -z, y)); // swap yz to align the poles along y
		v.m_position = v.m_normal * _radius;
	}
	if (_desc.findVertexAttr(VertexAttr::Semantic_Tangents)) {
		mesh.generateTangents();
	}
	mesh.updateBounds();
	return Create(_desc, mesh);
}

void MeshData::Destroy(MeshData*& _meshData_)
{
	delete _meshData_;
	_meshData_ = nullptr;
}

void frm::swap(MeshData& _a, MeshData& _b)
{
	using std::swap;
	swap(_a.m_path,           _b.m_path);
	swap(_a.m_bindPose,       _b.m_bindPose);
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
			memcpy(dst, src, DataTypeSizeBytes(_srcType) * attr->getCount());
			src += DataTypeSizeBytes(_srcType) * _srcCount;
			dst += m_desc.getVertexSize();
		}

	} else {
	 // type mismatch, convert
		for (auto i = 0; i < getVertexCount(); ++i) {
			DataTypeConvert(_srcType, attr->getDataType(), src, dst, attr->getCount());
			src += DataTypeSizeBytes(_srcType) * _srcCount;
			dst += m_desc.getVertexSize();
		}

	}
}

void MeshData::setIndexData(const void* _src)
{
	APT_ASSERT(_src);
	APT_ASSERT(m_indexData);
	memcpy(m_indexData, _src, DataTypeSizeBytes(m_indexDataType) * getIndexCount());
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
			DataTypeConvert(_srcType,	m_indexDataType, src, dst);
			src += DataTypeSizeBytes(_srcType);
			dst += DataTypeSizeBytes(m_indexDataType);
		}

	}
}

void MeshData::beginSubmesh(uint _materialId)
{
	Submesh submesh;
	submesh.m_materialId = _materialId;
	if (!m_submeshes.empty()) {
		const Submesh& prevSubmesh = m_submeshes.back();
		submesh.m_indexOffset = prevSubmesh.m_indexOffset + prevSubmesh.m_indexCount * DataTypeSizeBytes(m_indexDataType);
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
	uint indexSize = DataTypeSizeBytes(m_indexDataType);
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
		return HashString<uint64>((const char*)m_path);
	} else {
		uint64 ret = m_desc.getHash();
		if (m_vertexData) {
			ret = Hash<uint64>(m_vertexData, m_desc.getVertexSize() * getVertexCount(), ret);
		}
		if (m_indexData) {
			ret = Hash<uint64>(m_indexData, DataTypeSizeBytes(m_indexDataType) * getIndexCount(), ret);
		}
		if (m_bindPose) {
			for (int i = 0; i < m_bindPose->getBoneCount(); ++i) {
				const Skeleton::Bone& bone = m_bindPose->getBone(i);
				ret = HashString<uint64>(m_bindPose->getBoneName(i), ret);
			}
		}
		return ret;
	}
}

void MeshData::setBindPose(const Skeleton& _skel)
{
	if (!m_bindPose) {
		m_bindPose = new Skeleton;
	}
	*m_bindPose = _skel;
}

// PRIVATE

MeshData::MeshData()
	: m_bindPose(nullptr)
	, m_vertexData(nullptr)
	, m_indexData(nullptr)
{
}

MeshData::MeshData(const MeshDesc& _desc)
	: m_desc(_desc)
	, m_bindPose(nullptr)
	, m_vertexData(nullptr)
	, m_indexData(nullptr)
{
	m_submeshes.push_back(Submesh());
}

MeshData::MeshData(const MeshDesc& _desc, const MeshBuilder& _meshBuilder)
	: m_desc(_desc)
	, m_bindPose(nullptr)
	, m_vertexData(nullptr)
	, m_indexData(nullptr)
{
	const VertexAttr* positionsAttr   = m_desc.findVertexAttr(VertexAttr::Semantic_Positions);
	const VertexAttr* texcoordsAttr   = m_desc.findVertexAttr(VertexAttr::Semantic_Texcoords);
	const VertexAttr* normalsAttr     = m_desc.findVertexAttr(VertexAttr::Semantic_Normals);
	const VertexAttr* tangentsAttr    = m_desc.findVertexAttr(VertexAttr::Semantic_Tangents);
	const VertexAttr* colorsAttr      = m_desc.findVertexAttr(VertexAttr::Semantic_Colors);
	const VertexAttr* boneWeightsAttr = m_desc.findVertexAttr(VertexAttr::Semantic_BoneWeights);
	const VertexAttr* boneIndicesAttr = m_desc.findVertexAttr(VertexAttr::Semantic_BoneIndices);
	m_vertexData = (char*)APT_MALLOC(m_desc.getVertexSize() * _meshBuilder.getVertexCount());
	for (uint32 i = 0, n = _meshBuilder.getVertexCount(); i < n; ++i) {
		char* dst = m_vertexData + i * m_desc.getVertexSize();
		const MeshBuilder::Vertex& src = _meshBuilder.getVertex(i);
		if (positionsAttr) {
			DataTypeConvert(DataType_Float32, positionsAttr->getDataType(), &src.m_position, dst + positionsAttr->getOffset(), APT_MIN(3, (int)positionsAttr->getCount()));
		}
		if (texcoordsAttr) {
			DataTypeConvert(DataType_Float32, texcoordsAttr->getDataType(), &src.m_texcoord, dst + texcoordsAttr->getOffset(), APT_MIN(2, (int)positionsAttr->getCount()));
		}
		if (normalsAttr) {
			DataTypeConvert(DataType_Float32, normalsAttr->getDataType(), &src.m_normal, dst + normalsAttr->getOffset(), APT_MIN(3, (int)normalsAttr->getCount()));
		}
		if (tangentsAttr) {
			DataTypeConvert(DataType_Float32, tangentsAttr->getDataType(), &src.m_tangent, dst + tangentsAttr->getOffset(), APT_MIN(4, (int)tangentsAttr->getCount()));
		}
		if (boneWeightsAttr) {
			DataTypeConvert(DataType_Float32, boneWeightsAttr->getDataType(), &src.m_boneWeights, dst + boneWeightsAttr->getOffset(), APT_MIN(4, (int)boneWeightsAttr->getCount()));
		}
		if (boneIndicesAttr) {
			DataTypeConvert(DataType_Uint32, boneIndicesAttr->getDataType(), &src.m_boneIndices, dst + boneIndicesAttr->getOffset(), APT_MIN(4, (int)boneIndicesAttr->getCount()));
		}
	}

	m_indexDataType = GetIndexDataType(_meshBuilder.getVertexCount());
	m_indexData = (char*)APT_MALLOC(_meshBuilder.getIndexCount() * DataTypeSizeBytes(m_indexDataType));
	DataTypeConvert(DataType_Uint32, m_indexDataType, _meshBuilder.m_triangles.data(), m_indexData, _meshBuilder.getIndexCount());

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
		m_submeshes.back().m_indexOffset  *= DataTypeSizeBytes(m_indexDataType);
	}
}

MeshData::~MeshData()
{
	if (m_bindPose) {
		delete m_bindPose;
	}
	APT_FREE(m_vertexData);
	APT_FREE(m_indexData);
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
			DataTypeConvert(
				posAttr->getDataType(),
				DataType_Float32,
				data + j * DataTypeSizeBytes(posAttr->getDataType()) * j,
				&v[j]
				);
		}
		_submesh.m_boundingBox.m_min = min(_submesh.m_boundingBox.m_min, v);
		_submesh.m_boundingBox.m_max = max(_submesh.m_boundingBox.m_max, v);
		data += m_desc.getVertexSize();
	}
	_submesh.m_boundingSphere = Sphere(_submesh.m_boundingBox);
}

/*******************************************************************************

                                 MeshBuilder

*******************************************************************************/

// PUBLIC

MeshBuilder::MeshBuilder()
	: m_boundingBox(vec3(FLT_MAX), vec3(-FLT_MAX))
	, m_boundingSphere(vec3(0.0f), FLT_MAX)
{
}

void MeshBuilder::transform(const mat4& _mat)
{
	mat3 nmat = transpose(inverse(mat3(_mat)));

	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_position = TransformPosition(_mat, vert->m_position);
		vert->m_normal   = normalize(nmat * vert->m_normal);

		vec3 tng = normalize(nmat * vert->m_tangent.xyz());
		vert->m_tangent = vec4(tng, vert->m_tangent.w);
	}
}

void MeshBuilder::transformTexcoords(const mat3& _mat)
{
	for (auto vert = m_vertices.begin(); vert != m_vertices.end(); ++vert) {
		vert->m_texcoord = TransformPosition(_mat, vert->m_texcoord);
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
{
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
{
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
{
	if (m_vertices.empty()) {
		return;
	}
	m_boundingBox.m_min = m_boundingBox.m_max = m_vertices[0].m_position;
	for (auto vert = m_vertices.begin() + 1; vert != m_vertices.end(); ++vert) {
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
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_position.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Texcoords:
					APT_ASSERT(srcAttr.getCount() <= 2);
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_texcoord.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Normals:
					APT_ASSERT(srcAttr.getCount() <= 3);
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_normal.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Tangents:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_tangent.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_Colors:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_color.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_BoneWeights:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataTypeConvert(srcAttr.getDataType(), DataType_Float32, src + srcAttr.getOffset(), &v.m_boneWeights.x, srcAttr.getCount());
					break;
				case VertexAttr::Semantic_BoneIndices:
					APT_ASSERT(srcAttr.getCount() <= 4);
					DataTypeConvert(srcAttr.getDataType(), DataType_Uint32, src + srcAttr.getOffset(), &v.m_boneIndices.x, srcAttr.getCount());
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
	uint32* tmp = APT_NEW_ARRAY(uint32, _count);
	DataTypeConvert(_type, DataType_Uint32, _data, tmp, _count);
	uint32 off = m_submeshes.back().m_vertexOffset;
	for (uint32 i = 0; i < _count; i += 3) {
		m_triangles.push_back(Triangle(tmp[i] + off, tmp[i + 1] + off, tmp[i + 2] + off));
	}
	APT_DELETE_ARRAY(tmp);
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
	MeshData::Submesh& submesh = m_submeshes.back();
	submesh.m_vertexCount = m_vertices.size() - submesh.m_vertexOffset;
	submesh.m_indexCount  = m_triangles.size() * 3 - submesh.m_indexOffset;
	if (submesh.m_vertexCount == 0) {
		return;
	}
	submesh.m_boundingBox.m_min = submesh.m_boundingBox.m_max = m_vertices[submesh.m_vertexOffset].m_position;
	for (uint i = submesh.m_vertexOffset + 1, n = submesh.m_vertexOffset + submesh.m_vertexCount; i < n; ++i) {
		submesh.m_boundingBox.m_min = min(submesh.m_boundingBox.m_min, m_vertices[i].m_position);
		submesh.m_boundingBox.m_max = max(submesh.m_boundingBox.m_max, m_vertices[i].m_position);
	}
	submesh.m_boundingSphere = Sphere(submesh.m_boundingBox);

	m_boundingBox.m_min = Min(m_boundingBox.m_min, submesh.m_boundingBox.m_min);
	m_boundingBox.m_max = Max(m_boundingBox.m_max, submesh.m_boundingBox.m_max);
	m_boundingSphere    = Sphere(m_boundingBox);
}
