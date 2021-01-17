#include "PhysicsInternal.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/MeshData.h>
#include <frm/core/Pool.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>

#include <im3d/im3d.h>
#include <EASTL/vector.h>

#pragma comment(lib, "PhysX_64")
#pragma comment(lib, "PhysXCommon_64")
#pragma comment(lib, "PhysXFoundation_64")
#pragma comment(lib, "PhysXExtensions_static_64")
#pragma comment(lib, "PhysXCharacterKinematic_static_64")
#pragma comment(lib, "PhysXCooking_64")

namespace {

	eastl::vector<frm::Physics::CollisionEvent>* g_collisionEvents;

	class AllocatorCallback : public physx::PxAllocatorCallback
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

	class ErrorCallback : public physx::PxErrorCallback
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

	class EventCallback : public physx::PxSimulationEventCallback
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
			using namespace physx;
			using namespace frm;

			for (PxU32 i = 0; i < nbPairs; ++i)
			{
				Physics::CollisionEvent collisionEvent;
				collisionEvent.components[0] = (PhysicsComponent*)pairHeader.actors[0]->userData;
				collisionEvent.components[1] = (PhysicsComponent*)pairHeader.actors[1]->userData;

				const PxContactPair& cp     = pairs[i];
				const PxU32 flippedContacts = (cp.flags & PxContactPairFlag::eINTERNAL_CONTACTS_ARE_FLIPPED);
				const PxU32 hasImpulses     = (cp.flags & PxContactPairFlag::eINTERNAL_HAS_IMPULSES);
				FRM_ASSERT(hasImpulses != 0);
				
				PxU32 nbContacts = 0;				
				PxContactStreamIterator it(cp.contactPatches, cp.contactPoints, cp.getInternalFaceIndices(), cp.patchCount, cp.contactCount);
				while(it.hasNextPatch())
				{
					it.nextPatch();
					
					while(it.hasNextContact())
					{
						it.nextContact();
						
						PxVec3 point   = it.getContactPoint();
						PxVec3 normal  = it.getContactNormal();
						PxReal impulse = cp.contactImpulses[nbContacts];

						collisionEvent.point = PxToVec3(point);
						collisionEvent.normal = PxToVec3(normal);
						collisionEvent.impulse = impulse;
						g_collisionEvents->push_back(collisionEvent);
											
						++nbContacts;
					}
				}
			}
		}

		void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
		{
		}

		void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override
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
		// Pass triggers through.
		if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
		{
			pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
			return physx::PxFilterFlag::eDEFAULT;
		}

		// Generate contacts for all that were not filtered above.
		pairFlags |= physx::PxPairFlag::eCONTACT_DEFAULT;

		// Generate collision events for all pairs (kinematic-kinematic and kinematic-static collisions are off by default).
		pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
		pairFlags |= physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
		pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;

		return physx::PxFilterFlag::eDEFAULT;
	}
} // namespace

namespace frm {

physx::PxFoundation*           g_pxFoundation        = nullptr;
physx::PxControllerManager*    g_pxControllerManager = nullptr;
physx::PxPhysics*              g_pxPhysics           = nullptr;
physx::PxDefaultCpuDispatcher* g_pxDispatcher        = nullptr;
physx::PxScene*                g_pxScene             = nullptr;
physx::PxCooking*              g_pxCooking           = nullptr;
Pool<PxComponentImpl>          g_pxComponentPool(128);

bool PxInit(const PxSettings& _settings, eastl::vector<frm::Physics::CollisionEvent>& collisionEvents_)
{
	Properties::PushGroup("#Physics");
	Properties::PushGroup("#PhysX");
			
	Properties::Add("cpuThreadCount", 0,                                    0,  32                                     );
	Properties::Add("broadPhaseType", (int)physx::PxBroadPhaseType::eABP,   0,  (int)physx::PxBroadPhaseType::eLAST - 1);
	Properties::Add("enableCCD",      true                                                                             );

	g_collisionEvents = &collisionEvents_;

	physx::PxTolerancesScale toleranceScale;
	toleranceScale.length = _settings.toleranceLength;
	toleranceScale.speed  = _settings.toleranceSpeed;

	g_pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocatorCallback, g_errorCallback);
	FRM_ASSERT(g_pxFoundation);
	g_pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pxFoundation, toleranceScale);
	FRM_ASSERT(g_pxPhysics);
	
	const int cpuThreadCount = *Properties::Find("cpuThreadCount")->get<int>();
	g_pxDispatcher = physx::PxDefaultCpuDispatcherCreate(cpuThreadCount);
	FRM_ASSERT(g_pxDispatcher);

	physx::PxSceneDesc sceneDesc(g_pxPhysics->getTolerancesScale());
	sceneDesc.gravity = Vec3ToPx(_settings.gravity);
	sceneDesc.cpuDispatcher = g_pxDispatcher;
	sceneDesc.filterShader = FilterShader;

	const bool enableCCD = *Properties::Find("enableCCD")->get<bool>();
	if (enableCCD)
	{
		sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD | physx::PxSceneFlag::eDISABLE_CCD_RESWEEP;
	}

	static_assert(physx::PxBroadPhaseType::eLAST == 4, "physx::PxBroadPhaseType has changed, existing property values may be invalid");
	const physx::PxBroadPhaseType::Enum broadPhaseType = (physx::PxBroadPhaseType::Enum)*Properties::Find("broadPhaseType")->get<int>();
	sceneDesc.broadPhaseType = broadPhaseType;

	g_pxScene = g_pxPhysics->createScene(sceneDesc);
	FRM_ASSERT(g_pxScene);
	g_pxScene->setSimulationEventCallback(&g_eventCallback);

	g_pxControllerManager = PxCreateControllerManager(*g_pxScene);

	Properties::PopGroup(2);

	// \todo Handle errors correctly.
	return true;
}

