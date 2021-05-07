#include "DrawMesh.h"

#include <frm/core/gl.h>
#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/GlContext.h>
#include <frm/core/Json.h>
#include <frm/core/Resource.h>
#include <frm/core/Serializer.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/Time.h>

static GLenum PrimitiveToGl(frm::Mesh::Primitive _prim)
{
	switch (_prim)
	{
		case frm::Mesh::Primitive_Triangles: return GL_TRIANGLES;
		case frm::Mesh::Primitive_Points:    return GL_POINTS;
		case frm::Mesh::Primitive_Lines:     return GL_LINES;
		default: FRM_ASSERT(false);          return GL_INVALID_VALUE;
	};
}

static frm::Mesh::Primitive GlToPrimitive(frm::Mesh::Primitive _prim)
{
	switch (_prim)
	{
		case GL_TRIANGLES:           return frm::Mesh::Primitive_Triangles;
		case GL_POINTS:              return frm::Mesh::Primitive_Points;
		case GL_LINES:               return frm::Mesh::Primitive_Lines;
		default: FRM_ASSERT(false);  return frm::Mesh::Primitive_Invalid;
	};
}


namespace frm {

/*******************************************************************************

                                   DrawMesh

*******************************************************************************/

// PUBLIC

DrawMesh* DrawMesh::CreateUnique(Mesh::Primitive _primitive, const VertexLayout& _vertexLayout)
{
	DrawMesh* ret = FRM_NEW(DrawMesh(GetUniqueId(), ""));
	ret->m_primitive = PrimitiveToGl(_primitive);
	
	VertexData vertexData;
	vertexData.layout = _vertexLayout;
	ret->m_vertexData.push_back(vertexData);
	
	LOD& lod0 = ret->m_lods.push_back();
	lod0.submeshes.push_back();

	Use(ret);
	return ret;
}

DrawMesh* DrawMesh::Create(Mesh& _mesh, const VertexLayout& _vertexLayout)
{
	DrawMesh* ret = FRM_NEW(DrawMesh(GetUniqueId(), _mesh.getPath()));
	FRM_VERIFY(ret->load(_mesh, _vertexLayout));
	Use(ret);
	return ret;
}

DrawMesh* DrawMesh::Create(const char* _path)
{
	Id id = GetHashId(_path);
	DrawMesh* ret = Find(id);
	if (!ret)
	{
		ret = FRM_NEW(DrawMesh(id, _path));
		ret->m_path = _path;
	}
	Use(ret);

	return ret;
}

void DrawMesh::Destroy(DrawMesh*& _inst_)
{
	FRM_DELETE(_inst_);
}

bool DrawMesh::reload()
{
	if (m_path.isEmpty())
	{
		// DrawMesh not from a file, do nothing.
		return true;
	}

	FRM_AUTOTIMER("DrawMesh::load(%s)", m_path.c_str());

	File cachedData;
	PathStr cachedPath = m_path.c_str();

	#if 1
		if (FileSystem::CompareExtension("drawmesh", m_path.c_str()))
		{
			// Path is a DrawMesh.
			if (!FileSystem::Read(cachedData, m_path.c_str()))
			{
				return false;
			}
		}
		else
		{
			// Path is not a DrawMesh, check the cache.
			cachedPath.setf("_cache/%s.drawmesh", FileSystem::GetFileName(m_path.c_str()).c_str());

			if (FileSystem::Exists(cachedPath.c_str()))
			{
				DateTime sourceDate = FileSystem::GetTimeModified(m_path.c_str());
				DateTime cachedDate = FileSystem::GetTimeModified(cachedPath.c_str());
				if (sourceDate <= cachedDate)
				{
					FRM_LOG("DrawMesh: Loading cached data '%s'", cachedPath.c_str());
					if (!FileSystem::Read(cachedData, cachedPath.c_str()))
					{
						FRM_LOG_ERR("DrawMesh: Error loading cached data '%s'", cachedPath.c_str());
						return false;
					}
				}
			}
		}
	#endif

	if (cachedData.getDataSize() > 0)
	{
		// If we loaded DrawMesh data (either directly or from the cache) we can serrialize here.
		Json json;
		FRM_VERIFY(Json::Read(json, cachedData));
		SerializerJson serializer(json, SerializerJson::Mode_Read);

		if (!serialize(serializer))
		{
			FRM_LOG_ERR("Error serializing '%s': %s", cachedPath.c_str(), serializer.getError());
			return false;
		}
		return true;
	}
	else
	{
		// Else load via Mesh.
		Mesh* data = Mesh::Create(m_path.c_str());
		if (!data)
		{
			return false;
		}
		
		data->finalize(); // Convert mesh data buffers to minimal precision format.
		FRM_VERIFY(load(*data, VertexLayout()));
		Mesh::Destroy(data);

		// Cache the result.
		Json json;
		SerializerJson serializer(json, SerializerJson::Mode_Write);
		FRM_VERIFY(serialize(serializer));

		cachedPath.setf("_cache/%s.drawmesh", FileSystem::GetFileName(m_path.c_str()).c_str());
		Json::Write(json, cachedData);
		FileSystem::Write(cachedData, cachedPath.c_str());
	}

	return true;
}

bool DrawMesh::serialize(Serializer& _serializer_)
{
	bool ret = true;

	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		unload();
	}

