#include "BasicRenderer.h"

#include <frm/core/geom.h>
#include <frm/core/memory.h>
#include <frm/core/types.h>
#include <frm/core/BasicMaterial.h>
#include <frm/core/Buffer.h>
#include <frm/core/Camera.h>
#include <frm/core/Component.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Scene.h>
#include <frm/core/Shader.h>
#include <frm/core/ShadowAtlas.h>
#include <frm/core/Texture.h>

#include <imgui/imgui.h>

#include <EASTL/vector.h>

			#include "AppSample3d.h"

namespace frm {

// PUBLIC

BasicRenderer* BasicRenderer::Create(int _resolutionX, int _resolutionY, uint32 _flags)
{
	BasicRenderer* ret = FRM_NEW(BasicRenderer(_resolutionX, _resolutionY, _flags));

	return ret;
}

void BasicRenderer::Destroy(BasicRenderer*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

void BasicRenderer::draw(float _dt)
{
	PROFILER_MARKER("BasicRenderer::draw");

	GlContext* ctx = GlContext::GetCurrent();

	const bool isPostProcess       = BitfieldGet(flags, (uint32)Flag_PostProcess);
	const bool isFXAA              = BitfieldGet(flags, (uint32)Flag_FXAA);
	const bool isTAA               = BitfieldGet(flags, (uint32)Flag_TAA);
	const bool isInterlaced        = BitfieldGet(flags, (uint32)Flag_Interlaced);
	const bool isWriteToBackBuffer = BitfieldGet(flags, (uint32)Flag_WriteToBackBuffer);
	const bool isWireframe         = BitfieldGet(flags, (uint32)Flag_WireFrame);

	sceneCamera.copyFrom(*Scene::GetDrawCamera()); // \todo separate draw/cull cameras
	if (isTAA)
	{
		const int kFrameIndex = (int)(ctx->getFrameIndex() & 1);
		const vec2 kOffsets[2] = { vec2(0.5f, 0.0f), vec2(0.0f, 0.5f) };
		float jitterScale = 1.0f;
		sceneCamera.m_proj[2][0] = kOffsets[kFrameIndex].x * 2.0f / (float)resolution.x * jitterScale;
		sceneCamera.m_proj[2][1] = kOffsets[kFrameIndex].y * 2.0f / (float)resolution.y * jitterScale;
	}
	if (isInterlaced)
	{
		// NB offset by the full target res, *not* the checkerboard res
		const int kFrameIndex = (int)(ctx->getFrameIndex() & 1);
		const vec2 kOffsets[2] = { vec2(0.0f, 0.0f), vec2(1.0f, 0.0f) };
		sceneCamera.m_proj[2][0] += kOffsets[kFrameIndex].x * 2.0f / (float)resolution.x;
		sceneCamera.m_proj[2][1] += kOffsets[kFrameIndex].y * 2.0f / (float)resolution.y;
	}
	sceneCamera.m_viewProj = sceneCamera.m_proj * sceneCamera.m_view;
	sceneCamera.updateGpuBuffer();

	if (!pauseUpdate)
	{
	 // \todo can skip updates if nothing changed
		updateMaterialInstances();
		updateDrawCalls();
		updateImageLightInstances();
	}
	if (sceneDrawCalls.empty() && imageLightInstances.empty())
	{
		return;
	}

	postProcessData.motionBlurScale = motionBlurTargetFps * _dt;
	bfPostProcessData->setData(sizeof(PostProcessData), &postProcessData);

	const vec2 texelSize = vec2(1.0f) / vec2(fbGBuffer->getWidth(), fbGBuffer->getHeight());
	
	for (int i = 0; i < Target_Count; ++i)
	{
		renderTargets[i].nextFrame();
	}

	// Get current render targets.
	Texture* txGBuffer0 = renderTargets[Target_GBuffer0].getTexture(0);
	Texture* txGBufferDepthStencil = renderTargets[Target_GBufferDepthStencil].getTexture(0);
	Texture* txVelocityTileMinMax = renderTargets[Target_VelocityTileMinMax].getTexture(0);
	Texture* txVelocityTileNeighborMax = renderTargets[Target_VelocityTileNeighborMax].getTexture(0);
	Texture* txScene = renderTargets[Target_Scene].getTexture(0);
	Texture* txPostProcessResult = renderTargets[Target_PostProcessResult].getTexture(0);
	Texture* txFXAAResult = renderTargets[Target_FXAAResult].getTexture(0);
	Texture* txFinal = renderTargets[Target_Final].getTexture(0);

	// Init framebuffers.
	fbGBuffer->attach(txGBuffer0, GL_COLOR_ATTACHMENT0);
	fbGBuffer->attach(txGBufferDepthStencil, GL_DEPTH_STENCIL_ATTACHMENT);
	fbScene->attach(txScene, GL_COLOR_ATTACHMENT0);
	fbScene->attach(txGBufferDepthStencil, GL_DEPTH_STENCIL_ATTACHMENT);
	fbPostProcessResult->attach(txPostProcessResult, GL_COLOR_ATTACHMENT0);
	fbPostProcessResult->attach(txGBufferDepthStencil, GL_DEPTH_STENCIL_ATTACHMENT);
	fbFXAAResult->attach(txFXAAResult, GL_COLOR_ATTACHMENT0);
	fbFinal->attach(txFinal, GL_COLOR_ATTACHMENT0);

	{	PROFILER_MARKER("Shadow Maps");
		
		glAssert(glColorMask(false, false, false, false));
		glScopedEnable(GL_POLYGON_OFFSET_FILL, GL_TRUE);
		glAssert(glPolygonOffset(8.0f, 1.0f)); // \todo
		
		for (size_t i = 0; i < shadowCameras.size(); ++i)
		{
			const Camera& shadowCamera = shadowCameras[i];
			const ShadowAtlas::ShadowMap* shadowMap = (ShadowAtlas::ShadowMap*)shadowMapAllocations[i];
			const DrawCallMap& drawCalls = shadowDrawCalls[i];
	
			ctx->setFramebuffer(shadowAtlas->getFramebuffer(shadowMap->arrayIndex));
			ctx->setViewport(shadowMap->origin.x, shadowMap->origin.y, shadowMap->size, shadowMap->size);

			// Clear shadow map.
			{	glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
				glAssert(glDepthFunc(GL_ALWAYS));				
				ctx->setShader(shDepthClear);
				ctx->setUniform("uClearDepth", 1.0f);
				ctx->drawNdcQuad();
			}

			// Draw.
			{
				glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
				glAssert(glDepthFunc(GL_LESS));
				glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
		
				for (auto& it : drawCalls)
				{
					const DrawCall& drawCall = it.second;
					if (!drawCall.shaders[Pass_Shadow])
					{
						continue;
					}

					ctx->setShader(drawCall.shaders[Pass_Shadow]);
					ctx->bindBuffer(shadowCamera.m_gpuBuffer);
					ctx->setUniform("uTexelSize", vec2(shadowMap->uvScale));
					bindAndDraw(drawCall);
				}

				if (drawCallback)
				{
					PROFILER_MARKER("drawCallback");
					drawCallback(Pass_Shadow, shadowCamera);
				}
			}
		}

		glAssert(glColorMask(true, true, true, true));
	}

	{	PROFILER_MARKER("GBuffer");

		ctx->setFramebufferAndViewport(fbGBuffer);

		{	PROFILER_MARKER("Geometry");

	 		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
			//glAssert(glClearDepth(1.0)); // \todo set the depth clear value based on the camera's projection mode
			glAssert(glClearStencil(0));
			glAssert(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
			glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
			glAssert(glDepthFunc(GL_LESS));
			glScopedEnable(GL_STENCIL_TEST, GL_TRUE);
			glAssert(glStencilFunc(GL_ALWAYS, 0xff, 0x01)); // \todo only stencil dynamic objects
			glAssert(glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE));
			glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?

			for (auto& it : sceneDrawCalls)
			{
				const DrawCall& drawCall = it.second;
				if (!drawCall.shaders[Pass_GBuffer])
				{
					continue;
				}

				ctx->setShader(drawCall.shaders[Pass_GBuffer]);
				ctx->bindBuffer(sceneCamera.m_gpuBuffer);
				ctx->setUniform("uTexelSize", texelSize);
				bindAndDraw(drawCall);				
			}

			if (drawCallback)
			{
				PROFILER_MARKER("drawCallback");
				drawCallback(Pass_GBuffer, sceneCamera);
			}
		}

		{	PROFILER_MARKER("Static Velocity");
	
			glScopedEnable(GL_STENCIL_TEST, GL_TRUE);
			glAssert(glStencilFunc(GL_NOTEQUAL, 0xff, 0x01));
			glAssert(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
			glAssert(glColorMask(false, false, true, true));

			ctx->setShader(shStaticVelocity);
			ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
			ctx->drawNdcQuad(&sceneCamera);

			glAssert(glColorMask(true, true, true, true));
		}

		{	PROFILER_MARKER("Velocity Dilation");

			{	PROFILER_MARKER("Tile Min/Max");

				FRM_ASSERT(shVelocityMinMax->getLocalSize().x == motionBlurTileWidth);


				ctx->setShader(shVelocityMinMax);
				ctx->bindTexture("txGBuffer0", txGBuffer0);
				ctx->bindImage("txVelocityTileMinMax", txVelocityTileMinMax, GL_WRITE_ONLY);
				ctx->dispatch(txVelocityTileMinMax->getWidth(), txVelocityTileMinMax->getHeight()); // 1 group per texel

				glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			}
			{	PROFILER_MARKER("Neighborhood Max");

				ctx->setShader(shVelocityNeighborMax);
				ctx->bindTexture("txVelocityTileMinMax", txVelocityTileMinMax);
				ctx->bindImage("txVelocityTileNeighborMax", txVelocityTileNeighborMax, GL_WRITE_ONLY);
				ctx->dispatch(txVelocityTileNeighborMax);

				glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			}
		}
	}

	{	PROFILER_MARKER("Scene");

		ctx->setFramebufferAndViewport(fbScene);

		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glAssert(glDepthFunc(GL_EQUAL));

		if (imageLightInstances.size() > 0 && imageLightInstances[0].texture && imageLightInstances[0].isBackground)
		{
			ctx->setShader(shImageLightBg);
			ctx->bindTexture("txEnvmap", imageLightInstances[0].texture);
			ctx->drawNdcQuad(&sceneCamera);
		}
		else
		{
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, Abs(sceneCamera.m_far)));
			glAssert(glClear(GL_COLOR_BUFFER_BIT));
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}

		glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
		for (auto& it : sceneDrawCalls)
		{
			const DrawCall& drawCall = it.second;
			if (!drawCall.shaders[Pass_Scene])
			{
				continue;
			}

			ctx->setShader(drawCall.shaders[Pass_Scene]);
			ctx->bindTexture("txGBuffer0", txGBuffer0);
			ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
			ctx->bindBuffer(sceneCamera.m_gpuBuffer);

			ctx->setUniform("uLightCount", (int)lightInstances.size());
			if (bfLights)
			{
				ctx->bindBuffer("bfLights", bfLights);
			}

			ctx->setUniform("uShadowLightCount", (int)shadowLightInstances.size());
			if (bfShadowLights)
			{
				ctx->bindBuffer("bfShadowLights", bfShadowLights);
			}
			ctx->bindTexture("txShadowMap", shadowAtlas->getTexture());
			
			// \todo support >1 image light
			ctx->setUniform("uImageLightCount", (int)imageLightInstances.size());
			if (imageLightInstances.size() > 0 && imageLightInstances[0].texture)
			{
				ctx->bindTexture("txImageLight", imageLightInstances[0].texture);
				ctx->setUniform("uImageLightBrightness", imageLightInstances[0].brightness);
			}
			else
			{
				ctx->setUniform("uImageLightCount", 0);
			}
			ctx->setUniform("uTexelSize", texelSize);
			bindAndDraw(drawCall);
		}

		if (drawCallback)
		{
			PROFILER_MARKER("drawCallback");
			drawCallback(Pass_Scene, sceneCamera);
		}
	}

	if (isWireframe)
	{
		ctx->setFramebufferAndViewport(fbScene);

		glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_BLEND, GL_TRUE);
		glAssert(glDepthFunc(GL_LEQUAL));
		glAssert(glLineWidth(3.0f));
	
		for (auto& it : sceneDrawCalls)
		{
			const DrawCall& drawCall = it.second;
			if (!drawCall.shaders[Pass_Wireframe])
			{
				continue;
			}

			ctx->setShader(drawCall.shaders[Pass_Wireframe]); // reset shader per call because we want to clear all the bindings to avoid running out of slots
			ctx->bindBuffer(sceneCamera.m_gpuBuffer);
			ctx->setUniform("uTexelSize", texelSize);
			bindAndDraw(drawCall);
		}

		if (drawCallback)
		{
			PROFILER_MARKER("drawCallback");
			drawCallback(Pass_Wireframe, sceneCamera);
		}

		glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
	}

