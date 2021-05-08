#include "Mesh.h"

#include <frm/core/log.h>
#include <frm/core/math.h>
#include <frm/core/memory.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Serializer.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/Time.h>

#include <frm/core/extern/meshoptimizer/meshoptimizer.h>

namespace frm {

// PUBLIC

const char* Mesh::kPrimitiveStr[Primitive_Count] =
{
	"Invalid",   // Primitive_Invalid  
	"Points",    // Primitive_Points   
	"Lines",     // Primitive_Lines    
	"Triangles", // Primitive_Triangles
};

const char* Mesh::kVertexDataSemanticStr[Semantic_Count]
{
	"Positions",      // Semantic_Positions
	"Normals",        // Semantic_Normals
	"Tangents",       // Semantic_Tangents
	"Colors",         // Semantic_Colors
	"BoneWeights",    // Semantic_BoneWeights
	"BoneIndices",    // Semantic_BoneIndices
	"MaterialUVs",    // Semantic_MaterialUVs
	"LightmapUVs",    // Semantic_LightmapUVs
	"User0",          // Semantic_User0
	"User1",          // Semantic_User1
	"User2",          // Semantic_User2
	"User3",          // Semantic_User3
};

Mesh* Mesh::CreatePlane(float _sizeX, float _sizeZ, int _segsX, int _segsZ, const mat4& _transform, CreateFlags _createFlags)
{
	Mesh* ret = FRM_NEW(Mesh());

	const uint32 vertexCount = (_segsX + 1) * (_segsZ + 1);
	ret->setVertexCount(vertexCount);
	
	VertexDataView<vec3> positions   = ret->getVertexDataView<vec3>(Semantic_Positions);
	VertexDataView<vec3> normals     = ret->getVertexDataView<vec3>(Semantic_Normals);
	VertexDataView<vec4> tangents    = ret->getVertexDataView<vec4>(Semantic_Tangents);
	VertexDataView<vec2> materialUVs = ret->getVertexDataView<vec2>(Semantic_MaterialUVs);
	for (int x = 0; x <= _segsX; ++x)
	{
		const float fx = (float)x;

		for (int z = 0; z <= _segsZ; ++z)
		{
			const float fz = (float)z;
			const int i = x * (_segsZ + 1) + z;

			positions[i]    = vec3(_sizeX * -0.5f + (_sizeX / _segsX) * fx, 0.0f, _sizeZ * -0.5f + (_sizeZ / _segsZ) * fz);
			normals[i]      = vec3(0.0f, 1.0f, 0.0f);
			tangents[i]     = vec4(1.0f, 0.0f, 0.0f, 1.0f);
			materialUVs[i]	= vec2(fx / _segsX, 1.0f - fz / _segsZ); 
		}
	}

	// Indices - given a quad ABCD
	//  A---B
	//  |   |
	//  C---D
	// There are two orientations for the central diagonal:
	//  ABD,ADC    BCA,BDC
	//  A---B      A---B
	//  | / |      | \ |
	//  C---D      C---D
	// We alternate between the orientation per quad in a row, and alternate the starting orientation per row to generate the following:
	//  +---+---+---+
	//  | / | \ | / |
	//  +---+---+---+
	//  | \ | / | \ |
	//  +---+---+---+
	const uint32 indexCount = (_segsX * _segsZ) * 2 * 3; // *2 triangles per quad *3 indices per triangle.
	ret->setIndexData(0, DataType_Uint32, indexCount);
	IndexDataView<uvec3> triangles = ret->getIndexDataView<uvec3>();
	int triangleIndex = 0;
	uint32 zoff = _segsZ + 1;
	for (int x = 0; x < _segsX; ++x)
	{
		for (int z = 0; z < _segsZ; ++z)
		{
			uint32 a, b, c, d;

			#if 1
				a = x * zoff + z;
				b = a + 1;
				c = (x + 1) * zoff + z;
				d = c + 1;
				if ((x & 1) == (z & 1)) //if (x % 2 == z % 2)
				{
					triangles[triangleIndex++] = uvec3(a, b, d);
					triangles[triangleIndex++] = uvec3(a, d, c);
				}
				else
				{
					triangles[triangleIndex++] = uvec3(b, c, a);
					triangles[triangleIndex++] = uvec3(b, d, c);
				}
			#else
				a = z + x * zoff;
				b = a + zoff + 1;
				c = a + zoff;
				triangles[triangleIndex++] = uvec3(a, b, c);
				b = a + 1;
				c = a + zoff + 1;
				triangles[triangleIndex++] = uvec3(a, b, c);
			#endif
		}
	}

	ret->transform(_transform);
	ret->computeBounds();

	return ret;
}

Mesh* Mesh::CreateDisc(float _radius, int _sides, const mat4& _transform, CreateFlags _createFlags)
{
	Mesh* ret = FRM_NEW(Mesh());

	_sides = Max(3, _sides);
	const int vertexCount = _sides + 1;
	ret->setVertexCount(vertexCount);
	
	VertexDataView<vec3> positions   = ret->getVertexDataView<vec3>(Semantic_Positions);
	VertexDataView<vec3> normals     = ret->getVertexDataView<vec3>(Semantic_Normals);
	VertexDataView<vec4> tangents    = ret->getVertexDataView<vec4>(Semantic_Tangents);
	VertexDataView<vec2> materialUVs = ret->getVertexDataView<vec2>(Semantic_MaterialUVs);

	positions[0]   = vec3(0.0f);
	normals[0]     = vec3(0.0f, 1.0f, 0.0f);
	tangents[0]    = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	materialUVs[0] = vec2(0.5f);
	for (int i = 1; i < vertexCount; ++i)
	{
		float theta = (float)i / (float)_sides * kTwoPi - kHalfPi; // - kHalfPi to correct for misalignment with cone/cylinder meshes
		float x = sinf(theta);
		float z = cosf(theta);

		positions[i]   = vec3(x, 0.0f, z) * _radius;
		normals[i]     = vec3(0.0f, 1.0f, 0.0f);
		tangents[i]    = vec4(1.0f, 0.0f, 0.0f, 1.0f);
		materialUVs[i] = vec2(x, z) * 0.5f + 0.5f;
	}

	const uint32 indexCount = _sides * 3;
	ret->setIndexData(0, DataType_Uint32, indexCount);
	IndexDataView<uvec3> triangles = ret->getIndexDataView<uvec3>();
	for (int i = 1; i <= _sides; ++i)
	{
		int j = i + 1;
		if (j > (vertexCount - 1))
		{
			j = 1;
		}
		triangles[i - 1] = uvec3(0, i, j);
	}

	ret->transform(_transform);
	ret->computeBounds();

	return ret;
}

Mesh* Mesh::CreateBox(float _sizeX, float _sizeY, float _sizeZ, int _segsX, int _segsY, int _segsZ, const mat4& _transform, CreateFlags _createFlags)
{
	Mesh* ret = FRM_NEW(Mesh());

	const vec3 size = vec3(_sizeX, _sizeY, _sizeZ);
	const vec3 halfSize = size / 2.0f;

	Mesh* faceXZ = CreatePlane(_sizeX, _sizeZ, _segsX, _segsZ);
	faceXZ->transform(TransformationMatrix(vec3(0.0f, halfSize.y, 0.0f), identity));
	ret->addSubmesh(0, *faceXZ);
	faceXZ->transform(RotationMatrix(vec3(1.0f, 0.0f, 0.0f), Radians(180.0f)));
	ret->addSubmesh(0, *faceXZ);
	Mesh::Destroy(faceXZ);

	Mesh* faceXY = CreatePlane(_sizeX, _sizeY, _segsX, _segsY);
	faceXY->transform(TransformationMatrix(vec3(0.0f, 0.0f, halfSize.z), RotationQuaternion(vec3(1.0f, 0.0f, 0.0f), Radians(90.0f))));
	ret->addSubmesh(0, *faceXY);
	faceXY->transform(RotationMatrix(vec3(1.0f, 0.0f, 0.0f), Radians(180.0f)));
	ret->addSubmesh(0, *faceXY);
	Mesh::Destroy(faceXY);
	
	Mesh* faceYZ = CreatePlane(_sizeY, _sizeZ, _segsY, _segsZ);
	faceYZ->transform(TransformationMatrix(vec3(halfSize.x, 0.0f, 0.0f), RotationQuaternion(vec3(0.0f, 0.0f, 1.0f), Radians(-90.0f))));
	ret->addSubmesh(0, *faceYZ);
	faceYZ->transform(RotationMatrix(vec3(0.0f, 0.0f, 1.0f), Radians(180.0f)));
	ret->addSubmesh(0, *faceYZ);
	Mesh::Destroy(faceYZ);

	ret->transform(_transform);
	ret->computeBounds();

	return ret;
}

Mesh* Mesh::CreateSphere(float _radius, int _segsLat, int _segsLong, const mat4& _transform, CreateFlags _createFlags)
{
	Mesh* ret = CreatePlane(kTwoPi, kPi, _segsLong, _segsLat);

	VertexDataView<vec3> positions = ret->getVertexDataView<vec3>(Semantic_Positions);
	VertexDataView<vec3> normals   = ret->getVertexDataView<vec3>(Semantic_Normals);
	VertexDataView<vec4> tangents  = ret->getVertexDataView<vec4>(Semantic_Tangents);
	for (uint32 i = 0; i < ret->m_vertexCount; ++i)
	{
		const vec3 position = positions[i];
		float x = sinf(position.x) * sinf(position.z + kHalfPi);
		float y = cosf(position.x) * sinf(position.z + kHalfPi);
		float z = position.z;
		normals[i] = Normalize(vec3(x, -z, y)); // swap yz to align the poles along y
		positions[i] = normals[i] * _radius;
	}
	ret->generateTangents(); // \todo Generate tangents directly in the loop above.

	ret->transform(_transform);
	ret->computeBounds();

	return ret;
}

Mesh* Mesh::CreateCylinder(float _radius, float _length, int _sides, int _segs, bool _capped, const mat4& _transform, CreateFlags _createFlags)
{
	return CreateCone(_length, _radius, _radius, _sides, _segs, _capped, _transform, _createFlags);
}

Mesh* Mesh::CreateCone(float _height, float _radiusTop, float _radiusBottom, int _sides, int _segs, bool _capped, const mat4& _transform, CreateFlags _createFlags)
{
	Mesh* ret = CreatePlane(kTwoPi, _height, _sides, _segs);
	
	_sides = Max(_sides, 3);

	VertexDataView<vec3> positions = ret->getVertexDataView<vec3>(Semantic_Positions);
	VertexDataView<vec3> normals   = ret->getVertexDataView<vec3>(Semantic_Normals);
	VertexDataView<vec4> tangents  = ret->getVertexDataView<vec4>(Semantic_Tangents);

	const float r  = Max(_radiusTop, _radiusBottom) - Min(_radiusTop, _radiusBottom);
	const float hr = _height / r;
	const float rh = (r / _height) * ((_radiusTop > _radiusBottom) ? -1.0f : 1.0f);
	for (uint32 i = 0; i < ret->m_vertexCount; ++i)
	{
		const vec3 position = positions[i];
		float x = sinf(position.x + kHalfPi);
		float y = cosf(position.x + kHalfPi);
		float z = position.z;
		float alpha = z / _height + 0.5f;
		float radius = _radiusBottom * alpha + _radiusTop * (1.0f - alpha);
		positions[i] = vec3(x * radius, -z, y * radius); // swap yz to align on y
		
		vec3 normal = Normalize(vec3(position.x, 0.0f, position.z));
		tangents[i] = vec4(normal.z, 0.0f, -normal.x, 1.0f);
		normals[i] = vec3(normal.x * hr, rh, normal.z * hr);		
	}

	if (_capped)
	{
		// Add a submesh for the existing data.
		Submesh& submesh = ret->m_lods[0].submeshes.push_back();
		submesh.indexCount = ret->m_lods[0].submeshes[0].indexCount;

		if (_radiusTop > 0.0f)
		{
			Mesh* capTop = CreateDisc(_radiusTop, _sides);
			capTop->transform(TranslationMatrix(vec3(0.0f, _height / 2.0f, 0.0f)));
			ret->addSubmesh(0, *capTop);
			Mesh::Destroy(capTop);
		}

		if (_radiusBottom > 0.0f)
		{
			Mesh* capBottom = CreateDisc(_radiusBottom, _sides);
			capBottom->transform(TranslationMatrix(vec3(0.0f, -_height / 2.0f, 0.0f)) * RotationMatrix(vec3(1.0f, 0.0f, 0.0f), kPi));
			ret->addSubmesh(0, *capBottom);
			Mesh::Destroy(capBottom);
		}
	}

	ret->transform(_transform);
	ret->computeBounds();

	return ret;
}

Mesh* Mesh::Create(const char* _path, CreateFlags _createFlags)
{	
	Mesh* ret = FRM_NEW(Mesh());
	ret->m_path = _path;
	if (!ret->load(_createFlags))
	{
		FRM_DELETE(ret);
		return nullptr;
	}

	return ret;
}

void Mesh::Destroy(Mesh*& _mesh_)
{
	FRM_DELETE(_mesh_);
}

void Mesh::setSkeleton(const Skeleton& _skeleton)
{
	if (!m_skeleton)
	{
		m_skeleton = FRM_NEW(Skeleton);
	}

	*m_skeleton = _skeleton;
}

void Mesh::transform(const mat4& _transform)
{
	// \todo Comparison operators for math types?
	const mat4 id = identity;
	if (memcmp(&_transform, &id, sizeof(mat4)) == 0)
	{
		return;
	}

	VertexDataView<vec3> positions = getVertexDataView<vec3>(Semantic_Positions);
	if (positions.getCount() > 0)
	{
		for (vec3& position : positions)
		{
			position = TransformPosition(_transform, position);
		}
	}

	const mat3 nmat = Transpose(Inverse(mat3(_transform)));

	VertexDataView<vec3> normals = getVertexDataView<vec3>(Semantic_Normals);
	if (normals.getCount() > 0)
	{
		for (vec3& normal : normals)
		{
			normal = Normalize(nmat * normal);
		}
	}

	VertexDataView<vec4> tangents = getVertexDataView<vec4>(Semantic_Tangents);
	if (tangents.getCount() > 0)
	{
		for (vec4& tangent : tangents)
		{
			tangent = vec4(Normalize(nmat * tangent.xyz()), tangent.w);
		}
	}
}

void* Mesh::getVertexData(VertexDataSemantic _semantic)
{
	return m_vertexData[_semantic].data;
}

void Mesh::setVertexData(VertexDataSemantic _semantic, DataType _dataType, uint32 _dataCount, const void* _data)
{
	FRM_ASSERT(m_vertexCount > 0); // Set vertex count before calling this function.

	VertexData& vertexData = m_vertexData[_semantic];
	if (!vertexData.data)
	{
		vertexData.semantic = _semantic;
		vertexData.dataType = _dataType;
		vertexData.dataCount = _dataCount;
		const size_t dataSizeBytes = vertexData.getDataSizeBytes() * m_vertexCount;
		vertexData.data = FRM_MALLOC_ALIGNED(dataSizeBytes, alignof(float));
	}

	FRM_ASSERT(vertexData.semantic == _semantic);
	FRM_ASSERT(_dataCount == vertexData.dataCount);

	if (_data)
	{
		DataTypeConvert(_dataType, vertexData.dataType, _data, vertexData.data, vertexData.dataCount * m_vertexCount);
	}
}

void Mesh::setVertexCount(uint32 _count)
{
	m_vertexCount = _count;
	for (VertexData& vertexData : m_vertexData)
	{
		const size_t dataSizeBytes = vertexData.getDataSizeBytes() * m_vertexCount;
		if (dataSizeBytes > 0)
		{
			vertexData.data = FRM_REALLOC_ALIGNED(vertexData.data, dataSizeBytes, alignof(float));
		}
	}
}

void* Mesh::getIndexData(uint32 _lod, uint32 _submesh)
{
	if (_lod >= m_lods.size())
	{
		return nullptr;
	}
	LOD& lod = m_lods[_lod];

	if (_submesh >= lod.submeshes.size())
	{
		return nullptr;
	}
	Submesh& submesh = lod.submeshes[_submesh];
	
	char* ret = (char*)lod.indexData;
	return ret + DataTypeSizeBytes(m_indexDataType) * submesh.indexOffset;
}

void Mesh::setIndexData(uint32 _lod, DataType _dataType, uint32 _indexCount, const void* _data)
{
	FRM_ASSERT(_lod <= m_lods.size()); // \todo Add LODs?
	LOD& lod = m_lods[_lod];

	if (lod.submeshes.empty())
	{
		lod.submeshes.push_back();
	}

	if (!lod.indexData || lod.submeshes[0].indexCount != _indexCount)
	{
		const size_t dataSizeBytes = DataTypeSizeBytes(_dataType) * _indexCount;
		lod.indexData = FRM_REALLOC_ALIGNED(lod.indexData, dataSizeBytes, alignof(uint32));
		lod.submeshes[0].indexCount = _indexCount;
	}

	if (_data)
	{
		DataTypeConvert(_dataType, m_indexDataType, _data, lod.indexData, _indexCount);
	}
}

void Mesh::generateNormals()
{	
	FRM_AUTOTIMER("Mesh::generateNormals");

	FRM_ASSERT(!m_lods.empty() && !m_lods[0].submeshes.empty());
	FRM_ASSERT(m_primitive == Primitive_Triangles);

	setVertexData(Semantic_Normals, DataType_Float32, 3);
	VertexDataView<vec3> normals = getVertexDataView<vec3>(Semantic_Normals);
	memset(normals.begin(), 0, sizeof(vec3) * normals.getCount()); // Zero for accumulation.

	VertexDataView<vec3> positions = getVertexDataView<vec3>(Semantic_Positions);
	FRM_ASSERT(positions.getCount() > 0);

	IndexDataView<uvec3> triangles = getIndexDataView<uvec3>(0);
	for (const uvec3& triangle : triangles)
	{
		const vec3 a = positions[triangle.x];
		const vec3 b = positions[triangle.y];
		const vec3 c = positions[triangle.z];

		const vec3 ab = b - a;
		const vec3 ac = c - a;
		const vec3 n  = cross(ab, ac);

		normals[triangle.x] += n;
		normals[triangle.y] += n;
		normals[triangle.z] += n;
	}

	for (vec3& n : normals)
	{
		n = Normalize(n);
	}
}

void Mesh::generateTangents()
{
	FRM_AUTOTIMER("Mesh::generateTangents");

	FRM_ASSERT(!m_lods.empty() && !m_lods[0].submeshes.empty());
	FRM_ASSERT(m_primitive == Primitive_Triangles);

	setVertexData(Semantic_Tangents, DataType_Float32, 4);
	VertexDataView<vec4> tangents = getVertexDataView<vec4>(Semantic_Tangents);
	memset(tangents.begin(), 0, sizeof(vec3) * tangents.getCount()); // Zero for accumulation.

	VertexDataView<vec3> positions = getVertexDataView<vec3>(Semantic_Positions);
	FRM_ASSERT(positions.getCount() > 0);
	VertexDataView<vec2> uvs = getVertexDataView<vec2>(Semantic_MaterialUVs);
	FRM_ASSERT(uvs.getCount() > 0);

	IndexDataView<uvec3> triangles = getIndexDataView<uvec3>(0);
	for (const uvec3& triangle : triangles)
	{
		const vec3 pa  = positions[triangle.x];
		const vec3 pb  = positions[triangle.y];
		const vec3 pc  = positions[triangle.z];
		const vec3 pab = pb - pa;
		const vec3 pac = pc - pa;

		const vec2 ta  = uvs[triangle.x];
		const vec2 tb  = uvs[triangle.y];
		const vec2 tc  = uvs[triangle.z];
		const vec2 tab = tb - ta;
		const vec2 tac = tc - ta;

		vec4 t(
			tac.y * pab.x - tab.y * pac.x,
			tac.y * pab.y - tab.y * pac.y,
			tac.y * pab.z - tab.y * pac.z,
			0.0f
			);
		t /= (tab.x * tac.y - tab.y * tac.x);

		tangents[triangle.x] += t;
		tangents[triangle.y] += t;
		tangents[triangle.z] += t;
	}

	for (vec4& t : tangents)
	{
		t = vec4(Normalize(t.xyz()), 1.0f);
	}
}

void Mesh::computeBounds()
{
	FRM_AUTOTIMER("Mesh::computeBounds");

	FRM_ASSERT(!m_lods.empty());

	VertexDataView<vec3> vertexData = getVertexDataView<vec3>(Semantic_Positions);
	FRM_ASSERT(vertexData.getCount() == m_vertexCount);

	// We compute submesh bounds for all LODs to account for the possibility that LODs may have different submeshes (e.g. a small submesh being removed).
	for (LOD& lod : m_lods)
	{
		for (Submesh& submesh : lod.submeshes)
		{
			vec3 boundsMin = vec3(FLT_MAX);
			vec3 boundsMax = vec3(-FLT_MAX);
			for (uint32 i = submesh.indexOffset; i < (submesh.indexOffset + submesh.indexCount); ++i)
			{
				const uint32 vertexIndex = ((uint32*)lod.indexData)[i];
				vec3 vertex = vertexData[vertexIndex];
				boundsMin = Min(boundsMin, vertex);
				boundsMax = Max(boundsMax, vertex);
			}

			submesh.boundingBox = AlignedBox(boundsMin, boundsMax);
			submesh.boundingSphere = Sphere(submesh.boundingBox);
		}
	}
}

void Mesh::optimize()
{
	// Note that it's only valid to move indices around *within* a submesh.
	#define OPTIMIZE_PER_SUBMESH 1

	FRM_AUTOTIMER("Mesh::optimize");

	FRM_ASSERT(!m_lods.empty());
	LOD& lod0 = m_lods[0];
	FRM_ASSERT(!lod0.submeshes.empty());
	Submesh& submesh0 = lod0.submeshes[0];

	FRM_ASSERT(m_indexDataType == DataType_Uint32); // \todo Support other index data types?

	const size_t submeshCount = lod0.submeshes.size();
	const size_t firstSubmeshIndex = (submeshCount > 1) ? 1 : 0;

	uint32*  newIndexData     = (uint32*)FRM_MALLOC_ALIGNED(submesh0.indexCount * sizeof(uint32), alignof(uint32));		
	uint32*& oldIndexData     = (uint32*&)lod0.indexData;
	uint32   indexCount       = submesh0.indexCount;
	float*   vertexPositions  = (float*)m_vertexData[Semantic_Positions].data;
	uint32   vertexCount      = m_vertexCount;

	{	FRM_AUTOTIMER("Vertex cache optimization");

		#if OPTIMIZE_PER_SUBMESH
		{
			for (size_t submeshIndex = firstSubmeshIndex; submeshIndex < submeshCount; ++submeshIndex)
			{
				const uint32 offset = lod0.submeshes[submeshIndex].indexOffset;
				const uint32 count  = lod0.submeshes[submeshIndex].indexCount;
				meshopt_optimizeVertexCache<uint32>(newIndexData + offset, oldIndexData + offset, count, m_vertexCount);
			}
		}
		#else
		{
			meshopt_optimizeVertexCache<uint32>(newIndexData, oldIndexData, indexCount, m_vertexCount);
		}
		#endif
		std::swap(oldIndexData, newIndexData);
	}

	{	FRM_AUTOTIMER("Overdraw optimization");

		#if OPTIMIZE_PER_SUBMESH
		{
			for (size_t submeshIndex = firstSubmeshIndex; submeshIndex < submeshCount; ++submeshIndex)
			{
				const uint32 offset = lod0.submeshes[submeshIndex].indexOffset;
				const uint32 count  = lod0.submeshes[submeshIndex].indexCount;
				meshopt_optimizeOverdraw<uint32>(newIndexData + offset, oldIndexData + offset, count, vertexPositions, vertexCount, sizeof(vec3), 1.05f);
			}
		}
		#else
		{
			meshopt_optimizeOverdraw<uint32>(newIndexData, oldIndexData, indexCount, vertexPositions, vertexCount, sizeof(vec3), 1.05f);
		}
		#endif
		std::swap(oldIndexData, newIndexData);
	}

	// \todo Fails?
	/*{	FRM_AUTOTIMER("Vertex fetch optimization");

		unsigned int* vertexRemapTable = (unsigned int*)FRM_MALLOC_ALIGNED(vertexCount * sizeof(unsigned int), alignof(unsigned int));
		#if OPTIMIZE_PER_SUBMESH
		{
			for (uint32 submeshIndex = firstSubmeshIndex; submeshIndex < submeshCount; ++submeshIndex)
			{
				const uint32 offset = lod0.submeshes[submeshIndex].indexOffset;
				const uint32 count  = lod0.submeshes[submeshIndex].indexCount;
				FRM_VERIFY(meshopt_optimizeVertexFetchRemap<uint32>(vertexRemapTable, oldIndexData + offset, count, vertexCount) == count);
			}
		}
		#else
		{
			FRM_VERIFY(meshopt_optimizeVertexFetchRemap<uint32>(vertexRemapTable, oldIndexData, indexCount, vertexCount) == vertexCount);
		}
		#endif

		for (VertexData vertexData : m_vertexData)
		{
			if (!vertexData.data)
			{
				continue;
			}

			const size_t vertexDataSizeBytes = vertexData.getDataSizeBytes();
			void* newVertexBuffer = FRM_MALLOC_ALIGNED(vertexDataSizeBytes * vertexCount, alignof(float));
			meshopt_remapVertexBuffer(newVertexBuffer, vertexData.data, vertexCount, vertexDataSizeBytes, vertexRemapTable);
			std::swap(vertexData.data, newVertexBuffer);
			FRM_FREE_ALIGNED(newVertexBuffer);
		}

		FRM_FREE_ALIGNED(vertexRemapTable);
	}*/

	FRM_FREE_ALIGNED(newIndexData);
}

void Mesh::generateLODs(int _lodCount, float _targetReduction, float _targetError)
{
	FRM_AUTOTIMER("Mesh::generateLODs");

	// Clear lods > 0.
	while (m_lods.size() != 1)
	{
		LOD& lod = m_lods.back();
		FRM_FREE_ALIGNED(lod.indexData);
		m_lods.pop_back();
	}

	_targetReduction = Max(_targetReduction, 0.01f);
		
	const size_t submeshCount = m_lods[0].submeshes.size();
	const size_t firstSubmeshIndex = (submeshCount > 1) ? 1 : 0;

	float* vertexPositions  = (float*)m_vertexData[Semantic_Positions].data;
	uint32 vertexCount      = m_vertexCount;

	// \todo Use max deviation to auto-compute LOD scale.
	const float errorScale  = meshopt_simplifyScale(vertexPositions, vertexCount, sizeof(vec3));
	float maxError          = 0.0f;

	for (int lodIndex = 1; lodIndex < _lodCount; ++lodIndex)
	{
		FRM_AUTOTIMER("LOD%d", lodIndex);

		LOD& lod = m_lods.push_back();
		const LOD& prevLOD = m_lods[lodIndex - 1];
		
		// \todo Remove submeshes based on some size heuristic.
		eastl::vector<eastl::vector<uint32> > indexDataPerSubmesh;
		uint32 indexCount = 0;
		for (size_t submeshIndex = firstSubmeshIndex; submeshIndex < submeshCount; ++submeshIndex)
		{
			const uint32 offset = prevLOD.submeshes[submeshIndex].indexOffset;
			const uint32 count  = prevLOD.submeshes[submeshIndex].indexCount;

			auto& newIndexData = indexDataPerSubmesh.push_back();
			newIndexData.resize(count);

			// Don't simplify if triangle count <= 32. \todo Better heuristic for this? Optimal threshold?
			if (count / 3 <= 32)
			{
				memcpy(newIndexData.data(), (uint32*)prevLOD.indexData + offset, count * sizeof(uint32));
			}
			else
			{
				// See here for an explanation of geometric deviation: https://documentation.simplygon.com/SimplygonSDK_8.3.31500.0/articles/simplygonapi/apiuserguide/deviationscreensize.html
				float resultError = 0.0f;
				newIndexData.resize(meshopt_simplify(newIndexData.data(), (uint32*)prevLOD.indexData + offset, count, vertexPositions, vertexCount, sizeof(vec3), (size_t)Ceil(_targetReduction * count), _targetError, &resultError));
				resultError *= errorScale;
				maxError = Max(maxError, resultError);
			}

			indexCount += (uint32)newIndexData.size();
		}

		// Stop if meshopt failed to simplify the mesh.
		if (indexCount == prevLOD.submeshes[0].indexCount)
		{
			m_lods.pop_back();
			break;
		}

		uint32* indexData = (uint32*)FRM_MALLOC_ALIGNED(sizeof(uint32) * indexCount, alignof(uint32));
		lod.indexData = indexData;
		lod.submeshes.resize(prevLOD.submeshes.size());
		lod.submeshes[0].indexOffset = 0;
		lod.submeshes[0].indexCount = indexCount;

		for (size_t submeshIndex = firstSubmeshIndex; submeshIndex < submeshCount; ++submeshIndex)
		{
			uint32 perSubmeshDataIndex = (uint32)(submeshIndex - firstSubmeshIndex);
			Submesh& submesh = lod.submeshes[submeshIndex];
			submesh.indexOffset = (submeshIndex > 1) ? (lod.submeshes[submeshIndex - 1].indexOffset + lod.submeshes[submeshIndex - 1].indexCount) : 0;
			submesh.indexCount  = (uint32)indexDataPerSubmesh[perSubmeshDataIndex].size();

			// \todo Copy these from the previous LOD's submesh when processing above.
			//submesh.boundingBox =
			//submesh.boundingSphere =

			memcpy((uint32*)lod.indexData + submesh.indexOffset, indexDataPerSubmesh[perSubmeshDataIndex].data(), submesh.indexCount * sizeof(uint32));
		}
	}
}

void Mesh::finalize()
{
	FRM_AUTOTIMER("Mesh::finalize");

	// \todo None of this can be automated if vertex data semantics are generalized. 

	auto ConvertBuffer = [](DataType* _srcType_, void*& _src_, DataType _newType, uint32 _count)
		{
			if (*_srcType_ == _newType)
			{
				return;
			}

			void* newBuffer = FRM_MALLOC_ALIGNED(DataTypeSizeBytes(_newType) * _count, 4);
			DataTypeConvert(*_srcType_, _newType, _src_, newBuffer, _count);
			FRM_FREE_ALIGNED(_src_);
			_src_ = newBuffer;
			*_srcType_ = _newType;
		};

	// \todo Positions: Float16 if error ratio with bounds is small. I.e. if the maximum error < 1% of the bounding volume size.
	//if (m_vertexData[Semantic_Positions].data)
	//{
	//	VertexData& positions = m_vertexData[Semantic_Positions];
	//	ConvertBuffer(positions.dataType, positions.data, DataType_Float16, m_vertexCount * positions.dataCount);
	//}
	
	// \todo MaterialUVs/LightMapUVs: Float16 isn't enough precision for textures > 2K. Uint16N would be enough but requires processing UV data to force a [0,1] range which should be valid for most cases.
	//if (m_vertexData[Semantic_MaterialUVs].data)
	//{
	//}

	// Colors: Uint8N for normalized values, else Float16.
	// \todo RGBM? Would need hints or separate semantics.
	if (m_vertexData[Semantic_Colors].data)
	{
		VertexData& colors = m_vertexData[Semantic_Colors];

		VertexDataView<float> colorView = getVertexDataView<float>(Semantic_Colors);
		float colorMin = FLT_MAX;
		float colorMax = -FLT_MAX;
		for (float color : colorView)
		{
			colorMin = Min(color, colorMin);
			colorMax = Max(color, colorMax);
		}

		if (colorMin < 0.0f || colorMax > 1.0f)
		{
			ConvertBuffer(&colors.dataType, colors.data, DataType_Float16, m_vertexCount * colors.dataCount);
		}
		else
		{
			ConvertBuffer(&colors.dataType, colors.data, DataType_Uint8N, m_vertexCount * colors.dataCount);
		}
	}

	// Normals/tangents: convert to sint16. 
	// \todo Test sint8?
	if (m_vertexData[Semantic_Normals].data)
	{
		VertexData& normals = m_vertexData[Semantic_Normals];
		ConvertBuffer(&normals.dataType, normals.data, DataType_Sint16N, m_vertexCount * normals.dataCount);
	}
	if (m_vertexData[Semantic_Tangents].data)
	{
		VertexData& tangents = m_vertexData[Semantic_Tangents];
		ConvertBuffer(&tangents.dataType, tangents.data, DataType_Sint16N, m_vertexCount * tangents.dataCount);
	}

	// Bone indices: convert to uint8/uint16 depending on skeleton bone count.
	if (m_vertexData[Semantic_BoneIndices].data && m_skeleton)
	{
		const size_t boneCount = m_skeleton->getBoneCount();
		VertexData& boneIndices = m_vertexData[Semantic_BoneIndices];

		if (boneCount < FRM_DATA_TYPE_MAX(uint32))
		{
			if (boneCount < FRM_DATA_TYPE_MAX(uint8))
			{
				ConvertBuffer(&boneIndices.dataType, boneIndices.data, DataType_Uint8, m_vertexCount * boneIndices.dataCount);
			}
			else
			{
				ConvertBuffer(&boneIndices.dataType, boneIndices.data, DataType_Uint16, m_vertexCount * boneIndices.dataCount);
			}
		}
	}

	// Index data: convert to 16 bit indices where appropriate.
	if (m_vertexCount < FRM_DATA_TYPE_MAX(uint16))
	{
		for (LOD& lod : m_lods)
		{
			DataType dataType = m_indexDataType; // Need to make a copy since ConvertBuffer overwrites _srcType with _newType and so subsequent LODs will fail to be converted.
			ConvertBuffer(&dataType, lod.indexData, DataType_Uint16, lod.submeshes[0].indexCount);
		}

		m_indexDataType = DataType_Uint16;
	}
}

// PROTECTED

Mesh::Mesh(Primitive _primitive)
	: m_primitive(_primitive)
{
	LOD& lod0 = m_lods.push_back();
	lod0.submeshes.push_back();
}

Mesh::Mesh(const char* _path, CreateFlags _createFlags)
	: m_path(_path)
{
	load(_createFlags);
}

Mesh::~Mesh()
{
	unload();
}

bool Mesh::load(CreateFlags _createFlags)
{
	if (m_path.isEmpty())
	{
		return true;
	}

	FRM_AUTOTIMER("Mesh::reload(%s)", m_path.c_str());

	File f;
	if (!FileSystem::Read(f, m_path.c_str()))
	{
		return nullptr;
	}

	if (FileSystem::CompareExtension("mesh", m_path.c_str()))
	{
		Json json;
		if (!Json::Read(json, m_path.c_str()))
		{
			return false;
		}

		SerializerJson serializer(json, SerializerJson::Mode_Read);
		return serialize(serializer);
	}
	else if (FileSystem::CompareExtension("gltf", m_path.c_str()))
	{
		if (!ReadGLTF(*this, f.getData(), f.getDataSize(), _createFlags))
		{
			return false;
		}
	}
	else
	{
		FRM_LOG_ERR("Mesh::load -- Unsupported format ('%s')", m_path.c_str());
		FRM_ASSERT(false); // unsupported format
		return false;
	}
	
	return true;
}

void Mesh::unload()
{	
	for (VertexData& vertexData : m_vertexData)
	{
		FRM_FREE_ALIGNED(vertexData.data);
		vertexData = VertexData();
	}

	while (!m_lods.empty())
	{
		LOD& lod = m_lods.back();
		FRM_FREE_ALIGNED(lod.indexData);
		m_lods.pop_back();
	}

	FRM_DELETE(m_skeleton);
}

bool Mesh::serialize(Serializer& _serializer_)
{
	FRM_ASSERT(false); // \todo
	return false;
}

void Mesh::addSubmesh(uint32 _lod, uint32 _indexOffset, uint32 _indexCount)
{
	FRM_ASSERT(_lod < m_lods.size()); // Set index data first.
	LOD& lod = m_lods[_lod];

	FRM_ASSERT(!lod.submeshes.empty()); // Init whole mesh first.
	Submesh& wholeMesh = lod.submeshes[0];
	FRM_ASSERT(_indexOffset < wholeMesh.indexCount);
	FRM_ASSERT(_indexCount < (wholeMesh.indexCount - _indexOffset));

	lod.submeshes.push_back();
	Submesh& submesh    = lod.submeshes.back();
	submesh.indexOffset = _indexOffset;
	submesh.indexCount  = _indexCount;
}

void Mesh::addSubmesh(uint32 _lod, Mesh& _mesh)
{
	FRM_ASSERT(_lod < m_lods.size());
	const LOD& srcLod = _mesh.m_lods[_lod];
	LOD& dstLod = m_lods[_lod];
	FRM_ASSERT(!dstLod.submeshes.empty());
	FRM_ASSERT(_mesh.m_indexDataType == DataType_Uint32);
	
	// Append vertex data.
	const uint32 vertexOffset = m_vertexCount;
	setVertexCount(m_vertexCount + _mesh.getVertexCount());
	for (int semantic = 0; semantic < Semantic_Count; ++semantic)
	{
		if (!_mesh.m_vertexData[semantic].data)
		{
			continue;
		}

		const VertexData& srcVertexData = _mesh.m_vertexData[semantic];
		const size_t dataSizeBytes = srcVertexData.getDataSizeBytes();
		VertexData& dstVertexData = m_vertexData[semantic];
		if (!dstVertexData.data)
		{
			dstVertexData = srcVertexData;
			dstVertexData.data = FRM_MALLOC_ALIGNED(dataSizeBytes * m_vertexCount, alignof(float));
		}
		FRM_ASSERT(dstVertexData.dataType  == srcVertexData.dataType);
		FRM_ASSERT(dstVertexData.dataCount == srcVertexData.dataCount);
		memcpy((char*)dstVertexData.data + vertexOffset * dataSizeBytes, srcVertexData.data, _mesh.getVertexCount() * dataSizeBytes);
	}

	// Append index data.		
	const Submesh& srcSubmesh = srcLod.submeshes[0];
	const uint32 dstSubmeshIndex = (uint32)dstLod.submeshes.size();
	Submesh& dstSubmesh = dstLod.submeshes.push_back();
	dstSubmesh.boundingBox = srcSubmesh.boundingBox;
	dstSubmesh.boundingSphere = srcSubmesh.boundingSphere;
	dstSubmesh.indexOffset = dstLod.submeshes[0].indexCount;
	dstSubmesh.indexCount = srcSubmesh.indexCount;
	dstLod.indexData = FRM_REALLOC_ALIGNED(dstLod.indexData, (dstSubmesh.indexOffset + dstSubmesh.indexCount) * sizeof(uint32), alignof(uint32));
	
	IndexDataView<uint32> srcIndexData = _mesh.getIndexDataView<uint32>(_lod);
	IndexDataView<uint32> dstIndexData = getIndexDataView<uint32>(_lod, dstSubmeshIndex);
	FRM_ASSERT(srcIndexData.getCount() == dstIndexData.getCount());
	for (uint32 i = 0; i < srcIndexData.getCount(); ++i)
	{
		dstIndexData[i] = srcIndexData[i] + vertexOffset;
	}

	dstLod.submeshes[0].indexCount += srcSubmesh.indexCount;
	// \todo Grow BB/BS?
}

//void Mesh::debugValidate()
//{
//	for (uint32 lodIndex = 0; lodIndex < m_lods.size(); ++lodIndex)
//	{
//		const LOD& lod = m_lods[lodIndex];
//
//		if (lod.submeshes.size() > 1)
//		{
//			uint32 totalIndices = 0;
//			for (uint32 submeshIndex = 1; submeshIndex < lod.submeshes.size(); ++submeshIndex)
//			{
//				const Submesh& submesh = lod.submeshes[submeshIndex];
//
//				FRM_ASSERT(submesh.indexOffset == totalIndices);
//				totalIndices += submesh.indexCount;
//
//				IndexDataView<uint32> indexData = getIndexDataView<uint32>(lodIndex, submeshIndex);
//				for (uint32 index : indexData)
//				{
//					FRM_ASSERT(index < m_vertexCount);
//				}
//			}
//			FRM_ASSERT(totalIndices = lod.submeshes[0].indexCount);
//		}
//
//		IndexDataView<uint32> indexData = getIndexDataView<uint32>(lodIndex);
//		for (uint32 index : indexData)
//		{
//			FRM_ASSERT(index < m_vertexCount);
//		}
//	}
//}

} // namespace frm