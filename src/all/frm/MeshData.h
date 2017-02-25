#pragma once
#ifndef frm_MeshData_h
#define frm_MeshData_h

#include <frm/def.h>
#include <frm/geom.h>

#include <apt/String.h>

#include <vector>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// VertexAttr
// \note m_offset is 8 bits, which limits the total vertex size to 256 bytes.
////////////////////////////////////////////////////////////////////////////////
class VertexAttr
{
public:
	enum Semantic 
	{
		Semantic_Positions,
		Semantic_Texcoords,
		Semantic_Normals,
		Semantic_Tangents,
		Semantic_Colors,
		Semantic_BoneWeights,
		Semantic_BoneIndices,
		Semantic_Padding,

		Semantic_Count
	};
	
	VertexAttr()
		: m_semantic(Semantic_Count)
		, m_dataType((uint8)DataType::kInvalidType)
		, m_count(0)
		, m_offset(0)
	{
	}

	VertexAttr(Semantic _semantic, DataType _dataType, uint8 _count)
		: m_semantic(_semantic)
		, m_dataType((uint8)_dataType)
		, m_count(_count)
		, m_offset(0)
	{
	}

	// Null semantics may be used to indicate the end of a vertex declaration.
	bool     isNull() const                   { return m_count == 0;         }
	Semantic getSemantic() const              { return (Semantic)m_semantic; }
	DataType getDataType() const              { return (DataType)m_dataType; }
	uint8    getCount() const                 { return m_count;              }
	uint8    getOffset() const                { return m_offset;             }
	uint8    getSize() const                  { return m_count * (uint8)DataType::GetSizeBytes(getDataType()); }

	void     setSemantic(Semantic _semantic)  { m_semantic   = _semantic;        }
	void     setDataType(DataType _dataType)  { m_dataType   = (uint8)_dataType; }
	void     setCount(uint8 _count)           { m_count      = _count;           }
	void     setOffset(uint8 _offset)         { m_offset     = _offset;          }

private:
	uint8 m_semantic;  // Data semantic.
	uint8 m_dataType;  // Data type per component.
	uint8 m_count;     // Number of components (1,2,3 or 4).
	uint8 m_offset;    // Byte offset of the first component.

}; // class VertexAttr


////////////////////////////////////////////////////////////////////////////////
// MeshDesc
////////////////////////////////////////////////////////////////////////////////
class MeshDesc
{
	friend class Mesh;
public:
	enum Primitive
	{
		Primitive_Points,
		Primitive_Triangles,
		Primitive_TriangleStrip,
		Primitive_Lines,
		Primitive_LineStrip,

		Primitive_Count
	};

	MeshDesc(Primitive _prim = Primitive_Triangles)
		: m_vertexAttrCount(0)
		, m_vertexSize(0)
		, m_primitive(_prim)
	{
	}

	// Append a new VertexComponent to the vertex desc. The order of calls to
	// addVertexComponent() must correspond to the order of the vertex components
	// in the vertex data. Ensures 4 byte alignment.
	VertexAttr* addVertexAttr(
		VertexAttr::Semantic _semantic, 
		uint8                _count, 
		DataType             _dataType
		);

	// Return VertexAttr matching _semantic, or nullptr if not present.
	const VertexAttr* findVertexAttr(VertexAttr::Semantic _semantic) const;

	uint64    getHash() const;
	Primitive getPrimitive() const               { return (Primitive)m_primitive; }
	void      setPrimitive(Primitive _primitive) { m_primitive = (uint8)_primitive; }
	uint8     getVertexSize() const              { return m_vertexSize; }

private:
	static const int  kMaxVertexAttrCount = VertexAttr::Semantic_Count + 1;
	VertexAttr        m_vertexDesc[kMaxVertexAttrCount];
	uint8             m_vertexAttrCount;
	uint8             m_vertexSize;
	uint8             m_primitive;

}; // class MeshDesc


////////////////////////////////////////////////////////////////////////////////
// MeshData
// Cpu-side mesh data.
// \note The first submesh always represents the entire mesh data. Additional
//   submeshes are optional.
// \todo Submesh API.
////////////////////////////////////////////////////////////////////////////////
class MeshData: private apt::non_copyable<MeshData>
{
	friend class Mesh;
public:
	struct Submesh
	{ 
		uint       m_indexOffset;
		uint       m_indexCount;
		uint       m_vertexOffset;
		uint       m_vertexCount;
		uint       m_materialId;
		AlignedBox m_boundingBox;
		Sphere     m_boundingSphere;

		Submesh();
	};

	static MeshData* Create(const char* _path);
	static MeshData* Create(
		const MeshDesc& _desc, 
		uint            _vertexCount, 
		uint            _indexCount, 
		const void*     _vertexData = nullptr,
		const void*     _indexData  = nullptr
		);
	static MeshData* Create(
		const MeshDesc&    _desc,
		const MeshBuilder& _meshBuilder
		);

