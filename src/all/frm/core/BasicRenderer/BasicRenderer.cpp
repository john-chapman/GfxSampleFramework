#include "BasicRenderer.h"
#include "BasicRenderableComponent.h"
#include "BasicLightComponent.h"
#include "BasicMaterial.h"
#include "ImageLightComponent.h"

#include <frm/core/geom.h>
#include <frm/core/memory.h>
#include <frm/core/types.h>
#include <frm/core/AppSample.h>
#include <frm/core/Buffer.h>
#include <frm/core/Camera.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/DrawMesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/ShadowAtlas.h>
#include <frm/core/Texture.h>
#include <frm/core/world/World.h>

#include <frm/vr/VRContext.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

#include <EASTL/vector.h>

namespace frm {

// PUBLIC

BasicRenderer* BasicRenderer::Create(Flags _flags)
{
	BasicRenderer* ret = FRM_NEW(BasicRenderer(_flags));
	return ret;
}

void BasicRenderer::Destroy(BasicRenderer*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

void BasicRenderer::nextFrame(float _dt, Camera* _drawCamera, Camera* _cullCamera)
{
	PROFILER_MARKER("BasicRenderer::nextFrame");

	if (!pauseUpdate)
	{
	 // \todo can skip updates if nothing changed
		updateMaterialInstances();
		updateDrawCalls(_cullCamera);
		updateImageLightInstances();
	}

	for (int i = 0; i < Target_Count; ++i)
	{
		renderTargets[i].nextFrame();
	}

	updatePostProcessData(_dt, postProcessData.frameIndex + 1);
	
	GlContext* ctx = GlContext::GetCurrent();
	const Framebuffer* fbRestore = ctx->getFramebuffer();
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

				for (auto& it : drawCalls)
				{
					const DrawCall& drawCall = it.second;
					if (!drawCall.shaders[Pass_Shadow])
					{
						continue;
					}
					glScopedEnable(GL_CULL_FACE, drawCall.cullBackFace ? GL_TRUE : GL_FALSE);

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

	ctx->setFramebufferAndViewport(fbRestore);
}

void BasicRenderer::draw(float _dt, Camera* _drawCamera, Camera* _cullCamera)
{
	PROFILER_MARKER("BasicRenderer::draw");

	GlContext* ctx = GlContext::GetCurrent();
	const Framebuffer* fbRestore = ctx->getFramebuffer();
	#if FRM_MODULE_VR
		VRContext* vrCtx = VRContext::GetCurrent();
	#endif

	const ivec2 resolution          = ivec2(renderTargets[Target_Final].getTexture(0)->getWidth(), renderTargets[Target_Final].getTexture(0)->getHeight());
	const bool  isPostProcess       = flags.get(Flag::PostProcess);
	const bool  isFXAA              = flags.get(Flag::FXAA);
	const bool  isTAA               = flags.get(Flag::TAA);
	const bool  isInterlaced        = flags.get(Flag::Interlaced);
	const bool  isWriteToBackBuffer = flags.get(Flag::WriteToBackBuffer);
	const bool  isWireframe         = flags.get(Flag::WireFrame);

	sceneCamera.copyFrom(*_drawCamera); // \todo separate draw/cull cameras
	const bool sceneReverseProj = sceneCamera.getProjFlag(Camera::ProjFlag_Reversed);

	if (isTAA)
	{
		const int kFrameIndex = (int)(ctx->getFrameIndex() & 1);
		const vec2 kOffsets[2] = { vec2(0.5f, 0.0f), vec2(0.0f, 0.5f) };
		float jitterScale = 1.0f;
		sceneCamera.m_proj[2][0] += kOffsets[kFrameIndex].x * 2.0f / (float)resolution.x * jitterScale;
		sceneCamera.m_proj[2][1] += kOffsets[kFrameIndex].y * 2.0f / (float)resolution.y * jitterScale;
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

	const vec2 texelSize = vec2(1.0f) / vec2(fbGBuffer->getWidth(), fbGBuffer->getHeight());

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

	if (sceneDrawCalls.empty() && imageLightInstances.empty())
	{
		return;
	}

	{	PROFILER_MARKER("GBuffer");

		ctx->setFramebufferAndViewport(fbGBuffer);

		{	PROFILER_MARKER("Geometry");

			glAssert(glClearDepth(sceneReverseProj ? 0.0f : 1.0f));
			glAssert(glClearStencil(0));

			#if FRM_MODULE_VR
				if (vrCtx && vrCtx->isActive())
				{
				 // need to clear velocity since we don't run the static velocity pass
	 				glAssert(glClearColor(0.0f, 0.0f, 0.5f, 0.5f));
					glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)); // \todo set stencil mask
					vrCtx->primeDepthBuffer(0);
					const int eyeIndex = (_drawCamera == vrCtx->getEyeCamera(0)) ? 0 : 1;
					vrCtx->primeDepthBuffer(eyeIndex);
				}
				else
			#endif
				{
					glAssert(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
				}

			glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
			glAssert(glDepthFunc(sceneReverseProj ? GL_GREATER : GL_LESS));
			glScopedEnable(GL_STENCIL_TEST, GL_TRUE);
			glAssert(glStencilFunc(GL_ALWAYS, 0xff, 0x01)); // \todo only stencil dynamic objects
			glAssert(glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE));


			for (auto& it : sceneDrawCalls)
			{
				const DrawCall& drawCall = it.second;
				if (!drawCall.shaders[Pass_GBuffer])
				{
					continue;
				}
				glScopedEnable(GL_CULL_FACE, drawCall.cullBackFace ? GL_TRUE : GL_FALSE);

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

		#if FRM_MODULE_VR
			if (!vrCtx || !vrCtx->isActive())
		#endif
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

					FRM_ASSERT(shVelocityMinMax->getLocalSize().x == settings.motionBlurTileWidth);

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
			ctx->setUniform("uLod", imageLightInstances[0].backgroundLod);
			ctx->setUniform("uMultiplier", vec3(imageLightInstances[0].brightness));
			ctx->bindTexture("txEnvmap", imageLightInstances[0].texture);
			ctx->drawNdcQuad(&sceneCamera);
		}
		else
		{
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, Abs(sceneCamera.m_far)));
			glAssert(glClear(GL_COLOR_BUFFER_BIT));
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}

		for (auto& it : sceneDrawCalls)
		{
			const DrawCall& drawCall = it.second;
			if (!drawCall.shaders[Pass_Scene])
			{
				continue;
			}
			glScopedEnable(GL_CULL_FACE, drawCall.cullBackFace ? GL_TRUE : GL_FALSE);

			ctx->setShader(drawCall.shaders[Pass_Scene]);
			ctx->bindTexture("txGBuffer0", txGBuffer0);
			ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
			ctx->bindTexture("txBRDFLut", txBRDFLut);
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

			ctx->setUniform("uImageLightCount", (int)imageLightInstances.size());
			if (imageLightInstances.size() > 0 && imageLightInstances[0].texture)
			{
				ctx->bindTexture("txImageLight", imageLightInstances[0].texture);
				ctx->setUniform("uImageLightBrightness", imageLightInstances[0].brightness);
			}
			else
			{
				// \todo Crashes if no texture bound?
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

	{
		PROFILER_MARKER("Bloom");

		{	PROFILER_MARKER("Downsample");

			const ivec2 localSize = shBloomDownsample->getLocalSize().xy();
			ctx->setShader(shBloomDownsample);
			txScene->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // prevent any filtering between mips
			for (int level = 1; level < txScene->getMipCount(); ++level)
			{
				ctx->clearTextureBindings();
				ctx->clearImageBindings();
				ctx->setUniform("uSrcLevel", level - 1);
				ctx->bindTexture("txSrc", txScene);
				ctx->bindImage("txDst", txScene, GL_WRITE_ONLY, level);

				const int w = (int)txScene->getWidth() >> level;
				const int h = (int)txScene->getHeight() >> level;
				ctx->dispatch(
					Max((w + localSize.x - 1) / localSize.x, 1),
					Max((h + localSize.y - 1) / localSize.y, 1)
					);

				glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			}
		}

		{	PROFILER_MARKER("Upsample");

			const ivec2 localSize = shBloomUpsample->getLocalSize().xy();
			ctx->setShader(shBloomUpsample);
			txScene->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // prevent any filtering between mips
			for (int level = 1; level < txScene->getMipCount() - 1; ++level)
			{
				ctx->clearTextureBindings();
				ctx->clearImageBindings();
				ctx->setUniform("uSrcLevel", level + 1);
				ctx->bindTexture("txSrc", txScene);
				ctx->bindImage("txDst", txScene, GL_WRITE_ONLY, level);

				const int w = (int)txScene->getWidth() >> level;
				const int h = (int)txScene->getHeight() >> level;
				ctx->dispatch(
					Max((w + localSize.x - 1) / localSize.x, 1),
					Max((h + localSize.y - 1) / localSize.y, 1)
					);

				glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			}
		}

		txScene->setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
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

	if (drawCallback)
	{
		PROFILER_MARKER("drawCallback");
		ctx->setFramebufferAndViewport(fbPostProcessResult);
		drawCallback(Pass_Final, sceneCamera);
	}

	if (isFXAA)
	{
		PROFILER_MARKER("FXAA");

		ctx->setShader(shFXAA);
		ctx->setUniform("uTexelScaleX", isInterlaced ? 0.5f : 1.0f);
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

		const vec2 resolveKernel = vec2(-settings.taaSharpen, (1.0f + (2.0f * settings.taaSharpen)) / 2.0f);
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
		ctx->blitFramebuffer(fbFinal, nullptr, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

	ctx->setFramebufferAndViewport(fbRestore);
}

bool BasicRenderer::edit()
{
	bool ret = false;

	bool reinitRenderTargets = false;
	bool reinitShaders = false;

	ret |= ImGui::Checkbox("Pause Update",    &pauseUpdate);
	ret |= ImGui::Checkbox("Frustum Culling", &settings.enableCulling);
	ret |= ImGui::Checkbox("Cull by Submesh", &settings.cullBySubmesh);
	ret |= ImGui::SliderInt("LOD Bias", &settings.lodBias, -4, 4);

	const char* resolutionStr[] = { "Default (Window)", "3840x2160",       "2560x1440",       "1920x1080",       "1280x720",       "640x360"       };
	const ivec2 resolutionVal[] = { ivec2(-1, -1),      ivec2(3840, 2160), ivec2(2560, 1440), ivec2(1920, 1080), ivec2(1280, 720), ivec2(640, 360) };
	int selectedResolution = 0;
	for (int i = 0; i < (int)FRM_ARRAY_COUNT(resolutionVal); ++i)
	{
		if (resolutionVal[i] == settings.resolution)
		{
			selectedResolution = i;
		}
	}
	if (ImGui::BeginCombo("Resolution", resolutionStr[selectedResolution]))
	{
		for (int i = 0; i < (int)FRM_ARRAY_COUNT(resolutionVal); ++i)
		{
			const bool selected = i == selectedResolution;
			if (ImGui::Selectable(resolutionStr[i], selected))
			{
				selectedResolution = i;
				settings.resolution = resolutionVal[i];
				ret = reinitRenderTargets = true;
			}

			if (selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Flags"))
	{
		ret |= editFlag("Post Process", Flag::PostProcess);
		if (editFlag("FXAA", Flag::FXAA))
		{
			ret = reinitRenderTargets = true;
		}

		if (editFlag("TAA", Flag::TAA))
		{
			ret = reinitRenderTargets = true;
		}
		if (flags.get(Flag::TAA))
		{
			ImGui::SameLine();
			ret |= ImGui::SliderFloat("TAA Sharpen", &settings.taaSharpen, 0.0f, 2.0f);
		}

		if (editFlag("Interlaced", Flag::Interlaced))
		{
			ret = reinitRenderTargets = true;
		}

		ret |= editFlag("Write to Backbuffer", Flag::WriteToBackBuffer);
		ret |= editFlag("Wireframe", Flag::WireFrame);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Motion Blur"))
	{
		ret |= ImGui::SliderFloat("Motion Blur Target FPS", &settings.motionBlurTargetFps, 0.0f, 90.0f);

		if (ImGui::SliderInt("Motion Blur Quality", &settings.motionBlurQuality, 0, 1))
		{
			ret = reinitShaders = true;
		}

		ImGui::TreePop();
	}
	

	if (ImGui::TreeNode("Bloom"))
	{
		ImGui::SliderFloat("Bloom Brightness", &settings.bloomBrightness, 0.0f, 1.0f);
		ImGui::SliderFloat("Bloom Scale", &settings.bloomScale, -4.0f, 4.0f);
		ImGui::Text("Bloom Weights: %.3f, %.3f, %.3f, %.3f", postProcessData.bloomWeights.x, postProcessData.bloomWeights.y, postProcessData.bloomWeights.z, postProcessData.bloomWeights.w);

		ImGui::Spacing();
		if (ImGui::SliderInt("Bloom Quality", &settings.bloomQuality, 0, 1))
		{
			ret = reinitShaders = true;
		}

		ImGui::TreePop();
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

	// Debug
	#if 0
	{
		ImGui::Spacing();
		if (ImGui::Button("Re-compute BRDF Lut"))
		{
			initBRDFLut();
		}
	}
	#endif

	if (reinitRenderTargets)
	{
		initRenderTargets();
	}

	if (reinitShaders)
	{
		initShaders();
	}

	return ret;
}

void BasicRenderer::setFlag(Flag _flag, bool _value)
{
	flags.set(_flag, _value);

	if (_flag == Flag::TAA || _flag == Flag::Interlaced)
	{
		const bool isTAA = getFlag(Flag::TAA);
		const bool isInterlaced = getFlag(Flag::Interlaced);
		if (isTAA || isInterlaced)
		{
			ssMaterial->setLodBias(-1.0f);
		}
		else
		{
			ssMaterial->setLodBias(0.0f);
		}

		shTAAResolve->addGlobalDefines({
			String<32>("TAA %d", isTAA ? 1 : 0).c_str(),
			String<32>("INTERLACED %d", isInterlaced ? 1 : 0).c_str()
			});
	}
}

void BasicRenderer::setResolution(int _resolutionX, int _resolutionY)
{
	ivec2 newResolution = ivec2(_resolutionX, _resolutionY);
	if (newResolution != settings.resolution)
	{		
		settings.resolution = newResolution;
		initRenderTargets();
	}
}

// PRIVATE

BasicRenderer::BasicRenderer(Flags _flags)
{
	flags = _flags;

	Properties::PushGroup("#BasicRenderer");

		Properties::Add("resolution",                settings.resolution,                ivec2(0),   ivec2(8192), &settings.resolution);
		Properties::Add("motionBlurTargetFps",       settings.motionBlurTargetFps,       0.f,        128.f,       &settings.motionBlurTargetFps);
		Properties::Add("motionBlurQuality",         settings.motionBlurQuality,         0,          1,           &settings.motionBlurQuality);
		Properties::Add("taaSharpen",                settings.taaSharpen,                0.f,        2.f,         &settings.taaSharpen);
		Properties::Add("enableCulling",             settings.enableCulling,                                      &settings.enableCulling);
		Properties::Add("cullBySubmesh",             settings.cullBySubmesh,                                      &settings.cullBySubmesh);
		Properties::Add("bloomScale",                settings.bloomScale,               -2.f,        2.f,         &settings.bloomScale);
		Properties::Add("bloomBrightness",           settings.bloomBrightness,           0.f,        2.f,         &settings.bloomBrightness);
		Properties::Add("bloomQuality",              settings.bloomQuality,              0,          1,           &settings.bloomQuality);
		Properties::Add("maxShadowMapResolution",    settings.maxShadowMapResolution,    16,         16*1024,     &settings.maxShadowMapResolution);
		Properties::Add("minShadowMapResolution",    settings.minShadowMapResolution,    16,         16*1024,     &settings.minShadowMapResolution);
		Properties::Add("materialTextureAnisotropy", settings.materialTextureAnisotropy, 0.f,        16.f,        &settings.materialTextureAnisotropy);

	Properties::PopGroup();

	initShaders();
	initRenderTargets();

	bfPostProcessData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(PostProcessData), GL_DYNAMIC_STORAGE_BIT);
	bfPostProcessData->setName("bfPostProcessData");

	ssMaterial = TextureSampler::Create(GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, settings.materialTextureAnisotropy);
	if (flags.get(Flag::TAA) || flags.get(Flag::Interlaced))
	{
		ssMaterial->setLodBias(-1.0f);
	}
	else
	{
		ssMaterial->setLodBias(0.0f);
	}

	fbGBuffer           = Framebuffer::Create();
	fbScene             = Framebuffer::Create();
	fbPostProcessResult = Framebuffer::Create();
	fbFXAAResult        = Framebuffer::Create();
	fbFinal             = Framebuffer::Create();

	shadowAtlas = ShadowAtlas::Create(settings.maxShadowMapResolution, settings.minShadowMapResolution, GL_DEPTH_COMPONENT24);

	initBRDFLut();
}

BasicRenderer::~BasicRenderer()
{
	shutdownBRDFLut();

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

	Properties::InvalidateGroup("#BasicRenderer");
}

bool BasicRenderer::editFlag(const char* _name, Flag _flag)
{
	bool flagValue = flags.get(_flag);
	if (!ImGui::Checkbox(_name, &flagValue))
	{
		return false;
	}
	setFlag(_flag, flagValue);
	return true;
}

void BasicRenderer::initRenderTargets()
{
	shutdownRenderTargets();

	const bool  isFXAA         = flags.get(Flag::FXAA);
	const bool  isTAA          = flags.get(Flag::TAA);
	const bool  isInterlaced   = flags.get(Flag::Interlaced);
	const ivec2 appResolution  = AppSample::GetCurrent()->getResolution();
	const ivec2 fullResolution = ivec2(settings.resolution.x <= 0 ? appResolution.x : settings.resolution.x, settings.resolution.y <= 0 ? appResolution.y : settings.resolution.y);
	ivec2 interlacedResolution = isInterlaced ? ivec2(fullResolution.x / 2, fullResolution.y) : fullResolution;

	renderTargets[Target_GBuffer0].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, isInterlaced ? 2 : 1);
	renderTargets[Target_GBuffer0].setName("#BasicRenderer_txGBuffer0");

	renderTargets[Target_GBufferDepthStencil].init(interlacedResolution.x, interlacedResolution.y, GL_DEPTH32F_STENCIL8, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_GBufferDepthStencil].setName("#BasicRenderer_txGBufferDepth");

	//FRM_ASSERT(interlacedResolution.x % motionBlurTileWidth == 0 && interlacedResolution.y % motionBlurTileWidth == 0);
	renderTargets[Target_VelocityTileMinMax].init(interlacedResolution.x / settings.motionBlurTileWidth, interlacedResolution.y / settings.motionBlurTileWidth, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileMinMax].setName("#BasicRenderer_txVelocityTileMinMax");

	renderTargets[Target_VelocityTileNeighborMax].init(interlacedResolution.x / settings.motionBlurTileWidth, interlacedResolution.y / settings.motionBlurTileWidth, GL_RG16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileNeighborMax].setName("#BasicRenderer_txVelocityTileNeighborMax");

	renderTargets[Target_Scene].init(interlacedResolution.x, interlacedResolution.y, GL_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR, 1, 8); // RGB = color, A = abs(linear depth) + mip chain for blur
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
	shutdownShaders();

	#define DEF_STR(_name, _val) String<32>(_name " %d", _val).c_str()

	shStaticVelocity      = Shader::CreateVsFs("shaders/NdcQuad_vs.glsl", "shaders/BasicRenderer/StaticVelocity.glsl");
	shVelocityMinMax      = Shader::CreateCs("shaders/BasicRenderer/VelocityMinMax.glsl", settings.motionBlurTileWidth);
	shVelocityNeighborMax = Shader::CreateCs("shaders/BasicRenderer/VelocityNeighborMax.glsl", 8, 8);
	shImageLightBg	      = Shader::CreateVsFs("shaders/Envmap_vs.glsl", "shaders/Envmap_fs.glsl", { "ENVMAP_CUBE" } );
	shPostProcess         = Shader::CreateCs("shaders/BasicRenderer/PostProcess.glsl", 8, 8, 1, { DEF_STR("MOTION_BLUR_QUALITY", settings.motionBlurQuality) });
	shFXAA                = Shader::CreateCs("shaders/BasicRenderer/FXAA.glsl", 8, 8);
	shDepthClear          = Shader::CreateVsFs("shaders/BasicRenderer/DepthClear.glsl", "shaders/BasicRenderer/DepthClear.glsl");
	shBloomDownsample     = Shader::CreateCs("shaders/BasicRenderer/BloomDownsample.glsl", 8, 8, 1, { DEF_STR("BLOOM_QUALITY", settings.bloomQuality) });
	shBloomUpsample       = Shader::CreateCs("shaders/BasicRenderer/BloomUpsample.glsl", 8, 8);

	#undef DEF_STR

	const bool isTAA        = flags.get(Flag::TAA);
	const bool isInterlaced = flags.get(Flag::Interlaced);
	shTAAResolve = Shader::CreateCs("shaders/BasicRenderer/TAAResolve.glsl", 8, 8, 1,
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
	Shader::Release(shBloomDownsample);
	Shader::Release(shBloomUpsample);
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
	updateBuffer(bfMaterials, "bfBasicMaterial_Instances", bfMaterialsSize, materialInstances.data());
}

Shader* BasicRenderer::findShader(ShaderMapKey _key)
{
	static constexpr const char* kPassDefines[] =
	{
		"Pass_Shadow",
		"Pass_GBuffer",
		"Pass_Scene",
		"Pass_Wireframe",
		"Pass_Final"
	};
	static_assert(Pass_Count == FRM_ARRAY_COUNT(kPassDefines), "Pass_Count != FRM_ARRAY_COUNT(kPassDefines)");

	static constexpr const char* kGeometryDefines[] =
	{
		"Geometry_Mesh",
		"Geometry_SkinnedMesh",
	};
	static_assert(GeometryType_Count == FRM_ARRAY_COUNT(kGeometryDefines), "GeometryType_Count != FRM_ARRAY_COUNT(kGeometryDefines)");

	static constexpr const char* kMaterialDefines[] =
	{		
		"BasicMaterial_Flag_FlipV",
		"BasicMaterial_Flag_NormalMapBC5",
		"BasicMaterial_Flag_AlphaTest",
		"BasicMaterial_Flag_AlphaDither",
		"BasicMaterial_Flag_ThinTranslucency",
	};
	static_assert(BasicMaterial::Flag_Count == FRM_ARRAY_COUNT(kMaterialDefines), "BasicMaterial::Flag_Count != FRM_ARRAY_COUNT(kMaterialDefines)");

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

		ret = Shader::CreateVsFs("shaders/BasicRenderer/BasicRenderer.glsl", "shaders/BasicRenderer/BasicRenderer.glsl", std::initializer_list<const char*>(defines.begin(), defines.end()));
	}

	return ret;
}

void BasicRenderer::updateDrawCalls(Camera* _cullCamera)
{
	PROFILER_MARKER("BasicRenderer::updateDrawCalls");

	sceneBounds.m_min = shadowSceneBounds.m_min = vec3( FLT_MAX);
	sceneBounds.m_max = shadowSceneBounds.m_max = vec3(-FLT_MAX);

// Phase 1: Cull renderables, gather shadow renderables, generate scene and shadow scene bounds.
	{	PROFILER_MARKER_CPU("Phase 1");

		const auto& activeRenderables = BasicRenderableComponent::GetActiveComponents();
		culledSceneRenderables.clear();
		culledSceneRenderables.reserve(activeRenderables.size());
		shadowRenderables.clear();
		shadowRenderables.reserve(activeRenderables.size());
		for (BasicRenderableComponent* renderable : activeRenderables)
		{
			if (!renderable->m_mesh || renderable->m_materials.empty() || renderable->m_colorAlpha.w <= 0.0f)
			{
				continue;
			}

			const mat4 world = renderable->m_world;
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

			if (settings.enableCulling && (!_cullCamera->m_worldFrustum.insideIgnoreNear(bs) || !_cullCamera->m_worldFrustum.insideIgnoreNear(bb)))
			{
				continue;
			}

			// \todo
			// - Eccentricity/velocity LOD coefs probably not useful in the general case.
			// - Size coef should be computed/tweaked per mesh.
			// - Need a system whereby projected size (see MeshViewer) maps to a LOD index via the scale. Look at Unreal?

			LODCoefficients lodCoefficients;
			const vec3  toCamera         = GetTranslation(renderable->m_world) - _cullCamera->getPosition();
			const float distance         = Length(GetTranslation(renderable->m_world) - _cullCamera->getPosition());
			lodCoefficients.size         = distance / _cullCamera->m_proj[1][1];
			lodCoefficients.eccentricity = 1.0f - Max(0.0f, Dot(toCamera / distance, _cullCamera->getViewVector()));
			lodCoefficients.velocity     = Length(GetTranslation(renderable->m_world) - GetTranslation(renderable->m_prevWorld)); // \todo Account for rotation cheaply? Use Length2? Include camera motion?

			LODCoefficients renderableLODCoefficients;
			renderableLODCoefficients.size          = 0.2f;
			renderableLODCoefficients.eccentricity  = 0.0f; // \todo Experiment with cranking this up for VR? 
			renderableLODCoefficients.velocity      = 5.0f; 

			float flod = 0.0f;
			flod = Max(flod, lodCoefficients.size         * renderableLODCoefficients.size);
			flod = Max(flod, lodCoefficients.eccentricity * renderableLODCoefficients.eccentricity);
			flod = Max(flod, lodCoefficients.velocity     * renderableLODCoefficients.velocity);

			int selectedLOD = (int)flod;
			selectedLOD = (renderable->m_lodOverride >= 0) ? renderable->m_lodOverride : selectedLOD;
			selectedLOD = Clamp(selectedLOD + settings.lodBias, 0, renderable->m_mesh->getLODCount() - 1);
			renderable->m_selectedLOD = selectedLOD;
//Im3d::Text(GetTranslation(renderable->m_world), 1.0f, Im3d::Color_Magenta, Im3d::TextFlags_Default, "LOD%d", selectedLOD);
			culledSceneRenderables.push_back(renderable);
		}
	}

// Phase 2: Generate draw calls for culled scene renderables, optionally cull by submesh.
	{	PROFILER_MARKER_CPU("Phase 2");

		clearDrawCalls(sceneDrawCalls);
		for (BasicRenderableComponent* renderable : culledSceneRenderables)
		{
			const mat4 world = renderable->m_world;

			int submeshIndexMin = 0;
			int submeshIndexMax = 0;
			if (renderable->m_subMeshOverride >= 0)
			{
				submeshIndexMin = submeshIndexMax = renderable->m_subMeshOverride;
			}
			else
			{
				submeshIndexMax = Min((int)renderable->m_materials.size(), renderable->m_mesh->getSubmeshCount() - 1);
			}

			for (int submeshIndex = submeshIndexMin; submeshIndex <= submeshIndexMax; ++submeshIndex)
			{
				if (!renderable->m_materials[submeshIndex]) // skip submesh if no material set
				{
					continue;
				}

				if (submeshIndex > 0 && settings.enableCulling && settings.cullBySubmesh)
				{
					Sphere bs = renderable->m_mesh->getBoundingSphere(submeshIndex);
					bs.transform(world);
					AlignedBox bb = renderable->m_mesh->getBoundingBox(submeshIndex);
					bb.transform(world);

					if (!_cullCamera->m_worldFrustum.insideIgnoreNear(bs) || !_cullCamera->m_worldFrustum.insideIgnoreNear(bb))
					{
						continue;
					}
				}
				
				addDrawCall(renderable, renderable->m_selectedLOD, submeshIndex, sceneDrawCalls);

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

		const auto& activeLights = BasicLightComponent::GetActiveComponents();

		shadowCameras.clear();
		// \todo map allocations -> lights, avoid realloc every frame
		while (!shadowMapAllocations.empty())
		{
			ShadowAtlas::ShadowMap* shadowMap = (ShadowAtlas::ShadowMap*)shadowMapAllocations.back();
			shadowAtlas->free(shadowMap);
			shadowMapAllocations.pop_back();
		}

		culledLights.clear();
		culledLights.reserve(activeLights.size());
		culledShadowLights.clear();
		culledShadowLights.reserve(activeLights.size());
		shadowMapAllocations.reserve(activeLights.size());
		for (BasicLightComponent* light : activeLights)
		{
			const SceneNode* sceneNode = light->getParentNode();
			if (light->m_colorBrightness.w <= 0.0f)
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

				const vec3 lightPosition = sceneNode->getPosition();
				const vec3 lightDirection = sceneNode->getForward();

				// \todo generate shadow camera + matrix
				Camera& shadowCamera = shadowCameras.push_back();

				switch (light->m_type)
				{
					default: break;
					case BasicLightComponent::Type_Direct:
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
					case BasicLightComponent::Type_Spot:
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
		lightInstances.reserve(culledLights.size());
		for (BasicLightComponent* light : culledLights)
		{
			const mat4 world = light->getParentNode()->getWorld();

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
		shadowLightInstances.reserve(culledShadowLights.size());
		for (size_t i = 0; i < culledShadowLights.size(); ++i)
		{
			const BasicLightComponent* light = culledShadowLights[i];
			const ShadowAtlas::ShadowMap* shadowMap = (ShadowAtlas::ShadowMap*)shadowMapAllocations[i];
			FRM_ASSERT(shadowMap != nullptr);
			const mat4 world = light->getParentNode()->getWorld();

			ShadowLightInstance& shadowLightInstance = shadowLightInstances.push_back();
			shadowLightInstance.position             = vec4(world[3].xyz(), (float)light->m_type);
			shadowLightInstance.direction            = vec4(normalize(world[2].xyz()), 0.0f);
			shadowLightInstance.color                = vec4(light->m_colorBrightness.xyz() * light->m_colorBrightness.w, light->m_colorBrightness.w);

			shadowLightInstance.invRadius2           = 1.0f / (light->m_radius * light->m_radius);

			const float cosOuter                     = cosf(Radians(light->m_coneOuterAngle));
			const float cosInner                     = cosf(Radians(light->m_coneInnerAngle));
			shadowLightInstance.spotScale            = 1.0f / Max(cosInner - cosOuter, 1e-4f);
			shadowLightInstance.spotBias             = -cosOuter * shadowLightInstance.spotScale;

			shadowLightInstance.worldToShadow        = shadowCameras[i].m_viewProj;
			shadowLightInstance.uvBias               = shadowMap->uvBias;
			shadowLightInstance.uvScale              = shadowMap->uvScale;
			shadowLightInstance.arrayIndex           = (float)shadowMap->arrayIndex;
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

			for (BasicRenderableComponent* renderable : shadowRenderables)
			{
				const mat4 world = renderable->m_world;

				int submeshIndexMin = 0;
				int submeshIndexMax = 0;
				if (renderable->m_subMeshOverride >= 0)
				{
					submeshIndexMin = submeshIndexMax = renderable->m_subMeshOverride;
				}
				else
				{
					submeshIndexMax = Min((int)renderable->m_materials.size(), renderable->m_mesh->getSubmeshCount() - 1);
				}

				for (int submeshIndex = submeshIndexMin; submeshIndex <= submeshIndexMax; ++submeshIndex)
				{
					if (!renderable->m_materials[submeshIndex]) // skip submesh if no material set
					{
						continue;
					}

					if (submeshIndex > 0 && settings.cullBySubmesh)
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

					addDrawCall(renderable, renderable->m_selectedLOD, submeshIndex, drawCallMap);

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

void BasicRenderer::addDrawCall(const BasicRenderableComponent* _renderable, int _lodIndex, int _submeshIndex, DrawCallMap& map_)
{
	const BasicMaterial* material = _renderable->m_materials[_submeshIndex];
	const DrawMesh* mesh = _renderable->m_mesh;

	// \todo This should be per-pass. Note that the order here has no meaning - it resolves to a bitfield.
	const DrawMesh::VertexSemantic vertexAttributes[] =
		{
			Mesh::Semantic_Positions, 
			Mesh::Semantic_Normals, 
			Mesh::Semantic_Tangents, 
			Mesh::Semantic_MaterialUVs,
			Mesh::Semantic_BoneWeights,
			Mesh::Semantic_BoneIndices
		};

	uint64 drawCallKey = 0;
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)material->getIndex(),         40, 24);
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)mesh->getIndex() + _lodIndex, 16, 24);
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)_lodIndex,                    12, 4);
	drawCallKey = BitfieldInsert(drawCallKey, (uint64)_submeshIndex,                0,  12);

	DrawCall& drawCall           = map_[drawCallKey];
	drawCall.material            = material;
	drawCall.cullBackFace        = (material->getFlags() & (1 << BasicMaterial::Flag_ThinTranslucent)) == 0;
	drawCall.mesh                = mesh;
	drawCall.lodIndex            = _lodIndex;
	drawCall.submeshIndex        = _submeshIndex;
	drawCall.bindHandleKey       = mesh->makeBindHandleKey(vertexAttributes, FRM_ARRAY_COUNT(vertexAttributes)); // \todo Store the actual bind handle here?

	DrawInstance& drawInstance   = drawCall.instanceData.push_back();
	drawInstance.world           = _renderable->m_world;
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
	_drawCall.material->bind(ctx, ssMaterial);
	ctx->setMesh(_drawCall.mesh, _drawCall.lodIndex, _drawCall.submeshIndex, _drawCall.bindHandleKey);
	ctx->draw((GLsizei)_drawCall.instanceData.size());
}

void BasicRenderer::updateImageLightInstances()
{
	PROFILER_MARKER_CPU("updateImageLightInstances");

	// \todo need a separate system for this, see ImageLightComponent

	const auto& activeImageLights = ImageLightComponent::GetActiveComponents();
	FRM_ASSERT(activeImageLights.size() <= 1); // only 1 image light is currently supported

	imageLightInstances.clear();
	imageLightInstances.reserve(activeImageLights.size());
	for (ImageLightComponent* light : activeImageLights)
	{
		const SceneNode* sceneNode = light->getParentNode();
		if (light->m_brightness <= 0.0f)
		{
			continue;
		}

		ImageLightInstance& lightInstance = imageLightInstances.push_back();
		lightInstance.brightness          = light->m_brightness;
		lightInstance.backgroundLod       = light->m_backgroundLod;
		lightInstance.isBackground        = light->m_isBackground;
		lightInstance.texture             = light->m_texture;
	}
}

void BasicRenderer::updatePostProcessData(float _dt, uint32 _frameIndex)
{
	postProcessData.motionBlurScale = settings.motionBlurTargetFps * _dt;
	postProcessData.frameIndex = _frameIndex;
	
	{
		// Bloom weights are sampled along a line, slope is determined by bloomScale.
		float bloomBias  = 1.0f;
		const float bloomWeightScale = 2.0f;
		vec4 bloomWeights = vec4(
			Max((settings.bloomScale * bloomWeightScale) * -1.0f   + bloomBias, 0.0f),
			Max((settings.bloomScale * bloomWeightScale) * -0.333f + bloomBias, 0.0f),
			Max((settings.bloomScale * bloomWeightScale) *  0.333f + bloomBias, 0.0f),
			Max((settings.bloomScale * bloomWeightScale) *  1.0f   + bloomBias, 0.0f)
			);
		bloomWeights = Normalize(bloomWeights);
		postProcessData.bloomWeights = bloomWeights * settings.bloomBrightness;
	}

	bfPostProcessData->setData(sizeof(PostProcessData), &postProcessData);
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

void BasicRenderer::initBRDFLut()
{
	if (!txBRDFLut)
	{
		txBRDFLut = Texture::Create2d(128, 128, GL_RGBA16F);
		txBRDFLut->setWrap(GL_CLAMP_TO_EDGE);
		txBRDFLut->setName("#BasicRenderer_txBRDFLut");
	}

	Shader* sh = Shader::CreateCs("shaders/BasicRenderer/BRDFLut.glsl", 8, 8);
	if (!sh || sh->getState() != Shader::State_Loaded)
	{
		FRM_ASSERT(false);
		return;
	}

	GlContext* ctx = GlContext::GetCurrent();
	ctx->setShader(sh);
	ctx->bindImage("txBRDFLut", txBRDFLut, GL_WRITE_ONLY);
	ctx->dispatch(txBRDFLut);
	glAssert(glFinish());

	Shader::Release(sh);
}

void BasicRenderer::shutdownBRDFLut()
{
	Texture::Release(txBRDFLut);
}

} // namespace frm
