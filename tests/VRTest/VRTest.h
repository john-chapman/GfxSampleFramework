#if FRM_MODULE_VR

#pragma once

#include <frm/vr/AppSampleVR.h>

typedef frm::AppSampleVR AppBase;

class VRTest: public AppBase
{
public:

	             VRTest();
	virtual      ~VRTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	enum Geometry
	{
		Geometry_Box,
		Geometry_Capsule,
		Geometry_Cylinder,
		Geometry_Sphere,

		Geometry_Count,
		Geometry_Random = Geometry_Count
	};
	Geometry              m_spawnType                         = Geometry_Random;
	frm::Mesh*            m_meshes[Geometry_Count]            = { nullptr };
	frm::PhysicsGeometry* m_physicsGeometries[Geometry_Count] = { nullptr };
	frm::Node*            m_physicsRoot                       = nullptr;
	frm::BasicMaterial*   m_defaultMaterial                   = nullptr;

	void spawnPhysicsObject(Geometry _type, const frm::vec3& _position, const frm::vec3& _linearVelocity);	
	void shutdownPhysicsRoot(frm::Node*& _root_);
};

#endif // FRM_MODULE_VR