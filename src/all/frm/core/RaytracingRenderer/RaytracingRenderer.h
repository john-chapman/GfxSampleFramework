#pragma once

#include <frm/core/frm.h>
#include <frm/core/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// RaytracingRenderer
//
// \todo
// - MeshData only needs to load copy the relevant verts for the submesh 
//   (currently copying the whole mesh). Would need to use an indirection table
//   to remap index data.
// - Cache optimized mesh data with the PhysX trimesh.
// - Cleanup multithreading stuff. 
// - Texture LOD estimation (use hit distance/normal, requires source ray).
// - Setters for modifying thread count, etc. without reloading the scene.
// - Separate callback for OnEdit (do minimal reset of the instance).
// - Need to generate lightmap UVs on demand (force DrawMesh reload?). Ultimately
//   need to associate lightmaps with a cached draw mesh since the resolution 
//   affects chart packing (need a static scene processor basically). Note that
//   any scaling applied to an instance must also be applied to the static mesh
//   before lightmap UV generation.
// - Full GPU support (requires custom BVH + CS traversal).
////////////////////////////////////////////////////////////////////////////////
class RaytracingRenderer
{
public:

	static RaytracingRenderer* Create(uint32 _threadCount = 1, uint32 _maxRaysPerThread = 512);
	static void                Destroy(RaytracingRenderer*& _inst_);

	void update();
	void drawDebug();

	
	struct Ray
	{		
		vec3  origin      = vec3(0.0f);
		vec3  direction   = vec3(0.0f);
		float maxDistance = 1e10f;
	};

	struct /*alignas(16)*/ RayHit // Sorting eastl::vector crashes with aligned types?
	{
		vec3   position       = vec3(0.0f); // Position of the intersection (world space).
		float  distance       = 0.0f;       // Hit distance along ray.
		vec3   normal         = vec3(0.0f); // Normal at the intersection (world space).
		uint32 triangleIndex  = 0;          // Triangle index.
		vec2   barycentrics   = vec2(0.0f); // Triangle barycentrics at the hit location.
		uint32 isHit          = 0;          // Whether hit is valid (for batched raycasts).
		uint32 rayID          = 0;          // Index of the ray in the source ray buffer.
		void*  instance       = nullptr;    // Ptr to the internal instance data (see bindInstance()).
		void*  meshData       = nullptr;    // Ptr to the internal mesh data - use as a sort key.
	};
	static_assert(sizeof(RayHit) % 16 == 0, "Need to manually pad RayHit to 16-byte alignment");

	struct VertexData
	{
		vec2  materialUV;
		vec2  lightmapUV;
		vec3  normal;
		float pad0;
	};

	// Perform a single raycast. Return true if intersection found.
	bool raycast(const Ray& _in, RayHit& out_); 

	// Perform a block of raycasts. _in and out_ must contain _count elements.
	void raycast(const Ray* _in, RayHit* out_, size_t _count);

	// \todo Do this automatically at the end of raycast?
	void sortRayHits(RayHit* _hits_, size_t _count);

	// Bind mesh/material data for a hit shader.
	void bindInstance(GlContext* _ctx_, const void* _instance);

protected:

	struct Impl;
	Impl* m_impl = nullptr;

	struct MeshData;
	struct Instance;

	static void OnNodeShutdown(SceneNode* _node, RaytracingRenderer* _renderer);

	RaytracingRenderer() = default;
	~RaytracingRenderer();

	bool init(uint32 _threadCount, uint32 _maxRaysPerThread);
	void shutdown();

	bool addInstances(BasicRenderableComponent* _renderable);
	void removeInstances(BasicRenderableComponent* _renderable);
};

} // namespace frm