#pragma once

#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/Mesh.h>
#include <frm/core/Resource.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// DrawMesh
// Wraps vertex/index buffers for rendering.
//
// - Binding locations for vertex data are determined by the vertex layout. For
//   non-interleaved vertex data, this means that binding location order matches
//   the order of buffers in m_vertexData. Shader declarations should match this
//   ordering and should be complete (regardless of whether a vertex input is
//   used).
// - Submesh index offsets are converted to bytes.
////////////////////////////////////////////////////////////////////////////////
class DrawMesh: public Resource<DrawMesh>
{
public:

	using Submesh = Mesh::Submesh;
	using VertexSemantic = Mesh::VertexDataSemantic;
	using BindHandleKey = uint16;

	// Describes the layout of a vertex buffer. This is required to support interleaved vertex data (required by ImGui, Im3d).
	struct VertexLayout
	{
		struct VertexAttribute
		{
			VertexSemantic semantic     = Mesh::Semantic_Invalid;
			DataType       dataType     = DataType_Invalid;
			uint8          dataCount    = 0;
			uint8          offsetBytes  = 0;

			VertexAttribute() = default;

			VertexAttribute(VertexSemantic _semantic, DataType _dataType, int _dataCount)
				: semantic(_semantic)
				, dataType(_dataType)
				, dataCount(_dataCount)
			{
			}

			uint32 getSizeBytes() const
			{
				return (uint32)DataTypeSizeBytes(dataType) * dataCount;
			}
		};

		VertexLayout() = default;
		VertexLayout(std::initializer_list<VertexAttribute> _attributes);

		void addAttribute(VertexSemantic _semantic, DataType _dataType, int _dataCount);

		uint8 vertexSizeBytes = 0;
		uint8 alignmentBytes  = 4;
		eastl::vector<VertexAttribute> attributes;

	};
	
	static DrawMesh*   CreateUnique(Mesh::Primitive _primitive, const VertexLayout& _vertexLayout);
	static DrawMesh*   Create(Mesh& _mesh, const VertexLayout& _vertexLayout = VertexLayout());
	static DrawMesh*   Create(const char* _path);
	static void        Destroy(DrawMesh*& _inst_);

	bool               load()   { return reload(); }
	bool               reload();
	bool               serialize(Serializer& _serializer_);

	const char*        getPath() const { return m_path.c_str(); }

	void               setVertexData(const void* _data, uint32 _vertexCount, GLenum _usage = GL_STREAM_DRAW);
	void               setIndexData(DataType _dataType, const void* _data, uint32 _indexCount, GLenum _usage = GL_STREAM_DRAW);

	BindHandleKey      makeBindHandleKey(const VertexSemantic _attributeList[], uint32 _attributeCount) const;

	const int          getSubmeshCount() const                     { return (int)m_lods[0].submeshes.size(); }
	const int          getLODCount() const                         { return (int)m_lods.size(); }
	const AlignedBox&  getBoundingBox(int _submesh = 0) const      { return m_lods[0].submeshes[_submesh].boundingBox;    }
	const Sphere&      getBoundingSphere(int _submesh = 0) const   { return m_lods[0].submeshes[_submesh].boundingSphere; }
	const Skeleton*    getSkeleton() const                         { return m_skeleton; }
	void               setSkeleton(const Skeleton& _skeleton);
	const mat4*        getBindPose() const;
	int                getBindPoseSize() const;

private:

	struct LOD
	{
		using SubmeshList   = eastl::vector<Submesh>;
		using BindHandleMap = eastl::vector_map<BindHandleKey, GLuint>;

		SubmeshList    submeshes;
		BindHandleMap  bindHandleMap; // VAOs indexed by bitfields of semantic combos.
		GLuint         indexBuffer     = 0;
	};

	struct VertexData
	{
		GLuint         buffer   = 0;
		VertexLayout   layout;
	};

	using VertexDataList = eastl::vector<VertexData>;

	VertexDataList     m_vertexData;
	eastl::vector<LOD> m_lods;
	GLenum             m_primitive     = GL_TRIANGLES;
	uint32             m_vertexCount   = 0;
	GLenum             m_indexDataType = GL_NONE;
	Skeleton*          m_skeleton      = nullptr;
	frm::String<32>    m_path          = ""; // Empty if not from a file

	
	                   DrawMesh(uint64 _id, const char* _name);
	                   ~DrawMesh();

	// Given a bind handle key (see makeBindHandleKey), return or create a bind handle for the relevant LOD.
	GLuint             findOrCreateBindHandle(int _lodIndex, BindHandleKey _bindHandleKey);

	// Load from Mesh.
	bool               load(const Mesh& _src, const VertexLayout& _vertexLayout);

	// Release GPU resources, etc.
	void               unload();

	void               setVertexData(VertexData& _vertexData_, const void* _data, GLenum _usage);
	void*              getVertexData(const VertexData& _vertexData) const;
	void               setIndexData(LOD& _lod_, const void* _data, uint32 _indexCount, GLenum _usage);
	void*              getIndexData(const LOD& _lod) const;

	friend class GlContext;
};

} // namespace frm
