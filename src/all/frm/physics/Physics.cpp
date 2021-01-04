#include "Physics.h"
#include "PhysicsInternal.h"
#include "PhysicsMaterial.h"
#include "PhysicsGeometry.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/BasicRenderer/BasicRenderableComponent.h> // used by PhysicsComponentTemp to manage fadeout
#include <frm/core/Properties.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/Serializer.h>
#include <frm/core/Time.h>
#include <frm/core/world/World.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>


namespace frm {


/******************************************************************************

									Physics

******************************************************************************/

static const char* kPhysicsFlagStr[] =
{
	"Static",
	"Kinematic",
	"Dynamic",
	"Simulation",
	"Query",
	"DisableGravity",
	"EnableCCD",
};

static PhysicsMaterial* s_defaultMaterial;

// PUBLIC

bool Physics::Init()
{
	FRM_AUTOTIMER("#Physics::Init");

	FRM_ASSERT(!s_instance);
	s_instance = FRM_NEW(Physics);

	PxSettings pxSettings;
	pxSettings.gravity         = s_instance->m_gravity * s_instance->m_gravityDirection;
	pxSettings.toleranceLength = 1.0f;
	pxSettings.toleranceSpeed  = s_instance->m_gravity;

	if (!PxInit(pxSettings))
	{
		return false;
	}

	return true;
}

void Physics::Shutdown()
{
	FRM_ASSERT(s_instance);

	if (s_defaultMaterial)
	{
		PhysicsMaterial::Release(s_defaultMaterial);
	}

	PxShutdown();

	FRM_DELETE(s_instance);
	s_instance = nullptr;
}

void Physics::Update(float _dt)
{
	PROFILER_MARKER_CPU("#Physics::Update");
	
	if (s_instance->m_paused)
	{
		if (s_instance->m_step)
		{
			s_instance->m_step = false;
			_dt = s_instance->m_stepLengthSeconds;
		}
		else
		{
			//s_instance->updateComponentTransforms(); // \todo Required? Component update should handle this.
			return;
		}
	}

	// Set kinematic transforms.
	for (PhysicsComponent* component : s_instance->m_kinematic)
	{
		PxComponentImpl* impl = (PxComponentImpl*)component->m_impl;
		FRM_ASSERT(impl);
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = component->getParentNode()->getWorld();
		actor->setKinematicTarget(Mat4ToPxTransform(worldMatrix));
	}

	// Step the simulation.
	float stepLengthSeconds = Min(s_instance->m_stepLengthSeconds, _dt);
	float stepCount = Floor((_dt + s_instance->m_stepAccumulator) / stepLengthSeconds);
	s_instance->m_stepAccumulator += _dt - (stepCount * stepLengthSeconds);
	for (int i = 0; i < (int)stepCount; ++i)
	{
		g_pxScene->simulate(stepLengthSeconds);
		FRM_VERIFY(g_pxScene->fetchResults(true)); // true = block until the results are ready
	}
}

void Physics::Edit()
{
	if (ImGui::Button(s_instance->m_paused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause"))
	{
		s_instance->m_paused = !s_instance->m_paused;
	}

	if (s_instance->m_paused)
	{
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_STEP_FORWARD " Step"))
		{
		#if 0
			// this doesn't work well because calling Update() here sets the world matrix, which subsequently gets overwritten during the scene update
			s_instance->m_paused = false;
			s_instance->m_stepAccumulator = 0.0f;
			Update(s_instance->m_stepLengthSeconds);
			s_instance->m_paused = true;
		#else
			s_instance->m_step = true;
		#endif
		}
	}

	if (ImGui::Button("Reset"))
	{
		for (PhysicsComponent* component : s_instance->m_kinematic)
		{
			component->reset();
		}

		for (PhysicsComponent* component : s_instance->m_dynamic)
		{
			component->reset();
		}
	}
	
	if (ImGui::SliderFloat("Gravity", &s_instance->m_gravity, 0.0f, 30.0f))
	{
		vec3 g = s_instance->m_gravityDirection * s_instance->m_gravity;
		g_pxScene->setGravity(Vec3ToPx(g));
	}

	bool& debugDraw = s_instance->m_drawDebug;
	if (ImGui::Checkbox("Debug Draw", &debugDraw))
	{
		if (debugDraw)
		{
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE,            1.0f);

			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
			//g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_AABBS,  1.0f);
			//g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES,        1.0f);
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL,     1.0f);
			//g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 0.5f);
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LIMITS,       1.0f);
		}
		else
		{
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 0.0f);
		}
	}

	ImGui::Spacing();
	if (ImGui::TreeNode("Stats"))
	{
		physx::PxSimulationStatistics stats;
		g_pxScene->getSimulationStatistics(stats);

		ImGui::Text("Static Bodies:      %u",             stats.nbStaticBodies);
		ImGui::Text("Dynamic Bodies:     %u (%u active)", stats.nbDynamicBodies,   stats.nbActiveDynamicBodies);
		ImGui::Text("Kinematic Bodies:   %u (%u active)", stats.nbKinematicBodies, stats.nbActiveKinematicBodies);
		ImGui::Text("Active Constraints: %u ",            stats.nbActiveConstraints);

		ImGui::Spacing();
		
		ImGui::Text("Broad Phase:");
		ImGui::Text("\tAdds:    %u", stats.getNbBroadPhaseAdds());
		ImGui::Text("\tRemoves: %u", stats.getNbBroadPhaseRemoves());

		ImGui::TreePop();
	}
}

