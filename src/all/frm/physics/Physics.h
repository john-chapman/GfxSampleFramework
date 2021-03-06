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

class PhysicsWorld;

////////////////////////////////////////////////////////////////////////////////
// Physics
// \todo
// - Raycast CCD (cheaper alternative to full builtin CCD).
// - Suppress triangle remapping tables on the cooker?
////////////////////////////////////////////////////////////////////////////////
class Physics
{
public:
	
	// Get/create the default material.
	static const PhysicsMaterial* GetDefaultMaterial();

	// Get/set the current world instance.
	static PhysicsWorld*          GetCurrentWorld()                      { return s_currentWorld; }
	static void                   SetCurrentWorld(PhysicsWorld* _world)  { s_currentWorld = _world; }


	enum class Flag
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
	
 // Ray casts
	enum class RayCastFlag: uint8
	{
		Position,       // Get the position.
		Normal,         // Get the normal.
		AnyHit,         // Get any hit (else closest).
		TriangleIndex,  // Get the triangle index.

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
		vec3              position       = vec3(0.0f); // Position of the intersection.
		vec3              normal         = vec3(0.0f); // Normal at the intersection.
		float             distance       = 0.0f;       // Hit distance along ray.
		uint32            triangleIndex  = 0;          // Triangle index (for tri mesh hits).
		PhysicsComponent* component      = nullptr;    // Component that was hit.
	};

 // Collision events
	// \todo Use threshold impact forces to generate on/persists/off events (see physx docs).
	struct CollisionEvent
	{
		PhysicsComponent* components[2] = { nullptr };
		vec3              point         = vec3(0.0f);
		vec3              normal        = vec3(0.0f);
		float             impulse       = 0.0f;
	};


private:

	static PhysicsWorld* s_currentWorld;
};

////////////////////////////////////////////////////////////////////////////////
// PhysicsWorld
////////////////////////////////////////////////////////////////////////////////
class PhysicsWorld
{
public:

	using Flags          = Physics::Flags;
	using RayCastIn      = Physics::RayCastIn;
	using RayCastOut     = Physics::RayCastOut;
	using RayCastFlags   = Physics::RayCastFlags;
	using CollisionEvent = Physics::CollisionEvent;

		 PhysicsWorld();
	     ~PhysicsWorld();

	bool init();
	void shutdown();
	void update(float _dt);
	bool edit();
	void drawDebug();

	void registerComponent(PhysicsComponent* _component_);
	void unregisterComponent(PhysicsComponent* _component_);

	void addGroundPlane(const PhysicsMaterial* _material = Physics::GetDefaultMaterial());

	// Return true if an intersection was found, in which case out_ contains valid data.
	bool rayCast(const RayCastIn& _in, RayCastOut& out_, RayCastFlags _flags = RayCastFlags());

	// Valid between calls to update().
	eastl::span<CollisionEvent> getCollisionEvents();

	void setPaused(bool _paused);
	void reset();

	struct Impl;
	Impl* m_impl              = nullptr; // PhysicsInternal.h

private:

	bool  m_paused            = false;
	bool  m_step              = true;
	bool  m_drawDebug         = false;
	float m_stepLengthSeconds = 1.0f/60.0f;
	float m_stepAccumulator   = 0.0f;
	float m_gravity           = 15.0f;
	vec3  m_gravityDirection  = vec3(0.0f, -1.0f, 0.0f);


	// \todo better containers for these - fast iteration, insertion/deletion?
	eastl::vector<PhysicsComponent*> m_static;
	eastl::vector<PhysicsComponent*> m_kinematic;
	eastl::vector<PhysicsComponent*> m_dynamic;

	void updateComponentTransforms();
	void setDrawDebug(bool _enable);

	friend class Physics;
};

////////////////////////////////////////////////////////////////////////////////
// PhysicsComponent
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(PhysicsComponent)
{
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
	virtual void reset() override;

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

	friend class Physics;
	friend class PhysicsWorld;
};

} // namespace frm