	//m_luminanceMeter.draw(ctx, _dt, txScene);
	
	if (isPostProcess)
	{	
		PROFILER_MARKER("Post Process");

		ctx->setShader(shPostProcess);
		ctx->bindBuffer(bfPostProcessData);
		ctx->bindBuffer(sceneCamera.m_gpuBuffer);
		ctx->bindTexture("txScene", txScene);
		ctx->bindTexture("txGBuffer0", txGBuffer0);
		ctx->bindTexture("txVelocityTileNeighborMax", txVelocityTileNeighborMax);
		ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
		ctx->bindImage("txOut", txPostProcessResult, GL_WRITE_ONLY);
		ctx->dispatch(txPostProcessResult);

		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
	}
	else
	{
		ctx->blitFramebuffer(fbScene, fbPostProcessResult, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (isFXAA)
	{
		PROFILER_MARKER("FXAA");
		
		ctx->setShader(shFXAA);
		ctx->bindTexture("txIn", txPostProcessResult);
		ctx->bindImage("txOut", txFXAAResult, GL_WRITE_ONLY);
		ctx->dispatch(txFXAAResult);

		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
	}
	else if (!isTAA && !isInterlaced)
	{
		ctx->blitFramebuffer(fbPostProcessResult, fbFinal, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (isTAA || isInterlaced)
	{
		PROFILER_MARKER("TAA Resolve");
		
		const vec2 resolveKernel = vec2(-taaSharpen, (1.0f + (2.0f * taaSharpen)) / 2.0f);
		Texture* txCurrent = isFXAA ? txFXAAResult : txPostProcessResult;
		Texture* txPrevious = isInterlaced ? (isFXAA ? renderTargets[Target_FXAAResult].getTexture(-1) : renderTargets[Target_PostProcessResult].getTexture(-1)) : nullptr;
		Texture* txCurrentResolve  = renderTargets[Target_TAAResolve].getTexture(0);
		Texture* txPreviousResolve = renderTargets[Target_TAAResolve].getTexture(-1);
		Texture* txPreviousGBuffer0 = renderTargets[Target_GBuffer0].getTexture(-1);
				
		ctx->setShader(shTAAResolve);
		ctx->setUniform("uFrameIndex", (int)(ctx->getFrameIndex() & 1));
		ctx->setUniform("uResolveKernel", resolveKernel);
		ctx->bindBuffer(sceneCamera.m_gpuBuffer);
		ctx->bindTexture("txGBuffer0", txGBuffer0);
		ctx->bindTexture("txPreviousGBuffer0", txPreviousGBuffer0);
		ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
		ctx->bindTexture("txCurrent", txCurrent);
		ctx->bindTexture("txPrevious", txPrevious);
		ctx->bindTexture("txPreviousResolve", txPreviousResolve);
		ctx->bindImage("txCurrentResolve", txCurrentResolve, GL_WRITE_ONLY);
		ctx->bindImage("txFinal", txFinal, GL_WRITE_ONLY);
		ctx->dispatch(txFinal);
		
		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
	}
	else if (isFXAA)
	{
		ctx->blitFramebuffer(fbFXAAResult, fbFinal, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (isWriteToBackBuffer)
	{
		ctx->blitFramebuffer(fbFinal, nullptr, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
}

bool BasicRenderer::edit()
{
	bool ret = false;

	ret |= ImGui::Checkbox("Pause Update", &pauseUpdate);
	ret |= ImGui::Checkbox("Cull by Submesh", &cullBySubmesh);
	ret |= ImGui::SliderFloat("Motion Blur Target FPS", &motionBlurTargetFps, 0.0f, 90.0f);
	if (BitfieldGet(flags, (uint32)Flag_TAA))
	{
		ret |= ImGui::SliderFloat("TAA Sharpen", &taaSharpen, 0.0f, 2.0f);
	}

	//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Material Sampler"))
	{
		float lodBias = ssMaterial->getLodBias();
		if (ImGui::SliderFloat("LOD Bias", &lodBias, -4.0f, 4.0f))
		{
			ssMaterial->setLodBias(lodBias);
		}

		float anisotropy = ssMaterial->getAnisotropy();
		if (ImGui::SliderFloat("Anisotropy", &anisotropy, 1.0f, 16.0f))
		{
			ssMaterial->setAnisotropy(anisotropy);
		}

		ImGui::TreePop();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Flags"))
	{
		ret |= editFlag("Post Process", Flag_PostProcess);
		if (editFlag("FXAA", Flag_FXAA))
		{
			ret = true;
			initRenderTargets();
		}
		if (editFlag("TAA", Flag_TAA))
		{
			ret = true;
			initRenderTargets();
		}
		if (editFlag("Interlaced", Flag_Interlaced))
		{
			ret = true;
			initRenderTargets();
		}

		ret |= editFlag("Write to Backbuffer", Flag_WriteToBackBuffer);

		ret |= editFlag("Wireframe", Flag_WireFrame);

		if (ret)
		{
			// update material sampeler if interleaved or TAA
			const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);
			const bool isInterlaced = BitfieldGet(flags, (uint32)Flag_Interlaced);
			if (isTAA || isInterlaced)
			{
				ssMaterial->setLodBias(-1.0f);
			}
			else
			{
				ssMaterial->setLodBias(0.0f);					
			}

			shTAAResolve->addGlobalDefines({ 
				String<32>("TAA %d", !!isTAA).c_str(), 
				String<32>("INTERLACED %d", !!isInterlaced).c_str() 
				});
		}

		ImGui::TreePop();
	}

	return ret;
}

void BasicRenderer::setResolution(int _resolutionX, int _resolutionY)
{
	ivec2 newResolution = ivec2(_resolutionX, _resolutionY);
	if (newResolution != resolution)
	{
		resolution = newResolution;
		initRenderTargets();
	}
}

// PRIVATE

BasicRenderer::BasicRenderer(int _resolutionX, int _resolutionY, uint32 _flags)
{
	resolution = ivec2(_resolutionX, _resolutionY);
	flags      = _flags;

	initShaders();
	initRenderTargets();

	bfPostProcessData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(PostProcessData), GL_DYNAMIC_STORAGE_BIT);
	bfPostProcessData->setName("bfPostProcessData");

	ssMaterial = TextureSampler::Create(GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, 4.0f); // \todo global anisotropy config
	if (BitfieldGet(flags, (uint32)Flag_TAA))
	{
		ssMaterial->setLodBias(-1.0f);
	}

	fbGBuffer           = Framebuffer::Create();
	fbScene             = Framebuffer::Create();
	fbPostProcessResult = Framebuffer::Create();
	fbFXAAResult        = Framebuffer::Create();
	fbFinal             = Framebuffer::Create();

	shadowAtlas = ShadowAtlas::Create(4096, 256, GL_DEPTH_COMPONENT24); // \todo config

	//FRM_VERIFY(m_luminanceMeter.init(_resolutionY / 2));
	//m_colorCorrection.m_luminanceMeter = &m_luminanceMeter;
}

BasicRenderer::~BasicRenderer()
{
	//m_luminanceMeter.shutdown();

	shutdownRenderTargets();
	shutdownShaders();
	
	clearDrawCalls(sceneDrawCalls);
	sceneDrawCalls.clear();
	for (DrawCallMap& drawCallMap : shadowDrawCalls)
	{
		clearDrawCalls(drawCallMap);
		drawCallMap.clear();
	}
	shadowDrawCalls.clear();

	Framebuffer::Destroy(fbGBuffer);
	Framebuffer::Destroy(fbScene);
	Framebuffer::Destroy(fbPostProcessResult);
	Framebuffer::Destroy(fbFXAAResult);
	Framebuffer::Destroy(fbFinal);

	Buffer::Destroy(bfMaterials);
	Buffer::Destroy(bfLights);
	Buffer::Destroy(bfShadowLights);
	Buffer::Destroy(bfImageLights);
	Buffer::Destroy(bfPostProcessData);

	for (size_t i = 0; i < shadowMapAllocations.size(); ++i)
	{			
		shadowAtlas->free((ShadowAtlas::ShadowMap*&)shadowMapAllocations[i]);
	}
	ShadowAtlas::Destroy(shadowAtlas);
}

bool BasicRenderer::editFlag(const char* _name, Flag _flag)
{
	bool flagValue = BitfieldGet(flags, (uint32)_flag);
	bool ret = ImGui::Checkbox(_name, &flagValue);
	flags = BitfieldSet(flags, _flag, flagValue);
	return ret;
}

void BasicRenderer::initRenderTargets()
{
	shutdownRenderTargets();

	const bool isFXAA = BitfieldGet(flags, (uint32)Flag_FXAA);
	const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);
	const bool isInterlaced = BitfieldGet(flags, (uint32)Flag_Interlaced);
	const ivec2 fullResolution = resolution;
	const ivec2 interlacedResolution = isInterlaced ? ivec2(fullResolution.x / 2, fullResolution.y) : fullResolution;

	renderTargets[Target_GBuffer0].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, isInterlaced ? 2 : 1);
	renderTargets[Target_GBuffer0].setName("#BasicRenderer_txGBuffer0");

	renderTargets[Target_GBufferDepthStencil].init(interlacedResolution.x, interlacedResolution.y, GL_DEPTH32F_STENCIL8, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_GBufferDepthStencil].setName("#BasicRenderer_txGBufferDepth");
		
	//FRM_ASSERT(interlacedResolution.x % motionBlurTileWidth == 0 && interlacedResolution.y % motionBlurTileWidth == 0);
	renderTargets[Target_VelocityTileMinMax].init(interlacedResolution.x / motionBlurTileWidth, interlacedResolution.y / motionBlurTileWidth, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileMinMax].setName("#BasicRenderer_txVelocityTileMinMax");

	renderTargets[Target_VelocityTileNeighborMax].init(interlacedResolution.x / motionBlurTileWidth, interlacedResolution.y / motionBlurTileWidth, GL_RG16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileNeighborMax].setName("#BasicRenderer_txVelocityTileNeighborMax");

	renderTargets[Target_Scene].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR, 1); // RGB = color, A = abs(linear depth)
	renderTargets[Target_Scene].setName("#BasicRenderer_txScene");

	renderTargets[Target_PostProcessResult].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, (isInterlaced && !isFXAA) ? 2 : 1);
	renderTargets[Target_PostProcessResult].setName("#BasicRenderer_txPostProcessResult");

	renderTargets[Target_FXAAResult].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, isFXAA ? (isInterlaced ? 2 : 1) : 0);
	renderTargets[Target_FXAAResult].setName("#BasicRenderer_txFXAAResult");

	renderTargets[Target_TAAResolve].init(fullResolution.x, fullResolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, (isTAA || isInterlaced) ? 2 : 1);
	renderTargets[Target_TAAResolve].setName("#BasicRenderer_txTAAResolve");

	renderTargets[Target_Final].init(fullResolution.x, fullResolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, 1);
	renderTargets[Target_Final].setName("#BasicRenderer_txFinal");
}

void BasicRenderer::shutdownRenderTargets()
{
	for (int i = 0; i < Target_Count; ++i)
	{
		renderTargets[i].shutdown();
	}
}

void BasicRenderer::initShaders()
{
	shStaticVelocity      = Shader::CreateVsFs("shaders/NdcQuad_vs.glsl", "shaders/BasicRenderer/StaticVelocity.glsl");
	shVelocityMinMax      = Shader::CreateCs("shaders/BasicRenderer/VelocityMinMax.glsl", motionBlurTileWidth);
	shVelocityNeighborMax = Shader::CreateCs("shaders/BasicRenderer/VelocityNeighborMax.glsl", 8, 8);
	shImageLightBg	      = Shader::CreateVsFs("shaders/Envmap_vs.glsl", "shaders/Envmap_fs.glsl", { "ENVMAP_CUBE" } );
	shPostProcess         = Shader::CreateCs("shaders/BasicRenderer/PostProcess.glsl", 8, 8);
	shFXAA                = Shader::CreateCs("shaders/BasicRenderer/FXAA.glsl", 8, 8);
	shDepthClear          = Shader::CreateVsFs("shaders/BasicRenderer/DepthClear.glsl", "shaders/BasicRenderer/DepthClear.glsl"); 

	const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);
	const bool isInterlaced = BitfieldGet(flags, (uint32)Flag_Interlaced);
	shTAAResolve          = Shader::CreateCs("shaders/BasicRenderer/TAAResolve.glsl", 8, 8, 1,
		{
			String<32>("TAA %d", !!isTAA).c_str(), 
			String<32>("INTERLACED %d", !!isInterlaced).c_str() 
		});
}

void BasicRenderer::shutdownShaders()
{
	Shader::Release(shStaticVelocity);
	Shader::Release(shVelocityMinMax);
	Shader::Release(shVelocityNeighborMax);
	Shader::Release(shImageLightBg);
	Shader::Release(shPostProcess);
	Shader::Release(shFXAA);
	Shader::Release(shDepthClear);
	Shader::Release(shTAAResolve);

	for (auto& it : shaderMap)
	{
		Shader::Release(it.second);
	}
	shaderMap.clear();
}

void BasicRenderer::updateMaterialInstances()
{
	PROFILER_MARKER_CPU("updateMaterialInstances");

	materialInstances.resize(BasicMaterial::GetInstanceCount());
	
	for (int i = 0; i < BasicMaterial::GetInstanceCount(); ++i)
	{
		BasicMaterial* material = BasicMaterial::GetInstance(i);

		MaterialInstance& materialInstance = materialInstances[i];
		materialInstance.baseColorAlpha    = vec4(material->getBaseColor(), material->getAlpha());
		materialInstance.emissiveColor     = vec4(material->getEmissiveColor(), 1.0f);
		materialInstance.metallic          = material->getMetallic();
		materialInstance.roughness         = material->getRoughness();
		materialInstance.reflectance       = material->getReflectance();
		materialInstance.height            = material->getHeight();
	}

	GLsizei bfMaterialsSize = (GLsizei)(sizeof(MaterialInstance) * materialInstances.size());
	updateBuffer(bfMaterials, "bfMaterials", bfMaterialsSize, materialInstances.data());
}

Shader* BasicRenderer::findShader(ShaderMapKey _key)
{
	static constexpr const char* kPassDefines[] =
	{
		"Pass_Shadow",
		"Pass_GBuffer",
		"Pass_Scene",
		"Pass_Wireframe",
	};
	static constexpr const char* kGeometryDefines[] =
	{
		"Geometry_Mesh",
		"Geometry_SkinnedMesh",
	};
	static constexpr const char* kMaterialDefines[] =
	{
		"Material_AlphaTest",
		"Material_AlphaDither",
	};

	Shader*& ret = shaderMap[_key];
	if (!ret)
	{
		eastl::vector<const char*> defines;

		for (int i = 0; i < Pass_Count; ++i)
		{
			if (BitfieldGet(_key.fields.pass, i))
			{
				defines.push_back(kPassDefines[i]);
			}
		}

		for (int i = 0; i < GeometryType_Count; ++i)
		{
			if (BitfieldGet(_key.fields.geometryType, i))
			{
				defines.push_back(kGeometryDefines[i]);
			}
		}	

		for (int i = 0; i < BasicMaterial::Flag_Count; ++i)
		{
			if (BitfieldGet(_key.fields.materialFlags, i))
			{
				defines.push_back(kMaterialDefines[i]);
			}
		}

		ret = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", std::initializer_list<const char*>(defines.begin(), defines.end()));
	}

	return ret;
}

void BasicRenderer::updateDrawCalls()
{
	PROFILER_MARKER("BasicRenderer::updateDrawCalls");

	const Camera* sceneCullCamera = Scene::GetCullCamera();
	
// \todo move these aux lists to the class, split this into multiple functions again

	eastl::vector<Component_BasicRenderable*> culledSceneRenderables;
	culledSceneRenderables.reserve(Component_BasicRenderable::s_instances.size());

	eastl::vector<Component_BasicRenderable*> shadowRenderables;
	shadowRenderables.reserve(Component_BasicRenderable::s_instances.size());

	eastl::vector<Component_BasicLight*> culledLights;
	culledLights.reserve(Component_BasicLight::s_instances.size());
	
	eastl::vector<Component_BasicLight*> culledShadowLights;
	culledShadowLights.reserve(Component_BasicLight::s_instances.size());

	sceneBounds.m_min = shadowSceneBounds.m_min = vec3( FLT_MAX);
	sceneBounds.m_max = shadowSceneBounds.m_max = vec3(-FLT_MAX);

// Phase 1: Cull renderables, gather shadow renderables, generate scene and shadow scene bounds.
// \todo LOD selection should happen here.
	{	PROFILER_MARKER_CPU("Phase 1");

		for (Component_BasicRenderable* renderable : Component_BasicRenderable::s_instances)
		{
			Node* sceneNode = renderable->getNode();
			if (!sceneNode->isActive() || !renderable->m_mesh || renderable->m_materials.empty())
			{
				continue;
			}

			mat4 world = sceneNode->getWorldMatrix();
			Sphere bs = renderable->m_mesh->getBoundingSphere();
			bs.transform(world);
			AlignedBox bb = renderable->m_mesh->getBoundingBox();
			bb.transform(world);
			sceneBounds.m_min = Min(sceneBounds.m_min, bb.m_min);
			sceneBounds.m_max = Max(sceneBounds.m_max, bb.m_max);

			if (renderable->m_castShadows)
			{
				shadowSceneBounds.m_min = Min(shadowSceneBounds.m_min, bb.m_min);
				shadowSceneBounds.m_max = Max(shadowSceneBounds.m_max, bb.m_max);
				shadowRenderables.push_back(renderable);
			}

			if (sceneCullCamera->m_worldFrustum.insideIgnoreNear(bs) && sceneCullCamera->m_worldFrustum.insideIgnoreNear(bb))
			{
				culledSceneRenderables.push_back(renderable);
			}
		}
	}

// Phase 2: Generate draw calls for culled scene renderables, optionally cull by submesh.
	{	PROFILER_MARKER_CPU("Phase 2");
	
		clearDrawCalls(sceneDrawCalls);
		for (Component_BasicRenderable* renderable : culledSceneRenderables)
		{
			Node* sceneNode = renderable->getNode();
			mat4 world = sceneNode->getWorldMatrix();
			
			int submeshCount = Min((int)renderable->m_materials.size(), renderable->m_mesh->getSubmeshCount());
			for (int submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
			{
				if (!renderable->m_materials[submeshIndex]) // skip submesh if no material set
				{
					continue;
				}

				if (submeshIndex > 0 && cullBySubmesh)
				{
					Sphere bs = renderable->m_mesh->getBoundingSphere(submeshIndex);
					bs.transform(world);
					AlignedBox bb = renderable->m_mesh->getBoundingBox(submeshIndex);
					bb.transform(world);

					if (!sceneCullCamera->m_worldFrustum.insideIgnoreNear(bs) || !sceneCullCamera->m_worldFrustum.insideIgnoreNear(bb))
					{
						continue;
					}
				}

				addDrawCall(renderable, submeshIndex, sceneDrawCalls);

				// If we added submesh index 0, assume we don't need to look at the other submeshes since 0 represents the whole mesh.
				if (submeshIndex == 0)
				{
					break;
				}
			}
		}
	}

// Phase 3: Cull lights, generate shadow light cameras.
	{	PROFILER_MARKER("Phase 3");
		
		shadowCameras.clear();
		// \todo map allocations -> lights, avoid realloc every frame
		for (size_t i = 0; i < shadowMapAllocations.size(); ++i)
		{			
			shadowAtlas->free((ShadowAtlas::ShadowMap*&)shadowMapAllocations[i]);
		}
		shadowMapAllocations.clear();

		for (Component_BasicLight* light : Component_BasicLight::s_instances)
		{
			Node* sceneNode = light->getNode();
			if (!sceneNode->isActive() || light->m_colorBrightness.w <= 0.0f)
			{
				continue;
			}

			// \todo cull here

			if (light->m_castShadows)
			{
				ShadowAtlas::ShadowMap* shadowMap = shadowAtlas->alloc(1.0f);
				if (!shadowMap)
				{
					// alloc failed, draw as a non-shadow light
					culledLights.push_back(light);
					continue;
				}
				shadowMapAllocations.push_back(shadowMap);

				const vec3 lightPosition = sceneNode->getWorldPosition();
				const vec3 lightDirection = sceneNode->getWorldMatrix()[2].xyz();

				// \todo generate shadow camera + matrix
				Camera& shadowCamera = shadowCameras.push_back();
				
				switch (light->m_type)
				{
					default: break;
					case Component_BasicLight::Type_Direct:
					{
						const vec3 shadowSceneOrigin = shadowSceneBounds.getOrigin();
						
						shadowCamera.setOrtho(1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 1.0f);
						shadowCamera.m_world = LookAt(shadowSceneOrigin - lightDirection, shadowSceneOrigin);
						shadowCamera.update();

					 // \todo center on the scene camera frustum
						vec3 shadowSceneBoundsMin(FLT_MAX);
						vec3 shadowSceneBoundsMax(-FLT_MAX);
						vec3 verts[8];
						shadowSceneBounds.getVertices(verts);
						for (int i = 0; i < 8; ++i)
						{
							vec4 v = shadowCamera.m_viewProj * vec4(verts[i], 1.0f);
							shadowSceneBoundsMin.x = Min(shadowSceneBoundsMin.x, v.x);
							shadowSceneBoundsMin.y = Min(shadowSceneBoundsMin.y, v.y);
							shadowSceneBoundsMin.z = Min(shadowSceneBoundsMin.z, v.z);
							shadowSceneBoundsMax.x = Max(shadowSceneBoundsMax.x, v.x);
							shadowSceneBoundsMax.y = Max(shadowSceneBoundsMax.y, v.y);
							shadowSceneBoundsMax.z = Max(shadowSceneBoundsMax.z, v.z);
						}
						vec3 scale = vec3(2.0f) /  (shadowSceneBoundsMax - shadowSceneBoundsMin);
						vec3 bias  = vec3(-0.5f) * (shadowSceneBoundsMax + shadowSceneBoundsMin) * scale;
						#ifdef FRM_NDC_Z_ZERO_TO_ONE
							scale.z = 1.0f / (shadowSceneBoundsMax.z - shadowSceneBoundsMin.z);
							bias.z = -shadowSceneBoundsMin.z * scale.z;
						#endif
						//bias = ceil(bias * shadowMapSize * 0.5f) / shadowMapSize * 0.5f;

						// Create a 1 texel empty border to prevent bleeding with clamp-to-edge lookup.
						const float kBorder = 2.0f / (float)shadowMap->size;
						scale.x = scale.x * (1.0f - kBorder);
						scale.y = scale.y * (1.0f - kBorder);
						bias.x  = bias.x + kBorder * 0.5f;
						bias.y  = bias.y + kBorder * 0.5f;

						mat4 cropMatrix(
							scale.x,  0.0f,    0.0f,    bias.x,
							0.0f,     scale.y, 0.0f,    bias.y,
							0.0f,     0.0f,    scale.z, bias.z,
							0.0f,     0.0f,    0.0f,    1.0f
							);
						
						shadowCamera.setProj(cropMatrix * shadowCamera.m_proj, shadowCamera.m_projFlags);
						shadowCamera.updateView();
	
						//AppSample3d::DrawFrustum(shadowCamera.m_worldFrustum);

						break;
					}
					case Component_BasicLight::Type_Spot:
					{						
						shadowCamera.setPerspective(Radians(light->m_coneOuterAngle) * 2.0f, 1.0f, 0.02f, light->m_radius);
						shadowCamera.m_world = LookAt(lightPosition, lightPosition + lightDirection);
						shadowCamera.update();

						//AppSample3d::DrawFrustum(shadowCamera.m_worldFrustum);

						break;
					}
					
				};

				// \todo apply uv scale/bias to proj matrix				
				shadowCamera.updateGpuBuffer();

				culledShadowLights.push_back(light);
			}
			else
			{
				culledLights.push_back(light);
			}
		}
	}

// Phase 4: Update light instances.
	{	PROFILER_MARKER("Phase 4");

		lightInstances.clear();
		for (Component_BasicLight* light : culledLights)
		{
			mat4 world = light->getNode()->getWorldMatrix();

			LightInstance& lightInstance = lightInstances.push_back();
			lightInstance.position       = vec4(world[3].xyz(), (float)light->m_type);
			lightInstance.direction      = vec4(normalize(world[2].xyz()), 0.0f);
			lightInstance.color          = vec4(light->m_colorBrightness.xyz() * light->m_colorBrightness.w, light->m_colorBrightness.w);

			lightInstance.invRadius2     = 1.0f / (light->m_radius * light->m_radius);

			const float cosOuter         = cosf(Radians(light->m_coneOuterAngle));
			const float cosInner         = cosf(Radians(light->m_coneInnerAngle));
			lightInstance.spotScale      = 1.0f / Max(cosInner - cosOuter, 1e-4f);
			lightInstance.spotBias       = -cosOuter * lightInstance.spotScale;
		}
		GLsizei bfLightsSize = (GLsizei)(sizeof(LightInstance) * lightInstances.size());
		updateBuffer(bfLights, "bfLights", bfLightsSize, lightInstances.data());

		shadowLightInstances.clear();
		for (size_t i = 0; i < culledShadowLights.size(); ++i)
		{
			const Component_BasicLight* light = culledShadowLights[i];
			const ShadowAtlas::ShadowMap* shadowMap = (ShadowAtlas::ShadowMap*)shadowMapAllocations[i];
			mat4 world = light->getNode()->getWorldMatrix();

			ShadowLightInstance& shadowLightInstance = shadowLightInstances.push_back();
			shadowLightInstance.position             = vec4(world[3].xyz(), (float)light->m_type);
			shadowLightInstance.direction            = vec4(normalize(world[2].xyz()), 0.0f);
			shadowLightInstance.color                = vec4(light->m_colorBrightness.xyz() * light->m_colorBrightness.w, light->m_colorBrightness.w);
			
			shadowLightInstance.invRadius2           = 1.0f / (light->m_radius * light->m_radius);

			const float cosOuter                     = cosf(Radians(light->m_coneOuterAngle));
			const float cosInner                     = cosf(Radians(light->m_coneInnerAngle));
			shadowLightInstance.spotScale            = 1.0f / Max(cosInner - cosOuter, 1e-4f);
			shadowLightInstance.spotBias             = -cosOuter * shadowLightInstance.spotScale;

			shadowLightInstance.worldToShadow = shadowCameras[i].m_viewProj;
			shadowLightInstance.uvBias        = shadowMap->uvBias;
			shadowLightInstance.uvScale       = shadowMap->uvScale;
			shadowLightInstance.arrayIndex    = (float)shadowMap->arrayIndex;
		}
		GLsizei bfShadowLightsSize = (GLsizei)(sizeof(ShadowLightInstance) * shadowLightInstances.size());
		updateBuffer(bfShadowLights, "bfShadowLights", bfShadowLightsSize, shadowLightInstances.data());
	}


// Phase 5: Cull shadow renderables per shadow light, generate draw calls.
	{	PROFILER_MARKER("Phase 5");

		for (DrawCallMap& drawCallMap : shadowDrawCalls)
		{
			clearDrawCalls(drawCallMap);
		}
		shadowDrawCalls.clear();

		for (size_t i = 0; i < shadowCameras.size(); ++i)
		{
			const Camera& shadowCamera = shadowCameras[i];
			DrawCallMap& drawCallMap = shadowDrawCalls.push_back();

			for (Component_BasicRenderable* renderable : shadowRenderables)
			{
				Node* sceneNode = renderable->getNode();
				mat4 world = sceneNode->getWorldMatrix();
				
				int submeshCount = Min((int)renderable->m_materials.size(), renderable->m_mesh->getSubmeshCount());
				for (int submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
				{
					if (!renderable->m_materials[submeshIndex]) // skip submesh if no material set
					{
						continue;
					}

					if (submeshIndex > 0 && cullBySubmesh)
					{
						Sphere bs = renderable->m_mesh->getBoundingSphere(submeshIndex);
						bs.transform(world);
						AlignedBox bb = renderable->m_mesh->getBoundingBox(submeshIndex);
						bb.transform(world);

						if (!shadowCamera.m_worldFrustum.insideIgnoreNear(bs) || !shadowCamera.m_worldFrustum.insideIgnoreNear(bb))
						{
							continue;
						}
					}

					addDrawCall(renderable, submeshIndex, drawCallMap);

					// If we added submesh index 0, assume we don't need to look at the other submeshes since 0 represents the whole mesh.
					if (submeshIndex == 0)
					{
						break;
					}
				}
			}
		}
	}

// Phase 6: Update draw call instance data.
	{	PROFILER_MARKER("Phase 5");

		auto UpdateDrawCalls = [](DrawCallMap& drawCallMap)
			{
				for (auto& it : drawCallMap)
				{
					DrawCall& drawCall = it.second;

					drawCall.bfInstances = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(DrawInstance) * drawCall.instanceData.size()), 0u, drawCall.instanceData.data());
					drawCall.bfInstances->setName("bfDrawInstances");

					if (!drawCall.skinningData.empty())
					{
						drawCall.bfSkinning = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(mat4) * drawCall.skinningData.size()), 0u, drawCall.skinningData.data());
						drawCall.bfSkinning->setName("bfSkinning");
					}
				}
			};

