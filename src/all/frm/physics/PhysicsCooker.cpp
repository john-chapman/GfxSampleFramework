#include "Physics.h"

#include "PhysicsGeometry.h"
#include "PhysicsInternal.h"
#include "PhysicsMaterial.h"

#include <frm/core/Log.h>
#include <frm/core/MeshData.h>
#include <frm/core/Time.h>

#pragma comment(lib, "PhysXCooking_64")

using namespace frm;

physx::PxCooking* frm::g_pxCooking = nullptr;

// \todo cook static BVH?

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

bool frm::CookConvexMesh(const MeshData* _meshData, physx::PxOutputStream& out_)
{
	FRM_AUTOTIMER("Physics::CookConvexMesh");

	FRM_ASSERT(_meshData);

	InitCooker();
	FRM_ASSERT(g_pxCooking);

	// \todo polygons?
	physx::PxConvexMeshDesc meshDesc = {};
	meshDesc.points.count  = (physx::PxU32)_meshData->getVertexCount();
	meshDesc.points.stride = _meshData->getDesc().getVertexSize();
	meshDesc.points.data   = _meshData->getVertexData();
	meshDesc.flags         = physx::PxConvexFlag::eCOMPUTE_CONVEX;// | physx::PxConvexFlag::eDISABLE_MESH_VALIDATION;
	
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
	FRM_LOG_ERR("Physics::CookConvexMesh failed: '%s'", errStr);
	
	return false;
}

bool frm::CookTriangleMesh(const MeshData* _meshData, physx::PxOutputStream& out_)
{
	FRM_AUTOTIMER("Physics::CookTriangleMesh");

	FRM_ASSERT(_meshData);

	InitCooker();
	FRM_ASSERT(g_pxCooking);
	
	physx::PxTriangleMeshDesc meshDesc = {};
	meshDesc.points.count     = (physx::PxU32)_meshData->getVertexCount();
	meshDesc.points.stride    = (physx::PxU32)_meshData->getDesc().getVertexSize();
	meshDesc.points.data      = _meshData->getVertexData();
	meshDesc.triangles.count  = (physx::PxU32)_meshData->getIndexCount() / 3;
	meshDesc.triangles.stride = (physx::PxU32)DataTypeSizeBytes(_meshData->getIndexDataType()) * 3;
	meshDesc.triangles.data   = _meshData->getIndexData();
	meshDesc.flags            = (physx::PxMeshFlags)0; //| physx::PxMeshFlag::eFLIPNORMALS; // use eFLIPNORMALS to flip triangle winding
	if (_meshData->getIndexDataType() == DataType_Uint16)
	{
		meshDesc.flags |=  physx::PxMeshFlag::e16_BIT_INDICES;
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
	FRM_LOG_ERR("Physics::CookTriangleMesh failed: '%s'", errStr);
	
	return false;

	return false;
}