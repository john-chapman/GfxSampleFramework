#include "AnimationTest.h"

#include <frm/core/frm.h>
#include <frm/core/BasicMaterial.h>
#include <frm/core/BasicRenderer.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Mesh.h>
#include <frm/core/Scene.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/physics/Physics.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;

static AnimationTest s_inst;

AnimationTest::AnimationTest()
	: AppBase("Animation") 
{
	PropertyGroup& props = m_props.addGroup("Animation");
	//             name                     default                  min     max                   storage
	props.addFloat("m_animSpeed",           m_animSpeed,             0.0f,   2.0f,                 &m_animSpeed);
	props.addPath ("m_animPath",            m_animPath.c_str(),                                    &m_animPath);
	props.addPath ("m_meshPath",            m_meshPath.c_str(),                                    &m_meshPath);
}

AnimationTest::~AnimationTest()
{
}

bool AnimationTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create(m_resolution.x, m_resolution.y);

	Physics::AddGroundPlane(Physics::GetDefaultMaterial());

	m_material = BasicMaterial::Create();
	
	m_node = m_scene->createNode(Node::Type_Object);
	m_node->setName("#AnimationTest");
	m_node->setActive(true);
	m_node->setDynamic(true);

	m_world = TransformationMatrix(vec3(0.0f), RotationQuaternion(vec3(1.0f, 0.0f, 0.0f), Radians(-90.0f)), vec3(0.01f));

	initMesh();
	initAnim();

	return true;
}

void AnimationTest::shutdown()
{
	BasicMaterial::Release(m_material);
 	BasicRenderer::Destroy(m_basicRenderer);

	AppBase::shutdown();
}

bool AnimationTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	if (ImGui::Button("Anim Path"))
	{
		if (FileSystem::PlatformSelect(m_animPath, { "*.md5anim" }))
		{
			initAnim();
		}
	}
	if (!m_animPath.isEmpty())
	{
		ImGui::SameLine();
		ImGui::Text(FileSystem::StripPath(m_animPath.c_str()).c_str());
	}	

	if (ImGui::Button("Mesh Path"))
	{
		if (FileSystem::PlatformSelect(m_meshPath, { "*.md5mesh" }))
		{
			initMesh();
		}
	}
	if (!m_meshPath.isEmpty())
	{
		ImGui::SameLine();
		ImGui::Text(FileSystem::StripPath(m_meshPath.c_str()).c_str());
	}

	Skeleton framePose;
	if (CheckResource(m_anim))
	{
		framePose = m_anim->getBaseFrame();
		m_animTime = Fract(m_animTime + (float)getDeltaTime() * m_animSpeed);		
		m_anim->sample(m_animTime, framePose);
		framePose.resolve();
		m_renderable->setPose(framePose);

		ImGui::SliderFloat("Time", &m_animTime, 0.0f, 1.0f);
		ImGui::SliderFloat("Speed", &m_animSpeed, 0.0f, 2.0f);

		static int selectedBone = 0;
		ImGui::SliderInt("Bone", &selectedBone, -1, framePose.getBoneCount() - 1);
		if (selectedBone >= 0)
		{
			ImGui::Text(framePose.getBoneName(selectedBone));
			Im3d::PushSize(4.0f);
			Im3d::PushMatrix(m_world * framePose.getPose()[selectedBone] * ScaleMatrix(vec3(10.0f)));
				Im3d::DrawXyzAxes();
			Im3d::PopMatrix();
			Im3d::PopSize();
		}
		else
		{
			ImGui::Text("--");
		}

	}

	if (m_showHelpers)
	{
		Im3d::Gizmo("world", (float*)&m_world);
		Im3d::PushMatrix(m_world);
			framePose.draw();
		Im3d::PopMatrix();
	}
	m_node->setWorldMatrix(m_world);

	if (ImGui::TreeNode("Material"))
	{
		m_material->edit();
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Renderer"))
	{
		m_basicRenderer->edit();
	
		ImGui::TreePop();
	}

	return true;
}

void AnimationTest::draw()
{
	Camera* drawCamera = Scene::GetDrawCamera();

	m_basicRenderer->draw((float)getDeltaTime());

	AppBase::draw();
}

bool AnimationTest::initMesh()
{
	shutdownMesh();

	if (!m_meshPath.isEmpty())
	{
		m_mesh = Mesh::Create(m_meshPath.c_str());
		if (CheckResource(m_mesh))
		{
			if (!m_renderable)
			{
				m_renderable = Component_BasicRenderable::Create(m_mesh, m_material);
				m_node->addComponent(m_renderable);
			}
			m_renderable->m_mesh = m_mesh;

			return true;
		}
	}

	return false;
}

void AnimationTest::shutdownMesh()
{
	Mesh::Release(m_mesh);
}

bool AnimationTest::initAnim()
{	
	shutdownAnim();

	if (m_animPath.isEmpty())
	{
		return false;
	}

	m_anim = SkeletonAnimation::Create(m_animPath.c_str());
	return CheckResource(m_anim);
}

void AnimationTest::shutdownAnim()
{
	SkeletonAnimation::Release(m_anim);
}
