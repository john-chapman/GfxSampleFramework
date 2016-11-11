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
/// \class Mesh
/// Wraps a single vertex buffer and optional index buffer.
////////////////////////////////////////////////////////////////////////////////
class Mesh: public Resource<Mesh>
{
public:
	static const int kDefaultSubMeshId = -1;

	static Mesh* Create(const char* _path);
	static Mesh* Create(const MeshData& _meshData);
	static Mesh* Create(const MeshDesc& _desc); // create a unique empty mesh
	static void  Destroy(Mesh*& _inst_);

	bool load()   { return reload(); }
	bool reload();

	void setVertexData(const void* _data, uint _vertexCount, GLenum _usage = GL_STREAM_DRAW);
	void setIndexData(DataType _dataType, const void* _data, uint _indexCount, GLenum _usage = GL_STREAM_DRAW);

	uint getVertexCount() const                        { return getSubmesh(kDefaultSubMeshId).m_vertexCount; }
	uint getIndexCount() const                         { return getSubmesh(kDefaultSubMeshId).m_indexCount; }
	int  getSubmeshCount() const                       { return (int)m_submeshes.size(); }
	const MeshData::Submesh& getSubmesh(int _id) const { APT_ASSERT((_id + 1) < getSubmeshCount()); return m_submeshes[_id + 1]; };

	GLuint getVertexArrayHandle() const                { return m_vertexArray; }
	GLuint getVertexBufferHandle() const               { return m_vertexBuffer; }
	GLuint getIndexBufferHandle() const                { return m_indexBuffer; }
	GLenum getIndexDataType() const                    { return m_indexDataType; }
	GLenum getPrimitive() const                        { return m_primitive; }

private:
	apt::String<32> m_path; //< Empty if not from a file.

	MeshDesc m_desc;
	std::vector<MeshData::Submesh> m_submeshes;

	GLuint m_vertexArray;   //< Vertex array state (only bind this when drawing).
	GLuint m_vertexBuffer;
	GLuint m_indexBuffer;
	GLenum m_indexDataType;
	GLenum m_primitive;

	Mesh(uint64 _id, const char* _name);
	~Mesh();
	
	void unload();

	void load(const MeshData& _data);
	void load(const MeshDesc& _desc);

	static bool ReadObj(MeshDesc& _desc, const char* _data, uint _dataSize);

}; // class Mesh

} // namespace frm

#endif // frm_Mesh_h