	Mesh::Primitive primitive = GlToPrimitive(m_primitive);
	ret &= SerializeEnum<Mesh::Primitive, Mesh::Primitive_Count>(_serializer_, primitive, Mesh::kPrimitiveStr, "m_primitive");
	m_primitive = PrimitiveToGl(primitive);

	DataType indexDataType = internal::GLenumToDataType(m_indexDataType);
	ret &= SerializeEnum(_serializer_, indexDataType, kDataTypeStr, "m_indexDataType");
	m_indexDataType = internal::DataTypeToGLenum(indexDataType);

	ret &= Serialize(_serializer_, m_vertexCount, "m_vertexCount");

	uint vertexDataCount = Mesh::Semantic_Count;
	if (_serializer_.beginArray(vertexDataCount, "m_vertexData"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_vertexData.resize(vertexDataCount);
		}

		for (VertexData& vertexData : m_vertexData)
		{
			if (_serializer_.beginObject())
			{
				if (_serializer_.beginObject("layout"))
				{
					ret &= Serialize(_serializer_, vertexData.layout.vertexSizeBytes, "vertexSizeBytes");
					ret &= Serialize(_serializer_, vertexData.layout.alignmentBytes, "alignmentBytes");

					uint vertexAttributeCount = vertexData.layout.attributes.size();
					if (_serializer_.beginArray(vertexAttributeCount, "attributes"))
					{
						if (_serializer_.getMode() == Serializer::Mode_Read)
						{
							vertexData.layout.attributes.resize(vertexAttributeCount);
						}

						for (VertexLayout::VertexAttribute& attribute : vertexData.layout.attributes)
						{
							if (_serializer_.beginObject())
							{
								ret &= SerializeEnum(_serializer_, attribute.semantic, Mesh::kVertexDataSemanticStr, "semantic");
								ret &= SerializeEnum(_serializer_, attribute.dataType, kDataTypeStr, "dataType");
								ret &= Serialize(_serializer_, attribute.dataCount, "dataCount");
								ret &= Serialize(_serializer_, attribute.offsetBytes, "offsetBytes");

								_serializer_.endObject();
							}
						}

						_serializer_.endArray();
					}
					else
					{
						ret = false;
					}

					_serializer_.endObject(); // layout
				}
								
				if (_serializer_.getMode() == Serializer::Mode_Read)
				{
					uint dataSizeBytes = 0;
					void* data = nullptr;
					ret &= _serializer_.binary(data, dataSizeBytes, "data");
					setVertexData(vertexData, data, GL_STATIC_DRAW);
					FRM_FREE(data);
				}
				else
				{
					uint dataSizeBytes = vertexData.layout.vertexSizeBytes * m_vertexCount;
					void* data = getVertexData(vertexData);
					FRM_ASSERT(data);
					ret &= _serializer_.binary(data, dataSizeBytes, "data", CompressionFlags_Size);
					FRM_FREE(data);
				}

				_serializer_.endObject(); // vertexData
			}
		}

		_serializer_.endArray(); // m_vertexData
	}
	else
	{
		ret = false;
	}

