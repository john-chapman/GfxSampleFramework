#include "PhysicsInternal.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/MeshData.h>
#include <frm/core/Pool.h>
#include <frm/core/Profiler.h>

#pragma comment(lib, "PhysX_64")
#pragma comment(lib, "PhysXCommon_64")
#pragma comment(lib, "PhysXFoundation_64")
#pragma comment(lib, "PhysXExtensions_static_64")
#pragma comment(lib, "PhysXCooking_64")

namespace {

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
			// \todo
			// Push contact events into a queue (ring buffer?).
			// Remove duplicates?
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

bool PxInit(const PxSettings& _settings)
{
	// \todo config
	physx::PxTolerancesScale toleranceScale;
	toleranceScale.length = _settings.toleranceLength;
	toleranceScale.speed  = _settings.toleranceSpeed;

	g_pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocatorCallback, g_errorCallback);
	FRM_ASSERT(g_pxFoundation);
	g_pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *g_pxFoundation, toleranceScale);
	FRM_ASSERT(g_pxPhysics);
	g_pxDispatcher = physx::PxDefaultCpuDispatcherCreate(0); // \todo Config worker threads.
	FRM_ASSERT(g_pxDispatcher);

	physx::PxSceneDesc sceneDesc(g_pxPhysics->getTolerancesScale());
	sceneDesc.gravity = Vec3ToPx(_settings.gravity);
	sceneDesc.cpuDispatcher = g_pxDispatcher;
	sceneDesc.filterShader = FilterShader;
	sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD | physx::PxSceneFlag::eDISABLE_CCD_RESWEEP; // \todo Config?
	g_pxScene = g_pxPhysics->createScene(sceneDesc);
	FRM_ASSERT(g_pxScene);

	g_pxScene->setSimulationEventCallback(&g_eventCallback);

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
