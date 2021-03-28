#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/BitFlags.h>
#include <frm/core/world/World.h>
#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// PhysicsConstraint
//
// \todo
// - OnBreak callback (or event model, like for collisions?)
// - Edit frames with raycasts.
// - Drive forces.
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(PhysicsConstraint)
{
	friend class Physics;

public:

	enum class Flag: uint8
	{
		CollisionsEnabled,
		StartBroken,

		BIT_FLAGS_COUNT_DEFAULT_ZERO()
	};
	using Flags = BitFlags<Flag>;

	enum Type_
	{
		Type_Distance,
		Type_Sphere,
		Type_Revolute,

		Type_Count,
		Type_Invalid = Type_Count
	};
	typedef int Type;

	// Spring limit.
	struct LimitSpring
	{
		float stiffness; // in [0, FLT_MAX], inactive if <= 0
		float damping;   // 0 = undamped, <1 = under-damped (will oscillate), 1 = critically damped (slows to equilibrium), >1 = over-damped
	};

	// Eliptical cone limit, aligned on +X.
	struct LimitCone
	{
		float angleX; // in radians
		float angleY; //    "
	};

	// Linear distance constraint. If stiffness > 0 the constraint acts as a spring, activated at maxDistance.
	struct Distance
	{
		float minDistance;
		float maxDistance;
		LimitSpring spring;
	};

	// Constrains the component's frames to be coincident, with free rotation within a cone limit.
	struct Sphere
	{
		LimitCone cone;
		LimitSpring spring;
	};

	// Constrain the component's motion to rotation around the frame's x axis.
	struct Revolute
	{
		float minAngle;
		float maxAngle;
		LimitSpring spring;
	};

	static void Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase);
	static eastl::span<PhysicsConstraint*> GetActiveComponents();

	static PhysicsConstraint* CreateDistance(
		PhysicsComponent* _componentA, const mat4& _frameA,
		PhysicsComponent* _componentB, const mat4& _frameB,
		const Distance& _data
		);

	static PhysicsConstraint* CreateSphere(
		PhysicsComponent* _componentA, const mat4& _frameA,
		PhysicsComponent* _componentB, const mat4& _frameB,
		const Sphere& _data
		);

	static PhysicsConstraint* CreateRevolute(
		PhysicsComponent* _componentA, const mat4& _frameA,
		PhysicsComponent* _componentB, const mat4& _frameB,
		const Revolute& _data
		);

	static void Destroy(PhysicsConstraint*& _inst_);


	       PhysicsConstraint() = default;
	       ~PhysicsConstraint() = default;

	void   setFlags(Flags _flags);
	void   setFlag(Flag _flag, bool _value);
	Flags  getFlags() const                   { return m_flags; }
	bool   getFlag(Flag _flag) const          { return m_flags.get(_flag); }

	bool   isBroken() const;
	void   setBroken(bool _broken);

	void   setNode(int _i, SceneNode* _node);

	void   setComponent(int _i, PhysicsComponent* _component);
	void   setComponentFrame(int _i, const mat4& _frame);

private:

	union ConstraintData
	{
		Distance distance;
		Sphere   sphere;
		Revolute revolute;
	};
		
	Type                 m_type               = Type_Invalid;
	Flags                m_flags;
	GlobalNodeReference  m_nodes[2];
	PhysicsComponent*    m_components[2]      = { nullptr, nullptr };
	mat4                 m_componentFrames[2] = { identity, identity };
	float                m_breakForce         = 0.0f;
	float                m_breakTorque        = 0.0f;    
	ConstraintData       m_constraintData;
	void*                m_impl               = nullptr;

	static void OnNodeShutdown(SceneNode* _node, void* _component);

	bool editCone(LimitCone& cone);
	bool editSpring(LimitSpring& spring);

	bool initImpl() override;
	bool postInitImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;
	bool isStatic() override { return true; }

	void setImplData(Type _newType);
	void wakeComponents();

	void draw();
};

} // namespace frm