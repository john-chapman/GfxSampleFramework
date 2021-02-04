#include "PhysicsConstraint.h"
#include "Physics.h"
#include "PhysicsInternal.h"

#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/Serializer.h>
#include <frm/core/world/World.h>
#include <frm/core/world/WorldEditor.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

static const char* kTypeStr[PhysicsConstraint::Type_Count]
{
	"Distance", //Type_Distance,
	"Sphere",   //Type_Sphere,
	"Revolute"  //Type_Revolute
};

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

void PhysicsConstraint::setNode(int _i, SceneNode* _node)
{
	FRM_STRICT_ASSERT(_i < 2);

	if (m_nodes[_i].isResolved())
	{
		m_nodes[_i]->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
		m_nodes[_i] = GlobalNodeReference();
	}

	if (_node)
	{
		m_nodes[_i] = m_parentNode->getParentScene()->findGlobal(_node);
		FRM_ASSERT(m_nodes[_i].isResolved());
		if (m_nodes[_i].isResolved())
		{
			_node->registerCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
			m_components[_i] = (PhysicsComponent*)m_nodes[_i]->findComponent(StringHash("PhysicsComponent"));
			FRM_ASSERT(m_components[_i] != nullptr);
		}
	}
}

bool PhysicsConstraint::isBroken() const
{
	if (!m_impl)
	{
		return true;
	}

	physx::PxJoint* joint = (physx::PxJoint*)m_impl;
	return joint->getConstraintFlags().isSet(physx::PxConstraintFlag::eBROKEN);
}

void PhysicsConstraint::setBroken(bool _broken)
{
	if (!m_impl)
	{
		return;
	}

	physx::PxJoint* joint = (physx::PxJoint*)m_impl;
	const bool brokenState = joint->getConstraintFlags().isSet(physx::PxConstraintFlag::eBROKEN);
	
	if (_broken == brokenState)
	{
		return;
	}
	else if (brokenState)
	{
		// Joint broken, unbreak.
		FRM_ASSERT(false); // \todo Need to fully re-init the joint in this case.
	}
	else
	{
		// Joint not broken, force break.
		joint->setBreakForce(0.0f, 0.0f);
		wakeComponents();
	}
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
		pxJoint->setLocalPose((_i == 0) ? physx::PxJointActorIndex::eACTOR0 : physx::PxJointActorIndex::eACTOR1, Mat4ToPxTransform(_frame));
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

void PhysicsConstraint::setFlags(Flags _flags)
{
	if (_flags == m_flags)
	{
		return;
	}

	m_flags = _flags;
	if (m_impl)
	{
		physx::PxJoint* joint = (physx::PxJoint*)m_impl;
		joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, m_flags.get(Flag::CollisionsEnabled));
		joint->setConstraintFlag(physx::PxConstraintFlag::eBROKEN, m_flags.get(Flag::StartBroken));
	}
}

void PhysicsConstraint::setFlag(Flag _flag, bool _value)
{
	Flags newFlags = m_flags;
	newFlags.set(_flag, _value);
	setFlags(newFlags);
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

	ret |= ImGui::DragFloat("Stiffness", &spring.stiffness, 1.0f, -1.0f);
	ImGui::SameLine();
	ImGui::Text(spring.stiffness < 0.0f ? "(Inactive)" : "");
	
	ret |= ImGui::DragFloat("Damping", &spring.damping, 0.1f);
	ImGui::SameLine();
	if (spring.damping <= 0.0f)
	{
		ImGui::Text("(Undamped)");
	}
	else if (spring.damping < 1.0f)
	{
		ImGui::Text("(Under-damped)");
	}
	else if (spring.damping == 1.0f)
	{
		ImGui::Text("(Critically-damped)");
	}
	else if (spring.damping > 1.0f)
	{
		ImGui::Text("(Over-damped)");
	}
	else
	{
		ImGui::Text("");
	}

	return ret;
}


bool PhysicsConstraint::initImpl()
{
	return true;
}

bool PhysicsConstraint::postInitImpl()
{
	if (m_type == Type_Invalid)
	{
		return true;
	}

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

		if (m_nodes[i].isResolved())
		{
			m_nodes[i]->registerCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
		}
	}

	setImplData(m_type);

	return true;
}

void PhysicsConstraint::shutdownImpl()
{
	if (m_impl)
	{
		((physx::PxJoint*)m_impl)->release();
	}

	for (int i = 0; i < 2; ++i)
	{
		m_components[i] = nullptr;
		if (m_nodes[i].isResolved())
		{
			m_nodes[i]->unregisterCallback(SceneNode::Event::OnShutdown, &OnNodeShutdown, this);
			m_nodes[i].node = nullptr;
		}
	}
}

