/* \todo
	- Mass ratio/interpenetration issues.
		- Test gym for different cases: material friction (boxes on a slope), stacked boxes.
	- Trigger shape component + callback.
	- Joint API (do CreateJoint(Type, Component*, Mat4, Component*, Mat4) like the PhysX API).
	- Aggregate API (do BeginGroup()/EndGroup() - give groups an ID and allow them to be reset independently).
	- Filtering for raycasts (static vs. dynamic objects).
	- Articulation component (e.g. for ragdolls).
	- Vehicle component.
*/
#pragma once

#include <frm/core/frm.h>
#include <frm/core/Component.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

namespace frm {

class Physics;
class PhysicsMaterial;
class PhysicsGeometry;
class PhysicsConstraint;
struct Component_Physics;

////////////////////////////////////////////////////////////////////////////////
// Physics
////////////////////////////////////////////////////////////////////////////////
class Physics
{
public:
	
	enum Flags_
	{
		Flags_Static,
		Flags_Kinematic,
		Flags_Dynamic,

		Flags_Simulation,
		Flags_Query,

		Flags_Default = (1 << Flags_Dynamic) | (1 << Flags_Simulation) | (1 << Flags_Query)
	};
	typedef uint32 Flags;

	static bool Init();
	static void Shutdown();
	static void Update(float _dt);
	static void Edit();

	static bool InitComponent(Component_Physics* _component_);
	static void ShutdownComponent(Component_Physics* _component_);
	static void RegisterComponent(Component_Physics* _component_);
	static void UnregisterComponent(Component_Physics* _component_);

	static const PhysicsMaterial* GetDefaultMaterial();

	static void AddGroundPlane(const PhysicsMaterial* _material = nullptr);

	
	enum RayCastFlags_
	{
		RayCastFlags_Position, // get the position
		RayCastFlags_Normal,   // get the normal
		RayCastFlags_AnyHit,   // get any hit (else closest)

		RayCastFlags_Default = (1 << RayCastFlags_Position | 1 << RayCastFlags_Normal)
	};
	typedef uint32 RayCastFlags;

	struct RayCastIn
	{
		vec3  origin      = vec3(0.0f);
		vec3  direction   = vec3(0.0f);
		float maxDistance = 1e10f;

		RayCastIn(const vec3& _origin, const vec3& _direction, float _maxDistance = 1e10f)
			: origin(_origin)
			, direction(_direction)
			, maxDistance(_maxDistance)
		{
		}
	};

	struct RayCastOut
	{
		vec3               position  = vec3(0.0f); // position of the intersection
		vec3               normal    = vec3(0.0f); // normal at the intersection
		float              distance  = 0.0f;       // intersection distance along ray
		Component_Physics* component = nullptr;    // component
	};

	// Return true if an intersection was found, in which case out_ contains valid data.
	static bool RayCast(const RayCastIn& _in, RayCastOut& out_, RayCastFlags _flags = RayCastFlags_Default);

private:

	static Physics* s_instance;

	bool  m_paused            = true;
	bool  m_step              = true;
	bool  m_debugDraw         = false;
	float m_stepLengthSeconds = 1.0f/60.0f;
	float m_stepAccumulator   = 0.0f;

	float m_gravity           = 15.0f;
	vec3  m_gravityDirection  = vec3(0.0f, -1.0f, 0.0f);

	// \todo better containers for these - fast iteration, insertion/deletion?
	eastl::vector<Component_Physics*> m_static;
	eastl::vector<Component_Physics*> m_kinematic;
	eastl::vector<Component_Physics*> m_dynamic;

	static void ApplyNodeTransforms();
};

////////////////////////////////////////////////////////////////////////////////
// Component_Physics
////////////////////////////////////////////////////////////////////////////////
struct Component_Physics: public Component
{
	friend class Physics;

	static Component_Physics* Create(
		const PhysicsGeometry* _geometry, 
		const PhysicsMaterial* _material, 
		float                  _mass, 
		const mat4&            _initialTransform = identity, 
		Physics::Flags        _flags             = Physics::Flags_Default
		);

	void  setFlags(Physics::Flags _flags) { Physics::UnregisterComponent(this); m_flags = _flags; Physics::RegisterComponent(this); }
	Physics::Flags getFlags() const       { return m_flags; }

	virtual bool init() override;
	virtual void shutdown() override;
	virtual void update(float _dt) override;
	virtual bool edit() override;
	virtual bool serialize(Serializer& _serializer_) override;

	virtual void reset();

	void addForce(const vec3& _force);
	void setLinearVelocity(const vec3& _linearVelocity);


	// Explicitly copy internal transform to the node's transform.
	void forceUpdateNode();

	// Explicitly wake the physics actor.
	void forceWake() { addForce(vec3(0.0f)); }

	
	struct Impl;
	Impl* getImpl() { return m_impl; }

protected:

	Impl* m_impl = nullptr;

	mat4             m_initialTransform	= identity;
	Physics::Flags   m_flags            = Physics::Flags_Default;
	PhysicsGeometry* m_geometry         = nullptr;
	PhysicsMaterial* m_material         = nullptr;
	float            m_mass             = 1.0f;

	bool editFlags();
	bool serializeFlags(Serializer& _serializer_);

	bool editGeometry();
	bool editMaterial();

};

struct Component_PhysicsTemporary: public Component_Physics
{
	friend class Physics;

	static Component_PhysicsTemporary* Create(
		const PhysicsGeometry* _geometry, 
		const PhysicsMaterial* _material, 
		float                  _mass,
		const mat4&            _initialTransform = identity, 
		Physics::Flags         _flags            = Physics::Flags_Default
		);

	virtual bool init() override;
	virtual void shutdown() override;
	virtual void update(float _dt) override;
	virtual bool edit() override;
	virtual bool serialize(Serializer& _serializer_) override;
	virtual void reset() override;

	float velocityThreshold = 1e-1f;
	float idleTimeout       = 0.5f;
	float timer             = 0.0f;

	Component_BasicRenderable* basicRenderable = nullptr;

};

} // namespace frm