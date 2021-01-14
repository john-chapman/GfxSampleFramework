#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/Resource.h>
#include <frm/physics/Physics.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// PhysicsGeometry
////////////////////////////////////////////////////////////////////////////////
class PhysicsGeometry: public Resource<PhysicsGeometry>
{
	friend class Physics;

public:

	enum Type_
	{
		Type_Sphere,
		Type_Box,
		Type_Plane,
		Type_Capsule,
		Type_ConvexMesh,
		Type_TriangleMesh,
		Type_Heightfield,

		Type_Count
	};
	typedef int Type;

	// Load from a file.
	static PhysicsGeometry* Create(const char* _path);
	// Create a unique instance.
	static PhysicsGeometry* CreateSphere(float _radius, const char* _name = nullptr);
	static PhysicsGeometry* CreateBox(const vec3& _halfExtents, const char* _name = nullptr);
	static PhysicsGeometry* CreateCapsule(float _radius, float _halfHeight, const char* _name = nullptr);
	static PhysicsGeometry* CreatePlane(const vec3& _normal, const vec3& _origin, const char* _name = nullptr);
	static PhysicsGeometry* CreateConvexMesh(const char* _path, const char* _name = nullptr);
	static PhysicsGeometry* CreateTriangleMesh(const char* _path, const char* _name = nullptr);
	// Create a unique instance from a serializer (e.g. for inline geometries).
	static PhysicsGeometry* Create(Serializer& _serializer_);
	// Destroy _inst_.
	static void             Destroy(PhysicsGeometry*& _inst_);

	static bool             Edit(PhysicsGeometry*& _physicsGeometry_, bool* _open_);

	bool                    load() { return reload(); }
	bool                    reload();

	bool                    edit();
	bool                    serialize(Serializer& _serializer_);

	const char*             getPath() const { return m_path.c_str(); }
	Type                    getType() const { return m_type; }

	Id                      getHash() const;
	
//private:

	struct Sphere
	{
		float radius;
	};

	struct Box
	{
		vec3 halfExtents;
	};

	struct Capsule
	{
		float radius;
		float halfHeight;
	};

	struct Plane
	{
		vec3  normal;
		float offset;
	};

	union Data
	{
		Sphere  sphere;
		Box     box;
		Capsule capsule;
		Plane   plane;

		Data()
		{
			memset(this, 0, sizeof(Data));
		}
	};

	PathStr m_path      = "";         // Empty if not from a file.
	Type    m_type      = Type_Count; // Geometry type (determines how m_data is interpreted).
	Data    m_data;                   // Type-dependent data.
	PathStr m_dataPath  = "";         // Source path for convex/triangle meshes and heightfield data.
	void*   m_impl      = nullptr;


	     PhysicsGeometry(uint64 _id, const char* _name);
	     ~PhysicsGeometry();

	bool initImpl();
	void shutdownImpl();

	bool editDataPath();
};

} // namespace frm