void Physics::DrawDebug()
{
	if (!s_instance->m_drawDebug)
	{
		return;
	}
	
	// \todo This seems to have a 1 frame latency, calling DrawDebug() before/after the update doesn't seem to have any effect.
	const physx::PxRenderBuffer& drawList = g_pxScene->getRenderBuffer();

	Im3d::PushDrawState();

	Im3d::BeginTriangles();
		for (auto i = 0u; i < drawList.getNbTriangles(); ++i)
		{
			const physx::PxDebugTriangle& tri = drawList.getTriangles()[i];
			Im3d::Vertex(PxToVec3(tri.pos0), Im3d::Color(tri.color0));
			Im3d::Vertex(PxToVec3(tri.pos1), Im3d::Color(tri.color1));
			Im3d::Vertex(PxToVec3(tri.pos2), Im3d::Color(tri.color2));				
		}
	Im3d::End();

	Im3d::SetSize(2.0f); // \todo parameterize
	Im3d::BeginLines();
		for (auto i = 0u; i < drawList.getNbLines(); ++i)
		{
			const physx::PxDebugLine& line = drawList.getLines()[i];
			Im3d::Vertex(PxToVec3(line.pos0), Im3d::Color(line.color0));
			Im3d::Vertex(PxToVec3(line.pos1), Im3d::Color(line.color1));
		}
	Im3d::End();

	Im3d::BeginPoints();
		for (auto i = 0u; i < drawList.getNbPoints(); ++i)
		{
			const physx::PxDebugPoint& point = drawList.getPoints()[i];
			Im3d::Vertex(PxToVec3(point.pos), Im3d::Color(point.color));
		}
	Im3d::End();

	for (physx::PxU32 i = 0; i < drawList.getNbTexts(); ++i)
	{
	 // \todo
	}

	Im3d::PopDrawState();
}

void Physics::RegisterComponent(PhysicsComponent* _component_)
{
	PROFILER_MARKER_CPU("#Physics::RegisterComponent");

	PxComponentImpl* impl = (PxComponentImpl*)_component_->m_impl;
	FRM_ASSERT(impl);
	g_pxScene->addActor(*impl->pxRigidActor);

	Flags flags = _component_->getFlags();
	if (flags.get(Flag::Dynamic))
	{
		s_instance->m_dynamic.push_back(_component_);
	}
	else if (flags.get(Flag::Kinematic))
	{
		s_instance->m_kinematic.push_back(_component_);
	}
	else
	{
		s_instance->m_static.push_back(_component_);
	}
}

void Physics::UnregisterComponent(PhysicsComponent* _component_)
{
	PROFILER_MARKER_CPU("#Physics::UnregisterComponent");

	PxComponentImpl* impl = (PxComponentImpl*)_component_->m_impl;
	FRM_ASSERT(impl);
	g_pxScene->removeActor(*impl->pxRigidActor);

	Flags flags = _component_->getFlags();
	if (flags.get(Flag::Dynamic))
	{
		s_instance->m_dynamic.erase_first_unsorted(_component_);
	}
	else if (flags.get(Flag::Kinematic))
	{
		s_instance->m_kinematic.erase_first_unsorted(_component_);
	}
	else
	{
		s_instance->m_static.erase_first_unsorted(_component_);
	}
}