bool PhysicsConstraint::editImpl()
{
	bool ret = false;

	WorldEditor* worldEditor = WorldEditor::GetCurrent();

	const bool brokenState = isBroken();
	if (ImGui::Button(brokenState ? "Unbreak" : "Break"))
	{
		setBroken(!brokenState);
	}

	ImGui::Spacing();

	bool flagCollisionEnabled = m_flags.get(Flag::CollisionsEnabled);
	bool flagStartBroken = m_flags.get(Flag::StartBroken);
	
	ret |= ImGui::Checkbox("Collisions Enabled", &flagCollisionEnabled);
	ret |= ImGui::Checkbox("Start Broken", &flagStartBroken);
	
	m_flags.set(Flag::CollisionsEnabled, flagCollisionEnabled);
	m_flags.set(Flag::StartBroken, flagStartBroken);

	ImGui::Spacing();

	for (int i = 0; i < 2; ++i)
	{
		ImGui::PushID(i);
		if (ImGui::Button(String<32>("Node %d", i).c_str()))
		{
			worldEditor->beginSelectNode();
		}
		GlobalNodeReference newNodeRef = worldEditor->selectNode(m_nodes[i], m_parentNode->getParentScene());
		if (newNodeRef != m_nodes[i])
		{
			setNode(i, newNodeRef.node);
			FRM_ASSERT(m_nodes[i] == newNodeRef);
			ret = true;
		}

		if (m_nodes[i].isResolved())
		{
			ImGui::SameLine();
			ImGui::Text(m_nodes[i]->getName());
		}

		ImGui::PopID();
	}

	ImGui::Spacing();
	
	Type newType = m_type;
	if (ImGui::Combo("Type", &newType, kTypeStr, Type_Count))
	{
		ret |= newType != m_type;
	}

	switch (m_type)
	{
		default:
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
			ret |= ImGui::SliderAngle("Min Angle", &m_constraintData.revolute.minAngle,  0.0f, 360.0f);
			ret |= ImGui::SliderAngle("Max Angle", &m_constraintData.revolute.maxAngle,  0.0f, 360.0f);

			ret |= editSpring(m_constraintData.revolute.spring);

			break;
		}
	};

	ImGui::Spacing();
	ret |= ImGui::DragFloat("Break Force", &m_breakForce, 0.1f, 0.0f);
	ret |= ImGui::DragFloat("Break Torque", &m_breakTorque, 0.1f, 0.0f);

	if (ImGui::TreeNode("Edit Frames"))
	{
		static int editFrame = 0;
		ImGui::RadioButton("A", &editFrame, 0);
		ImGui::SameLine();
		ImGui::RadioButton("B", &editFrame, 1);

		mat4 toWorld = identity;
		if (m_nodes[editFrame].isResolved())
		{
			toWorld = m_nodes[editFrame]->getWorld();
		}
		else if (m_components[editFrame])
		{
			toWorld = m_components[editFrame]->getParentNode()->getWorld();
		}

		mat4& frame = toWorld * m_componentFrames[editFrame];
		Im3d::GetContext().m_gizmoLocal = true;
		if (Im3d::Gizmo(Im3d::MakeId(this), (float*)&frame))
		{
			setComponentFrame(editFrame, AffineInverse(toWorld) * frame);
		}
		Im3d::GetContext().m_gizmoLocal = false;

		ImGui::TreePop();
	}

	if (ret)
	{
		setImplData(newType);
	}

	draw();

	return ret;
}

