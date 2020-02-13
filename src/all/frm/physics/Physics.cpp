#include "Physics.h"

#include "PhysicsGeometry.h"
#include "PhysicsInternal.h"
#include "PhysicsMaterial.h"

#include <frm/core/geom.h>
#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/Camera.h>
#include <frm/core/Component.h>
#include <frm/core/Profiler.h>
#include <frm/core/Scene.h>
#include <frm/core/Serializer.h>
#include <frm/core/Time.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>

#pragma comment(lib, "PhysX_64")
#pragma comment(lib, "PhysXCommon_64")
#pragma comment(lib, "PhysXFoundation_64")
#pragma comment(lib, "PhysXExtensions_static_64")

using namespace frm;

/*******************************************************************************

                                   Physics

*******************************************************************************/

struct Component_Physics::Impl
{
	physx::PxRigidActor* pxRigidActor = nullptr;
	physx::PxShape*      pxShape      = nullptr;
};

physx::PxFoundation*           frm::g_pxFoundation = nullptr;
physx::PxPhysics*              frm::g_pxPhysics    = nullptr;
physx::PxDefaultCpuDispatcher* frm::g_pxDispatcher = nullptr;
physx::PxScene*                frm::g_pxScene      = nullptr;

namespace {

class AllocatorCallback: public physx::PxAllocatorCallback
{
public:
	void* allocate(size_t _size, const char* _type, const char* _file, int _line) override
	{
		FRM_UNUSED(_type);
		FRM_UNUSED(_file);
		FRM_UNUSED(_line);
		return FRM_MALLOC_ALIGNED(_size, 16);
	}

	void deallocate(void* _ptr) override
	{
		FRM_FREE_ALIGNED(_ptr);
	}
};
AllocatorCallback g_allocatorCallback;

class ErrorCallback: public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum _code, const char* _message, const char* _file, int _line) override
	{
		switch (_code)
		{
			default:
			case physx::PxErrorCode::eDEBUG_INFO:
			case physx::PxErrorCode::eDEBUG_WARNING:
				FRM_LOG("PhysX Error:\n\t%s\n\t'%s' (%d)", _message, _file, _line);
				break;
			case physx::PxErrorCode::eINTERNAL_ERROR:
			case physx::PxErrorCode::eINVALID_OPERATION:
			case physx::PxErrorCode::eINVALID_PARAMETER:
				FRM_LOG_ERR("PhysX Error:\n\t%s\n\t'%s' (%d)", _message, _file, _line);
				break;
		};
	}
};
ErrorCallback g_errorCallback;

class EventCallback: public physx::PxSimulationEventCallback
{
public:

	void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override
	{
	}

	void onWake(physx::PxActor** actors, physx::PxU32 count) override
	{
	}

	void onSleep(physx::PxActor** actors, physx::PxU32 count) override	
	{
	}

	void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override
	{
		// \todo
		// Push contact events into a queue (ring buffer?).
		// Remove duplicates?
	}

	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
	{
	}

	void onAdvance(const physx::PxRigidBody*const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override
	{
	}
};
EventCallback g_eventCallback;

physx::PxFilterFlags FilterShader(	
	physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, 
	physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
	physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize
	)
{
	// let triggers through
	if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
		return physx::PxFilterFlag::eDEFAULT;
	}
	// generate contacts for all that were not filtered above
	pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

	// trigger the contact callback for pairs (A,B) where 
	// the filtermask of A contains the ID of B and vice versa.
	//if((filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1))
	//	pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
	pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
	//pairFlags |= physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
	
	return physx::PxFilterFlag::eDEFAULT;
}


inline bool CheckFlag(uint32 _flags, uint32 _flag)
{
	return (_flags & (1 << _flag)) != 0;
}

inline void SetFlag(uint32& _flags_, uint32 _flag, bool _value)
{
	uint32 mask = 1 << _flag;
	_flags_ = _value ? (_flags_ | mask) : (_flags_ & ~mask);
}

} // namespace

// PUBLIC

