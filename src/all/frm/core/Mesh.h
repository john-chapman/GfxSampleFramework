#pragma once

#include <frm/core/frm.h>
#include <frm/core/geom.h>
#include <frm/core/BitFlags.h>
#include <frm/core/Resource.h>
#include <frm/core/String.h>
#include <frm/core/StringHash.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Mesh
//
// \todo
// - Implement CreateFlags for for procedural meshes. LOD generation should *not* 
//   use meshopt but instead just generate simpler primitives.
// - Generalize vertex data semantics. Ideal solution would be to use a map with
//   arbitrary strings as keys, however this will necessitate a more complex
//   binding model for DrawMesh to match semantics with vertex attribute 
//   locations on the shader (via introspection + setting vertex locations per
//   draw).
// - Auto-generate lightmap UVs?
////////////////////////////////////////////////////////////////////////////////
class Mesh
{
public:

	enum _Primitive
	{
		Primitive_Invalid     = 0,
		Primitive_Points      = 1,
		Primitive_Lines       = 2,
		Primitive_Triangles   = 3,

		Primitive_Count
	};
	using Primitive = int;
	static const char* kPrimitiveStr[Primitive_Count];

	// Must match VertexSemantic_ defines in def.glsl.
	enum _VertexDataSemantic
	{
		Semantic_Positions,
		Semantic_Normals, 
		Semantic_Tangents,
		Semantic_Colors,  
		Semantic_MaterialUVs,
		Semantic_LightmapUVs,
		Semantic_BoneWeights,
		Semantic_BoneIndices,
		Semantic_User0,
		Semantic_User1,
		Semantic_User2,
		Semantic_User3,

		Semantic_Count,
		Semantic_Invalid = -1
	};
	using VertexDataSemantic = int;
	static const char* kVertexDataSemanticStr[Semantic_Count];

	enum class CreateFlag
	{
		Optimize,
		GenerateLODs,
		GenerateNormals,
		GenerateTangents,

		BIT_FLAGS_COUNT_DEFAULT(Optimize, GenerateLODs, GenerateNormals, GenerateTangents)
	};
	using CreateFlags = BitFlags<CreateFlag>;

