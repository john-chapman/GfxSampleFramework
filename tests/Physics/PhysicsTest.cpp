#if FRM_MODULE_PHYSICS

#include "PhysicsTest.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/BasicRenderer/BasicRenderableComponent.h>
#include <frm/core/BasicRenderer/BasicMaterial.h>
#include <frm/core/DrawMesh.h>
#include <frm/core/GlContext.h>
#include <frm/core/Input.h>
#include <frm/core/Mesh.h>
#include <frm/core/world/World.h>

#include <frm/physics/Physics.h>
#include <frm/physics/PhysicsConstraint.h>
#include <frm/physics/PhysicsGeometry.h>
#include <frm/physics/PhysicsMaterial.h>

#if FRM_MODULE_AUDIO
	#include <frm/audio/Audio.h>
	#include <frm/audio/AudioData.h>
#endif

#include <imgui/imgui.h>

using namespace frm;

static PhysicsTest s_inst;

static bool GetRayCameraPlaneIntersection(const Ray& _ray, const vec3& _planeOrigin, vec3& out_)
{
	const Plane plane(-World::GetDrawCamera()->getViewVector(), _planeOrigin);
	float t0;
	if (Intersect(_ray, plane, t0))
	{
		out_ = _ray.m_origin + _ray.m_direction * t0;
		return true;
	}
	return false;
}

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

	m_basicRenderer = BasicRenderer::Create();

	m_defaultMaterial = BasicMaterial::Create("materials/Grid1Light.mat");
	FRM_ASSERT(m_defaultMaterial->getState() == BasicMaterial::State_Loaded);

	m_meshes[Geometry_Box]                 = DrawMesh::Create("models/Box1.gltf");
	m_physicsGeometries[Geometry_Box]      = PhysicsGeometry::CreateBox(vec3(0.5f));
	m_meshes[Geometry_Capsule]             = DrawMesh::Create("models/Capsule1.gltf");
	m_physicsGeometries[Geometry_Capsule]  = PhysicsGeometry::CreateCapsule(0.25f, 0.25f);
	m_meshes[Geometry_Cylinder]            = DrawMesh::Create("models/Cylinder1.gltf");
	m_physicsGeometries[Geometry_Cylinder] = PhysicsGeometry::CreateConvexMesh("models/Cylinder1.gltf"); // cylinder primitives aren't supported by PhysX
	m_meshes[Geometry_Sphere]              = DrawMesh::Create("models/Sphere1.gltf");
	m_physicsGeometries[Geometry_Sphere]   = PhysicsGeometry::CreateSphere(0.5f);
	for (int i = 0; i < (int)Geometry_Count; ++i)
	{
		PhysicsGeometry::Use(m_physicsGeometries[i]);
		FRM_ASSERT(CheckResource(m_meshes[i]));
		FRM_ASSERT(CheckResource(m_physicsGeometries[i]));
	}

	#if FRM_MODULE_AUDIO
		m_hitSounds[0] = AudioData::Create("audio/hit1_light.wav");
		m_hitSounds[1] = AudioData::Create("audio/hit1_medium.wav");
		m_hitSounds[2] = AudioData::Create("audio/hit1_heavy.wav");
	#endif

	return true;
}

