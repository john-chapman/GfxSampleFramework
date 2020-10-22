#include "SceneTest.h"

#include <frm/core/frm.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/world/World.h>
#include <frm/core/world/WorldEditor.h>

#include <im3d/im3d.h>

using namespace frm;

static SceneTest s_inst;

SceneTest::SceneTest()
	: AppBase("SceneTest") 
{
}

SceneTest::~SceneTest()
{
}

bool SceneTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	m_basicRenderer = BasicRenderer::Create(m_resolution.x, m_resolution.y);
	m_basicRenderer->setFlag(BasicRenderer::Flag::WriteToBackBuffer, false); // We manually draw ImGui/Im3d and then blit.

	return true;
}

void SceneTest::shutdown()
{
	BasicRenderer::Destroy(m_basicRenderer);

	AppBase::shutdown();
}

bool SceneTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	m_basicRenderer->edit();

	return true;
}

void SceneTest::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

	Im3d::EndFrame();
	drawIm3d(
		drawCamera,
		m_basicRenderer->fbFinal,
		m_basicRenderer->fbFinal->getViewport(),
		m_basicRenderer->renderTargets[BasicRenderer::Target_GBufferDepthStencil].getTexture(0)
		);
	Im3d::NewFrame();

	ctx->blitFramebuffer(m_basicRenderer->fbFinal, nullptr, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	AppBase::draw();
}