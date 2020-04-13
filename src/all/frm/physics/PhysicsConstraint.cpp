#include "PhysicsConstraint.h"

#include "PhysicsInternal.h"

#include <frm/core/memory.h>
#include <frm/core/interpolation.h>
#include <frm/core/Scene.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>

using namespace frm;

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

// PUBLIC

PhysicsConstraint* PhysicsConstraint::CreateDistance(
	Component_Physics* _componentA, const mat4& _frameA,
	Component_Physics* _componentB, const mat4& _frameB,
	const Distance& _data
)
{
	PhysicsConstraint* ret = FRM_NEW(PhysicsConstraint(_componentA, _frameA, _componentB, _frameB));
	
	ret->m_type = Type_Distance;
	ret->m_constraintData.distance = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

PhysicsConstraint* PhysicsConstraint::CreateSphere(
	Component_Physics* _componentA, const mat4& _frameA,
	Component_Physics* _componentB, const mat4& _frameB,
	const Sphere& _data
)
{
	PhysicsConstraint* ret = FRM_NEW(PhysicsConstraint(_componentA, _frameA, _componentB, _frameB));
	
	ret->m_type = Type_Sphere;
	ret->m_constraintData.sphere = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

PhysicsConstraint* PhysicsConstraint::CreateRevolute(
	Component_Physics* _componentA, const mat4& _frameA,
	Component_Physics* _componentB, const mat4& _frameB,
	const Revolute& _data
)
{
	PhysicsConstraint* ret = FRM_NEW(PhysicsConstraint(_componentA, _frameA, _componentB, _frameB));
	
	ret->m_type = Type_Sphere;
	ret->m_constraintData.revolute = _data;
	FRM_VERIFY(ret->initImpl());

	return ret;
}

void PhysicsConstraint::Destroy(PhysicsConstraint*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

void PhysicsConstraint::setComponent(int _i, Component_Physics* _component)
{
	FRM_STRICT_ASSERT(_i < 2);
	if (m_impl)
	{
		physx::PxJoint* pxJoint = (physx::PxJoint*)m_impl;
		physx::PxRigidActor* actors[2];
		pxJoint->getActors(actors[0], actors[1]);
		actors[_i] = _component ? _component->getImpl()->pxRigidActor : nullptr;
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

bool PhysicsConstraint::edit()
{
	if (!m_impl)
	{
		return false;
	}

	draw();

	ImGui::PushID(this);

	bool ret = false;

	if (ImGui::Combo("Type", &m_type, kTypeStr, Type_Count))
	{
		ret = true;
		initImpl();
	}

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

	if (ImGui::TreeNode("Edit Frames"))
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
	}

	if (ret)
	{
		setImplData();
	}

	ImGui::PopID();

	return ret;
}

void PhysicsConstraint::draw()
{
	Im3d::PushId(this);

	mat4 worldFrames[2] = { m_componentFrames[0], m_componentFrames[1] };
	for (int i = 0; i < 2; ++i)
	{
		if (m_components[i] && m_components[i]->getNode())
		{
			worldFrames[i] = m_components[i]->getNode()->getWorldMatrix() * worldFrames[i];
		}
		Im3d::PushMatrix(worldFrames[i]);
			Im3d::Scale(.25f, .25f, .25f);
			Im3d::PushSize(3.0f);
				Im3d::DrawXyzAxes();
			Im3d::PopSize();
		Im3d::PopMatrix();
	}
	Im3d::PushAlpha(0.7f);
		Im3d::DrawPoint(GetTranslation(worldFrames[0]), 12.0f, Im3d::Color_Cyan);
		Im3d::DrawPoint(GetTranslation(worldFrames[1]), 12.0f, Im3d::Color_Magenta);
	Im3d::PopAlpha();

	switch (m_type)
	{
		default:
			break;
		case Type_Distance:
		{
			const vec3 lineStart = GetTranslation(worldFrames[0]);
			const vec3 lineEnd   = GetTranslation(worldFrames[1]);
			const float length = Length(lineEnd - lineStart);
			Im3d::Color color = Im3d::Color_Yellow; 
			Im3d::DrawLine(lineStart, lineEnd, 3.0f, color);
			break;
		}
		case Type_Sphere:
		{
			break;
		}
		case Type_Revolute:
		{
			break;
		}
	};

	Im3d::PopId();
}

// PRIVATE


PhysicsConstraint::PhysicsConstraint(
	Component_Physics* _componentA, const mat4& _frameA,
	Component_Physics* _componentB, const mat4& _frameB
	)
{
	m_components[0] = _componentA;
	m_components[1] = _componentB;
	m_componentFrames[0] = _frameA;
	m_componentFrames[1] = _frameB;
}

PhysicsConstraint::~PhysicsConstraint()
{
	shutdownImpl();
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
	shutdownImpl();
	
	FRM_ASSERT(g_pxPhysics);

	physx::PxRigidActor* actorA = m_components[0] ? m_components[0]->getImpl()->pxRigidActor : nullptr;
	physx::PxRigidActor* actorB = m_components[1] ? m_components[1]->getImpl()->pxRigidActor : nullptr;
	
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

	return true;
}

void PhysicsConstraint::setImplData()
{
	FRM_ASSERT(m_impl);

 // type-specific data
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

 // common data
	physx::PxJoint* joint = (physx::PxJoint*)m_impl;
	
	joint->setActors(
		m_components[0] ? m_components[0]->getImpl()->pxRigidActor : nullptr,
		m_components[1] ? m_components[1]->getImpl()->pxRigidActor : nullptr
		);
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, Mat4ToPxTransform(SwapXZ(m_componentFrames[0])));
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR1, Mat4ToPxTransform(SwapXZ(m_componentFrames[1])));

	joint->setBreakForce(m_breakForce, m_breakTorque);

	wakeComponents();
}

void PhysicsConstraint::shutdownImpl()
{
	if (m_impl)
	{
		((physx::PxJoint*)m_impl)->release();
	}
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