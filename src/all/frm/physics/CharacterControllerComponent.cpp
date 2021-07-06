#include "CharacterControllerComponent.h"
#include "Physics.h"
#include "PhysicsInternal.h"

#include <frm/core/Input.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

FRM_COMPONENT_DEFINE(CharacterControllerComponent, 0);

// PUBLIC

void CharacterControllerComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("CharacterControllerComponent::Update");

	if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		CharacterControllerComponent* component = (CharacterControllerComponent*)*_from;
		component->update(_dt);
	}
}

// PRIVATE

void CharacterControllerComponent::update(float _dt)
{
	if (!m_impl)
	{
		return;
	}

	Keyboard* keyboard = Input::GetKeyboard();
	Mouse* mouse = Input::GetMouse();
	if (mouse->isDown(Mouse::Button_Right))
	{
		m_heading += mouse->getAxisState(Mouse::Axis_X) * 0.003f; 
	}
	//m_heading = Fract(Abs(m_heading));

	const float speed = 5.0f;

	float forward = 0.0f;
	if (keyboard->isDown(Keyboard::Key_W))
	{
		forward += speed * _dt;
	}
	if (keyboard->isDown(Keyboard::Key_S))
	{
		forward -= speed * _dt;
	}

	float strafe = 0.0f;
	if (keyboard->isDown(Keyboard::Key_D))
	{
		strafe += speed * _dt;
	}
	if (keyboard->isDown(Keyboard::Key_A))
	{
		strafe -= speed * _dt;
	}

	float theta = m_heading * kTwoPi;
	vec3 headingVector = vec3(cosf(theta - kHalfPi), 0.0f, sinf(theta - kHalfPi));
	vec3 strafeVector  = vec3(cosf(theta), 0.0f, sinf(theta));
	vec3 disp = headingVector * forward + strafeVector * strafe + vec3(0.0f, -10.0f, 0.0f) * _dt;

	physx::PxController* controller = (physx::PxController*)m_impl;
	controller->move(Vec3ToPx(disp), 1e-4f, _dt, physx::PxControllerFilters());

	vec3 worldPosition = PxToVec3(physx::toVec3(controller->getPosition()));
	quat worldOrientation = RotationQuaternion(vec3(0.0f, 1.0f, 0.0f), -theta);

	getParentNode()->setWorld(TransformationMatrix(worldPosition, worldOrientation));

	Im3d::Text(worldPosition, 1.0f, Im3d::Color_Cyan, Im3d::TextFlags_Default, "Heading: %f", m_heading);
	Im3d::DrawLine(worldPosition, worldPosition + headingVector, 4.0f, Im3d::Color_Cyan);	
}

bool CharacterControllerComponent::initImpl()
{
	FRM_ASSERT(g_pxPhysics);
	PhysicsWorld* physicsWorld = Physics::GetCurrentWorld();
	FRM_ASSERT(physicsWorld);
	PhysicsWorld::Impl* px = physicsWorld->m_impl;

	FRM_ASSERT(!m_impl);
	
	physx::PxCapsuleControllerDesc desc;
	desc.radius = m_radius;
	desc.height = m_height;
	desc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
	desc.userData = this;
	desc.material = g_pxPhysics->createMaterial(0.5f, 0.5f, 0.2f);
	
	const physx::PxVec3 position = Vec3ToPx(m_parentNode->getPosition());
	desc.position = physx::PxExtendedVec3(position.x, position.y, position.z);

	physx::PxController* controller = px->pxControllerManager->createController(desc);
	desc.material->release();

	if (!controller)
	{
		return false;
	}

	m_impl = controller;

	return true;
}

bool CharacterControllerComponent::postInitImpl()
{
	return true;
}

void CharacterControllerComponent::shutdownImpl()
{
	if (m_impl)
	{
		((physx::PxController*)m_impl)->release();
		m_impl = nullptr;
	}
}

bool CharacterControllerComponent::editImpl()
{
	return false;
}

bool CharacterControllerComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	return true;
}


} // namespace frm