void PxShutdown()
{
	if (g_pxCooking)
	{
		g_pxCooking->release();
		g_pxCooking = nullptr;
	}

	g_pxControllerManager->release();
	g_pxControllerManager = nullptr;
	g_pxScene->release();
	g_pxScene = nullptr;
	g_pxDispatcher->release();
	g_pxDispatcher = nullptr;
	g_pxPhysics->release();
	g_pxPhysics = nullptr;
	g_pxFoundation->release();
	g_pxFoundation = nullptr;
}

static void InitCooker()
{
	if (g_pxCooking)
	{
		return;
	}

	FRM_ASSERT(g_pxFoundation);
	FRM_ASSERT(g_pxPhysics);

	physx::PxCookingParams cookingParams(g_pxPhysics->getTolerancesScale());
	// \todo init cooking params, check defaults?

	g_pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *g_pxFoundation, cookingParams);
}

bool PxCookConvexMesh(const MeshData* _meshData, physx::PxOutputStream& out_)
{
	FRM_AUTOTIMER("Physics::CookConvexMesh");

	FRM_ASSERT(_meshData);

	InitCooker();
	FRM_ASSERT(g_pxCooking);

	// \todo polygons?
	physx::PxConvexMeshDesc meshDesc = {};
	meshDesc.points.count = (physx::PxU32)_meshData->getVertexCount();
	meshDesc.points.stride = _meshData->getDesc().getVertexSize();
	meshDesc.points.data = _meshData->getVertexData();
	meshDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;// | physx::PxConvexFlag::eDISABLE_MESH_VALIDATION;

	physx::PxConvexMeshCookingResult::Enum err = physx::PxConvexMeshCookingResult::Enum::eSUCCESS;
	if (g_pxCooking->cookConvexMesh(meshDesc, out_, &err))
	{
		return true;
	}

	const char* errStr = "unknown error";
	switch (err)
	{
		default:
			break;
		case physx::PxConvexMeshCookingResult::Enum::ePOLYGONS_LIMIT_REACHED:
			errStr = "polygon limit reached";
			break;
		case physx::PxConvexMeshCookingResult::Enum::eZERO_AREA_TEST_FAILED:
			errStr = "zero area test failed";
			break;
	};
	FRM_LOG_ERR("PxCookConvexMesh failed: '%s'", errStr);

	return false;
}

bool PxCookTriangleMesh(const MeshData* _meshData, physx::PxOutputStream& out_)
{
	FRM_AUTOTIMER("Physics::CookTriangleMesh");

	FRM_ASSERT(_meshData);

	InitCooker();
	FRM_ASSERT(g_pxCooking);

	physx::PxTriangleMeshDesc meshDesc = {};
	meshDesc.points.count = (physx::PxU32)_meshData->getVertexCount();
	meshDesc.points.stride = (physx::PxU32)_meshData->getDesc().getVertexSize();
	meshDesc.points.data = _meshData->getVertexData();
	meshDesc.triangles.count = (physx::PxU32)_meshData->getIndexCount() / 3;
	meshDesc.triangles.stride = (physx::PxU32)DataTypeSizeBytes(_meshData->getIndexDataType()) * 3;
	meshDesc.triangles.data = _meshData->getIndexData();
	meshDesc.flags = (physx::PxMeshFlags)0; //| physx::PxMeshFlag::eFLIPNORMALS; // use eFLIPNORMALS to flip triangle winding
	if (_meshData->getIndexDataType() == DataType_Uint16)
	{
		meshDesc.flags |= physx::PxMeshFlag::e16_BIT_INDICES;
	}

	physx::PxTriangleMeshCookingResult::Enum err = physx::PxTriangleMeshCookingResult::Enum::eSUCCESS;
	if (g_pxCooking->cookTriangleMesh(meshDesc, out_, &err))
	{
		return true;
	}

	const char* errStr = "unknown error";
	switch (err)
	{
		default:
			break;
		case physx::PxTriangleMeshCookingResult::Enum::eLARGE_TRIANGLE:
			errStr = "large triangle";
			break;
	};
	FRM_LOG_ERR("PxCookTriangleMesh failed: '%s'", errStr);

	return false;
}

} // namespace frm
