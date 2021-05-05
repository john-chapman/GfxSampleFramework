#pragma once

#if FRM_MODULE_PHYSICS

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class PhysicsTest: public AppBase
{
public:
	PhysicsTest();
	virtual ~PhysicsTest();

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
	float                 m_spawnSpeed                        = 30.0f;
	frm::DrawMesh*        m_meshes[Geometry_Count]            = { nullptr };
	frm::PhysicsGeometry* m_physicsGeometries[Geometry_Count] = { nullptr };
	
	frm::BasicRenderer*   m_basicRenderer                     = nullptr;
	frm::BasicMaterial*   m_defaultMaterial                   = nullptr;

	#if FRM_MODULE_AUDIO
		frm::AudioData*   m_hitSounds[3]                      = { nullptr };
	#endif

	void spawnPhysicsObject(Geometry _type, const frm::vec3& _position, const frm::vec3& _linearVelocity);

};

#endif