bool Physics::Init()
{
	FRM_AUTOTIMER("Physics::Init");

	FRM_ASSERT(!s_instance); // Init() called twice?

	s_instance = FRM_NEW(Physics); // \todo serialize config (e.g. gravity)
	
	physx::PxTolerancesScale toleranceScale; // \todo serialize
	toleranceScale.length = 10.0f;
	toleranceScale.speed = 15.0f;

	g_pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocatorCallback, g_errorCallback);
	FRM_ASSERT(g_pxFoundation);
	g_pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pxFoundation, toleranceScale);
	FRM_ASSERT(g_pxPhysics);
	g_pxDispatcher = physx::PxDefaultCpuDispatcherCreate(1); // \todo
	FRM_ASSERT(g_pxDispatcher);

	physx::PxSceneDesc sceneDesc(g_pxPhysics->getTolerancesScale());
	sceneDesc.gravity       = Vec3ToPx(s_instance->m_gravityDirection * s_instance->m_gravity);
	sceneDesc.cpuDispatcher = g_pxDispatcher;
	sceneDesc.filterShader  = FilterShader;//physx::PxDefaultSimulationFilterShader;
	g_pxScene = g_pxPhysics->createScene(sceneDesc);
	FRM_ASSERT(g_pxScene);

	g_pxScene->setSimulationEventCallback(&g_eventCallback);

	return true; // \todo error
}

void Physics::Shutdown()
{
	FRM_AUTOTIMER("Physics::Shutdown");

	FRM_ASSERT(s_instance); // Init() not called?

	if (g_pxCooking)
	{
		// g_pxCooking is lazy-initialized (see PhysicsCooker.cpp)
		g_pxCooking->release();
		g_pxCooking = nullptr;
	}

	g_pxScene->release();
	g_pxScene = nullptr;
	g_pxDispatcher->release();
	g_pxDispatcher = nullptr;
	g_pxPhysics->release();
	g_pxPhysics = nullptr;
	g_pxFoundation->release();
	g_pxFoundation = nullptr;
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
			ApplyNodeTransforms();
			return;
		}
	}

	// set kinematic transforms
	for (Component_Physics* component : s_instance->m_kinematic)
	{
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)component->m_impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = component->m_node->getWorldMatrix();
		actor->setKinematicTarget(Mat4ToPxTransform(worldMatrix));
	}

	// step the simulation
	float stepLengthSeconds = Min(s_instance->m_stepLengthSeconds, _dt);
	float stepCount = Floor((_dt + s_instance->m_stepAccumulator) / stepLengthSeconds);
	s_instance->m_stepAccumulator += _dt - (stepCount * stepLengthSeconds);
	for (int i = 0; i < (int)stepCount; ++i)
	{
		g_pxScene->simulate(stepLengthSeconds);
		FRM_VERIFY(g_pxScene->fetchResults(true)); // true = block until the results are ready
	}

	ApplyNodeTransforms();
}

void Physics::Edit()
{	
	if (ImGui::Button(s_instance->m_paused ? "Resume" : "Pause"))
	{
		s_instance->m_paused = !s_instance->m_paused;
	}
	if (s_instance->m_paused)
	{
		ImGui::SameLine();
		if (ImGui::Button("Step >> "))
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
		for (Component_Physics* component : s_instance->m_kinematic)
		{
			component->reset();
		}

		for (Component_Physics* component : s_instance->m_dynamic)
		{
			component->reset();
		}
	}
	
	if (ImGui::SliderFloat("Gravity", &s_instance->m_gravity, 0.0f, 30.0f))
	{
		vec3 g = s_instance->m_gravityDirection * s_instance->m_gravity;
		g_pxScene->setGravity(Vec3ToPx(g));
	}

	bool& debugDraw = s_instance->m_debugDraw;
	if (ImGui::Checkbox("Debug Draw", &debugDraw))
	{
		if (debugDraw)
		{
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE,            1.0f);

			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
			//g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_AABBS,  1.0f);
			//g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES,        1.0f);
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL,   1.0f);
		}
		else
		{
			g_pxScene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 0.0f);
		}
	}

	if (s_instance->m_debugDraw)
	{
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

		Im3d::SetSize(1.0f); // \todo parameterize
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
}