const PhysicsMaterial* Physics::GetDefaultMaterial()
{
	if (!s_defaultMaterial)
	{
		s_defaultMaterial = PhysicsMaterial::Create(0.5f, 0.5f, 0.2f, "#PhysicsDefaultMaterial");
		PhysicsMaterial::Use(s_defaultMaterial);
	}
	return s_defaultMaterial;
}

void Physics::AddGroundPlane(const PhysicsMaterial* _material)
{
	static SceneNode* groundNode;
	if (!groundNode)
	{
		Scene* rootScene = World::GetCurrent()->getRootScene();
		groundNode = rootScene->createTransientNode("#GroundPlane");

		PhysicsComponent* physicsComponent = PhysicsComponent::CreateTransient(
			PhysicsGeometry::CreatePlane(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f), "#GroundPlaneGeometry"),
			_material,
			1.0f,
			identity,
			{ Flag::Static, Flag::Simulation, Flag::Query}
			);			
		groundNode->addComponent(physicsComponent);

		FRM_VERIFY(groundNode->init());
		FRM_VERIFY(groundNode->postInit());
	}
}

bool Physics::RayCast(const RayCastIn& _in, RayCastOut& out_, RayCastFlags _flags)
{
	PROFILER_MARKER_CPU("#Physics::RayCast");

	physx::PxRaycastBuffer queryResult;
	physx::PxHitFlags flags = (physx::PxHitFlags)0
		| (_flags.get(RayCastFlag::Position) ? physx::PxHitFlag::ePOSITION : (physx::PxHitFlags)0)
		| (_flags.get(RayCastFlag::Normal)   ? physx::PxHitFlag::eNORMAL   : (physx::PxHitFlags)0)
		| (_flags.get(RayCastFlag::AnyHit)   ? physx::PxHitFlag::eMESH_ANY : (physx::PxHitFlags)0)
		;

	if (!g_pxScene->raycast(Vec3ToPx(_in.origin), Vec3ToPx(_in.direction), _in.maxDistance, queryResult) || !queryResult.hasBlock)
	{
		return false;
	}

	out_.position  = PxToVec3(queryResult.block.position);
	out_.normal    = PxToVec3(queryResult.block.normal);
	out_.distance  = queryResult.block.distance;
	FRM_ASSERT(false);//out_.component = (Component_Physics*)queryResult.block.actor->userData;

	return true;
}

// PRIVATE

Physics* Physics::s_instance;

Physics::Physics()
{
	Properties::PushGroup("Physics");

		Properties::Add("m_drawDebug",          m_drawDebug,                                       &m_drawDebug);
		Properties::Add("m_paused",             m_paused,                                          &m_paused);
		Properties::Add("m_stepLengthSeconds",  m_stepLengthSeconds,     0.0f,       1.0f,         &m_stepLengthSeconds);
		Properties::Add("m_gravity",            m_gravity,               0.0f,       20.0f,        &m_gravity);
		Properties::Add("m_gravityDirection",   m_gravityDirection,      vec3(0.0f), vec3(20.0f),  &m_gravityDirection);

	Properties::PopGroup();

	m_gravityDirection = Normalize(m_gravityDirection);
}

Physics::~Physics()
{
	Properties::InvalidateGroup("Physics");
}

void Physics::updateComponentTransforms()
{
	// \todo Consider the 'active actors' api, can skip updating nodes which haven't moved: https://gameworksdocs.nvidia.com/PhysX/4.0/documentation/PhysXGuide/Manual/RigidBodyDynamics.html#active-actors.
	// Given that we're already updating only components with the 'dynamic' flag set, this is likely to be more efficient only if there are many thousands of actors.
	for (PhysicsComponent* component : m_dynamic)
	{
		PxComponentImpl* impl = (PxComponentImpl*)component->m_impl;
		FRM_ASSERT(impl);
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = PxToMat4(actor->getGlobalPose());
		component->getParentNode()->setWorld(worldMatrix);
	}
}

/******************************************************************************

								PhysicsComponent

******************************************************************************/

FRM_COMPONENT_DEFINE(PhysicsComponent, 0);

// PUBLIC

void PhysicsComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("PhysicsComponent::Update");

	if (_phase != World::UpdatePhase::PostPhysics)
	{
		return;
	}

	// \todo Consider the 'active actors' api, can skip updating nodes which haven't moved: https://gameworksdocs.nvidia.com/PhysX/4.0/documentation/PhysXGuide/Manual/RigidBodyDynamics.html#active-actors.
	// Given that we're already updating only components with the 'dynamic' flag set, this is likely to be more efficient only if there are many thousands of actors.
	for (; _from != _to; ++_from)
	{
		PhysicsComponent* component = (PhysicsComponent*)*_from;
		if (!component->getFlag(Flag::Dynamic))
		{
			continue;
		}
		
		PxComponentImpl* impl = (PxComponentImpl*)component->m_impl;
		FRM_ASSERT(impl);
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = PxToMat4(actor->getGlobalPose());
		component->getParentNode()->setWorld(worldMatrix);
	}
}

eastl::span<PhysicsComponent*> PhysicsComponent::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("PhysicsComponent"));
	return eastl::span<PhysicsComponent*>(*((eastl::vector<PhysicsComponent*>*)&activeList));
}

PhysicsComponent* PhysicsComponent::CreateTransient(const PhysicsGeometry* _geometry, const PhysicsMaterial* _material, float _mass, const mat4& _initialTransform, Flags _flags)
{
	PhysicsComponent* ret   = (PhysicsComponent*)Create(StringHash("PhysicsComponent"));
	ret->m_geometry         = _geometry;
	ret->m_material         = _material;
	ret->m_mass             = _mass;
	ret->m_initialTransform = _initialTransform;
	ret->m_flags            = _flags;

	return ret;
}

void PhysicsComponent::setFlags(Flags _flags)
{
	if (_flags == m_flags)
	{
		return;
	}

	// \todo Fast path for mutable flags. Probably need a diff method on BitFlags?
	if (getState() == World::State::Shutdown)
	{
		m_flags = _flags;
	}
	else
	{
		shutdownImpl();
		m_flags = _flags;
		initImpl();
	}
}

void PhysicsComponent::setFlag(Flag _flag, bool _value)
{
	Flags newFlags = m_flags;
	newFlags.set(_flag, _value);
	setFlags(newFlags);
}

void PhysicsComponent::addForce(const vec3& _force)
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	pxRigidDynamic->addForce(Vec3ToPx(_force));
}

void PhysicsComponent::setLinearVelocity(const vec3& _linearVelocity)
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	pxRigidDynamic->setLinearVelocity(Vec3ToPx(_linearVelocity));
}

vec3 PhysicsComponent::getLinearVelocity() const
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return vec3(0.0f);
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	return PxToVec3(pxRigidDynamic->getLinearVelocity());
}

void PhysicsComponent::setAngularVelocity(const vec3& _angularVelocity)
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	pxRigidDynamic->setAngularVelocity(Vec3ToPx(_angularVelocity));
}

vec3 PhysicsComponent::getAngularVelocity() const
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return vec3(0.0f);
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	return PxToVec3(pxRigidDynamic->getAngularVelocity());
}

void PhysicsComponent::setMass(float _mass)
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return;
	}
	
	_mass = Max(1e-7f, _mass);
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	physx::PxRigidBodyExt::updateMassAndInertia(*pxRigidDynamic, _mass);
	m_mass = _mass;
}

