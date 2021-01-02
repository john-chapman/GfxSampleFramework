#include "BasicRendererTest.h"

#include <frm/core/frm.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/Buffer.h>
#include <frm/core/Camera.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/world/World.h>

#include <imgui/imgui.h>

using namespace frm;

static BasicRendererTest s_inst;

BasicRendererTest::BasicRendererTest()
	: AppBase("BasicRenderer") 
{
	Properties::PushGroup("BasicRendererTest");
		//              name                     default                  min     max                     storage
		//Properties::Add("m_reconstructPosition", m_reconstructPosition,                                   &m_reconstructPosition);
		//Properties::Add("m_instanceCount",       m_instanceCount,         1,      128,                    &m_instanceCount);
		//Properties::Add("m_depthFormat",         m_depthFormat,           0,      (int)DepthFormat_Count, &m_depthFormat);
		//Properties::Add("m_maxError",            m_maxError,              0.0f,   1.0f,                   &m_maxError);
	Properties::PopGroup();
}

BasicRendererTest::~BasicRendererTest()
{
	Properties::InvalidateGroup("BasicRendererTest");
}

bool BasicRendererTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create();

	return true;
}

void BasicRendererTest::shutdown()
{
	BasicRenderer::Destroy(m_basicRenderer);

	AppBase::shutdown();
}

bool BasicRendererTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	m_basicRenderer->edit();

	return true;
}

void BasicRendererTest::draw()
{
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

	AppBase::draw();
}

// PROTECTED
