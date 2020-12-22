#include "AnimationTest.h"

#include <frm/core/frm.h>
#include <frm/core/BasicRenderer/BasicMaterial.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/BasicRenderer/BasicRenderableComponent.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Mesh.h>
#include <frm/core/Properties.h>
#include <frm/core/SkeletonAnimation.h>
#include <frm/core/world/World.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

using namespace frm;

static AnimationTest s_inst;

AnimationTest::AnimationTest()
	: AppBase("Animation") 
{
	Properties::PushGroup("AnimationTest");
		//                  name                 default          min     max    storage
		Properties::Add    ("m_animSpeed",       m_animSpeed,     0.f,    2.f,   &m_animSpeed);
		Properties::AddPath("m_animPath",        m_animPath,                     &m_animPath);
		Properties::AddPath("m_meshPath",        m_meshPath,                     &m_meshPath);
	Properties::PopGroup();
}

AnimationTest::~AnimationTest()
{
	Properties::InvalidateGroup("AnimationTest");
}

bool AnimationTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create();

	m_material = BasicMaterial::Create();
	
	World* world = World::GetCurrent();
	Scene* scene = world->getRootScene();
	m_sceneNode = scene->createTransientNode("#AnimationTest");
	m_renderable = (BasicRenderableComponent*)Component::Create(StringHash("BasicRenderableComponent"));
	m_sceneNode->addComponent(m_renderable);

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
	m_sceneNode->setWorld(m_world);

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
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	const float dt = (float)getDeltaTime();
	m_basicRenderer->nextFrame(dt, drawCamera, cullCamera);
	m_basicRenderer->draw(dt, drawCamera, cullCamera);

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
			//m_renderable->m_mesh = m_mesh;

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