void PhysicsComponent::reset()
{
	if (!m_impl || !m_flags.get(Physics::Flag::Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	pxRigidDynamic->setGlobalPose(Mat4ToPxTransform(m_initialTransform));
	pxRigidDynamic->setLinearVelocity(Vec3ToPx(vec3(0.0f)));
	pxRigidDynamic->setAngularVelocity(Vec3ToPx(vec3(0.0f)));
}

void PhysicsComponent::forceUpdateNodeTransform()
{
	if (!m_impl)
	{
		return;
	}

	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
	FRM_ASSERT(pxRigidDynamic->is<physx::PxRigidDynamic>());
	mat4 worldMatrix = PxToMat4(pxRigidDynamic->getGlobalPose());
	m_parentNode->setWorld(worldMatrix);
}


// PROTECTED

bool PhysicsComponent::initImpl()
{
	PxComponentImpl*& impl = (PxComponentImpl*&)m_impl;

	if (!impl)
	{
		impl = g_pxComponentPool.alloc();
	}

	// \hack Assume identity means that the initial transform is uninitialized, in which case we copy from the parent node.
	if ((mat4)identity == m_initialTransform)
	{
		m_initialTransform = m_parentNode->getLocal();
	}

	// PxRigidActor
	physx::PxRigidActor*& pxRigidActor = impl->pxRigidActor;
	if (m_flags.get(Flag::Dynamic) || m_flags.get(Flag::Kinematic))
	{
		physx::PxRigidDynamic* pxRigidDynamic = g_pxPhysics->createRigidDynamic(Mat4ToPxTransform(m_initialTransform));
		if (m_flags.get(Flag::Kinematic))
		{
			pxRigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		}
		if (m_flags.get(Flag::EnableCCD))
		{
			pxRigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, true);
		}

		physx::PxRigidBodyExt::updateMassAndInertia(*pxRigidDynamic, m_mass);
		pxRigidActor = pxRigidDynamic;

		if (m_flags.get(Flag::DisableGravity))
		{
			pxRigidActor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
		}

		forceUpdateNodeTransform();
	}
	else if (m_flags.get(Flag::Static))
	{
		pxRigidActor = g_pxPhysics->createRigidStatic(Mat4ToPxTransform(m_initialTransform));
	}
	pxRigidActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true); // \todo enable/disable for all actors when toggling debug draw?
	pxRigidActor->userData = this;

	// Ensure geometry/material aren't null.
	if (!m_geometry)
	{
		m_geometry = PhysicsGeometry::CreateBox(vec3(0.5f), "#PhysicsTempBox");
	}
	PhysicsGeometry::Use(m_geometry);

	if (!m_material)
	{
		m_material = Physics::GetDefaultMaterial();
	}
	PhysicsMaterial::Use(m_material);
	

	// PxShape
	physx::PxShape*& pxShape = impl->pxShape;
	const physx::PxGeometry& geometry = ((physx::PxGeometryHolder*)m_geometry->m_impl)->any();
	const physx::PxMaterial& material = *((physx::PxMaterial*)m_material->m_impl);
	pxShape = g_pxPhysics->createShape(geometry, material, true);

	// some geometry types require a local pose to be set at the shape level
	switch (geometry.getType())
	{
		default:
			break;
		case physx::PxGeometryType::ePLANE:
		{			
			const physx::PxPlane plane = physx::PxPlane(Vec3ToPx(m_geometry->m_data.plane.normal), m_geometry->m_data.plane.offset);
			pxShape->setLocalPose(physx::PxTransformFromPlaneEquation(plane));
			break;
		}
		case physx::PxGeometryType::eCAPSULE:
		{
			pxShape->setLocalPose(physx::PxTransform(QuatToPx(RotationQuaternion(vec3(0.0f, 0.0f, 1.0f), Radians(90.0f)))));
			break;
		}		
	};
	pxRigidActor->attachShape(*pxShape);

	Physics::RegisterComponent(this);

	return true;
}

void PhysicsComponent::shutdownImpl()
{
	Physics::UnregisterComponent(this);

	PxComponentImpl*& impl = (PxComponentImpl*&)m_impl;

	impl->pxRigidActor->detachShape(*impl->pxShape);
	impl->pxShape->release();
	impl->pxRigidActor->release();
	g_pxComponentPool.free(impl);
	impl = nullptr;

	PhysicsMaterial::Release(m_material);
	PhysicsGeometry::Release(m_geometry);
}