		UpdateDrawCalls(sceneDrawCalls);
		for (auto& drawCallMap : shadowDrawCalls)
		{
			UpdateDrawCalls(drawCallMap);
		}
	}
	
}

void BasicRenderer::addDrawCall(const Component_BasicRenderable* _renderable, int _submeshIndex, DrawCallMap& map_)
{
	const Node* sceneNode = _renderable->getNode();
	const BasicMaterial* material = _renderable->m_materials[_submeshIndex];
	const Mesh* mesh = _renderable->m_mesh;

	uint64 drawCallKey = 0;	
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)material->getIndex(), 40, 24);
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)mesh->getIndex(),     16, 24);
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)_submeshIndex,        16, 0);

	DrawCall& drawCall           = map_[drawCallKey];
	drawCall.material            = material;
	drawCall.mesh                = mesh;
	drawCall.submeshIndex        = _submeshIndex;

	DrawInstance& drawInstance   = drawCall.instanceData.push_back();
	drawInstance.world           = sceneNode->getWorldMatrix();
	drawInstance.prevWorld       = _renderable->m_prevWorld;
	drawInstance.colorAlpha      = _renderable->m_colorAlpha;
	drawInstance.materialIndex   = _renderable->m_materials[_submeshIndex]->getIndex();
	drawInstance.submeshIndex    = _submeshIndex;

	ShaderMapKey shaderKey;
	shaderKey.value = 0u;

	if (!_renderable->m_pose.empty())
	{
		shaderKey.fields.geometryType = 1u << GeometryType_SkinnedMesh;

		const size_t boneCount = _renderable->m_pose.size();
		drawInstance.skinningOffset = (uint32)(boneCount * (drawCall.instanceData.size() - 1));
		drawCall.skinningData.reserve(drawCall.skinningData.size() + boneCount * 2);
		for (size_t bone = 0; bone < boneCount; ++bone)
		{
			drawCall.skinningData.push_back(_renderable->m_pose[bone]);
			drawCall.skinningData.push_back(_renderable->m_prevPose[bone]);
		}
	}
	else
	{
		shaderKey.fields.geometryType = 1u << GeometryType_Mesh;
	}

	shaderKey.fields.materialFlags = material->getFlags();

	// \todo not all passes are relevant to each draw call list (e.g. shadows only need Pass_Shadow)
	for (int pass = 0; pass < Pass_Count; ++pass)
	{
		if (pass == Pass_Shadow && !_renderable->m_castShadows)
		{
			continue;
		}

		shaderKey.fields.pass = (uint64)1u << pass;
		drawCall.shaders[pass] = findShader(shaderKey);
	}
}

