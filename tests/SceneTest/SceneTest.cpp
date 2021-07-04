#include "SceneTest.h"

#include <frm/core/frm.h>
#include <frm/core/BasicRenderer/BasicRenderer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/world/World.h>
#include <frm/core/world/WorldEditor.h>

#if FRM_MODULE_PHYSICS
	#include <frm/physics/Physics.h>
#endif

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

	m_basicRenderer = BasicRenderer::Create();
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

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Renderer"))
	{
		m_basicRenderer->edit();

		ImGui::TreePop();
	}
	#if FRM_MODULE_PHYSICS
		ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Physics"))
		{
			Physics::Edit();
			
			ImGui::TreePop();
		}
	#endif

	return true;
}

void SceneTest::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	Camera* drawCamera = World::GetDrawCamera();
	Camera* cullCamera = World::GetCullCamera();

	m_basicRenderer->nextFrame((float)getDeltaTime(), drawCamera, cullCamera);
	m_basicRenderer->draw((float)getDeltaTime(), drawCamera, cullCamera);

	// Manually call drawIm3d() so that we can pass the scene depth buffer.
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