	// Create a plane in XZ.
	static Mesh* CreatePlane(
		float       _sizeX, 
		float       _sizeZ, 
		int         _segsX, 
		int         _segsZ,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Create a disc in XZ.
	// \todo Radial segments?
	static Mesh* CreateDisc(
		float       _radius, 
		int         _sides,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Create a box. Each face is a submesh, ordering is X+,X-,Y+,Y-,Z+,Z-.
	static Mesh* CreateBox(
		float       _sizeX,
		float       _sizeY,
		float       _sizeZ,
		int         _segsX,
		int         _segsY,
		int         _segsZ,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Create a sphere with poles aligned to Y axis.
	static Mesh* CreateSphere(
		float       _radius,
		int         _segsLat, 
		int         _segsLong,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Create a cylinder aligned to the Y axis.
	static Mesh* CreateCylinder(
		float       _radius,
		float       _length,
		int         _sides,
		int         _segs,
		bool        _capped    = true,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Create a cone aligned to the Y axis.
	static Mesh* CreateCone(
		float       _height,
		float       _radiusTop,
		float       _radiusBottom,
		int         _sides,
		int         _segs,		
		bool        _capped    = true,
		const mat4& _transform = identity,
		CreateFlags _createFlags = CreateFlags()
		);

	// Load from path.
	static Mesh*          Create(const char* _path, CreateFlags _createFlags = CreateFlags());
	
	// Destroy _mesh_.
	static void           Destroy(Mesh*& _mesh_);

	
		                  Mesh(Primitive _primitive = Primitive_Triangles);
						  Mesh(const char* _path, CreateFlags _createFlags = CreateFlags());
	                      ~Mesh();

	const char*           getPath() const                                                  { return m_path.c_str(); }
	Primitive             getPrimitive() const                                             { return m_primitive; }
	uint32                getVertexCount() const                                           { return m_vertexCount; }
	uint32                getIndexCount(uint32 _lod = 0, uint32 _submesh = 0) const        { return m_lods[_lod].submeshes[_submesh].indexCount; }
	DataType              getIndexDataType() const                                         { return m_indexDataType; }
	const AlignedBox&     getBoundingBox(uint32 _lod = 0, uint32 _submesh = 0) const       { return m_lods[_lod].submeshes[_submesh].boundingBox; }
	const Sphere&         getBoundingSphere(uint32 _lod = 0, uint32 _submesh = 0) const    { return m_lods[_lod].submeshes[_submesh].boundingSphere; }
	Skeleton*             getSkeleton()                                                    { return m_skeleton; }
	void                  setSkeleton(const Skeleton& _skeleton);


	template <typename tType>
	struct VertexDataView;
	
	// Access a range of vertex data, cast to tType. If no vertex data exists for _semantic, allocate it.
	template <typename tType>
	VertexDataView<tType> getVertexDataView(VertexDataSemantic _semantic, uint32 _offset = 0, uint32 _count = ~0);

	// Direct vertex data access.
	void*                 getVertexData(VertexDataSemantic _semantic);

	// Allocate and optionally set vertex data.
	void                  setVertexData(VertexDataSemantic _semantic, DataType _dataType, uint32 _dataCount, const void* _data = nullptr);

	// Set vertex count, reallocate existing vertex data arrays.
	void                  setVertexCount(uint32 _count);


	template <typename tType>
	struct IndexDataView;

	// Access a range of index data, cast to tType. Unlike getVertexDataView(), this doesn't allocate index data if none is present.
	template <typename tType>
	IndexDataView<tType>  getIndexDataView(uint32 _lod = 0, uint32 _submesh = 0, uint32 _offset = 0, uint32 _count = ~0);

	// Direct index data access.
	void*                 getIndexData(uint32 _lod = 0, uint32 _submesh = 0);

	// Allocate and optionally set index data. 
	void                  setIndexData(uint32 _lod, DataType _dataType, uint32 _indexCount, const void* _data = nullptr);
	

	// Transform positions, normals and tangents.
	void                  transform(const mat4& _transform);

	// Generate normals. Requires vertex positions.
	void                  generateNormals();

	// Generate tangents. Requires vertex positions and material UVs.
	void                  generateTangents();

	// Compute bounds for all submeshes in all LODs. Requires vertex positions.
	void                  computeBounds();

	// Perform vertex cache, overdraw and vertex fetch optimizations.
	void                  optimize();

	// Generate LODs.
	// _lodCount specifies the max number of LODs to generate (includes LOD 0).
	// _targetReduction specifies the target triangle reduction per LOD (0.6 = 60%).
	// _targetError specifies the maximum deviation from the source mesh (0.1 = 10%).
	void                  generateLODs(int _lodCount = 4, float _targetReduction = 0.6f, float _targetError = 0.1f);

protected:

	struct VertexData
	{
		VertexDataSemantic semantic  = Semantic_Invalid;
		void*              data      = nullptr;
		DataType           dataType  = DataType_Invalid;
		int                dataCount = 0;

		uint32             getDataSizeBytes() const { return (uint32)DataTypeSizeBytes(dataType) * dataCount; }
	};

	struct Submesh
	{
		uint32         indexOffset     = 0;
		uint32         indexCount      = 0;
		AlignedBox     boundingBox;
		Sphere         boundingSphere;		
	};
	using SubmeshList = eastl::vector<Submesh>;
	
	struct LOD
	{
		SubmeshList    submeshes; // Submesh 0 represents the whole mesh.
		void*          indexData       = nullptr;
	};
	using LODList = eastl::vector<LOD>;


	String<32> m_path                        = ""; // Empty if not from a file.
	Primitive  m_primitive                   = Primitive_Invalid;
	uint32     m_vertexCount                 = 0;
	DataType   m_indexDataType               = DataType_Uint32;
	VertexData m_vertexData[Semantic_Count];
	LODList    m_lods;
	Skeleton*  m_skeleton                    = nullptr;

	static bool           ReadGLTF(Mesh& mesh_, const char* _srcData, size_t _srcDataSizeBytes, CreateFlags _createFlags);

	bool                  load(CreateFlags _createFlags);
	void                  unload();	
	bool                  serialize(Serializer& _serializer_);
	void                  addSubmesh(uint32 _lod, uint32 _indexOffset, uint32 _indexCount);
	void                  addSubmesh(uint32 _lod, Mesh& _mesh);
	
	// Finalize vertex/index data formats. Note that this is private as we only want to use reduced precision formats when converting to DrawMesh.
	void                  finalize();

		
	friend class DrawMesh;
};

template <typename tType>
struct Mesh::VertexDataView
{
	tType& operator[](uint32 i) { FRM_ASSERT(i < getCount()); return m_begin[i]; }
	tType* begin()              { return m_begin; }
	tType* end()                { return m_end; }
	uint32 getCount() const     { return m_count; }

private:

	VertexDataView() = default;
	VertexDataView(tType* _begin, tType* _end): m_begin(_begin), m_end(_end), m_count((uint32)(_end - _begin)) { FRM_STRICT_ASSERT(m_end >= m_begin); }
	
	tType* m_begin = nullptr;
	tType* m_end   = nullptr;
	uint32 m_count = 0;

	friend class Mesh;
};

template <typename tType>
inline Mesh::VertexDataView<tType> Mesh::getVertexDataView(VertexDataSemantic _semantic, uint32 _offset, uint32 _count)
{
	if (!m_vertexData[_semantic].data)
	{
		setVertexData(_semantic, FRM_DATA_TYPE_TO_ENUM(FRM_TRAITS_BASE_TYPE(tType)), FRM_TRAITS_COUNT(tType));
	}
	
	VertexData& vertexData = m_vertexData[_semantic];
	FRM_ASSERT(FRM_DATA_TYPE_TO_ENUM(FRM_TRAITS_BASE_TYPE(tType)) == vertexData.dataType);
	FRM_ASSERT(FRM_TRAITS_COUNT(tType) == vertexData.dataCount);
		
	_offset = Min(_offset, m_vertexCount);
	_count  = Min(_count, m_vertexCount - _offset);
	tType* begin = (tType*)vertexData.data + _offset;
	return VertexDataView<tType>(begin, begin + _count);
}

template <typename tType>
struct Mesh::IndexDataView
{
	tType& operator[](uint32 i) { FRM_ASSERT(i < getCount()); return m_begin[i]; }
	tType* begin()              { return m_begin; }
	tType* end()                { return m_end; }
	uint32 getCount() const     { return m_count; }

private:

	IndexDataView() = default;
	IndexDataView(tType* _begin, tType* _end): m_begin(_begin), m_end(_end), m_count((uint32)(_end - _begin)) { FRM_STRICT_ASSERT(m_end >= m_begin); }
	
	tType* m_begin = nullptr;
	tType* m_end   = nullptr;
	uint32 m_count = 0;

	friend class Mesh;
};

template <typename tType>
inline Mesh::IndexDataView<tType> Mesh::getIndexDataView(uint32 _lod, uint32 _submesh, uint32 _offset, uint32 _count)
{
	FRM_ASSERT(_lod < m_lods.size()); // \todo Add LODs?
	LOD& lod = m_lods[_lod];
	FRM_ASSERT(lod.indexData);

	FRM_ASSERT(_submesh < lod.submeshes.size()); // \todo Add submesh?
	Submesh& submesh = lod.submeshes[_submesh];
	
	FRM_ASSERT(FRM_DATA_TYPE_TO_ENUM(FRM_TRAITS_BASE_TYPE(tType)) == m_indexDataType);
	FRM_ASSERT(FRM_TRAITS_COUNT(tType) == m_primitive || FRM_TRAITS_COUNT(tType) == 1);

	const uint32 dataCount = FRM_TRAITS_COUNT(tType);			
	const uint32 maxCount = submesh.indexCount / dataCount;
	_count  = Min(_count, maxCount - _offset);
	_offset = Min(_offset / dataCount, maxCount) + submesh.indexOffset;

	tType* begin = (tType*)lod.indexData + _offset;
	return IndexDataView<tType>(begin, begin + _count);
}

} // namespace frm