	uint lodCount = m_lods.size();
	if (_serializer_.beginArray(lodCount, "m_lods"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_lods.resize(lodCount);
		}

		for (LOD& lod : m_lods)
		{
			if (_serializer_.beginObject())
			{
				uint submeshCount = lod.submeshes.size();
				if (_serializer_.beginArray(submeshCount, "submeshes"))
				{					
					if (_serializer_.getMode() == Serializer::Mode_Read)
					{
						lod.submeshes.resize(submeshCount);
					}

					for (Submesh& submesh : lod.submeshes)
					{
						if (_serializer_.beginObject())
						{
							ret &= Serialize(_serializer_, submesh.indexOffset, "indexOffset");
							ret &= Serialize(_serializer_, submesh.indexCount, "indexCount");

							if (_serializer_.beginObject("boundingBox"))
							{
								ret &= Serialize(_serializer_, submesh.boundingBox.m_min, "min");
								ret &= Serialize(_serializer_, submesh.boundingBox.m_max, "max");

								_serializer_.endObject(); // boundingBox
							}
							else
							{
								ret = false;
							}

							if (_serializer_.beginObject("boundingSphere"))
							{
								ret &= Serialize(_serializer_, submesh.boundingSphere.m_origin, "origin");
								ret &= Serialize(_serializer_, submesh.boundingSphere.m_radius, "radius");

								_serializer_.endObject(); // boundingSphere
							}
							else
							{
								ret = false;
							}

							_serializer_.endObject(); // submesh
						}
						else
						{
							ret = false;
						}
					}

					_serializer_.endArray(); // submeshes
				}
				else
				{
					ret = false;
				}

				if (_serializer_.getMode() == Serializer::Mode_Read)
				{
					uint dataSizeBytes = 0;
					void* data = nullptr;
					ret &= _serializer_.binary(data, dataSizeBytes, "indexData");
					setIndexData(lod, data, lod.submeshes[0].indexCount, GL_STATIC_DRAW);
					FRM_FREE(data);
				}
				else
				{					
					uint dataSizeBytes = lod.submeshes[0].indexCount * DataTypeSizeBytes(internal::GLenumToDataType(m_indexDataType));
					void* data = getIndexData(lod);
					FRM_ASSERT(data);
					ret &= _serializer_.binary(data, dataSizeBytes, "indexData", CompressionFlags_Size);
					FRM_FREE(data);
				}

				_serializer_.endObject(); // lod
			}
			else
			{
				ret = false;
			}
		}

		_serializer_.endArray(); // m_lods
	}
	else
	{
		ret = false;
	}

	if ((_serializer_.getMode() == Serializer::Mode_Read || m_skeleton) && _serializer_.beginObject("m_skeleton"))
	{
		if (!m_skeleton)
		{
			m_skeleton = FRM_NEW(Skeleton);
		}

		ret &= m_skeleton->serialize(_serializer_);

		_serializer_.endObject();
	}

	return ret;
}

void DrawMesh::setVertexData(const void* _data, uint32 _vertexCount, GLenum _usage)
{
	FRM_ASSERT(m_vertexData.size() == 1); // \todo Take an index, modified the specified buffer?
	
	m_vertexCount = _vertexCount;
	setVertexData(m_vertexData[0], _data, _usage);
}