bool PhysicsComponent::editImpl()
{
	bool ret = false;

	PxComponentImpl* impl = (PxComponentImpl*)m_impl;

	ret |= editFlags();

	if (impl && m_flags.get(Flag::Dynamic) && ImGui::DragFloat("Mass", &m_mass))
	{
		setMass(m_mass);
		ret = true;
	}

	if (m_geometry)
	{
		ret |= const_cast<PhysicsGeometry*>(m_geometry)->edit();
	}

	if (m_material)
	{
		ret |= const_cast<PhysicsMaterial*>(m_material)->edit();
	}

	if (ImGui::TreeNode("Initial Transform"))
	{
		if (Im3d::Gizmo("InitialTransform", (float*)&m_initialTransform) && m_impl)
		{
			/*physx::PxRigidActor* actor = (physx::PxRigidActor*)impl->pxRigidActor;
			FRM_ASSERT(actor->is<physx::PxRigidActor>());
			actor->setGlobalPose(Mat4ToPxTransform(m_initialTransform));*/
			reset();
			ret = true;
		}

		if (ImGui::Button("Copy from node"))
		{
			m_initialTransform = m_parentNode->getLocal();
			physx::PxRigidActor* actor = (physx::PxRigidActor*)impl->pxRigidActor;
			FRM_ASSERT(actor->is<physx::PxRigidActor>());
			actor->setGlobalPose(Mat4ToPxTransform(m_initialTransform));
			ret = true;
		}

		vec3 position = GetTranslation(m_initialTransform);
		vec3 rotation = ToEulerXYZ(GetRotation(m_initialTransform));
		vec3 scale    = GetScale(m_initialTransform);
		ImGui::Text("Position: %.3f, %.3f, %.3f", position.x, position.y, position.z);
		ImGui::Text("Rotation: %.3f, %.3f, %.3f", Degrees(rotation.x), Degrees(position.y), Degrees(position.z));
		ImGui::Text("Scale:    %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
		ImGui::TreePop();
	}

	return ret;
}

bool PhysicsComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	Serialize(_serializer_, m_flags, kPhysicsFlagStr, "m_flags");
	Serialize(_serializer_, m_initialTransform,       "m_initialTransform");
	Serialize(_serializer_, m_mass,                   "m_mass");

	bool ret = true;
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		if (_serializer_.beginObject("Geometry"))
		{
			// serialize inline
			m_geometry = PhysicsGeometry::Create(_serializer_);
			_serializer_.endObject();
		}
		else
		{
			// serialize from file
			PathStr path;
			if (!Serialize(_serializer_, path, "Geometry"))
			{
				FRM_LOG_ERR("Component_Physics::serialize; missing geometry");
				return false;
			}
			m_geometry = PhysicsGeometry::Create(path.c_str());
		}
		ret &= m_geometry && m_geometry->getState() != PhysicsGeometry::State_Error;

		if (_serializer_.beginObject("Material"))
		{
			// serialize inline
			m_material = PhysicsMaterial::Create(_serializer_);
			_serializer_.endObject();
		}
		else
		{
			// serialize from file
			PathStr path;
			if (!Serialize(_serializer_, path, "Material"))
			{
				FRM_LOG_ERR("Component_Physics::serialize; missing material");
				return false;
			}
			m_material = PhysicsMaterial::Create(path.c_str());
		}
		ret &= m_material && m_material->getState() != PhysicsMaterial::State_Error;
	}
	else
	{
		if (m_geometry)
		{
			if (*m_geometry->getPath() == '\0')
			{
				// serialize inline
				_serializer_.beginObject("Geometry");
				const_cast<PhysicsGeometry*>(m_geometry)->serialize(_serializer_);
				_serializer_.endObject();
			}
			else
			{
				// serialize path
				PathStr path = m_geometry->getPath();
				Serialize(_serializer_, path, "Geometry");
			}
		}

		if (m_material)
		{
			if (*m_material->getPath() == '\0')
			{
				// serialize inline
				_serializer_.beginObject("Material");
				const_cast<PhysicsMaterial*>(m_material)->serialize(_serializer_);
				_serializer_.endObject();
			}
			else
			{
				// serialize path
				PathStr path = m_material->getPath();
				Serialize(_serializer_, path, "Material");
			}
		}
	}

	return ret;
}

