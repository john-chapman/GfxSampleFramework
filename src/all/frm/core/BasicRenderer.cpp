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

	postProcessData.motionBlurScale = motionBlurTargetFps * _dt;
	bfPostProcessData->setData(sizeof(PostProcessData), &postProcessData);

 // \todo can skip updates if nothing changed
	updateMaterialInstances();
	updateDrawInstances(_camera);
	updateLightInstances(_camera);
	updateImageLightInstances(_camera);

	if (drawInstances.empty())
	{
		return;
	}

	GlContext* ctx = GlContext::GetCurrent();
	
	{	PROFILER_MARKER("GBuffer");

		ctx->setFramebufferAndViewport(fbGBuffer);
		const vec2 texelSize = vec2(1.0f) / vec2(fbGBuffer->getWidth(), fbGBuffer->getHeight());

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
				ctx->bindBuffer(_camera->m_gpuBuffer);
				ctx->bindBuffer(bfMaterials);
				ctx->setUniform("uTexelSize", texelSize);
				ctx->setUniform("uMaterialIndex", materialIndex);
				BasicMaterial* material = BasicMaterial::GetInstance(materialIndex);
				material->bind();

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
			ctx->bindTexture(txGBufferDepth);
			ctx->drawNdcQuad(_camera);

			glAssert(glColorMask(true, true, true, true));
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
			ctx->drawNdcQuad(_camera);
		}
		else
		{
			glAssert(glClear(GL_COLOR_BUFFER_BIT));
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
			ctx->bindTexture(txGBuffer0);
			ctx->bindTexture(txGBufferDepth);
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

			ctx->setUniform ("uMaterialIndex", materialIndex);
			BasicMaterial* material = BasicMaterial::GetInstance(materialIndex);
			material->bind();

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
		ctx->bindBuffer(_camera->m_gpuBuffer);
		ctx->bindTexture(txScene);
		ctx->bindTexture(txGBuffer0);
		ctx->bindTexture(txGBufferDepth);
		ctx->bindImage("txFinal", txFinal, GL_WRITE_ONLY);
		ctx->dispatch(txFinal);
	}

	if (BitfieldGet(flags, (uint32)Flag_WriteToBackBuffer))
	{
		ctx->blitFramebuffer(fbFinal, nullptr, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
}

bool BasicRenderer::edit()
{
	bool ret = false;

	ret &= ImGui::SliderFloat("Motion Blur Target FPS", &motionBlurTargetFps, 0.0f, 90.0f);

	ImGui::SetNextTreeNodeOpen(true);
	if (ImGui::TreeNode("Flags"))
	{
		bool flagPostProcess       = BitfieldGet(flags, (uint32)Flag_PostProcess);
		bool flagWriteToBackBuffer = BitfieldGet(flags, (uint32)Flag_WriteToBackBuffer);

		ret |= ImGui::Checkbox("Post Process",        &flagPostProcess);
		ret |= ImGui::Checkbox("Write To Backbuffer", &flagWriteToBackBuffer);

		flags = BitfieldSet(flags, Flag_PostProcess,       flagPostProcess);
		flags = BitfieldSet(flags, Flag_WriteToBackBuffer, flagWriteToBackBuffer);

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
	flags = _flags;

	shGBuffer        = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "GBuffer_OUT" });
	shStaticVelocity = Shader::CreateVsFs("shaders/NdcQuad_vs.glsl", "shaders/BasicRenderer/StaticVelocity.glsl");
	shScene          = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "Scene_OUT" });
	shImageLightBg	 = Shader::CreateVsFs("shaders/Envmap_vs.glsl", "shaders/Envmap_fs.glsl", { "ENVMAP_CUBE" } );
	shPostProcess    = Shader::CreateCs("shaders/BasicRenderer/PostProcess.glsl", 8, 8);

	bfPostProcessData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(PostProcessData), GL_DYNAMIC_STORAGE_BIT);
	bfPostProcessData->setName("bfPostProcessData");

	initRenderTargets();

	//FRM_VERIFY(m_luminanceMeter.init(_resolutionY / 2));
	//m_colorCorrection.m_luminanceMeter = &m_luminanceMeter;
}

BasicRenderer::~BasicRenderer()
{
	//m_luminanceMeter.shutdown();

	shutdownRenderTargets();

	Shader::Release(shGBuffer);
	Shader::Release(shStaticVelocity);
	Shader::Release(shScene);
	Shader::Release(shImageLightBg);
	Shader::Release(shPostProcess);

	Buffer::Destroy(bfMaterials);
	Buffer::Destroy(bfLights);
	Buffer::Destroy(bfPostProcessData);
}

void BasicRenderer::initRenderTargets()
{
	shutdownRenderTargets();

	txGBuffer0 = Texture::Create2d(resolution.x, resolution.y, GL_RGBA16);
	txGBuffer0->setName("txGBuffer0");
	txGBuffer0->setWrap(GL_CLAMP_TO_EDGE);

	txGBufferDepth = Texture::Create2d(resolution.x, resolution.y, GL_DEPTH32F_STENCIL8);
	txGBufferDepth->setName("txGBufferDepth");
	txGBufferDepth->setWrap(GL_CLAMP_TO_EDGE);

	txScene = Texture::Create2d(resolution.x, resolution.y, GL_RGBA16F);
	txScene->setName("txScene");
	txScene->setWrap(GL_CLAMP_TO_EDGE);

	txFinal = Texture::Create2d(resolution.x, resolution.y, GL_RGBA8);
	txFinal->setName("txFinal");
	txFinal->setWrap(GL_CLAMP_TO_EDGE);

	fbGBuffer = Framebuffer::Create(2, txGBuffer0, txGBufferDepth);
	fbScene   = Framebuffer::Create(2, txScene, txGBufferDepth);
	fbFinal   = Framebuffer::Create(2, txFinal, txGBufferDepth);
}

void BasicRenderer::shutdownRenderTargets()
{	
	Texture::Release(txGBuffer0);
	Texture::Release(txGBufferDepth);
	Texture::Release(txScene);
	Texture::Release(txFinal);

	Framebuffer::Destroy(fbGBuffer);
	Framebuffer::Destroy(fbScene);
	Framebuffer::Destroy(fbFinal);
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