void DrawMesh::setIndexData(DataType _dataType, const void* _data, uint32 _indexCount, GLenum _usage)
{
	FRM_ASSERT(m_lods.size() == 1);

	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);

	LOD& lod = m_lods[0];
	lod.submeshes[0].indexCount = _indexCount;
	m_indexDataType = internal::DataTypeToGLenum(_dataType);
	setIndexData(lod, _data, _indexCount, _usage);
}

DrawMesh::BindHandleKey DrawMesh::makeBindHandleKey(const VertexSemantic _attributeList[], uint32 _attributeCount) const
{
	BindHandleKey ret = 0;

	for (uint32 i = 0; i < _attributeCount; ++i)
	{
		ret = BitfieldInsert(ret, (BindHandleKey)1, (int)_attributeList[i], 1);
	}

	return ret;
}

const mat4* DrawMesh::getBindPose() const
{
	return m_skeleton ? m_skeleton->getPose() : nullptr;
}

int DrawMesh::getBindPoseSize() const
{
	return m_skeleton ? m_skeleton->getBoneCount() : 0;
}

// PRIVATE

DrawMesh::DrawMesh(uint64 _id, const char* _name)
	: Resource<DrawMesh>(_id, _name)
{
}

DrawMesh::~DrawMesh()
{
	unload();
}

GLuint DrawMesh::findOrCreateBindHandle(int _lodIndex, uint16 _bindHandleKey)
{
	LOD& lod = m_lods[_lodIndex];

	GLuint& ret = lod.bindHandleMap[_bindHandleKey];
	if (!ret)
	{
		// Note that we don't use glScopedBufferBinding() here as it's complicated by the VAO - we need to unbind VAO before we restore the previous buffer bindings and the previous VAO in order.
		GLint prevVertexArrayObject; glAssert(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVertexArrayObject));
		GLint prevVertexBuffer;      glAssert(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVertexBuffer));
		GLint prevIndexBuffer;       glAssert(glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevIndexBuffer));

		glAssert(glGenVertexArrays(1, &ret));
		glAssert(glBindVertexArray(ret));

		if (lod.indexBuffer)
		{
			glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lod.indexBuffer));
		}

		for (const VertexData& vertexData : m_vertexData)
		{
			for (const VertexLayout::VertexAttribute& attribute : vertexData.layout.attributes)
			{
				if (BitfieldExtract(_bindHandleKey, attribute.semantic, 1) == 0)
				{
					continue;
				}

				// Bind locations are determined by the attribute semantic.
				const int bindLocation = (int)attribute.semantic;
				
				glAssert(glBindBuffer(GL_ARRAY_BUFFER, vertexData.buffer)); // Note that this doesn't modify VAO state, the call to glVertexAttribIPointer*() binds the buffer to the attribute.
				glAssert(glEnableVertexAttribArray(bindLocation));
				if (DataTypeIsInt(attribute.dataType) && !DataTypeIsNormalized(attribute.dataType))
				{
					// Non-normalized integer types bind as ints.
					glAssert(glVertexAttribIPointer(bindLocation, attribute.dataCount, internal::DataTypeToGLenum(attribute.dataType), vertexData.layout.vertexSizeBytes, (const GLvoid*)attribute.offsetBytes));
				}
				else
				{
					// All other types bind as floats.
					glAssert(glVertexAttribPointer(bindLocation, attribute.dataCount, internal::DataTypeToGLenum(attribute.dataType), DataTypeIsNormalized(attribute.dataType), vertexData.layout.vertexSizeBytes, (const GLvoid*)attribute.offsetBytes));
				}
			}
		}

		glAssert(glBindVertexArray(0)); // Prevent changing the current vertex array object state.
		glAssert(glBindBuffer(GL_ARRAY_BUFFER, prevVertexBuffer));
		glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevIndexBuffer));
		glAssert(glBindVertexArray(prevVertexArrayObject));
	}

	return ret;
}

