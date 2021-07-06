#pragma once

#if !FRM_MODULE_PHYSICS
	#error FRM_MODULE_PHYSICS was not enabled
#endif

#include <frm/core/frm.h>
#include <frm/core/math.h>
#include <frm/core/Pool.h>
#include <frm/physics/Physics.h>

#include <PxPhysicsAPI.h>
#include <PxFiltering.h>
#include <cooking/PxCooking.h>
#include <characterkinematic/PxControllerManager.h>

namespace frm {

extern physx::PxFoundation*           g_pxFoundation;
extern physx::PxDefaultCpuDispatcher* g_pxDispatcher;
extern physx::PxPhysics*              g_pxPhysics;
extern physx::PxCooking*              g_pxCooking;

struct PxComponentImpl
{
	physx::PxRigidActor* pxRigidActor = nullptr;
	physx::PxShape*      pxShape      = nullptr;
};
extern Pool<PxComponentImpl> g_pxComponentPool;

struct PxSettings
{
	float toleranceLength  = 1.0f;
	float toleranceSpeed   = 10.0f;
	vec3  gravity          = vec3(0.0f, -10.0f, 0.0f);
};

struct PhysicsWorld::Impl: public physx::PxSimulationEventCallback
{
	physx::PxScene*                        pxScene              = nullptr;
	physx::PxControllerManager*            pxControllerManager  = nullptr;
	eastl::vector<Physics::CollisionEvent> collisionEvents;

	Impl(const PxSettings& _settings);
	~Impl();

	// PxSimulationEventCallback
	void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override;
	void onWake(physx::PxActor** actors, physx::PxU32 count) override;
	void onSleep(physx::PxActor** actors, physx::PxU32 count) override;
	void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;
	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override;
	void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override;
};

// Cooking
void PxInitCooker();
bool PxCookConvexMesh(Mesh& _mesh, physx::PxOutputStream& out_);
bool PxCookTriangleMesh(Mesh& _mesh, physx::PxOutputStream& out_);


// Px -> frm
inline vec3 PxToVec3(const physx::PxVec3& _v)
{
	return vec3(_v.x, _v.y, _v.z);
}

inline quat PxToQuat(const physx::PxQuat& _q)
{
	return quat(_q.x, _q.y, _q.z, _q.w);
}

inline mat4 PxToMat4(const physx::PxTransform& _transform)
{
	vec3 t = PxToVec3(_transform.p);
	quat r = PxToQuat(_transform.q);
	return TransformationMatrix(t, r);
}

// frm -> Px
inline physx::PxVec3 Vec3ToPx(const vec3& _v)
{
	return physx::PxVec3(_v.x, _v.y, _v.z);
}

inline physx::PxQuat QuatToPx(const quat& _q)
{
	return physx::PxQuat(_q.x, _q.y, _q.z, _q.w);
}

inline physx::PxTransform Mat4ToPxTransform(const mat4& _m)
{
	physx::PxTransform ret;
	ret.p = Vec3ToPx(GetTranslation(_m));
	ret.q = QuatToPx(RotationQuaternion(GetRotation(_m)));
	return ret;
}

} // namespace frm
