#include <frm/Mesh.h>

#include <frm/gl.h>
#include <frm/GlContext.h>
#include <frm/Resource.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/memory.h>
#include <apt/File.h>
#include <apt/FileSystem.h>
#include <apt/Time.h>

#include <cstring> // memcpy

using namespace frm;
using namespace apt;

static GLenum PrimitiveToGl(MeshDesc::Primitive _prim)
{
	switch (_prim) {
		case MeshDesc::Primitive_Points:        return GL_POINTS;
		case MeshDesc::Primitive_Triangles:     return GL_TRIANGLES;
		case MeshDesc::Primitive_TriangleStrip: return GL_TRIANGLE_STRIP;
		case MeshDesc::Primitive_Lines:         return GL_LINES;
		case MeshDesc::Primitive_LineStrip:     return GL_LINE_STRIP;
		default: APT_ASSERT(false);             return GL_INVALID_VALUE;
	};
}

/*******************************************************************************

                                   Mesh

*******************************************************************************/

// PUBLIC

Mesh* Mesh::Create(const char* _path)
{
	Id id = GetHashId(_path);
	Mesh* ret = Find(id);
	if (!ret) {
		ret = APT_NEW(Mesh(id, _path));
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != State_Loaded) {
	 // \todo replace with default
	}
	return ret;
}

Mesh* Mesh::Create(const MeshData& _data)
{
	Id id = _data.getHash();
	Mesh* ret = Find(id);
	if (!ret) {
		ret = APT_NEW(Mesh(id, ""));
		ret->load(_data); // explicit load from data
	}
	Use(ret);
	return ret;
}

Mesh* Mesh::Create(const MeshDesc& _desc)
{
	Mesh* ret = APT_NEW(Mesh(GetUniqueId(), ""));
	ret->load(_desc); // explicit load from desc
	Use(ret);
	return ret;
}

void Mesh::Destroy(Mesh*& _inst_)
{
	APT_DELETE(_inst_);
}

bool Mesh::reload()
{
	if (m_path.isEmpty()) {
	 // mesh not from a file, do nothing
		return true;
	}

	APT_AUTOTIMER("Mesh::load(%s)", (const char*)m_path);

	MeshData* data = MeshData::Create((const char*)m_path);
	if (!data) {
		return false;
	}
	load(*data);
	MeshData::Destroy(data);
	return true;
}

void Mesh::setVertexData(const void* _data, uint _vertexCount, GLenum _usage)
{
	APT_ASSERT(m_vertexArray);

	GLint prevVao; glAssert(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao));
	GLint prevVbo; glAssert(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVbo));

	glAssert(glBindVertexArray(m_vertexArray));
	if (!m_vertexBuffer) {
		glAssert(glGenBuffers(1, &m_vertexBuffer));
		glAssert(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer));
		
	 // glVertexAttribPointer() calls are where the vao associates the vertex attributes with the currently bound vertex buffer, which is why we bind the vertex buffer first
		for (GLuint i = 0; i < m_desc.m_vertexAttrCount; ++i) {
			const VertexAttr& attr = m_desc.m_vertexDesc[i];
			glAssert(glEnableVertexAttribArray(i));
			if (DataTypeIsInt(attr.getDataType()) && !DataTypeIsNormalized(attr.getDataType())) {
			 // non-normalized integer types bind as ints
				glAssert(glVertexAttribIPointer(i, attr.getCount(), internal::DataTypeToGLenum(attr.getDataType()), m_desc.m_vertexSize, (const GLvoid*)attr.getOffset()));
			} else {
			 // else bind as floats
				glAssert(glVertexAttribPointer(i, attr.getCount(), internal::DataTypeToGLenum(attr.getDataType()), (GLboolean)DataTypeIsNormalized(attr.getDataType()), m_desc.m_vertexSize, (const GLvoid*)attr.getOffset()));
			}
		}
	} else {
		glAssert(glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer));
	}
	glAssert(glBufferData(GL_ARRAY_BUFFER, _vertexCount * m_desc.getVertexSize(), _data, _usage));
	m_submeshes[0].m_vertexCount = _vertexCount;

	glAssert(glBindVertexArray(0)); // prevent changing the vao state
	glAssert(glBindBuffer(GL_ARRAY_BUFFER, prevVbo));
	glAssert(glBindVertexArray(prevVao));
}

void Mesh::setIndexData(DataType _dataType, const void* _data, uint _indexCount, GLenum _usage)
{
	APT_ASSERT(m_vertexArray);

	GLint prevVao; glAssert(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao));
	GLint prevIbo; glAssert(glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevIbo));

	glAssert(glBindVertexArray(m_vertexArray));
	if (!m_indexBuffer) {
		glAssert(glGenBuffers(1, &m_indexBuffer));
	}
	glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer));
	glAssert(glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indexCount * DataTypeSizeBytes(_dataType), _data, _usage));
	m_submeshes[0].m_indexCount = _indexCount;
	m_indexDataType = internal::DataTypeToGLenum(_dataType);

	glAssert(glBindVertexArray(0)); // prevent changing the vao state
	glAssert(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevIbo));
	glAssert(glBindVertexArray(prevVao));
}

void Mesh::setBindPose(const Skeleton& _skel)
{
	if (!m_bindPose) {
		m_bindPose = APT_NEW(Skeleton);
	}
	*m_bindPose = _skel;
}

// PRIVATE

Mesh::Mesh(uint64 _id, const char* _name)
	: Resource(_id, _name)
	, m_desc(MeshDesc::Primitive_Count)
	, m_bindPose(nullptr)
	, m_vertexArray(0)
	, m_vertexBuffer(0)
	, m_indexBuffer(0)
	, m_indexDataType(GL_NONE)
	, m_primitive(GL_NONE)
{
	APT_ASSERT(GlContext::GetCurrent());
	m_submeshes.push_back(MeshData::Submesh());
}

Mesh::~Mesh()
{
	unload();
}

void Mesh::unload()
{
	if (m_vertexArray) {
		glAssert(glDeleteVertexArrays(1, &m_vertexArray));
		m_vertexArray = 0;
	}
	if (m_vertexBuffer) {
		glAssert(glDeleteBuffers(1, &m_vertexBuffer));
		m_vertexBuffer = 0;
	}
	if (m_indexBuffer) {
		glAssert(glDeleteBuffers(1, &m_indexBuffer));
		m_indexBuffer = 0;
	}
	if (m_bindPose) {
		APT_DELETE(m_bindPose);
		m_bindPose = nullptr;
	}
	m_submeshes.clear();
	setState(State_Unloaded);
}


void Mesh::load(const MeshData& _data)
{
	unload();
	load(_data.m_desc);
	m_path = _data.m_path;
	m_submeshes = _data.m_submeshes;

	if (_data.m_vertexData) {
		setVertexData(_data.m_vertexData, _data.getVertexCount(), GL_STATIC_DRAW);
	}
	if (_data.m_indexData) {
		setIndexData((DataType)_data.m_indexDataType, _data.m_indexData, _data.getIndexCount(), GL_STATIC_DRAW);
	}
	if (_data.m_bindPose) {
		m_bindPose = APT_NEW(Skeleton);
		*m_bindPose = *_data.m_bindPose;
	}

	setState(State_Loaded);
}

void Mesh::load(const MeshDesc& _desc)
{
	m_desc = _desc;
	m_primitive = PrimitiveToGl(_desc.getPrimitive());
	glAssert(glGenVertexArrays(1, &m_vertexArray));
	setState(State_Loaded);
}