bool DrawMesh::load(const Mesh& _src, const VertexLayout& _vertexLayout)
{
	unload();

	m_path = _src.m_path;
	m_primitive = PrimitiveToGl(_src.m_primitive);
	
	#if 0
	// Deinterleave vertex data.
	const uint32 vertexCount = (uint32)_DrawMeshData.getVertexCount();
	const uint32 vertexSizeBytes = desc.getVertexSize();
	const char* interleavedData = (const char*)_DrawMeshData.getVertexData();
	for (int attrIndex = 0; attrIndex < desc.getVertexAttrCount(); ++attrIndex)
	{
		const VertexAttr& attr = desc[attrIndex];
		VertexData vertexData;
		switch (attr.getSemantic())
		{
			case VertexAttr::Semantic_Padding:
			default:                               continue;
			case VertexAttr::Semantic_Positions:   vertexData.semantic = VertexSemantic::Positions;   break;
			case VertexAttr::Semantic_Normals:     vertexData.semantic = VertexSemantic::Normals;     break;
			case VertexAttr::Semantic_Tangents:    vertexData.semantic = VertexSemantic::Tangents;    break;
			case VertexAttr::Semantic_Texcoords:   vertexData.semantic = VertexSemantic::MaterialUVs; break;
			case VertexAttr::Semantic_Colors:      vertexData.semantic = VertexSemantic::Colors;      break;
			case VertexAttr::Semantic_BoneWeights: vertexData.semantic = VertexSemantic::BoneWeights; break;
			case VertexAttr::Semantic_BoneIndices: vertexData.semantic = VertexSemantic::BoneIndices; break;			
		};
		vertexData.count = attr.getCount();
		vertexData.type  = attr.getDataType();

		const uint32 attrSizeBytes = attr.getSize();
		char* deinterleavedData = (char*)FRM_MALLOC(attrSizeBytes * vertexCount);
		for (uint32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			memcpy(
				deinterleavedData + vertexIndex * attrSizeBytes,
				interleavedData   + vertexIndex * vertexSizeBytes + attr.getOffset(),
				attrSizeBytes
				);
		}

		glAssert(glGenBuffers(1, &vertexData.buffer));
		glScopedBufferBinding(GL_ARRAY_BUFFER);
		glAssert(glBindBuffer(GL_ARRAY_BUFFER, vertexData.buffer)); // \todo This shouldn't be required however glNamedBufferData() fails with GL_INVALID_OPERATION without it.
		glAssert(glNamedBufferData(vertexData.buffer, vertexCount * attrSizeBytes, deinterleavedData, GL_STATIC_DRAW));
		
		FRM_FREE(deinterleavedData);

		m_vertexData[vertexData.semantic] = vertexData;
	}

	// Init a single LOD.
	m_lods.push_back();
	LOD& lod = m_lods.back();
	glAssert(glGenBuffers(1, &lod.indexBuffer));
	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);
	glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lod.indexBuffer)); // \todo See above.
	glAssert(glNamedBufferData(lod.indexBuffer, _DrawMeshData.getIndexCount() * DataTypeSizeBytes(_DrawMeshData.getIndexDataType()), _DrawMeshData.getIndexData(), GL_STATIC_DRAW));
	lod.indexType = internal::DataTypeToGLenum(_DrawMeshData.getIndexDataType());
	lod.subDrawMeshes = _DrawMeshData.m_subDrawMeshes;

	if (_DrawMeshData.getSkeleton())
	{
		if (!m_skeleton)
		{
			m_skeleton = FRM_NEW(Skeleton);
		}

		*m_skeleton = *_DrawMeshData.getSkeleton();
	}

	#endif

	glScopedBufferBinding(GL_ARRAY_BUFFER);
	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);

	m_vertexCount = _src.m_vertexCount;
	for (const Mesh::VertexData& srcVertexData : _src.m_vertexData)
	{
		if (!srcVertexData.data)
		{
			continue;
		}

		const uint32 srcDataSizeBytes = srcVertexData.getDataSizeBytes() * _src.m_vertexCount;

		VertexData& dstVertexData = m_vertexData.push_back();
		VertexLayout::VertexAttribute& attribute = dstVertexData.layout.attributes.push_back(); // Mesh data is separate, layout per vertex data is a single attribute.
		attribute.semantic = srcVertexData.semantic;
		attribute.dataType = srcVertexData.dataType;
		attribute.dataCount = srcVertexData.dataCount;
		dstVertexData.layout.vertexSizeBytes = (int)DataTypeSizeBytes(srcVertexData.dataType) * srcVertexData.dataCount;
		glAssert(glGenBuffers(1, &dstVertexData.buffer));
		glAssert(glBindBuffer(GL_ARRAY_BUFFER, dstVertexData.buffer)); // \todo This shouldn't be required however glNamedBufferData() fails with GL_INVALID_OPERATION without it.
		glAssert(glNamedBufferData(dstVertexData.buffer, srcDataSizeBytes, srcVertexData.data, GL_STATIC_DRAW));
	}

	const uint32 indexSizeBytes = (uint32)DataTypeSizeBytes(_src.m_indexDataType);
	m_indexDataType = internal::DataTypeToGLenum(_src.m_indexDataType);
	for (const Mesh::LOD& srcLod : _src.m_lods)
	{
		LOD& dstLod = m_lods.push_back();
		dstLod.submeshes = srcLod.submeshes;
		glAssert(glGenBuffers(1, &dstLod.indexBuffer));
		glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dstLod.indexBuffer)); // \todo See above.
		glAssert(glNamedBufferData(dstLod.indexBuffer, srcLod.submeshes[0].indexCount * indexSizeBytes, srcLod.indexData, GL_STATIC_DRAW));
				
		// Convert submesh offsets to bytes.
		for (Submesh& submesh : dstLod.submeshes)
		{
			submesh.indexOffset *= indexSizeBytes;
		}
	}

	if (_src.m_skeleton)
	{
		if (!m_skeleton)
		{
			m_skeleton = FRM_NEW(Skeleton);
		}

		*m_skeleton = *_src.m_skeleton;
	}

	setState(State_Loaded);

	return true;
}