bool Physics::InitComponent(Component_Physics* _component_)
{
	PROFILER_MARKER_CPU("#Physics::InitComponent");

	Component_Physics::Impl*& impl = _component_->m_impl;
	impl = FRM_NEW(Component_Physics::Impl);

	// PxRigidActor
	physx::PxRigidActor*& pxRigidActor = impl->pxRigidActor;
	if (CheckFlag(_component_->m_flags, Flags_Dynamic) || CheckFlag(_component_->m_flags, Flags_Kinematic))
	{
		physx::PxRigidDynamic* pxRigidDynamic = g_pxPhysics->createRigidDynamic(Mat4ToPxTransform(_component_->m_initialTransform));
		if (CheckFlag(_component_->m_flags, Flags_Kinematic))
		{
			pxRigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		}
		physx::PxRigidBodyExt::updateMassAndInertia(*pxRigidDynamic, _component_->m_mass);
		pxRigidActor = pxRigidDynamic;
		_component_->forceUpdateNode();
	}
	else if (CheckFlag(_component_->m_flags, Flags_Static))
	{
		pxRigidActor = g_pxPhysics->createRigidStatic(Mat4ToPxTransform(_component_->m_initialTransform));
	}

	// PxShape
	physx::PxShape*& pxShape = impl->pxShape;
	PhysicsGeometry::Use(_component_->m_geometry);
	const physx::PxGeometry& geometry = ((physx::PxGeometryHolder*)_component_->m_geometry->m_impl)->any();
	PhysicsMaterial::Use(_component_->m_material);
	const physx::PxMaterial& material = *((physx::PxMaterial*)_component_->m_material->m_impl);
	pxShape = g_pxPhysics->createShape(geometry, material);

	// some geometry types require a local pose to be set at the shape level
	switch (geometry.getType())
	{
		default:
			break;
		case physx::PxGeometryType::ePLANE:
		{			
			const physx::PxPlane plane = physx::PxPlane(Vec3ToPx(_component_->m_geometry->m_data.plane.normal), _component_->m_geometry->m_data.plane.offset);
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

	pxRigidActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true); // \todo enable/disable for all actors when toggling debug draw?

	pxRigidActor->userData = _component_;

	RegisterComponent(_component_);

	return true;
}

void Physics::ShutdownComponent(Component_Physics* _component_)
{
	PROFILER_MARKER_CPU("#Physics::ShutdownComponent");

	UnregisterComponent(_component_);
	
	Component_Physics::Impl*& impl = _component_->m_impl;
	impl->pxRigidActor->detachShape(*impl->pxShape);
	impl->pxShape->release();
	impl->pxRigidActor->release();
	FRM_DELETE(impl);
	impl = nullptr;
}

void Physics::RegisterComponent(Component_Physics* _component_)
{
	PROFILER_MARKER_CPU("#Physics::RegisterComponent");

	Flags flags = _component_->getFlags();
	if (CheckFlag(flags, Flags_Dynamic))
	{
		s_instance->m_dynamic.push_back(_component_);
	}
	else if (CheckFlag(flags, Flags_Kinematic))
	{
		s_instance->m_kinematic.push_back(_component_);
	}
	else
	{
		s_instance->m_static.push_back(_component_);
	}

	g_pxScene->addActor(*_component_->m_impl->pxRigidActor);
}

void Physics::UnregisterComponent(Component_Physics* _component_)
{
	PROFILER_MARKER_CPU("#Physics::UnregisterComponent");

	g_pxScene->removeActor(*_component_->m_impl->pxRigidActor);

	Flags flags = _component_->getFlags();
	if (CheckFlag(flags, Flags_Dynamic))
	{
		s_instance->m_dynamic.erase_first_unsorted(_component_);
	}
	else if (CheckFlag(flags, Flags_Kinematic))
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
	static PhysicsMaterial* s_defaultMaterial = PhysicsMaterial::Create(0.5f, 0.5f, 0.2f, "Default");
	return s_defaultMaterial;
}

void Physics::AddGroundPlane(const PhysicsMaterial* _material)
{
	static Component_Physics s_groundPlane;
	s_groundPlane.m_flags = (1 << Flags_Static);
	s_groundPlane.m_geometry = PhysicsGeometry::CreatePlane(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f));
	s_groundPlane.m_material = const_cast<PhysicsMaterial*>(_material);
	s_groundPlane.m_initialTransform = identity;
	s_groundPlane.m_node = nullptr;
	FRM_VERIFY(s_groundPlane.init());
}