bool PhysicsComponent::editFlags()
{
	bool ret = false;

	bool flagStatic         = m_flags.get(Flag::Static);
	bool flagKinematic      = m_flags.get(Flag::Kinematic);
	bool flagDynamic        = m_flags.get(Flag::Dynamic);
	bool flagSimulation     = m_flags.get(Flag::Simulation);
	bool flagQuery          = m_flags.get(Flag::Query);
	bool flagDisableGravity = m_flags.get(Flag::DisableGravity);
	bool flagEnableCCD      = m_flags.get(Flag::EnableCCD);

	if (ImGui::Checkbox("Static", &flagStatic))
	{
		flagKinematic = flagDynamic = false;
		ret = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Kinematic", &flagKinematic))
	{
		flagStatic = flagDynamic = false;
		ret = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Dynamic", &flagDynamic))
	{
		flagStatic = flagKinematic = false;
		ret = true;
	}
	
	ret |= ImGui::Checkbox("Simulation", &flagSimulation);
	ImGui::SameLine();
	ret |= ImGui::Checkbox("Query", &flagQuery);

	ret |= ImGui::Checkbox("Disable Gravity", &flagDisableGravity);
	ret |= ImGui::Checkbox("Enable CCD", &flagEnableCCD);

	if (ret)
	{
		Flags newFlags = m_flags;

		newFlags.set(Flag::Static,         flagStatic);
		newFlags.set(Flag::Kinematic,      flagKinematic);
		newFlags.set(Flag::Dynamic,        flagDynamic);
		newFlags.set(Flag::Simulation,     flagSimulation);
		newFlags.set(Flag::Query,          flagQuery);
		newFlags.set(Flag::DisableGravity, flagDisableGravity);
		newFlags.set(Flag::EnableCCD,      flagEnableCCD);

		setFlags(newFlags);
	}
	
	return ret;
}


/******************************************************************************

                             PhysicsComponentTemp

******************************************************************************/

FRM_COMPONENT_DEFINE(PhysicsComponentTemp, 0);

// PUBLIC

void PhysicsComponentTemp::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("PhysicsComponentTemp::Update");

	if (_phase != World::UpdatePhase::PostPhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		PhysicsComponentTemp* component = (PhysicsComponentTemp*)*_from;
		if (!component->getFlag(Flag::Dynamic))
		{
			continue;
		}
		
		PxComponentImpl* impl = (PxComponentImpl*)component->m_impl;
		FRM_ASSERT(impl);
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = PxToMat4(actor->getGlobalPose());
		component->getParentNode()->setWorld(worldMatrix);

		// \todo This may destroy the node and component, which will modify the list we're iterating on. Need to defer all deletions at the scene/world level.
		//component->updateTimer(_dt);
	}
}

eastl::span<PhysicsComponentTemp*> PhysicsComponentTemp::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("PhysicsComponentTemp"));
	return eastl::span<PhysicsComponentTemp*>(*((eastl::vector<PhysicsComponentTemp*>*)&activeList));
}

PhysicsComponentTemp* PhysicsComponentTemp::CreateTransient(const PhysicsGeometry* _geometry, const PhysicsMaterial* _material, float _mass, const mat4& _initialTransform, Flags _flags)
{
	PhysicsComponentTemp* ret = (PhysicsComponentTemp*)Create(StringHash("PhysicsComponentTemp"));
	ret->m_geometry           = _geometry;
	ret->m_material           = _material;
	ret->m_mass               = _mass;
	ret->m_initialTransform   = _initialTransform;
	ret->m_flags              = _flags;

	return ret;
}


// PRIVATE

bool PhysicsComponentTemp::postInitImpl()
{
	bool ret =PhysicsComponent::postInitImpl();
	m_basicRenderableComponent = (BasicRenderableComponent*)m_parentNode->findComponent(StringHash("BasicRenderableComponent"));

	if (m_flags.get(Flag::Static) || m_flags.get(Flag::Dynamic))
	{
		m_timer = m_idleTimeout; // Static and kinematic components begin to die immediately.
	}
	else
	{
		m_timer = 0.0f; // Dynamic components only die when idle, see updateTimer().
	}

	return ret;
}

bool PhysicsComponentTemp::editImpl()
{
	bool ret = PhysicsComponent::editImpl();
	ImGui::Spacing();
	ret |= ImGui::SliderFloat("Idle Timeout", &m_idleTimeout, 0.0f, 10.0f);
	ImGui::Text("Timer %f", m_timer);

	return ret;
}

bool PhysicsComponentTemp::serializeImpl(Serializer& _serializer_)
{
	FRM_ASSERT(false); // 
	return false;
}

void PhysicsComponentTemp::updateTimer(float _dt)
{
	if (m_timer > 0.0f)
	{
		if (m_basicRenderableComponent)
		{			
			float alpha = Clamp(m_timer / m_idleTimeout, 0.0f, 1.0f);
			m_basicRenderableComponent->setAlpha(alpha);
		}

		m_timer -= _dt;
		if (m_timer <= 0.0f)
		{
			m_parentNode->getParentScene()->destroyNode(m_parentNode);
		}
	}
	else
	{
		physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)((PxComponentImpl*)m_impl)->pxRigidActor;
		FRM_ASSERT(pxRigidDynamic->is<physx::PxRigidDynamic>());
		if (pxRigidDynamic->isSleeping())
		{
			m_timer = m_idleTimeout;
		}
	}
}

} // namespace frm