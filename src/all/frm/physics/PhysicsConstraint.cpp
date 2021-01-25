#include "PhysicsConstraint.h"
#include "Physics.h"
#include "PhysicsInternal.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/Serializer.h>
#include <frm/core/world/World.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

static const char* kTypeStr[PhysicsConstraint::Type_Count]
{
	"Distance", //Type_Distance,
	"Sphere",   //Type_Sphere,
	"Revolute"  //Type_Revolute
};

// PhysX constraint frames are oriented along +X, ours are oriented along +Z.
static mat4 SwapXZ(const mat4& m)
{
	mat4 ret;
	ret.x = m.z;
	ret.y = m.y;
	ret.z = m.x;
	ret.w = m.w;
	return ret;
}

static physx::PxRigidActor* GetActorFromComponent(PhysicsComponent* _component)
{
	if (!_component)
	{
		return false;
	}

	return ((PxComponentImpl*)_component->getImpl())->pxRigidActor;
}



FRM_COMPONENT_DEFINE(PhysicsConstraint, 0);

// PUBLIC

PhysicsConstraint* PhysicsConstraint::CreateDistance(
	PhysicsComponent* _componentA, const mat4& _frameA,
	PhysicsComponent* _componentB, const mat4& _frameB,
	const Distance& _data
)
{
	PhysicsConstraint* ret         = (PhysicsConstraint*)Create(StringHash("PhysicsConstraint"));
	ret->m_components[0]           = _componentA;
	ret->m_componentFrames[0]      = _frameA;
	ret->m_components[1]           = _componentB;
	ret->m_componentFrames[1]      = _frameB;

	ret->m_type                    = Type_Distance;
	ret->m_constraintData.distance = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

PhysicsConstraint* PhysicsConstraint::CreateSphere(
	PhysicsComponent* _componentA, const mat4& _frameA,
	PhysicsComponent* _componentB, const mat4& _frameB,
	const Sphere& _data
)
{
	PhysicsConstraint* ret         = (PhysicsConstraint*)Create(StringHash("PhysicsConstraint"));
	ret->m_components[0]           = _componentA;
	ret->m_componentFrames[0]      = _frameA;
	ret->m_components[1]           = _componentB;
	ret->m_componentFrames[1]      = _frameB;
	
	ret->m_type                    = Type_Sphere;
	ret->m_constraintData.sphere   = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

PhysicsConstraint* PhysicsConstraint::CreateRevolute(
	PhysicsComponent* _componentA, const mat4& _frameA,
	PhysicsComponent* _componentB, const mat4& _frameB,
	const Revolute& _data
)
{
	PhysicsConstraint* ret         = (PhysicsConstraint*)Create(StringHash("PhysicsConstraint"));
	ret->m_components[0]           = _componentA;
	ret->m_componentFrames[0]      = _frameA;
	ret->m_components[1]           = _componentB;
	ret->m_componentFrames[1]      = _frameB;
	
	ret->m_type                    = Type_Revolute;
	ret->m_constraintData.revolute = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

void PhysicsConstraint::Destroy(PhysicsConstraint*& _inst_)
{
	FRM_ASSERT(_inst_->getState() == World::State::Shutdown);
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

void PhysicsConstraint::setComponent(int _i, PhysicsComponent* _component)
{
	FRM_STRICT_ASSERT(_i < 2);
	if (m_impl)
	{
		physx::PxJoint* pxJoint = (physx::PxJoint*)m_impl;
		physx::PxRigidActor* actors[2];
		pxJoint->getActors(actors[0], actors[1]);
		actors[_i] = GetActorFromComponent(_component);
		pxJoint->setActors(actors[0], actors[1]);
		m_components[_i] = _component;
		wakeComponents();
	}
}

void PhysicsConstraint::setComponentFrame(int _i, const mat4& _frame)
{
	FRM_STRICT_ASSERT(_i < 2);
	if (m_impl)
	{
		physx::PxJoint* pxJoint = (physx::PxJoint*)m_impl;
		pxJoint->setLocalPose((_i == 0) ? physx::PxJointActorIndex::eACTOR0 : physx::PxJointActorIndex::eACTOR1, Mat4ToPxTransform(SwapXZ(_frame)));
		m_componentFrames[_i] = _frame;
		wakeComponents();
	}
}

void PhysicsConstraint::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("PhysicsConstraint::Update");

	/*if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		PhysicsConstraint* component = (PhysicsConstraint*)*_from;
	}*/
}

// PRIVATE

void PhysicsConstraint::OnNodeShutdown(SceneNode* _node, void* _component)
{
	FRM_STRICT_ASSERT(_node);
	FRM_STRICT_ASSERT(_component);

	PhysicsConstraint* constraint = (PhysicsConstraint*)_component;
	GlobalNodeReference* nodeRef = nullptr;
	for (int i = 0; i < 2; ++i)
	{
		if (constraint->m_nodes[i].node == _node)
		{
			nodeRef = &constraint->m_nodes[i];
			break;
		}
	}
	FRM_ASSERT(nodeRef);

	_node->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, _component);
	nodeRef->node = nullptr;
}

bool PhysicsConstraint::editCone(LimitCone& cone)
{
	bool ret = false;

	ret |= ImGui::SliderAngle("Angle X", &cone.angleX, 0.0f, 180.0f);
	ret |= ImGui::SliderAngle("Angle Y", &cone.angleY, 0.0f, 180.0f);

	return ret;
}

bool PhysicsConstraint::editSpring(LimitSpring& spring)
{
	bool ret = false;

	ret |= ImGui::DragFloat("Stiffness", &spring.stiffness,   1.0f, -1.0f);
	ret |= ImGui::DragFloat("Damping",   &spring.damping,     0.1f);

	return ret;
}


bool PhysicsConstraint::initImpl()
{
	return true;
}

bool PhysicsConstraint::postInitImpl()
{
	FRM_ASSERT(g_pxPhysics);

	bool ret = true;

	Scene* scene = m_parentNode->getParentScene();
	for (int i = 0; i < 2; ++i)
	{
		if (!m_components[i] && m_nodes[i].isValid())
		{
			ret &= scene->resolveReference(m_nodes[i]);
			m_components[i] = (PhysicsComponent*)m_nodes[i]->findComponent(StringHash("PhysicsComponent"));
			FRM_ASSERT(m_components[i] != nullptr);
			ret &= m_components[i] != nullptr;
		}
	}

	physx::PxRigidActor* actorA = GetActorFromComponent(m_components[0]);
	physx::PxRigidActor* actorB = GetActorFromComponent(m_components[1]);
	
	switch (m_type)
	{
		default:
			FRM_ASSERT(false);
			break;
		case Type_Distance:
		{
			m_impl = physx::PxDistanceJointCreate(*g_pxPhysics, actorA, Mat4ToPxTransform(m_componentFrames[0]), actorB, Mat4ToPxTransform(m_componentFrames[1]));
			setImplData();
			break;
		}
		case Type_Sphere:
		{
			m_impl = physx::PxSphericalJointCreate(*g_pxPhysics, actorA, Mat4ToPxTransform(m_componentFrames[0]), actorB, Mat4ToPxTransform(m_componentFrames[1]));
			setImplData();
			break;
		}
		case Type_Revolute:
		{
			m_impl = physx::PxRevoluteJointCreate(*g_pxPhysics, actorA, Mat4ToPxTransform(m_componentFrames[0]), actorB, Mat4ToPxTransform(m_componentFrames[1]));
			setImplData();
			break;
		}
	};

	((physx::PxJoint*)m_impl)->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, true); // \todo config
	((physx::PxJoint*)m_impl)->setConstraintFlag(physx::PxConstraintFlag::eVISUALIZATION, true);

	for (int i = 0; i < 2; ++i)
	{
		if (m_nodes[i].isValid())
		{
			m_nodes[i]->registerCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
		}
	}

	return true;
}

void PhysicsConstraint::shutdownImpl()
{
	if (m_impl)
	{
		((physx::PxJoint*)m_impl)->release();
	}
}

bool PhysicsConstraint::editImpl()
{
	bool ret = false;

	switch (m_type)
	{
		default:
			FRM_ASSERT(false);
			break;
		case Type_Distance:
		{
			ret |= ImGui::DragFloat("Min Distance", &m_constraintData.distance.minDistance, 0.1f);
			ret |= ImGui::DragFloat("Max Distance", &m_constraintData.distance.maxDistance, 0.1f);
			ret |= editSpring(m_constraintData.distance.spring);

			m_constraintData.distance.minDistance = Max(0.0f, Min(m_constraintData.distance.minDistance, m_constraintData.distance.maxDistance));
			m_constraintData.distance.maxDistance = Max(0.0f, Max(m_constraintData.distance.minDistance, m_constraintData.distance.maxDistance));

			break;
		}
		case Type_Sphere:
		{
			ret |= editCone(m_constraintData.sphere.cone);
			ret |= editSpring(m_constraintData.sphere.spring);

			break;
		}
		case Type_Revolute:
		{
			ret |= ImGui::SliderAngle("Min Angle", &m_constraintData.revolute.minAngle, -360.0f, 360.0f);
			ret |= ImGui::SliderAngle("Max Angle", &m_constraintData.revolute.maxAngle, -360.0f, 360.0f);

			ret |= editSpring(m_constraintData.revolute.spring);

			break;
		}
	};

	/*if (ImGui::TreeNode("Edit Frames"))
	{
		static int editFrame = 0;
		ImGui::RadioButton("A", &editFrame, 0);
		ImGui::SameLine();
		ImGui::RadioButton("B", &editFrame, 1);

		const mat4& toWorld = m_components[editFrame] ? m_components[editFrame]->getNode()->getWorldMatrix() : identity;
		mat4& frame = toWorld * m_componentFrames[editFrame];
		if (Im3d::Gizmo(Im3d::MakeId(this), (float*)&frame))
		{
			setComponentFrame(editFrame, AffineInverse(toWorld) * frame);
		}

		ImGui::TreePop();
	}*/

	if (ret)
	{
		setImplData();
	}
	
	return ret;
}

bool PhysicsConstraint::serializeImpl(Serializer& _serializer_) 
{
	bool ret = SerializeAndValidateClass(_serializer_);
	return ret;
}

void PhysicsConstraint::setImplData()
{
	FRM_ASSERT(m_impl);

	// Type-specific data.
	switch (m_type)
	{
		default:
			FRM_ASSERT(false);
			break;
		case Type_Distance:
		{
			const Distance& data = m_constraintData.distance;
			physx::PxDistanceJoint* joint = (physx::PxDistanceJoint*)m_impl;
			joint->setMinDistance(data.minDistance);
			joint->setMaxDistance(data.maxDistance);
			joint->setStiffness(data.spring.stiffness);
			joint->setDamping(data.spring.damping);
			//joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, true);
			joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, data.minDistance >= 0.0f);
			joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, data.maxDistance >= 0.0f);
			joint->setDistanceJointFlag(physx::PxDistanceJointFlag::eSPRING_ENABLED, data.spring.stiffness > 0.0f);
			break;
		}
		case Type_Sphere:
		{
			const Sphere& data = m_constraintData.sphere;
			physx::PxSphericalJoint* joint = (physx::PxSphericalJoint*)m_impl;
			joint->setLimitCone(physx::PxJointLimitCone(data.cone.angleY, data.cone.angleX, physx::PxSpring(data.spring.stiffness, data.spring.damping)));
			//joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, true);
			joint->setSphericalJointFlag(physx::PxSphericalJointFlag::eLIMIT_ENABLED, true);
			break;
		}
		case Type_Revolute:
		{
			const Revolute& data = m_constraintData.revolute;
			physx::PxRevoluteJoint* joint = (physx::PxRevoluteJoint*)m_impl;
			joint->setLimit(physx::PxJointAngularLimitPair(data.minAngle, data.maxAngle, physx::PxSpring(data.spring.stiffness, data.spring.damping)));
			//joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, true);
			joint->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, true);
			break;
		}
	};

	// Common data.
	physx::PxJoint* joint = (physx::PxJoint*)m_impl;
	
	joint->setActors(
		GetActorFromComponent(m_components[0]),
		GetActorFromComponent(m_components[1])
		);
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, Mat4ToPxTransform(SwapXZ(m_componentFrames[0])));
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR1, Mat4ToPxTransform(SwapXZ(m_componentFrames[1])));

	joint->setBreakForce(m_breakForce, m_breakTorque);

	wakeComponents();
}

void PhysicsConstraint::wakeComponents()
{
	for (int i = 0; i < 2; ++i)
	{
		if (m_components[i])
		{
			m_components[i]->forceWake();
		}
	}
}

} // namespace frm
