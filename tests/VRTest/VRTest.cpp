#if FRM_MODULE_VR

#include "VRTest.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/BasicMaterial.h>
#include <frm/core/Component.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Scene.h>

#include <frm/audio/Audio.h>
#include <frm/physics/Physics.h>
#include <frm/physics/PhysicsGeometry.h>

#include <im3d/im3d.h>

using namespace frm;

static VRTest s_inst;

VRTest::VRTest()
	: AppBase("VRTest") 
{
}

VRTest::~VRTest()
{
}

bool VRTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}
	
	Scene* scene = Scene::GetCurrent();
	
	m_defaultMaterial = BasicMaterial::Create("materials/Grid0Dark.json");
	FRM_ASSERT(m_defaultMaterial->getState() == BasicMaterial::State_Loaded);

	MeshData* boxData = MeshData::CreateBox(MeshDesc::GetDefault(), .5f, .5f, .5f, 1, 1, 1);
	m_meshes[Geometry_Box]                 = Mesh::Create(*boxData);//Mesh::Create("models/Box_1.obj");
	m_physicsGeometries[Geometry_Box]      = PhysicsGeometry::CreateBox(vec3(0.25f));
	m_meshes[Geometry_Capsule]             = Mesh::Create("models/Capsule_1.obj");
	m_physicsGeometries[Geometry_Capsule]  = PhysicsGeometry::CreateCapsule(0.25f, 0.25f);
	m_meshes[Geometry_Cylinder]            = Mesh::Create("models/Cylinder_1.obj");
	m_physicsGeometries[Geometry_Cylinder] = PhysicsGeometry::CreateConvexMesh("models/Cylinder_1.obj"); // cylinder primitives aren't supported by PhysX
	m_meshes[Geometry_Sphere]              = Mesh::Create("models/Sphere_1.obj");
	m_physicsGeometries[Geometry_Sphere]   = PhysicsGeometry::CreateSphere(0.5f);
	for (int i = 0; i < (int)Geometry_Count; ++i)
	{
		FRM_ASSERT(m_meshes[i]->getState() != Mesh::State_Error);
		FRM_ASSERT(m_physicsGeometries[i]->getState() != PhysicsGeometry::State_Error);
	}

	// Attach dynamically-created nodes to our own root which isn't serialized (name starts with a #).
	m_physicsRoot = m_scene->createNode(Node::Type_Root);
	m_physicsRoot->setName("#PhysicsRoot");

	Physics::AddGroundPlane(Physics::GetDefaultMaterial());

	return true;
}

void VRTest::shutdown()
{
	shutdownPhysicsRoot(m_physicsRoot);

	AppBase::shutdown();
}

bool VRTest::update()
{
	const VRInput& input = m_vrContext->getInputDevice();
	const VRContext::TrackedData& vrTrackedData = m_vrContext->getTrackedData();
	const VRContext::PoseData* vrHands = vrTrackedData.handPoses;
	

	if (input.wasPressed(VRInput::Button_RTrigger))
	{
		vec3 position = vrHands[VRContext::Hand_Right].pose[3].xyz();
		vec3 linearVelocity = vrHands[VRContext::Hand_Right].pose[2].xyz() * -10.0f + vrHands[VRContext::Hand_Right].linearVelocity;
		spawnPhysicsObject(/*m_spawnType*/Geometry_Box, position, linearVelocity);
	}

	if (!AppBase::update())
	{
		return false;
	}

	#if FRM_MODULE_PHYSICS
		static Component_Physics* physicsComponent = nullptr;
		if (input.isDown(VRInput::Button_RGrip))
		{
			const mat4& handPose = vrHands[VRContext::Hand_Right].pose;			
			const vec3  handPosition = vrHands[VRContext::Hand_Right].getPosition();
			const vec3  handDirection = vrHands[VRContext::Hand_Right].getForwardVector();
			Physics::RayCastIn  rayIn(handPosition + handDirection * 0.15f, handDirection);
			Physics::RayCastOut rayOut;

			Im3d::DrawLine(rayIn.origin, rayIn.origin + rayIn.direction, 4.0f, Im3d::Color_Cyan);

			if (!physicsComponent)
			{
				if (Physics::RayCast(rayIn, rayOut))
				{
					Im3d::DrawLine(rayIn.origin + rayIn.direction, rayOut.position, 4.0f, Im3d::Color_Cyan);

					if ((rayOut.component->getFlags() & (1 << Physics::Flags_Dynamic)) != 0)
					{
						physicsComponent = rayOut.component;
					}
				}
			}
			
			if (physicsComponent)
			{
				Im3d::DrawPoint(physicsComponent->getNode()->getWorldPosition(), 16.0f, Im3d::Color_Teal);
				physicsComponent->setLinearVelocity(vrHands[VRContext::Hand_Right].linearVelocity * 3.0f);
				physicsComponent->setAngularVelocity(vrHands[VRContext::Hand_Right].angularVelocity * 1.0f);
			}
		}
		else
		{
			physicsComponent = nullptr;
		}
	#endif

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Physics"))
	{
		Physics::Edit();

		ImGui::TreePop();
	}
	

	

	return true;
}

void VRTest::draw()
{
	AppBase::draw();
}

void VRTest::spawnPhysicsObject(Geometry _type, const frm::vec3& _position, const frm::vec3& _linearVelocity)
{
	if (_type == Geometry_Random)
	{
		static Rand<> rand;
		_type = (Geometry)rand.get<int>(0, Geometry_Random - 1);
	}

	int index = m_physicsRoot->getChildCount();
	Node* newNode = m_scene->createNode(Node::Type_Object, m_physicsRoot);
	newNode->setNamef("#PhysicsObject%d", index);
	newNode->setActive(true);
	newNode->setDynamic(true);
	
	Component_BasicRenderable* renderableComponent = Component_BasicRenderable::Create(m_meshes[_type], m_defaultMaterial);
	newNode->addComponent(renderableComponent);

	vec3 direction = normalize(_linearVelocity);
	mat4 initialTransform = LookAt(_position + direction, _position + direction + _linearVelocity);
	Component_PhysicsTemporary* physicsComponent = Component_PhysicsTemporary::Create(m_physicsGeometries[_type], Physics::GetDefaultMaterial(), 100.0f, initialTransform, Physics::Flags_Default);
	newNode->addComponent(physicsComponent);
	physicsComponent->setLinearVelocity(_linearVelocity);

	physicsComponent->idleTimeout = 1.0f;
}

void VRTest::shutdownPhysicsRoot(Node*& _root_)
{
	for (int childIndex = 0; childIndex < _root_->getChildCount(); ++childIndex)
	{
		Node* child = _root_->getChild(childIndex);
		shutdownPhysicsRoot(child);
	}
	m_scene->destroyNode(_root_);
}

#endif // FRM_MODULE_VR
