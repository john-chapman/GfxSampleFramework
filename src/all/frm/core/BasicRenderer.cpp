#include "BasicRenderer.h"

#include <frm/core/geom.h>
#include <frm/core/memory.h>
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
#include <frm/core/Texture.h>

#include <imgui/imgui.h>

#include <EASTL/vector.h>

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

void BasicRenderer::draw(Camera* _camera, float _dt)
{
	PROFILER_MARKER("BasicRenderer::draw");

	GlContext* ctx = GlContext::GetCurrent();

	camera.copyFrom(*_camera);
	const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);
	if (isTAA)
	{
		const int kFrameIndex = (int)(ctx->getFrameIndex() & 1);
		const vec2 kOffsets[2] = { vec2(0.5f, 0.0f), vec2(0.0f, 0.5f) };
		float jitterScale = 1.0f;
		camera.m_proj[2][0] = kOffsets[kFrameIndex].x * 2.0f / (float)resolution.x * jitterScale;
		camera.m_proj[2][1] = kOffsets[kFrameIndex].y * 2.0f / (float)resolution.y * jitterScale;
		camera.m_viewProj = camera.m_proj * camera.m_view;
	}
	camera.updateGpuBuffer();

	if (!pauseUpdate)
	{
	 // \todo can skip updates if nothing changed
		updateMaterialInstances();
		updateDrawInstances(&camera);
		updateLightInstances(&camera);
		updateImageLightInstances(&camera);
	}
	if (drawInstances.empty())
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
	fbFinal->attach(txGBufferDepthStencil, GL_DEPTH_STENCIL_ATTACHMENT);

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

			
			for (int materialIndex = 0; materialIndex < (int)drawInstances.size(); ++materialIndex)
			{
				if (drawInstances[materialIndex].empty())
				{
					continue;
				}

				// reset shader because we want to clear all the bindings to avoid running out of slots
				ctx->setShader(shGBuffer);
				ctx->bindBuffer(camera.m_gpuBuffer);
				ctx->bindBuffer(bfMaterials);
				ctx->setUniform("uTexelSize", texelSize);
				ctx->setUniform("uMaterialIndex", materialIndex);
				BasicMaterial* material = BasicMaterial::GetInstance(materialIndex);
				material->bind(ssMaterial);

				for (DrawInstance& drawInstance : drawInstances[materialIndex])
				{
					ctx->setMesh    (drawInstance.mesh, drawInstance.submeshIndex);
					ctx->setUniform ("uWorld",          drawInstance.world);
					ctx->setUniform ("uPrevWorld",      drawInstance.prevWorld);
					ctx->setUniform ("uBaseColorAlpha", drawInstance.colorAlpha);
					ctx->draw();
				}
			}
		}

		{	PROFILER_MARKER("Static Velocity");
	
			glScopedEnable(GL_STENCIL_TEST, GL_TRUE);
			glAssert(glStencilFunc(GL_NOTEQUAL, 0xff, 0x01));
			glAssert(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
			glAssert(glColorMask(false, false, true, true));

			ctx->setShader(shStaticVelocity);
			ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
			ctx->drawNdcQuad(&camera);

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

		if (imageLightInstances.size() > 0 && imageLightInstances[0].isBackground)
		{
			ctx->setShader(shImageLightBg);
			ctx->bindTexture("txEnvmap", imageLightInstances[0].texture);
			ctx->drawNdcQuad(&camera);
		}
		else
		{
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, Abs(camera.m_far)));
			glAssert(glClear(GL_COLOR_BUFFER_BIT));
			glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}

		glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
		for (int materialIndex = 0; materialIndex < (int)drawInstances.size(); ++materialIndex)
		{
			if (drawInstances[materialIndex].empty())
			{
				continue;
			}

			// reset shader because we want to clear all the bindings to avoid running out of slots
			ctx->setShader(shScene);
			ctx->bindTexture("txGBuffer0", txGBuffer0);
			ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
			ctx->setUniform("uLightCount", (int)lightInstances.size());
			if (bfLights)
			{
				ctx->bindBuffer("bfLights", bfLights);
			}
			if (bfMaterials)
			{
				ctx->bindBuffer("bfMaterials", bfMaterials);
			}
			ctx->setUniform("uImageLightCount", (int)imageLightInstances.size());
			if (imageLightInstances.size() > 0)
			{
				ctx->bindTexture("txImageLight", imageLightInstances[0].texture);
				ctx->setUniform("uImageLightBrightness", imageLightInstances[0].brightness);
			}

			ctx->setUniform("uTexelSize", texelSize);
			ctx->setUniform ("uMaterialIndex", materialIndex);
			BasicMaterial* material = BasicMaterial::GetInstance(materialIndex);
			material->bind(ssMaterial);

			for (DrawInstance& drawInstance : drawInstances[materialIndex])
			{
				ctx->setMesh(drawInstance.mesh,     drawInstance.submeshIndex);
				ctx->setUniform ("uWorld",          drawInstance.world);
				ctx->setUniform ("uPrevWorld",      drawInstance.prevWorld);
				ctx->setUniform ("uBaseColorAlpha", drawInstance.colorAlpha);
				ctx->draw();
			}
		}
	}

	//m_luminanceMeter.draw(ctx, _dt, txScene);
	
	if (BitfieldGet(flags, (uint32)Flag_PostProcess))
	{	
		PROFILER_MARKER("Post Process");

		ctx->setShader(shPostProcess);
		ctx->bindBuffer(bfPostProcessData);
		ctx->bindBuffer(camera.m_gpuBuffer);
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

	if (BitfieldGet(flags, (uint32)Flag_FXAA))
	{
		PROFILER_MARKER("FXAA");
		
		ctx->setShader(shFXAA);
		ctx->bindTexture("txIn", txPostProcessResult);
		ctx->bindImage("txOut", txFXAAResult, GL_WRITE_ONLY);
		ctx->dispatch(txFXAAResult);

		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
	}
	else if (!BitfieldGet(flags, (uint32)Flag_TAA))
	{
		ctx->blitFramebuffer(fbPostProcessResult, fbFinal, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (BitfieldGet(flags, (uint32)Flag_TAA))
	{
		PROFILER_MARKER("TAA Resolve");
		
		ctx->setShader(shTAAResolve);

float sharpening = 0.4f;
const vec2 resolveKernel = vec2(-sharpening, (1.0f + (2.0f * sharpening)) / 2.0f);

		Texture* txCurrent = BitfieldGet(flags, (uint32)Flag_FXAA) ? txFXAAResult : txPostProcessResult;
		Texture* txCurrentResolve  = renderTargets[Target_TAAResolve].getTexture(0);
		Texture* txPreviousResolve = renderTargets[Target_TAAResolve].getTexture(-1);
				
		ctx->setUniform("uFrameIndex", (int)(ctx->getFrameIndex() & 1));
		ctx->setUniform("uResolveKernel", resolveKernel);
		ctx->bindBuffer(camera.m_gpuBuffer);
		ctx->bindTexture("txGBuffer0", txGBuffer0);
		ctx->bindTexture("txGBufferDepthStencil", txGBufferDepthStencil);
		ctx->bindTexture("txCurrent", txCurrent);
		ctx->bindTexture("txPreviousResolve", txPreviousResolve);
		ctx->bindImage("txCurrentResolve", txCurrentResolve, GL_WRITE_ONLY);
		ctx->bindImage("txFinal", txFinal, GL_WRITE_ONLY);
		ctx->dispatch(txFinal);
		
		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
	}
	else if (BitfieldGet(flags, (uint32)Flag_FXAA))
	{
		ctx->blitFramebuffer(fbFXAAResult, fbFinal, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (BitfieldGet(flags, (uint32)Flag_WriteToBackBuffer))
	{
		ctx->blitFramebuffer(fbFinal, nullptr, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
}

bool BasicRenderer::edit()
{
	bool ret = false;

	ret |= ImGui::Checkbox("Pause Update", &pauseUpdate);
	ret |= ImGui::SliderFloat("Motion Blur Target FPS", &motionBlurTargetFps, 0.0f, 90.0f);

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
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
		ret |= editFlag("FXAA", Flag_FXAA);
		if (editFlag("TAA", Flag_TAA))
		{
			ret = true;
			initRenderTargets();
			const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);
			if (isTAA)
			{
				ssMaterial->setLodBias(-1.0f);
			}
			else
			{
				ssMaterial->setLodBias(0.0f);
			}
		}
		ret |= editFlag("Write to Backbuffer", Flag_WriteToBackBuffer);

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

	fbGBuffer           = Framebuffer::Create(2, renderTargets[Target_GBuffer0].getTexture(0), renderTargets[Target_GBufferDepthStencil].getTexture(0));
	fbScene             = Framebuffer::Create(2, renderTargets[Target_Scene].getTexture(0), renderTargets[Target_GBufferDepthStencil].getTexture(0));
	fbPostProcessResult = Framebuffer::Create(2, renderTargets[Target_PostProcessResult].getTexture(0), renderTargets[Target_GBufferDepthStencil].getTexture(0));
	fbFXAAResult        = Framebuffer::Create(1, renderTargets[Target_FXAAResult].getTexture(0));
	fbFinal             = Framebuffer::Create(2, renderTargets[Target_Final].getTexture(0), renderTargets[Target_GBufferDepthStencil].getTexture(0));

	//FRM_VERIFY(m_luminanceMeter.init(_resolutionY / 2));
	//m_colorCorrection.m_luminanceMeter = &m_luminanceMeter;
}

BasicRenderer::~BasicRenderer()
{
	//m_luminanceMeter.shutdown();

	shutdownRenderTargets();
	shutdownShaders();

	Framebuffer::Destroy(fbGBuffer);
	Framebuffer::Destroy(fbScene);
	Framebuffer::Destroy(fbPostProcessResult);
	Framebuffer::Destroy(fbFXAAResult);
	Framebuffer::Destroy(fbFinal);

	Buffer::Destroy(bfMaterials);
	Buffer::Destroy(bfLights);
	Buffer::Destroy(bfPostProcessData);
}

bool BasicRenderer::editFlag(const char* _name, Flag _flag)
{
	bool flagValue = BitfieldGet(flags, (uint32)_flag);
	bool ret =ImGui::Checkbox(_name, &flagValue);
	flags = BitfieldSet(flags, _flag, flagValue);
	return ret;
}

void BasicRenderer::initRenderTargets()
{
	shutdownRenderTargets();

	const bool isTAA = BitfieldGet(flags, (uint32)Flag_TAA);

	renderTargets[Target_GBuffer0].init(resolution.x, resolution.y, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_GBuffer0].setName("#BasicRenderer_txGBuffer0");

	renderTargets[Target_GBufferDepthStencil].init(resolution.x, resolution.y, GL_DEPTH32F_STENCIL8, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_GBufferDepthStencil].setName("#BasicRenderer_txGBufferDepth");
		
	FRM_ASSERT(resolution.x % motionBlurTileWidth == 0 && resolution.y % motionBlurTileWidth == 0);
	renderTargets[Target_VelocityTileMinMax].init(resolution.x / motionBlurTileWidth, resolution.y / motionBlurTileWidth, GL_RGBA16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileMinMax].setName("#BasicRenderer_txVelocityTileMinMax");

	renderTargets[Target_VelocityTileNeighborMax].init(resolution.x / motionBlurTileWidth, resolution.y / motionBlurTileWidth, GL_RG16, GL_CLAMP_TO_EDGE, GL_NEAREST, 1);
	renderTargets[Target_VelocityTileNeighborMax].setName("#BasicRenderer_txVelocityTileNeighborMax");

	renderTargets[Target_Scene].init(resolution.x, resolution.y, GL_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR, 1); // RGB = color, A = abs(linear depth)
	renderTargets[Target_Scene].setName("#BasicRenderer_txScene");

	renderTargets[Target_PostProcessResult].init(resolution.x, resolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, 1);
	renderTargets[Target_PostProcessResult].setName("#BasicRenderer_txPostProcessResult");

	renderTargets[Target_FXAAResult].init(resolution.x, resolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, 1);
	renderTargets[Target_FXAAResult].setName("#BasicRenderer_txFXAAResult");

	renderTargets[Target_TAAResolve].init(resolution.x, resolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, isTAA ? 2 : 1);
	renderTargets[Target_TAAResolve].setName("#BasicRenderer_txTAAResolve");

	renderTargets[Target_Final].init(resolution.x, resolution.y, GL_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, 1);
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
	shGBuffer             = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "GBuffer_OUT" });
	shStaticVelocity      = Shader::CreateVsFs("shaders/NdcQuad_vs.glsl", "shaders/BasicRenderer/StaticVelocity.glsl");
	shVelocityMinMax      = Shader::CreateCs("shaders/BasicRenderer/VelocityMinMax.glsl", motionBlurTileWidth);
	shVelocityNeighborMax = Shader::CreateCs("shaders/BasicRenderer/VelocityNeighborMax.glsl", 8, 8);
	shScene               = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "Scene_OUT" });
	shImageLightBg	      = Shader::CreateVsFs("shaders/Envmap_vs.glsl", "shaders/Envmap_fs.glsl", { "ENVMAP_CUBE" } );
	shPostProcess         = Shader::CreateCs("shaders/BasicRenderer/PostProcess.glsl", 8, 8);
	shFXAA                = Shader::CreateCs("shaders/BasicRenderer/FXAA.glsl", 8, 8);
	shTAAResolve          = Shader::CreateCs("shaders/BasicRenderer/TAAResolve.glsl", 8, 8);
}

void BasicRenderer::shutdownShaders()
{
	Shader::Release(shGBuffer);
	Shader::Release(shStaticVelocity);
	Shader::Release(shVelocityMinMax);
	Shader::Release(shVelocityNeighborMax);
	Shader::Release(shScene);
	Shader::Release(shImageLightBg);
	Shader::Release(shPostProcess);
	Shader::Release(shFXAA);
	Shader::Release(shTAAResolve);
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
		materialInstance.flags             = material->getFlags();
	}

	GLsizei bfMaterialsSize = (GLsizei)(sizeof(MaterialInstance) * materialInstances.size());
	if (bfMaterialsSize > 0)
	{
		if (bfMaterials && bfMaterials->getSize() != bfMaterialsSize)
		{
			Buffer::Destroy(bfMaterials);
			bfMaterials = nullptr;
		}
		
		if (!bfMaterials)
		{
			bfMaterials = Buffer::Create(GL_SHADER_STORAGE_BUFFER, bfMaterialsSize, GL_DYNAMIC_STORAGE_BIT);
			bfMaterials->setName("bfMaterials");
		}

		bfMaterials->setData(bfMaterialsSize, materialInstances.data());
	}
}

