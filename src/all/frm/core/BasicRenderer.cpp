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

BasicRenderer* BasicRenderer::Create(int _resolutionX, int _resolutionY)
{
	BasicRenderer* ret = FRM_NEW(BasicRenderer(_resolutionX, _resolutionY));

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

 // \todo can skip updates if nothing changed
	updateMaterialInstances();
	updateDrawInstances(_camera);
	updateLightInstances(_camera);

	if (drawInstances.empty())
	{
		return;
	}

	GlContext* ctx = GlContext::GetCurrent();
	
	{	PROFILER_MARKER("GBuffer");

		ctx->setFramebufferAndViewport(fbGBuffer);
	 // \todo set the depth clear value based on the camera's projection mode, clear the color buffer?
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glAssert(glDepthFunc(GL_LESS));
		glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
		ctx->setShader(shGBuffer);
		ctx->bindBuffer(_camera->m_gpuBuffer);
		ctx->bindBuffer(bfMaterials);
		
		for (int materialIndex = 0; materialIndex < (int)drawInstances.size(); ++materialIndex)
		{
			if (drawInstances[materialIndex].empty())
			{
				continue;
			}

			ctx->setUniform ("uMaterialIndex", materialIndex);
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

	{	PROFILER_MARKER("Scene");

		ctx->setFramebufferAndViewport(fbScene);
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glAssert(glDepthFunc(GL_EQUAL));
		glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
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
		

		for (int materialIndex = 0; materialIndex < (int)drawInstances.size(); ++materialIndex)
		{
			if (drawInstances[materialIndex].empty())
			{
				continue;
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
	txScene->setMagFilter(GL_NEAREST);
	m_colorCorrection.draw(ctx, txScene, nullptr);
	txScene->setMagFilter(GL_LINEAR);
}

bool BasicRenderer::edit()
{
	bool ret = false;

	if (ImGui::TreeNode("Color Correction"))
	{
		m_colorCorrection.edit();
		ImGui::TreePop();
	}

	return ret;
}

// PRIVATE

BasicRenderer::BasicRenderer(int _resolutionX, int _resolutionY)
{
	shGBuffer = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "GBuffer_OUT" });
	shScene   = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "Scene_OUT" });

	txGBuffer0 = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA16);
	txGBuffer0->setName("txGBuffer0");
	txGBuffer0->setWrap(GL_CLAMP_TO_EDGE);

	txGBufferDepth = Texture::Create2d(_resolutionX, _resolutionY, GL_DEPTH32F_STENCIL8);
	txGBufferDepth->setName("txGBufferDepth");
	txGBufferDepth->setWrap(GL_CLAMP_TO_EDGE);

	txScene = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA16F);
	txScene->setName("txScene");
	txScene->setWrap(GL_CLAMP_TO_EDGE);

	fbGBuffer = Framebuffer::Create(2, txGBuffer0, txGBufferDepth);
	fbScene   = Framebuffer::Create(2, txScene, txGBufferDepth);

	//FRM_VERIFY(m_luminanceMeter.init(_resolutionY / 2));
	//m_colorCorrection.m_luminanceMeter = &m_luminanceMeter;
	FRM_VERIFY(m_colorCorrection.init());	
}

BasicRenderer::~BasicRenderer()
{
	m_colorCorrection.shutdown();
	//m_luminanceMeter.shutdown();

	Texture::Release(txGBuffer0);
	Texture::Release(txGBufferDepth);
	Framebuffer::Destroy(fbGBuffer);
	Texture::Release(txScene);
	Framebuffer::Destroy(fbScene);
	Shader::Release(shGBuffer);
	Shader::Release(shScene);
	Buffer::Destroy(bfMaterials);
	Buffer::Destroy(bfLights);
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

} // namespace frm