	// Create a plane in XZ with the given dimensions and tesselation.
	static MeshData* CreatePlane(
		const MeshDesc& _desc, 
		float           _sizeX, 
		float           _sizeZ, 
		int             _segsX, 
		int             _segsZ
		);

	static void Destroy(MeshData*& _meshData_);

	friend void swap(MeshData& _a, MeshData& _b);

	// Copy vertex data directly from _src. The layout of _src must match the MeshDesc.
	void setVertexData(const void* _src);
	// Copy semantic data from _src, converting from _srcType.
	void setVertexData(VertexAttr::Semantic _semantic, DataType _srcType, uint _srcCount, const void* _src);
	
	// Copy index data from _src. The layout of _src must match the index data type/count.
	void setIndexData(const void* _src);
	// Copy index data from _src, converting from _srcType.
	void setIndexData(DataType _srcType, const void* _src);

	uint64          getHash() const;
	const char*     getPath() const               { return m_path; }
	const MeshDesc& getDesc() const               { return m_desc; }
	uint            getVertexCount() const        { return m_submeshes[0].m_vertexCount; }
	const void*     getVertexData() const         { return m_vertexData; }
	uint            getIndexCount() const         { return m_submeshes[0].m_indexCount; }
	const void*     getIndexData() const          { return m_indexData; }
	DataType        getIndexDataType() const      { return m_indexDataType; }

protected:
	apt::String<32> m_path; // empty if not from a file
	MeshDesc  m_desc;
	char*     m_vertexData;
	char*     m_indexData;
	DataType  m_indexDataType;

	std::vector<Submesh> m_submeshes;

	// \todo 
	void beginSubmesh(uint _materialId);
	void endSubmesh();

private:
	MeshData();
	MeshData(const MeshDesc& _desc);
	MeshData(const MeshDesc& _desc, const MeshBuilder& _meshBuilder);
	~MeshData();

	void updateSubmeshBounds(Submesh& _submesh);
	
	static bool ReadObj(MeshData& _mesh, const char* _data, uint _dataSize);

}; // class MeshData


////////////////////////////////////////////////////////////////////////////////
// MeshBuilder
// Mesh construction/manipulation tools.
////////////////////////////////////////////////////////////////////////////////
class MeshBuilder
{
	friend class MeshData;
public:
	struct Vertex
	{
		vec3   m_position;
		vec2   m_texcoord;
		vec3   m_normal;
		vec3   m_tangent;
		vec4   m_color;
		vec4   m_boneWeights;
		uvec4  m_boneIndices;
	};

	struct Triangle
	{
		uint32 a, b, c;
		Triangle(uint32 _a, uint32 _b, uint32 _c)
			: a(_a), b(_b), c(_c) 
		{
		}
		uint32 operator[](int _i)
		{ 
			return (&a)[_i]; 
		}
	};

	MeshBuilder();

	void              transform(const mat4& _mat);
	void              transformTexcoords(const mat3& _mat);
	void              transformColors(const mat4& _mat);
	void              normalizeBoneWeights();
	void              generateNormals();
	void              generateTangents();
	void              updateBounds();

	uint32            addTriangle(uint32 _a, uint32 _b, uint32 _c);
	uint32            addTriangle(const Triangle& _triangle);
	uint32            addVertex(const Vertex& _vertex);

	Vertex&           getVertex(uint32 _i)         { APT_ASSERT(_i < getVertexCount()); return m_vertices[_i]; }
	const Vertex&     getVertex(uint32 _i) const   { APT_ASSERT(_i < getVertexCount()); return m_vertices[_i]; }
	Triangle&         getTriangle(uint32 _i)       { APT_ASSERT(_i < getTriangleCount()); return m_triangles[_i]; }
	const Triangle&   getTriangle(uint32 _i) const { APT_ASSERT(_i < getTriangleCount()); return m_triangles[_i]; }
	uint32            getVertexCount() const       { return (uint32)m_vertices.size(); }
	uint32            getTriangleCount() const     { return (uint32)m_triangles.size(); }
	uint32            getIndexCount() const        { return (uint32)m_triangles.size() * 3; }
	const AlignedBox& getBoundingBox() const       { return m_boundingBox; }
	const Sphere&     getBoundingSphere() const    { return m_boundingSphere; }
private:
	MeshDesc m_desc;
	std::vector<Vertex>   m_vertices;
	std::vector<Triangle> m_triangles;

	AlignedBox m_boundingBox;
	Sphere     m_boundingSphere;

}; // class MeshBuilder

} // namespace frm

#endif // frm_MeshData_h