void BasicRenderer::updateDrawInstances(const Camera* _camera)
{
 // \todo sort each list of draw instances by mesh/submesh for auto batching

	PROFILER_MARKER_CPU("updateDrawInstances");

	drawInstances.clear();
	drawInstances.resize(BasicMaterial::GetInstanceCount());

	for (Component_BasicRenderable* renderable : Component_BasicRenderable::s_instances)
	{
		Node* sceneNode = renderable->getNode();
		if (!sceneNode->isActive())
		{
			continue;
		}
		mat4 world = sceneNode->getWorldMatrix();
		Sphere bs = renderable->m_mesh->getBoundingSphere();
		bs.transform(world);
		if (_camera->m_worldFrustum.insideIgnoreNear(bs))
		{
			AlignedBox bb = renderable->m_mesh->getBoundingBox();
			bb.transform(world);
			if (_camera->m_worldFrustum.insideIgnoreNear(bb))
			{
				int submeshCount = Min((int)renderable->m_materials.size(), renderable->m_mesh->getSubmeshCount());
				for (int submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
				{
					if (!renderable->m_materials[submeshIndex]) // skip submesh if no material set
					{
						continue;
					}
					int materialIndex            = renderable->m_materials[submeshIndex]->getIndex();
					DrawInstance& drawInstance   = drawInstances[materialIndex].push_back();
					drawInstance.mesh            = renderable->m_mesh;
					drawInstance.world           = world;
					drawInstance.prevWorld       = renderable->m_prevWorld;
					drawInstance.colorAlpha      = renderable->m_colorAlpha;
					drawInstance.materialIndex   = materialIndex;
					drawInstance.submeshIndex    = submeshIndex;
				}
			}
		}
	}
}

void BasicRenderer::updateLightInstances(const Camera* _camera)
{
	PROFILER_MARKER_CPU("updateLightInstances");

	lightInstances.clear();

	for (Component_BasicLight* light : Component_BasicLight::s_instances)
	{
		Node* sceneNode = light->getNode();
		if (!sceneNode->isActive())
		{
			continue;
		}
		mat4 world = sceneNode->getWorldMatrix();
		// \todo cull light volume against camera frustum

		LightInstance& lightInstance = lightInstances.push_back();
		lightInstance.position       = vec4(world[3].xyz(), (float)light->m_type);
		lightInstance.direction      = vec4(normalize(world[2].xyz()), 0.0f);
		lightInstance.color          = vec4(light->m_colorBrightness.xyz() * light->m_colorBrightness.w, light->m_colorBrightness.w);
		lightInstance.attenuation    = vec4(
			light->m_linearAttenuation.x, light->m_linearAttenuation.y,
			Radians(light->m_radialAttenuation.x), Radians(light->m_radialAttenuation.y)
			);
	}

	GLsizei bfLightsSize = (GLsizei)(sizeof(LightInstance) * lightInstances.size());
	if (bfLightsSize > 0)
	{
		if (bfLights && bfLights->getSize() != bfLightsSize)
		{
			Buffer::Destroy(bfLights);
			bfLights = nullptr;
		}

		if (!bfLights)
		{
			bfLights = Buffer::Create(GL_SHADER_STORAGE_BUFFER, bfLightsSize, GL_DYNAMIC_STORAGE_BIT);
			bfLights->setName("bfLights");
		}

		bfLights->setData(bfLightsSize, lightInstances.data());
	}
}

void BasicRenderer::updateImageLightInstances(const Camera* _camera)
{
	PROFILER_MARKER_CPU("updateImageLightInstances");

	// \todo need a separate system for this, see Component_ImageLight

	FRM_ASSERT(Component_ImageLight::s_instances.size() <= 1); // only 1 image light is currently supported
	
	imageLightInstances.clear();

	for (Component_ImageLight* light : Component_ImageLight::s_instances)
	{
		Node* sceneNode = light->getNode();
		if (!sceneNode->isActive())
		{
			continue;
		}

		ImageLightInstance& lightInstance = imageLightInstances.push_back();
		lightInstance.brightness   = light->m_brightness;
		lightInstance.isBackground = light->m_isBackground;
		lightInstance.texture      = light->m_texture;
	}
}

} // namespace frm