void DrawMesh::unload()
{
	while (!m_lods.empty())
	{
		LOD& lod = m_lods.back();
		if (lod.indexBuffer != 0)
		{
			glAssert(glDeleteBuffers(1, &lod.indexBuffer));
		}

		while (!lod.bindHandleMap.empty())
		{
			glAssert(glDeleteVertexArrays(1, &lod.bindHandleMap.back().second));
			lod.bindHandleMap.pop_back();
		}

		m_lods.pop_back();
	}

	while (!m_vertexData.empty())
	{
		glAssert(glDeleteBuffers(1, &m_vertexData.back().buffer));
		m_vertexData.pop_back();
	}

	FRM_DELETE(m_skeleton);
}

void DrawMesh::setVertexData(VertexData& _vertexData_, const void* _data, GLenum _usage)
{
	glScopedBufferBinding(GL_ARRAY_BUFFER);

	if (!_vertexData_.buffer)
	{
		glAssert(glGenBuffers(1, &_vertexData_.buffer));
	}

	glAssert(glBindBuffer(GL_ARRAY_BUFFER, _vertexData_.buffer)); // \todo This shouldn't be required however glNamedBufferData() fails with GL_INVALID_OPERATION without it.
	glAssert(glNamedBufferData(_vertexData_.buffer, _vertexData_.layout.vertexSizeBytes * m_vertexCount, _data, _usage));
}