bool PhysicsConstraint::serializeImpl(Serializer& _serializer_) 
{
	bool ret = SerializeAndValidateClass(_serializer_);

	ret |= SerializeEnum<Type, Type_Count>(_serializer_, m_type, kTypeStr, "Type");

	switch (m_type)
	{
		default:
		{
			FRM_LOG_ERR("PhysicsGeometry::serialize -- Invalid type (%d)", m_type);
			ret = false;
			break;
		}
		case Type_Distance:
		{
			Serialize(_serializer_, m_constraintData.distance.minDistance, "minDistance");
			Serialize(_serializer_, m_constraintData.distance.maxDistance, "maxDistance");
			Serialize(_serializer_, m_constraintData.distance.spring.stiffness, "stiffness");
			Serialize(_serializer_, m_constraintData.distance.spring.damping, "damping");

			break;
		}
		case Type_Sphere:
		{
			Serialize(_serializer_, m_constraintData.sphere.cone.angleX, "angleX");
			Serialize(_serializer_, m_constraintData.sphere.cone.angleY, "angleY");
			Serialize(_serializer_, m_constraintData.sphere.spring.stiffness, "stiffness");
			Serialize(_serializer_, m_constraintData.sphere.spring.damping, "damping");
			break;
		}
		case Type_Revolute:
		{
			Serialize(_serializer_, m_constraintData.revolute.minAngle, "minAngle");
			Serialize(_serializer_, m_constraintData.revolute.maxAngle, "maxAngle");
			Serialize(_serializer_, m_constraintData.revolute.spring.stiffness, "stiffness");
			Serialize(_serializer_, m_constraintData.revolute.spring.damping, "damping");
			break;
		}
	};

	if (_serializer_.beginArray("m_nodes"))
	{
		ret &= m_nodes[0].serialize(_serializer_);
		ret &= m_nodes[1].serialize(_serializer_);

		_serializer_.endArray();
	}

	if (_serializer_.beginArray("m_frames"))
	{
		Serialize(_serializer_, m_componentFrames[0]);
		Serialize(_serializer_, m_componentFrames[1]);

		_serializer_.endArray();
	}

	Serialize(_serializer_, m_breakForce, "m_breakForce");
	Serialize(_serializer_, m_breakTorque, "m_breakTorque");

	return ret;
}

void PhysicsConstraint::setImplData(Type _newType)
{
	if (_newType == Type_Invalid)
	{
		return;
	}

	if (!m_impl || _newType != m_type)
	{
		if (m_impl)
		{
			((physx::PxJoint*)m_impl)->release();
		}
		m_type = _newType;

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
				m_constraintData.distance.spring.stiffness = 100.0f;
				m_constraintData.distance.spring.damping = 1.0f;
				m_constraintData.distance.minDistance = 0.0f;
				m_constraintData.distance.maxDistance = 1.0f;
				break;
			}
			case Type_Sphere:
			{
				m_impl = physx::PxSphericalJointCreate(*g_pxPhysics, actorA, Mat4ToPxTransform(m_componentFrames[0]), actorB, Mat4ToPxTransform(m_componentFrames[1]));
				m_constraintData.sphere.spring.stiffness = 100.0f;
				m_constraintData.sphere.spring.damping = 1.0f;
				m_constraintData.sphere.cone.angleX = Radians(45.0f);
				m_constraintData.sphere.cone.angleY = Radians(45.0f);
				break;
			}
			case Type_Revolute:
			{
				m_impl = physx::PxRevoluteJointCreate(*g_pxPhysics, actorA, Mat4ToPxTransform(m_componentFrames[0]), actorB, Mat4ToPxTransform(m_componentFrames[1]));

				m_constraintData.revolute.spring.stiffness = 100.0f;
				m_constraintData.revolute.spring.damping = 1.0f;
				m_constraintData.revolute.minAngle = Radians(0.0f);
				m_constraintData.revolute.maxAngle = Radians(360.0f);
				break;
			}
		};

		if (!m_impl)
		{
			return;
		}
	}

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
			joint->setSphericalJointFlag(physx::PxSphericalJointFlag::eLIMIT_ENABLED, true);
			break;
		}
		case Type_Revolute:
		{
			const Revolute& data = m_constraintData.revolute;
			physx::PxRevoluteJoint* joint = (physx::PxRevoluteJoint*)m_impl;
			joint->setLimit(physx::PxJointAngularLimitPair(data.minAngle, data.maxAngle, physx::PxSpring(data.spring.stiffness, data.spring.damping)));
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
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, Mat4ToPxTransform(m_componentFrames[0]));
	joint->setLocalPose(physx::PxJointActorIndex::eACTOR1, Mat4ToPxTransform(m_componentFrames[1]));

	joint->setBreakForce(m_breakForce <= 0.0f ? FLT_MAX : m_breakForce, m_breakTorque <= 0.0f ? FLT_MAX : m_breakTorque);

	joint->setConstraintFlag(physx::PxConstraintFlag::eVISUALIZATION, true);
	Flags newFlags = m_flags;
	m_flags = 0;
	setFlags(newFlags);

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

void PhysicsConstraint::draw()
{
	Im3d::PushId(this);

	mat4 worldFrames[2] = { m_componentFrames[0], m_componentFrames[1] };
	for (int i = 0; i < 2; ++i)
	{
		if (m_nodes[i].isResolved())
		{
			worldFrames[i] = m_nodes[i]->getWorld() * worldFrames[i];
		}
		else if (m_components[i])
		{
			worldFrames[i] = m_components[i]->getParentNode()->getWorld() * worldFrames[i];
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

} // namespace frm
