#pragma once
#ifndef frm_Mesh_h
#define frm_Mesh_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/MeshData.h>
#include <frm/Resource.h>

#include <apt/String.h>

#include <vector>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Mesh
// Wraps a single vertex buffer + optional index buffer. Submeshes are offsets
// into the data, submesh 0 represents all submeshes.
////////////////////////////////////////////////////////////////////////////////
class Mesh: public Resource<Mesh>
{
public:
	static Mesh* Create(const char* _path);
	static Mesh* Create(const MeshData& _meshData);
	static Mesh* Create(const MeshDesc& _desc); // create a unique empty mesh
	static void  Destroy(Mesh*& _inst_);

	bool load()   { return reload(); }
	bool reload();

	void setVertexData(const void* _data, uint _vertexCount, GLenum _usage = GL_STREAM_DRAW);
	void setIndexData(DataType _dataType, const void* _data, uint _indexCount, GLenum _usage = GL_STREAM_DRAW);

	uint getVertexCount() const                        { return getSubmesh(0).m_vertexCount; }
	uint getIndexCount() const                         { return getSubmesh(0).m_indexCount;  }
	int  getSubmeshCount() const                       { return (int)m_submeshes.size();     }
	const MeshData::Submesh& getSubmesh(int _id) const { APT_ASSERT(_id < getSubmeshCount()); return m_submeshes[_id]; };

	GLuint getVertexArrayHandle() const                { return m_vertexArray;   }
	GLuint getVertexBufferHandle() const               { return m_vertexBuffer;  }
	GLuint getIndexBufferHandle() const                { return m_indexBuffer;   }
	GLenum getIndexDataType() const                    { return m_indexDataType; }
	GLenum getPrimitive() const                        { return m_primitive;     }

	const AlignedBox& getBoundingBox() const           { return m_submeshes[0].m_boundingBox;    }
	const Sphere&     getBoundingSphere() const        { return m_submeshes[0].m_boundingSphere; }

private:
	apt::String<32> m_path; // empty if not from a file

	MeshDesc m_desc;
	std::vector<MeshData::Submesh> m_submeshes;

	GLuint m_vertexArray;   // vertex array state (only bind this when drawing)
	GLuint m_vertexBuffer;
	GLuint m_indexBuffer;
	GLenum m_indexDataType;
	GLenum m_primitive;

	Mesh(uint64 _id, const char* _name);
	~Mesh();
	
	void unload();

	void load(const MeshData& _data);
	void load(const MeshDesc& _desc);

}; // class Mesh

} // namespace frm

#endif // frm_Mesh_h
