#include "BasicRenderer.h"

#include <frm/core/geom.h>
#include <frm/core/BasicMaterial.h>
#include <frm/core/Camera.h>
#include <frm/core/Component.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Scene.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/memory.h>

#include <EASTL/vector.h>

using namespace apt;

namespace frm {

// PUBLIC

BasicRenderer* BasicRenderer::Create(int _resolutionX, int _resolutionY)
{
	BasicRenderer* ret = APT_NEW(BasicRenderer(_resolutionX, _resolutionY));

	return ret;
}

void BasicRenderer::Destroy(BasicRenderer*& _inst_)
{
	APT_DELETE(_inst_);
	_inst_ = nullptr;
}

void BasicRenderer::draw(Camera* _camera)
{
	PROFILER_MARKER("BasicRenderer::draw");

 // per-pass, construct a PVS of draw instances per material
	struct DrawInstance
	{
		Mesh*  m_mesh          = nullptr;
		mat4   m_world         = identity;
		uint32 m_materialIndex = ~0;
		uint32 m_submeshIndex  = 0;
	};
	typedef eastl::vector<eastl::vector<DrawInstance> > DrawInstanceMap; // [material index][instance]
	DrawInstanceMap gbufferInstances(BasicMaterial::GetInstanceCount());

	{	PROFILER_MARKER_CPU("Gather Instances");

		for (Component_BasicRenderable* renderable : Component_BasicRenderable::s_instances)
		{
			mat4 world = renderable->getNode()->getWorldMatrix();
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
						DrawInstance& drawInstance   = gbufferInstances[materialIndex].push_back();
						drawInstance.m_mesh          = renderable->m_mesh;
						drawInstance.m_world         = world;
						drawInstance.m_materialIndex = materialIndex;
						drawInstance.m_submeshIndex  = submeshIndex;
					}
				}
			}
		}
	}

	GlContext* ctx = GlContext::GetCurrent();
	
	{	PROFILER_MARKER("Gbuffer");

		ctx->setFramebuffer(m_fbGbuffer);
	 // \todo set the depth clear value based on the camera's projection mode, clear the color buffer?
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_CULL_FACE, GL_TRUE); // \todo per material?
		ctx->setShader(m_shGbuffer);
		ctx->bindBuffer(_camera->m_gpuBuffer);
		
	 // \todo sort each DrawInstance list by mesh/submesh for auto batching
		for (int materialIndex = 0; materialIndex < (int)gbufferInstances.size(); ++materialIndex)
		{
			if (gbufferInstances[materialIndex].empty())
			{
				continue;
			}

			BasicMaterial* material = BasicMaterial::GetInstance(materialIndex);
			ctx->setUniform ("uColorAlpha", material->getColorAlpha());
			ctx->setUniform ("uRough",      material->getRough());
			ctx->bindTexture("txAlbedo",    material->getMap(BasicMaterial::Map_Albedo));
			ctx->bindTexture("txNormal",    material->getMap(BasicMaterial::Map_Normal));
			ctx->bindTexture("txRough",     material->getMap(BasicMaterial::Map_Rough));
			ctx->bindTexture("txCavity",    material->getMap(BasicMaterial::Map_Cavity));
			ctx->bindTexture("txHeight",    material->getMap(BasicMaterial::Map_Height));
			ctx->bindTexture("txEmissive",  material->getMap(BasicMaterial::Map_Emissive));

			for (DrawInstance& drawInstance : gbufferInstances[materialIndex])
			{
				ctx->setMesh(drawInstance.m_mesh, drawInstance.m_submeshIndex);
				ctx->setUniform("uWorld", drawInstance.m_world);
				ctx->draw();
			}
		}
	}
}

// PRIVATE

BasicRenderer::BasicRenderer(int _resolutionX, int _resolutionY)
{
	m_shGbuffer = Shader::CreateVsFs("shaders/BasicRenderer/BasicMaterial.glsl", "shaders/BasicRenderer/BasicMaterial.glsl", { "GBUFFER" });

	m_txGbuffer0 = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA8);
	m_txGbuffer0->setName("txGbuffer0");
	m_txGbuffer0->setWrap(GL_CLAMP_TO_EDGE);

	m_txGbuffer1 = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA8);
	m_txGbuffer1->setName("txGbuffer1");
	m_txGbuffer1->setWrap(GL_CLAMP_TO_EDGE);

	m_txGbuffer2 = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA8);
	m_txGbuffer2->setName("txGbuffer2");
	m_txGbuffer2->setWrap(GL_CLAMP_TO_EDGE);

	m_txGbufferDepth = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA8);
	m_txGbufferDepth->setName("txGbufferDepth");
	m_txGbufferDepth->setWrap(GL_CLAMP_TO_EDGE);

	m_fbGbuffer = Framebuffer::Create(4, m_txGbuffer0, m_txGbuffer1, m_txGbuffer2, m_txGbufferDepth);

	m_txScene = Texture::Create2d(_resolutionX, _resolutionY, GL_RGBA16F);
	m_txScene->setName("txScene");
	m_txScene->setWrap(GL_CLAMP_TO_EDGE);

	m_fbScene = Framebuffer::Create(2, m_txScene, m_txGbufferDepth);
}

BasicRenderer::~BasicRenderer()
{
	Texture::Release(m_txGbuffer0);
	Texture::Release(m_txGbuffer1);
	Texture::Release(m_txGbuffer2);
	Texture::Release(m_txGbufferDepth);
	Framebuffer::Destroy(m_fbGbuffer);
	Texture::Release(m_txScene);
	Framebuffer::Destroy(m_fbScene);
}

} // namespace frm