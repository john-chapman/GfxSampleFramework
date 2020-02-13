#include "PhysicsTest.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/BasicMaterial.h>
#include <frm/core/BasicRenderer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Input.h>
#include <frm/core/Mesh.h>
#include <frm/core/Scene.h>

#include <frm/physics/Physics.h>
#include <frm/physics/PhysicsGeometry.h>
#include <frm/physics/PhysicsMaterial.h>

#include <imgui/imgui.h>

using namespace frm;

static PhysicsTest s_inst;

PhysicsTest::PhysicsTest()
	: AppBase("Physics") 
{
}

PhysicsTest::~PhysicsTest()
{
}

bool PhysicsTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create(m_resolution.x, m_resolution.y);

	m_defaultMaterial = BasicMaterial::Create("materials/Grid1Light.json");
	FRM_ASSERT(m_defaultMaterial->getState() == BasicMaterial::State_Loaded);

	m_meshes[Geometry_Box]                 = Mesh::Create("models/Box_1.obj");
	m_physicsGeometries[Geometry_Box]      = PhysicsGeometry::CreateBox(vec3(0.5f));
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
	m_rootNode = m_scene->createNode(Node::Type_Root);
	m_rootNode->setName("#PhysicsTest");

	Physics::AddGroundPlane(Physics::GetDefaultMaterial());

	return true;
}

void PhysicsTest::shutdown()
{
	shutdownRootNode(m_rootNode);

	for (Mesh* mesh : m_meshes)
	{
		Mesh::Release(mesh);
	}

	BasicMaterial::Release(m_defaultMaterial);
	BasicRenderer::Destroy(m_basicRenderer);

	AppBase::shutdown();
}

bool PhysicsTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Spawn Projectile"))
	{
		constexpr const char* kGeometryTypeStr[] =
		{
			"Box",//Geometry_Box,
			"Capsule",//Geometry_Capsule,
			"Cylinder",//Geometry_Cylinder,
			"Sphere",//Geometry_Sphere,
			"Random"//Geometry_Random
		};
		if (ImGui::BeginCombo("Type", kGeometryTypeStr[m_spawnType]))
		{
			for (int typeIndex = 0; typeIndex <= Geometry_Count; ++typeIndex)
			{
				bool selected = typeIndex == m_spawnType;
				if (ImGui::Selectable(kGeometryTypeStr[typeIndex], selected))
				{
					m_spawnType = (Geometry)typeIndex;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Physics"))
	{
		Physics::Edit();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Renderer"))
	{
		m_basicRenderer->edit();

		ImGui::TreePop();
	}		

	const float dt = (float)getDeltaTime();
	Camera* drawCamera = Scene::GetDrawCamera();
	Camera* cullCamera = Scene::GetCullCamera();

	if (Input::GetKeyboard()->wasPressed(Keyboard::Key_Space))
	{
		vec3 position  = cullCamera->getPosition();
		vec3 linearVelocity = cullCamera->getViewVector() * 20.0f;
		spawnPhysicsObject(m_spawnType, position, linearVelocity);
	}

	return true;
}

void PhysicsTest::draw()
{
	const GlContext* ctx = GlContext::GetCurrent();	

	Camera* drawCamera = Scene::GetDrawCamera();
	Camera* cullCamera = Scene::GetCullCamera();

	m_basicRenderer->draw(drawCamera, (float)getDeltaTime());

	AppBase::draw();
}

// PROTECTED

void PhysicsTest::spawnPhysicsObject(Geometry _type, const frm::vec3& _position, const frm::vec3& _linearVelocity)
{
	if (_type == Geometry_Random)
	{
		static Rand<> rand;
		_type = (Geometry)rand.get<int>(0, Geometry_Random - 1);
	}

	int index = m_rootNode->getChildCount();
	Node* newNode = m_scene->createNode(Node::Type_Object, m_rootNode);
	newNode->setNamef("#PhysicsObject%d", index);
	newNode->setActive(true);
	newNode->setDynamic(true);
	
	Component_BasicRenderable* renderableComponent = Component_BasicRenderable::Create(m_meshes[_type], m_defaultMaterial);
	newNode->addComponent(renderableComponent);

	mat4 initialTransform = LookAt(_position, _position + _linearVelocity);						
	Component_PhysicsTemporary* physicsComponent = Component_PhysicsTemporary::Create(m_physicsGeometries[_type], Physics::GetDefaultMaterial(), 1.0f, initialTransform, Physics::Flags_Default);
	newNode->addComponent(physicsComponent);
	physicsComponent->setLinearVelocity(_linearVelocity);

	physicsComponent->idleTimeout = 0.1f;
}

void PhysicsTest::shutdownRootNode(Node*& _root_)
{
	for (int childIndex = 0; childIndex < _root_->getChildCount(); ++childIndex)
	{
		Node* child = _root_->getChild(childIndex);
		shutdownRootNode(child);
	}
	m_scene->destroyNode(_root_);
}