void* DrawMesh::getVertexData(const VertexData& _vertexData) const
{
	if (!_vertexData.buffer)
	{
		return nullptr;
	}

	size_t sizeBytes = _vertexData.layout.vertexSizeBytes * m_vertexCount;
	void* ret = FRM_MALLOC(sizeBytes);

	glScopedBufferBinding(GL_ARRAY_BUFFER);
	glAssert(glBindBuffer(GL_ARRAY_BUFFER, _vertexData.buffer));
	glAssert(glGetNamedBufferSubData(_vertexData.buffer, 0, sizeBytes, ret));
	glAssert(glFinish());

	return ret;
}

void DrawMesh::setIndexData(LOD& _lod_, const void* _data, uint32 _indexCount, GLenum _usage)
{
	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);

	if (!_lod_.indexBuffer)
	{
		glAssert(glGenBuffers(1, &_lod_.indexBuffer));
	}
	_lod_.submeshes[0].indexCount = _indexCount;

	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);
	glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lod_.indexBuffer)); // \todo This shouldn't be required however glNamedBufferData() fails with GL_INVALID_OPERATION without it.
	glAssert(glNamedBufferData(_lod_.indexBuffer, _indexCount * DataTypeSizeBytes(internal::GLenumToDataType(m_indexDataType)), _data, _usage));
}

void* DrawMesh::getIndexData(const LOD& _lod) const
{
	if (!_lod.indexBuffer)
	{
		return nullptr;
	}
	
	size_t sizeBytes = _lod.submeshes[0].indexCount * DataTypeSizeBytes(internal::GLenumToDataType(m_indexDataType));	
	void* ret = FRM_MALLOC(sizeBytes);

	glScopedBufferBinding(GL_ELEMENT_ARRAY_BUFFER);
	glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lod.indexBuffer));
	glAssert(glGetNamedBufferSubData(_lod.indexBuffer, 0, sizeBytes, ret));
	glAssert(glFinish());

	return ret;
}


/*******************************************************************************

                            DrawMesh::VertexLayout

*******************************************************************************/

DrawMesh::VertexLayout::VertexLayout(std::initializer_list<VertexAttribute> _attributes)
{
	for (const VertexAttribute& attribute : _attributes)
	{
		addAttribute(attribute.semantic, attribute.dataType, attribute.dataCount);
	}
}

void DrawMesh::VertexLayout::addAttribute(VertexSemantic _semantic, DataType _dataType, int _dataCount)
{
	int offsetBytes = vertexSizeBytes;

	if (!attributes.empty())
	{
		// Roll back padding if present.
		if (attributes.back().semantic == Mesh::Semantic_Invalid)
		{
			vertexSizeBytes -= attributes.back().getSizeBytes();
			attributes.pop_back();
		}

		// Modify offset to add implicit padding.
		offsetBytes = vertexSizeBytes;
		if (offsetBytes % alignmentBytes != 0)
		{
			offsetBytes += alignmentBytes - (offsetBytes % alignmentBytes);
		}		
	}

	VertexAttribute& attribute = attributes.push_back();
	attribute.semantic         = _semantic;
	attribute.dataType         = _dataType;
	attribute.dataCount        = _dataCount;
	attribute.offsetBytes      = offsetBytes;

	// Update vertex size, add padding if required.
	vertexSizeBytes = attribute.getSizeBytes() + offsetBytes;
	if (vertexSizeBytes % alignmentBytes != 0)
	{
		VertexAttribute& padding = attributes.push_back();
		padding.semantic         = Mesh::Semantic_Invalid;
		padding.dataType         = DataType_Uint8;
		padding.dataCount        = alignmentBytes - (vertexSizeBytes % alignmentBytes);
		padding.offsetBytes      = vertexSizeBytes;

		vertexSizeBytes += padding.getSizeBytes();
		FRM_ASSERT(vertexSizeBytes % alignmentBytes == 0);
	}
}

} // namespace frm
