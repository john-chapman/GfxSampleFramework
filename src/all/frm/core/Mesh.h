#pragma once

#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/MeshData.h>
#include <frm/core/Resource.h>
#include <frm/core/String.h>
#include <frm/core/SkeletonAnimation.h>

#include <EASTL/vector.h>

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

	const char* getPath() const { return m_path.c_str(); }

	void setVertexData(const void* _data, uint _vertexCount, GLenum _usage = GL_STREAM_DRAW);
	void setIndexData(frm::DataType _dataType, const void* _data, uint _indexCount, GLenum _usage = GL_STREAM_DRAW);

	uint getVertexCount() const                                   { return getSubmesh(0).m_vertexCount; }
	uint getIndexCount() const                                    { return getSubmesh(0).m_indexCount;  }
	int  getSubmeshCount() const                                  { return (int)m_submeshes.size();     }
	const MeshData::Submesh& getSubmesh(int _id) const            { FRM_ASSERT(_id < getSubmeshCount()); return m_submeshes[_id]; };

	GLuint getVertexArrayHandle() const                           { return m_vertexArray;   }
	GLuint getVertexBufferHandle() const                          { return m_vertexBuffer;  }
	GLuint getIndexBufferHandle() const                           { return m_indexBuffer;   }
	GLenum getIndexDataType() const                               { return m_indexDataType; }
	GLenum getPrimitive() const                                   { return m_primitive;     }

	const AlignedBox& getBoundingBox(int _submesh = 0) const      { return m_submeshes[_submesh].m_boundingBox;    }
	const Sphere&     getBoundingSphere(int _submesh = 0) const   { return m_submeshes[_submesh].m_boundingSphere; }

	const mat4*       getBindPose() const                         { return m_bindPose.data(); }
	uint              getBindPoseSize() const                     { return m_bindPose.size(); }
	void              setBindPose(const mat4* _pose, uint _size);

private:
	frm::String<32> m_path; // empty if not from a file

	MeshDesc m_desc;
	eastl::vector<MeshData::Submesh> m_submeshes;
	eastl::vector<mat4> m_bindPose;

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