bool Physics::RayCast(const RayCastIn& _in, RayCastOut& out_, RayCastFlags _flags)
{
	physx::PxRaycastBuffer queryResult;
	physx::PxHitFlags flags = (physx::PxHitFlags)0
		| (CheckFlag(_flags, RayCastFlags_Position) ? physx::PxHitFlag::ePOSITION : (physx::PxHitFlags)0)
		| (CheckFlag(_flags, RayCastFlags_Normal)   ? physx::PxHitFlag::eNORMAL   : (physx::PxHitFlags)0)
		| (CheckFlag(_flags, RayCastFlags_AnyHit)   ? physx::PxHitFlag::eMESH_ANY : (physx::PxHitFlags)0)
		;
	if (!g_pxScene->raycast(Vec3ToPx(_in.origin), Vec3ToPx(_in.direction), _in.maxDistance, queryResult) || !queryResult.hasBlock)
	{
		return false;
	}

	out_.position  = PxToVec3(queryResult.block.position);
	out_.normal    = PxToVec3(queryResult.block.normal);
	out_.distance  = queryResult.block.distance;
	out_.component = (Component_Physics*)queryResult.block.actor->userData;

	return true;
}

// PRIVATE

Physics* Physics::s_instance = nullptr;

void Physics::ApplyNodeTransforms()
{
	for (Component_Physics* component : s_instance->m_dynamic)
	{
		physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)component->m_impl->pxRigidActor;
		FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
		mat4 worldMatrix = PxToMat4(actor->getGlobalPose());
		component->m_node->setWorldMatrix(worldMatrix);
	}
}


/*******************************************************************************

                               Component_Physics

*******************************************************************************/

FRM_FACTORY_REGISTER_DEFAULT(Component, Component_Physics);

Component_Physics* Component_Physics::Create(const PhysicsGeometry* _geometry, const PhysicsMaterial* _material, float _mass, const mat4& _initialTransform, Physics::Flags _flags)
{
	auto classRef = Component::FindClassRef(StringHash("Component_Physics"));
	FRM_ASSERT(classRef);

	Component_Physics* ret = (Component_Physics*)Component::Create(classRef);
	ret->m_geometry         = const_cast<PhysicsGeometry*>(_geometry);
	ret->m_material         = const_cast<PhysicsMaterial*>(_material);
	ret->m_mass             = _mass;
	ret->m_initialTransform = _initialTransform;
	ret->m_flags            = _flags;

	return ret;
}

bool Component_Physics::init()
{
	shutdown();

	if (!m_geometry || !m_material)
	{
	// \todo in this case we can't call InitComponent but we want to return true so that the component will be created and we can edit it
		return true;
	}

	return Physics::InitComponent(this);
}

void Component_Physics::shutdown()
{
	if (!m_impl)
	{
		return;
	}

	Physics::UnregisterComponent(this);
	FRM_DELETE(m_impl);
	m_impl = nullptr;
}

void Component_Physics::update(float _dt)
{
}

