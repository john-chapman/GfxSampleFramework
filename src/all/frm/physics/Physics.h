#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/BitFlags.h>
#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>
#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Physics
// \todo
// - Raycast CCD (cheaper alternative to full builtin CCD).
////////////////////////////////////////////////////////////////////////////////
class Physics
{
public:

	enum class Flag: uint8
	{
		Static,           // Fixed position, infinite mass.
		Kinematic,        // Animated position, infinite mass.
		Dynamic,          // Simulated position.

		Simulation,       // Participates in simulation.
		Query,            // Visible to queries only.
	
		DisableGravity,   // Ignore global gravity.
		EnableCCD,        // Enable CCD (prevent tunnelling at high velocities).

		BIT_FLAGS_COUNT_DEFAULT(Dynamic, Simulation, Query)
	};
	using Flags = BitFlags<Flag>;

	static bool Init();
	static void Shutdown();
	static void Update(float _dt);
	static void Edit();
	static void DrawDebug();

	static void RegisterComponent(PhysicsComponent* _component_);
	static void UnregisterComponent(PhysicsComponent* _component_);

	static const PhysicsMaterial* GetDefaultMaterial();

	static void AddGroundPlane(const PhysicsMaterial* _material = GetDefaultMaterial());


 // Ray cast API
	enum class RayCastFlag: uint8
	{
		Position, // Get the position.
		Normal,   // Get the normal.
		AnyHit,   // Get any hit (else closest).

		BIT_FLAGS_COUNT_DEFAULT(Position, Normal)
	};
	using RayCastFlags = BitFlags<RayCastFlag>;

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
		vec3              position  = vec3(0.0f); // Position of the intersection.
		vec3              normal    = vec3(0.0f); // Normal at the intersection.
		float             distance  = 0.0f;       // Hit distance along ray.
		PhysicsComponent* component = nullptr;    // Component that was hit.
	};

	// Return true if an intersection was found, in which case out_ contains valid data.
	static bool RayCast(const RayCastIn& _in, RayCastOut& out_, RayCastFlags _flags = RayCastFlags());

 // Collision events API
	// \todo
	// - Use threshold impact forces to generate on/persists/off events (see physx docs).
	struct CollisionEvent
	{
		PhysicsComponent* components[2] = { nullptr };
		vec3              point         = vec3(0.0f);
		vec3              normal        = vec3(0.0f);
		float             impulse       = 0.0f;
	};

	static eastl::span<CollisionEvent> GetCollisionEvents() { return eastl::span<CollisionEvent>(s_instance->m_collisionEvents); }

private:

	static Physics* s_instance;

	bool  m_paused            = false;
	bool  m_step              = true;
	bool  m_drawDebug         = false;
	float m_stepLengthSeconds = 1.0f/60.0f;
	float m_stepAccumulator   = 0.0f;

	float m_gravity           = 15.0f;
	vec3  m_gravityDirection  = vec3(0.0f, -1.0f, 0.0f);

	eastl::vector<CollisionEvent> m_collisionEvents;

	// \todo better containers for these - fast iteration, insertion/deletion?
	eastl::vector<PhysicsComponent*> m_static;
	eastl::vector<PhysicsComponent*> m_kinematic;
	eastl::vector<PhysicsComponent*> m_dynamic;

	     Physics();
	     ~Physics();

	void updateComponentTransforms();
};


////////////////////////////////////////////////////////////////////////////////
// PhysicsComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(PhysicsComponent)
{
	friend class Physics;

public:

	using Flag  = Physics::Flag;
	using Flags = Physics::Flags;

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<PhysicsComponent*> GetActiveComponents();

	static PhysicsComponent* CreateTransient(
		const PhysicsGeometry* _geometry,
		const PhysicsMaterial* _material,
		float                  _mass,
		float                  _idleTimeout,
		const mat4&            _initialTransform = identity,
		Flags                  _flags            = Flags()
		); 

	void         setFlags(Flags _flags);
	void         setFlag(Flag _flag, bool _value);
	Flags        getFlags() const                   { return m_flags; }
	bool         getFlag(Flag _flag) const          { return m_flags.get(_flag); }

	void         addForce(const vec3& _force);
	void         setLinearVelocity(const vec3& _linearVelocity);
	vec3         getLinearVelocity() const;
	void         setAngularVelocity(const vec3& _angularVelocity);
	vec3         getAngularVelocity() const;

	void         setWorldTransform(const mat4& _world);
	mat4         getWorldTransform() const;

	void         setMass(float _mass);
	float        getMass() const                    { return m_mass; }

	void         setIdleTimeout(float _idleTimeout) { m_idleTimeout = _idleTimeout; }
	float        getIdleTimeout() const             { return m_idleTimeout; }

	const PhysicsGeometry* getGeometry() const  { return m_geometry; }
	const PhysicsMaterial* getMaterial() const  { return m_material; }

	void*        getImpl() { return m_impl; }

	// Reset to the initial state, zero velocities.
	virtual void reset();

	// Explicitly copy internal transform back to the parent node.
	void         forceUpdateNodeTransform();

	// Explicitly wake the physics actor.
	void         forceWake() { addForce(vec3(0.0f)); }

	// \hack Re-initialize (e.g. after edit).
	bool         reinit();


protected:

	Flags                     m_flags;
	mat4                      m_initialTransform         = identity;
	float                     m_mass                     = 1.0f;
	const PhysicsMaterial*    m_material                 = nullptr;
	const PhysicsGeometry*    m_geometry                 = nullptr;
	void*                     m_impl                     = nullptr;

	// Transient properties.
	float                     m_idleTimeout              = 0.5f;
	float                     m_timer                    = 0.0f;
	BasicRenderableComponent* m_basicRenderableComponent = nullptr;

	bool initImpl() override;
	bool postInitImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }

	bool editFlags();

	void updateTransient(float _dt);
};

} // namespace frm