void PhysicsTest::shutdown()
{
	#if FRM_MODULE_AUDIO
		for (auto& hitSound : m_hitSounds)
		{
			AudioData::Release(hitSound);
		}
	#endif

	for (DrawMesh* mesh : m_meshes)
	{
		DrawMesh::Release(mesh);
	}

	for (int i = 0; i < (int)Geometry_Count; ++i)
	{
		PhysicsGeometry::Release(m_physicsGeometries[i]);
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
		
	const float dt = (float)getDeltaTime();
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	#if FRM_MODULE_AUDIO
	{
		Audio::SetObserver(drawCamera->m_world);
		eastl::span<Physics::CollisionEvent> collisionEvents = Physics::GetCollisionEvents();
		for (Physics::CollisionEvent& collisionEvent : collisionEvents)
		{
			AudioSourceId sourceId = AudioSourceId_Invalid;
			if (collisionEvent.impulse < 1.0f)
			{
				sourceId = Audio::Play(m_hitSounds[0], 0.1f, 0.0f, 1);
			}
			else if (collisionEvent.impulse < 8.0f)
			{
				sourceId = Audio::Play(m_hitSounds[1], 0.1f, 0.0f, 1);
			}
			else
			{			
				sourceId = Audio::Play(m_hitSounds[2], 0.1f, 0.0f, 1);
			}
			Audio::SetSourceWorldPosition(sourceId, collisionEvent.point);
		}
	}
	#endif


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

		ImGui::SliderFloat("Spawn Speed", &m_spawnSpeed, 1.0f, 500.0f);

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Physics"))
	{
		Physics::Edit();
		ImGui::TreePop();
	}
	
	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Physics Edit"))
	{
		ImGuiIO& io = ImGui::GetIO();

		Ray cursorRay = getCursorRayW(drawCamera);
		Physics::RayCastOut raycastOut;

		enum BoxDrawState_
		{
			BoxDrawState_None,
			BoxDrawState_XZStart,
			BoxDrawState_XZDrag,
			BoxDrawState_Y
		};	
		typedef int BoxDrawState;
		static BoxDrawState boxDrawState;
		static Plane boxDrawPlane;
		static vec3 boxA;
		static vec3 boxB;

		if (ImGui::Button(boxDrawState == BoxDrawState_None ? "Draw Box" : "Cancel (ESC)"))
		{
			if (boxDrawState == BoxDrawState_None)
			{
				boxDrawState = BoxDrawState_XZStart;
			}
			else
			{
				boxDrawState = BoxDrawState_None;
			}
		}
		ImGui::SameLine();
		ImGui::Text("%d", boxDrawState);

		bool showBox = false;
		switch (boxDrawState)
		{
			default:
			case BoxDrawState_None:
				break;
			case BoxDrawState_XZStart:
			{
				if (Physics::RayCast(Physics::RayCastIn(cursorRay.m_origin, cursorRay.m_direction, 100.0f), raycastOut, Physics::RayCastFlag::Position))
				{
					Im3d::DrawPoint(raycastOut.position, 16.0f, Im3d::Color_White);
					if (io.MouseDown[0])
					{
						boxA = boxB = raycastOut.position;
						boxDrawPlane = Plane(vec3(0.0f, 1.0f, 0.0f), raycastOut.position);
						boxDrawState = BoxDrawState_XZDrag;
					}
				}

				break;
			}
			case BoxDrawState_XZDrag:
			{
				float t0;
				bool intersectsPlane = Intersect(cursorRay, boxDrawPlane, t0);
				if (intersectsPlane && io.MouseDown[0])
				{
					boxB = cursorRay.m_origin + cursorRay.m_direction * t0;
					showBox = true;
				}
				else if (intersectsPlane)
				{
					boxDrawState = BoxDrawState_Y;
				}
				else
				{
					boxDrawState = BoxDrawState_None;
				}
				break;
			}
			case BoxDrawState_Y:
			{
				boxDrawPlane = Plane(-cursorRay.m_direction, boxB);
				float t0;
				bool intersectsPlane = Intersect(cursorRay, boxDrawPlane, t0);
				if (intersectsPlane)
				{
					boxB.y = (cursorRay.m_origin + cursorRay.m_direction * t0).y;
					showBox = true;

					if (io.MouseClicked[0])
					{
						vec3 boxSize = Max(boxA, boxB) - Min(boxA, boxB);
						PhysicsGeometry* boxPhysicsGeometry = PhysicsGeometry::CreateBox(boxSize * 0.5f);
						Mesh* boxMeshData = Mesh::CreateBox(boxSize.x, boxSize.y, boxSize.z, 1, 1, 1, identity, Mesh::CreateFlags(0));
						DrawMesh* boxMesh = DrawMesh::Create(*boxMeshData);
						Mesh::Destroy(boxMeshData);

						SceneNode* newNode = World::GetCurrent()->getRootScene()->createTransientNode("#box");
	
						BasicRenderableComponent* renderableComponent = BasicRenderableComponent::Create(boxMesh, m_defaultMaterial);
						newNode->addComponent(renderableComponent);

						float boxMass = boxSize.x * boxSize.y * boxSize.z;
						mat4 initialTransform = TranslationMatrix(Min(boxA, boxB) + boxSize * 0.5f + vec3(0.0f, 1e-6f, 0.0f));
						PhysicsComponent* physicsComponent = PhysicsComponent::CreateTransient(boxPhysicsGeometry, Physics::GetDefaultMaterial(), boxMass, -1.0f, initialTransform, Physics::Flags());
						newNode->addComponent(physicsComponent);
						FRM_VERIFY(newNode->init() && newNode->postInit());

						DrawMesh::Release(boxMesh);
						//PhysicsGeometry::Release(boxPhysicsGeometry);						

						boxDrawState = BoxDrawState_None;
					}
				}
				break;
			}
		}
		if (ImGui::IsKeyPressed(Keyboard::Key_Escape))
		{
			boxDrawState = BoxDrawState_None;
		}

		if (showBox)
		{
			vec3 boxMin = Min(boxA, boxB);
			vec3 boxMax = Max(boxA, boxB);
			Im3d::PushDrawState();

			Im3d::SetColor(Im3d::Color_White);
			Im3d::SetSize(3.0f);
			Im3d::DrawAlignedBox(boxMin, boxMax);
			
			Im3d::SetColor(Im3d::Color_White);
			Im3d::SetAlpha(0.25f);
			Im3d::DrawAlignedBoxFilled(boxMin, boxMax);
			
			Im3d::PopDrawState();
		}

		static bool isSelecting = false;
		static SceneNode* selectedNode = nullptr;
		if (ImGui::Button(isSelecting ? "Cancel (ESC)" : "Select " ICON_FA_EYEDROPPER))
		{
			isSelecting = true;
		}
		if (selectedNode)
		{
			ImGui::SameLine();	
			ImGui::Text(selectedNode->getName());
		}

		if (isSelecting)
		{
			if (io.MouseClicked[0] && Physics::RayCast(Physics::RayCastIn(cursorRay.m_origin, cursorRay.m_direction, 100.0f), raycastOut, Physics::RayCastFlag::Position))
			{
				selectedNode = raycastOut.component->getParentNode();
				if (selectedNode)
				{
					FRM_ASSERT(false); // \todo set selected on editor
				}
				isSelecting = false;
			}
		}

		static bool isJointTest = false;
		static PhysicsComponent* jointTestComponent[2] = { nullptr };
		static mat4 sceneNodeFrame = identity;
		static PhysicsConstraint* joint = nullptr;
		if (ImGui::Button(isJointTest ? "Cancel (ESC)" : "Joint Test"))
		{
			isJointTest = true;
		}

		if (isJointTest)
		{
			if (joint)
			{
				if (!jointTestComponent[1])
				{
					vec3 intersection;
					if (GetRayCameraPlaneIntersection(cursorRay, GetTranslation(sceneNodeFrame), intersection))
					{
						mat4 cursorFrame = LookAt(intersection, GetTranslation(sceneNodeFrame));
						joint->setComponentFrame(1, cursorFrame);
					}
				}
				
				if (!jointTestComponent[1] && io.MouseClicked[0] && Physics::RayCast(Physics::RayCastIn(cursorRay.m_origin, cursorRay.m_direction, 100.0f), raycastOut))
				{
					jointTestComponent[1] = raycastOut.component;
					joint->setComponent(1, raycastOut.component);

					if (jointTestComponent[1])
					{
						SceneNode* sceneNode = jointTestComponent[1]->getParentNode();
						sceneNodeFrame = LookAt(raycastOut.position, raycastOut.position + raycastOut.normal);
						mat4 sceneNodeFrameLocal = AffineInverse(sceneNode->getWorld()) * sceneNodeFrame; // to node local space
						joint->setComponentFrame(1, sceneNodeFrameLocal);
					}
				}

				if (ImGui::IsKeyPressed(Keyboard::Key_Escape))
				{
					isJointTest = false;
					joint->shutdown();
					PhysicsConstraint::Destroy(joint);
				}
			}
			else
			{
				if (io.MouseClicked[0] && Physics::RayCast(Physics::RayCastIn(cursorRay.m_origin, cursorRay.m_direction, 100.0f), raycastOut))
				{
					jointTestComponent[0] = raycastOut.component;
					jointTestComponent[1] = nullptr;
					if (jointTestComponent[0])
					{
						SceneNode* sceneNode = jointTestComponent[0]->getParentNode();
						if (sceneNode)
						{
							sceneNodeFrame = LookAt(raycastOut.position, raycastOut.position + raycastOut.normal);
	
							mat4 cursorFrame = identity;
							vec3 intersection;
							if (GetRayCameraPlaneIntersection(cursorRay, sceneNode->getPosition(), intersection))
							{
								mat4 cursorFrame = LookAt(intersection, drawCamera->getPosition());
							}

							PhysicsConstraint::Distance distanceConstraint;
							distanceConstraint.minDistance      = 0.0f;
							distanceConstraint.maxDistance      = 0.1f;
							distanceConstraint.spring.stiffness = 100.0f;
							distanceConstraint.spring.damping   = 0.9f;
						
							mat4 sceneNodeFrameLocal = AffineInverse(sceneNode->getWorld()) * sceneNodeFrame; // to node local space
							joint = PhysicsConstraint::CreateDistance(jointTestComponent[0], sceneNodeFrameLocal, nullptr, cursorFrame, distanceConstraint);
							sceneNode->addComponent(joint);
						}
					}
				}
			}

			if (joint)
			{
				ImGui::Text("%0xllu -- %0xllu", jointTestComponent[0], jointTestComponent[1]);
				ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
				if (ImGui::TreeNode("Joint"))
				{
					joint->edit();
					ImGui::TreePop();
				}
			}
		}

		ImGui::TreePop();
	}


	if (ImGui::TreeNode("Renderer"))
	{
		m_basicRenderer->edit();
		ImGui::TreePop();
	}

	#if FRM_MODULE_AUDIO
		if (ImGui::TreeNode("Audio"))
		{
			Audio::Edit();
			ImGui::TreePop();
		}
	#endif

	if (Input::GetKeyboard()->wasPressed(Keyboard::Key_Space))
	{
		vec3 position  = cullCamera->getPosition();
		vec3 linearVelocity = cullCamera->getViewVector() * m_spawnSpeed;
		spawnPhysicsObject(m_spawnType, position, linearVelocity);
	}

	if (m_showHelpers)
	{
		Im3d::PushDrawState();
			Im3d::SetSize(2.0f);
			Im3d::SetColor(Im3d::Color_Yellow);
			Im3d::DrawAlignedBox(m_basicRenderer->shadowSceneBounds.m_min, m_basicRenderer->shadowSceneBounds.m_max);
			Im3d::SetColor(Im3d::Color_Magenta);
			Im3d::DrawAlignedBox(m_basicRenderer->sceneBounds.m_min, m_basicRenderer->sceneBounds.m_max);
		Im3d::PopDrawState();
	}

	return true;
}

void PhysicsTest::draw()
{
	const GlContext* ctx = GlContext::GetCurrent();	

	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

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

	SceneNode* newNode = World::GetCurrent()->getRootScene()->createTransientNode("#PhysicsTransient");
	
	BasicRenderableComponent* renderableComponent = BasicRenderableComponent::Create(m_meshes[_type], m_defaultMaterial);
	newNode->addComponent(renderableComponent);

	mat4 initialTransform = LookAt(_position, _position + _linearVelocity);						
	PhysicsComponent* physicsComponent = PhysicsComponent::CreateTransient(m_physicsGeometries[_type], Physics::GetDefaultMaterial(), 1.0f, 0.1f, initialTransform, Physics::Flags());
	//physicsComponent->setFlag(Physics::Flag::EnableCCD, true);

	newNode->addComponent(physicsComponent);
	FRM_VERIFY(newNode->init() && newNode->postInit());

	physicsComponent->setLinearVelocity(_linearVelocity);
}

#endif // FRM_MODULE_PHYSICS