bool Component_Physics::edit()
{
	bool ret = false;

	ImGui::PushID(this);
	Im3d::PushId(this);

	ret |= editFlags();

	ImGui::Spacing();

	if (m_impl && CheckFlag(m_flags, Physics::Flags_Dynamic) && ImGui::DragFloat("Mass", &m_mass))
	{
		m_mass = Max(m_mass, 0.0f);
		ret = true;
	}

	ImGui::Spacing();

	ret |= editGeometry();
	ret |= editMaterial();

	if (ImGui::TreeNode("Initial Transform"))
	{
		if (Im3d::Gizmo("InitialTransform", (float*)&m_initialTransform) && m_impl)
		{
			physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
			FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
			actor->setGlobalPose(Mat4ToPxTransform(m_initialTransform));
			ret = true;
		}

		if (ImGui::Button("Copy from node"))
		{
			m_initialTransform = m_node->getLocalMatrix();
			physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
			FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
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

	Im3d::PopId();
	ImGui::PopID();

	if (ret)
	{
		ret = init();
	}

	return ret;
}

bool Component_Physics::serialize(Serializer& _serializer_)
{
	bool ret = serializeFlags(_serializer_);

	// optional properties
	Serialize(_serializer_, m_mass, "Mass");

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
				m_geometry->serialize(_serializer_);
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
				m_material->serialize(_serializer_);
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

void Component_Physics::addForce(const vec3& _force)
{
	if (!m_impl || !CheckFlag(m_flags, Physics::Flags_Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
	pxRigidDynamic->addForce(Vec3ToPx(_force));
}

void Component_Physics::setLinearVelocity(const vec3& _linearVelocity)
{
	if (!m_impl || !CheckFlag(m_flags, Physics::Flags_Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
	pxRigidDynamic->setLinearVelocity(Vec3ToPx(_linearVelocity));
}

void Component_Physics::reset()
{
	if (!m_impl || !CheckFlag(m_flags, Physics::Flags_Dynamic))
	{
		return;
	}
	
	physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
	pxRigidDynamic->setGlobalPose(Mat4ToPxTransform(m_initialTransform));
	pxRigidDynamic->setLinearVelocity(Vec3ToPx(vec3(0.0f)));
	pxRigidDynamic->setAngularVelocity(Vec3ToPx(vec3(0.0f)));
}

void Component_Physics::forceUpdateNode()
{
	if (!m_impl)
	{
		return;
	}

	physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
	FRM_ASSERT(actor->is<physx::PxRigidDynamic>());
	mat4 worldMatrix = PxToMat4(actor->getGlobalPose());
	m_node->setWorldMatrix(worldMatrix);
}

// PRIVATE

bool Component_Physics::editFlags()
{
	bool ret = false;

	bool flagStatic     = CheckFlag(m_flags, Physics::Flags_Static);
	bool flagKinematic  = CheckFlag(m_flags, Physics::Flags_Kinematic);
	bool flagDynamic    = CheckFlag(m_flags, Physics::Flags_Dynamic);
	bool flagSimulation = CheckFlag(m_flags, Physics::Flags_Simulation);
	bool flagQuery      = CheckFlag(m_flags, Physics::Flags_Query);

	if (ImGui::Checkbox("Static", &flagStatic))
	{
		flagKinematic = flagDynamic = false;
		ret = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Kinematic",  &flagKinematic))
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
	ret |= ImGui::Checkbox("Query",      &flagQuery);

	if (ret)
	{
		SetFlag(m_flags, Physics::Flags_Static,     flagStatic);
		SetFlag(m_flags, Physics::Flags_Kinematic,  flagKinematic);
		SetFlag(m_flags, Physics::Flags_Dynamic,    flagDynamic);
		SetFlag(m_flags, Physics::Flags_Simulation, flagSimulation);
		SetFlag(m_flags, Physics::Flags_Query,      flagQuery);
	}
	

	return ret;
}

bool Component_Physics::serializeFlags(Serializer& _serializer_)
{
	if (!_serializer_.beginObject("Flags"))
	{
		return false;
	}

	bool flagStatic     = CheckFlag(m_flags, Physics::Flags_Static);
	bool flagKinematic  = CheckFlag(m_flags, Physics::Flags_Kinematic);
	bool flagDynamic    = CheckFlag(m_flags, Physics::Flags_Dynamic);
	bool flagSimulation = CheckFlag(m_flags, Physics::Flags_Simulation);
	bool flagQuery      = CheckFlag(m_flags, Physics::Flags_Query);
		
	Serialize(_serializer_, flagStatic,     "Static");
	Serialize(_serializer_, flagKinematic,  "Kinematic");
	Serialize(_serializer_, flagDynamic,    "Dynamic");
	Serialize(_serializer_, flagSimulation, "Simulation");
	Serialize(_serializer_, flagQuery,      "Query");

	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		SetFlag(m_flags, Physics::Flags_Static,     flagStatic);
		SetFlag(m_flags, Physics::Flags_Kinematic,  flagKinematic);
		SetFlag(m_flags, Physics::Flags_Dynamic,    flagDynamic);
		SetFlag(m_flags, Physics::Flags_Simulation, flagSimulation);
		SetFlag(m_flags, Physics::Flags_Query,      flagQuery);
	}
		
	_serializer_.endObject(); // Flags

	Serialize(_serializer_, m_initialTransform, "InitialTransform");

	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		if (_serializer_.beginObject("Material"))
		{
		 // inline material
			m_material = PhysicsMaterial::Create(_serializer_);

			_serializer_.endObject();
		}
		else
		{
		 // from file
			PathStr path;
			Serialize(_serializer_, path, "Material");
			m_material = PhysicsMaterial::Create(path.c_str());
		}
	}
	else if (m_material)
	{
		if (*m_material->getPath() == '\0')
		{
		 // inline material
			_serializer_.beginObject("Material");
			m_material->serialize(_serializer_);
			_serializer_.endObject();
		}
		else
		{
		 // from file
			Serialize(_serializer_, PathStr(m_material->getPath()), "Material");
		}
	}
	
	return true;
}

bool Component_Physics::editGeometry()
{
	bool ret = false;

	constexpr const char* kSelectGeometryPopupName = "Select Physics Geometry Popup";

	if (ImGui::TreeNode("Geometry"))
	{
		if (ImGui::Button(ICON_FA_LIST_UL " Select"))
		{
			ImGui::OpenPopup(kSelectGeometryPopupName);
		}
		if (ImGui::BeginPopup(kSelectGeometryPopupName))
		{
			static ImGuiTextFilter filter;
			filter.Draw("Filter##PhysicsGeometry");
	
			for (int i = 0; i < PhysicsGeometry::GetInstanceCount(); ++i)
			{
			 // note that inline geometry (i.e. without a path) can't be shared 
				PhysicsGeometry* geometry = PhysicsGeometry::GetInstance(i);
				if (*geometry->getPath() != '\0' && filter.PassFilter(geometry->getName()))
				{
					if (ImGui::Selectable(geometry->getName()))
					{
						if (geometry != m_geometry)
						{
							PhysicsGeometry::Release(m_geometry);
							m_geometry = geometry;
							PhysicsGeometry::Use(m_geometry);
							ret = true;
						}
					}
				}
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Create"))
		{
			PhysicsGeometry::Release(m_geometry);
			m_geometry = PhysicsGeometry::CreateBox(vec3(0.5f));
			PhysicsGeometry::Use(m_geometry);
			ret = true;
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Load"))
		{
			PathStr path;
			if (FileSystem::PlatformSelect(path) && FileSystem::CompareExtension(path.c_str(), "physgeo"))
			{
				path = FileSystem::MakeRelative(path.c_str());
				PhysicsGeometry* tmp = PhysicsGeometry::Create(path.c_str());
				PhysicsGeometry::Release(m_geometry);
				m_geometry = tmp;
				PhysicsGeometry::Use(m_geometry);
				ret = true;
			}
		}

		if (m_geometry)
		{
			m_geometry->edit();
		}

		ImGui::TreePop();
	}

	return ret;
}

bool Component_Physics::editMaterial()
{
	bool ret = false;

	constexpr const char* kSelectMaterialPopupName = "Select Physics Material Popup";

	if (ImGui::TreeNode("Material"))
	{
		if (ImGui::Button(ICON_FA_LIST_UL " Select"))
		{
			ImGui::OpenPopup(kSelectMaterialPopupName);
		}
		if (ImGui::BeginPopup(kSelectMaterialPopupName))
		{
			static ImGuiTextFilter filter;
			filter.Draw("Filter##PhysicsMaterial");
	
			for (int i = 0; i < PhysicsMaterial::GetInstanceCount(); ++i)
			{
			 // note that inline materials (i.e. without paths) can't be shared 
				PhysicsMaterial* material = PhysicsMaterial::GetInstance(i);
				if (*material->getPath() != '\0' && filter.PassFilter(material->getName()))
				{
					if (ImGui::Selectable(material->getName()))
					{
						if (material != m_material)
						{
							PhysicsMaterial::Release(m_material);
							m_material = material;
							PhysicsMaterial::Use(m_material);
							ret = true;
						}
					}
				}
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Create"))
		{
			PhysicsMaterial::Release(m_material);
			m_material = PhysicsMaterial::Create(0.5f, 0.5f, 0.2f);
			PhysicsMaterial::Use(m_material);
			ret = true;
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FILE_O " Load"))
		{
			PathStr path;
			if (FileSystem::PlatformSelect(path) && FileSystem::CompareExtension(path.c_str(), "physmat"))
			{
				path = FileSystem::MakeRelative(path.c_str());
				PhysicsMaterial* tmp = PhysicsMaterial::Create(path.c_str());
				PhysicsMaterial::Release(m_material);
				m_material = tmp;
				PhysicsMaterial::Use(m_material);
				ret = true;
			}
		}

		if (m_material)
		{
			ret |= m_material->edit();
		}

		ImGui::TreePop();
	}

	return ret;
}


/*******************************************************************************

                               Component_Physics

*******************************************************************************/

FRM_FACTORY_REGISTER_DEFAULT(Component, Component_PhysicsTemporary);

Component_PhysicsTemporary* Component_PhysicsTemporary::Create(const PhysicsGeometry* _geometry, const PhysicsMaterial* _material, float _mass, const mat4& _initialTransform, Physics::Flags _flags)
{
	auto classRef = Component::FindClassRef(StringHash("Component_PhysicsTemporary"));
	FRM_ASSERT(classRef);

	Component_PhysicsTemporary* ret = (Component_PhysicsTemporary*)Component::Create(classRef);
	ret->m_geometry         = const_cast<PhysicsGeometry*>(_geometry);
	ret->m_material         = const_cast<PhysicsMaterial*>(_material);
	ret->m_mass             = _mass;
	ret->m_initialTransform = _initialTransform;
	ret->m_flags            = _flags;

	return ret;
}

bool Component_PhysicsTemporary::init()
{
	bool ret = Component_Physics::init();

	basicRenderable = (Component_BasicRenderable*)m_node->findComponent(StringHash("Component_BasicRenderable"));
	timer = 0.0f;

	return ret;
}

void Component_PhysicsTemporary::shutdown()
{
	Component_Physics::shutdown();
}

void Component_PhysicsTemporary::update(float _dt)
{
	Component_Physics::update(_dt);

	if (!m_impl || !CheckFlag(m_flags, Physics::Flags_Dynamic))
	{
		return;
	}

	if (timer > 0.0f)
	{
		if (basicRenderable)
		{
			float alpha = Clamp(timer / idleTimeout, 0.0f, 1.0f);
			basicRenderable->m_colorAlpha.w = alpha;
		}

		timer -= _dt;
		if (timer <= 0.0f)
		{
			Scene::GetCurrent()->destroyNode(m_node);
		}
	}
	else
	{
		const physx::PxRigidDynamic* pxRigidDynamic = (physx::PxRigidDynamic*)m_impl->pxRigidActor;
		const float velocityMagnitude = Length2(PxToVec3(pxRigidDynamic->getAngularVelocity())) + Length2(PxToVec3(pxRigidDynamic->getLinearVelocity()));
		if (velocityMagnitude < velocityThreshold)
		{
			timer = idleTimeout;
		}
	}
}

bool Component_PhysicsTemporary::edit()
{
	bool ret = Component_Physics::edit();

	ret |= ImGui::SliderFloat("Idle Timeout", &idleTimeout, 1e-2f, 10.0f);
	
	return ret;
}

bool Component_PhysicsTemporary::serialize(Serializer& _serializer_)
{
	bool ret = Component_Physics::serialize(_serializer_);

	ret &= Serialize(_serializer_, idleTimeout, "IdleTimeout");
	ret &= Serialize(_serializer_, velocityThreshold, "VelocityThreshold");

	return ret;
}

void Component_PhysicsTemporary::reset()
{
	Scene::GetCurrent()->destroyNode(m_node);
}
