#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/Resource.h>
#include <frm/physics/Physics.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// PhysicsMaterials
// Notes:
// - Create* methods don't implicitly call Use() like other resources.
////////////////////////////////////////////////////////////////////////////////
class PhysicsMaterial: public Resource<PhysicsMaterial>
{
	friend class Physics;
	friend class PhysicsShape;

public:

	// Load from a file.
	static PhysicsMaterial* Create(const char* _path);
	// Create a unique instance.
	static PhysicsMaterial* Create(float _staticFriction, float _dynamicFriction, float _restitution, const char* _name = nullptr);
	// Create a unique instance from a serializer (e.g. for inline materials).
	static PhysicsMaterial* Create(Serializer& _serializer_);
	
	// Destroy _inst_.
	static void Destroy(PhysicsMaterial*& _inst_);

	bool load() { return reload(); }
	bool reload();

	bool edit();
	bool serialize(Serializer& _serializer_);

	const char* getPath() const             { return m_path.c_str(); }
	float       getStaticFriction() const   { return m_staticFriction; }
	float       getDynamicFriction() const  { return m_dynamicFriction; }
	float       getRestitution() const      { return m_restitution; }
	
private:

	PhysicsMaterial(uint64 _id, const char* _name);
	~PhysicsMaterial();

	PathStr m_path            = "";      // Empty if not from a file.
	float   m_staticFriction  = 0.5f;    // Friction coefficient for stationary objects, in [0,1].
	float   m_dynamicFriction = 0.5f;    // Friction coefficient for moving objects, in [0,1].
	float   m_restitution     = 0.2f;    // Coefficient of restitution, in [0,1].
	void*   m_impl            = nullptr;

	// Send material properties to the implementation.
	void updateImpl();

}; // class PhysicsMaterial

} // namespace frm