void BasicRenderer::clearDrawCalls(DrawCallMap& map_)
{
	for (auto& it : map_)
	{
		Buffer::Destroy(it.second.bfInstances);
		it.second.instanceData.clear();
		Buffer::Destroy(it.second.bfSkinning);
		it.second.skinningData.clear();
	}
	map_.clear();
}

void BasicRenderer::bindAndDraw(const DrawCall& _drawCall)
{
	GlContext* ctx = GlContext::GetCurrent();

	ctx->bindBuffer(bfMaterials);
	ctx->bindBuffer(_drawCall.bfInstances);
	if (_drawCall.bfSkinning)
	{
		ctx->bindBuffer(_drawCall.bfSkinning);
	}
	_drawCall.material->bind(ssMaterial);
	ctx->setMesh(_drawCall.mesh, _drawCall.submeshIndex);
	ctx->draw((GLsizei)_drawCall.instanceData.size());
}

void BasicRenderer::updateImageLightInstances()
{
	PROFILER_MARKER_CPU("updateImageLightInstances");

	// \todo need a separate system for this, see Component_ImageLight

	FRM_ASSERT(Component_ImageLight::s_instances.size() <= 1); // only 1 image light is currently supported
	
	imageLightInstances.clear();

	for (Component_ImageLight* light : Component_ImageLight::s_instances)
	{
		Node* sceneNode = light->getNode();
		if (!sceneNode->isActive() || light->m_brightness <= 0.0f)
		{
			continue;
		}

		ImageLightInstance& lightInstance = imageLightInstances.push_back();
		lightInstance.brightness   = light->m_brightness;
		lightInstance.isBackground = light->m_isBackground;
		lightInstance.texture      = light->m_texture;
	}
}

void BasicRenderer::updateBuffer(Buffer*& _bf_, const char* _name, GLsizei _size, void* _data)
{
	if (_size == 0)
	{
		return;
	}

	if (_bf_ && _bf_->getSize() != _size)
	{
		Buffer::Destroy(_bf_);
		_bf_ = nullptr;
	}

	if (!_bf_)
	{
		_bf_ = Buffer::Create(GL_SHADER_STORAGE_BUFFER, _size, GL_DYNAMIC_STORAGE_BIT);
		_bf_->setName(_name);
	}

	_bf_->setData(_size, _data);
}

} // namespace frm
