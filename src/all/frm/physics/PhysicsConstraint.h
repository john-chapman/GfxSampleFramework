#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/world/World.h>
#include <frm/core/world/components/Component.h>

#include <EASTL/span.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// PhysicsConstraint
////////////////////////////////////////////////////////////////////////////////
FRM_COMPONENT_DECLARE(PhysicsConstraint)
{
	friend class Physics;

public:

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

	// Eliptical cone limit, aligned on +Z.
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

	// Constrain the component's motion to rotation around the frame's z axis.
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

	void setComponent(int _i, PhysicsComponent* _component);
	void setComponentFrame(int _i, const mat4& _frame);

private:

	union ConstraintData
	{
		Distance distance;
		Sphere   sphere;
		Revolute revolute;
	};
		
	GlobalNodeReference  m_nodes[2];
	PhysicsComponent*    m_components[2]      = { nullptr };
	mat4                 m_componentFrames[2] = { identity };
	float                m_breakForce         = FLT_MAX;
	float                m_breakTorque        = FLT_MAX;    
	ConstraintData       m_constraintData;

	Type  m_type = Type_Invalid;
	void* m_impl = nullptr;

	static void OnNodeShutdown(SceneNode* _node, void* _component);

	bool editCone(LimitCone& cone);
	bool editSpring(LimitSpring& spring);

	bool initImpl() override;
	bool postInitImpl() override;
	void shutdownImpl() override;
	bool editImpl() override;
	bool serializeImpl(Serializer& _serializer_) override;

	void setImplData();
	void wakeComponents();
};

} // namespace frm