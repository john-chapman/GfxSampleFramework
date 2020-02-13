#include "DepthTest.h"

#include <frm/core/frm.h>
#include <frm/core/Buffer.h>
#include <frm/core/Camera.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Scene.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <imgui/imgui.h>

using namespace frm;

static DepthTest s_inst;

DepthTest::DepthTest()
	: AppBase("Depth") 
{
	PropertyGroup& props = m_props.addGroup("DepthTest");
	//             name                     default                  min     max                   storage
	props.addBool ("m_reconstructPosition", m_reconstructPosition,                                 &m_reconstructPosition);
	props.addInt  ("m_instanceCount",       m_instanceCount,         1,      128,                  &m_instanceCount);
	props.addInt  ("m_depthFormat",         m_depthFormat,           0,      DepthFormat_Count,    &m_depthFormat);
	props.addFloat("m_maxError",            m_maxError,              0.0f,   1.0f,                 &m_maxError);
}

DepthTest::~DepthTest()
{
}

bool DepthTest::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
		return false;
	}

	if (!initShaders())
	{
		return false;
	}

	if (!initTextures())
	{
		return false;
	}

	m_mesh = Mesh::Create("models/Teapot_1.obj");
	if (!CheckResource(m_mesh)) return false;

	m_txRadar = Texture::Create("textures/radar.tga");
	if (!CheckResource(m_txRadar)) return false;
	m_txRadar->setWrapU(GL_CLAMP_TO_EDGE);

	return true;
}

void DepthTest::shutdown()
{
	shutdownShaders();
	shutdownTextures();

	Mesh::Release(m_mesh);
	Buffer::Destroy(m_bfInstances);
	Texture::Release(m_txRadar);

	AppBase::shutdown();
}

bool DepthTest::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	Camera* drawCamera = Scene::GetDrawCamera();

	if (ImGui::Combo("Depth Format", &m_depthFormat,
		"DepthFormat_16\0"
		"DepthFormat_24\0"
		"DepthFormat_32\0"
		"DepthFormat_32F\0"
		))
	{
		initTextures();
	}

	ImGui::Text("Projection Type: %s %s %s", 
		drawCamera->getProjFlag(Camera::ProjFlag_Perspective) ? "PERSP " : "ORTHO ",
		drawCamera->getProjFlag(Camera::ProjFlag_Infinite)    ? "INF "   : "",
		drawCamera->getProjFlag(Camera::ProjFlag_Reversed)    ? "REV "   : ""
		);

	ImGui::SliderFloat("Max Error", &m_maxError, 0.0f, 1.0f, "%0.4f", 2.0f);
	ImGui::Checkbox("Reconstruct Position", &m_reconstructPosition);

	if (ImGui::SliderInt("Instance Count", &m_instanceCount, 1, 256) || !m_bfInstances)
	{
		const int totalInstanceCount = m_instanceCount * m_instanceCount;
		m_bfInstances = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(mat4) * totalInstanceCount, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
		
		PROFILER_MARKER_CPU("Instance Update");
		
		mat4* instanceData = (mat4*)m_bfInstances->map(GL_WRITE_ONLY);
		const float radius = m_mesh->getBoundingSphere().m_radius;
		const float spacing = radius * 2.0f;
		for (int x = 0; x < m_instanceCount; ++x) 
		{
			float px = (float)(x - m_instanceCount / 2) * spacing;
			for (int z = 0; z < m_instanceCount; ++z)
			{
				float pz = (float)(z - m_instanceCount / 2) * spacing;
				vec3 p = vec3(px, 0.0f, pz);
				instanceData[x * m_instanceCount + z] = TranslationMatrix(p);
			}
		}
		m_bfInstances->unmap();
	}

	return true;
}

void DepthTest::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	Camera* drawCamera = Scene::GetDrawCamera();

	{	PROFILER_MARKER("Depth Only");

		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_CULL_FACE, GL_TRUE);
		glAssert(glColorMask(0, 0, 0, 0));
		glAssert(glDepthFunc(drawCamera->getProjFlag(Camera::ProjFlag_Reversed) ? GL_GREATER : GL_LESS));
		glAssert(glClearDepth(drawCamera->getProjFlag(Camera::ProjFlag_Reversed) ? 0.0f : 1.0f));
		
		ctx->setFramebufferAndViewport(m_fbDepth);
		glAssert(glClear(GL_DEPTH_BUFFER_BIT));
		ctx->setShader(m_shDepthOnly);
		ctx->setMesh(m_mesh);
		ctx->bindBuffer("_bfInstances", m_bfInstances);
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->draw(m_instanceCount * m_instanceCount);
		
		glAssert(glColorMask(1, 1, 1, 1));
		glAssert(glDepthFunc(GL_LESS));
		glAssert(glClearDepth(1.0f));
	}

	{	PROFILER_MARKER("Depth Error");

		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_CULL_FACE, GL_TRUE);
		glAssert(glDepthMask(0));
		glAssert(glDepthFunc(GL_EQUAL));

		ctx->setFramebufferAndViewport(m_fbDepthColor);
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		ctx->setShader(m_shDepthError);
		ctx->setMesh(m_mesh);
		ctx->bindTexture("txDepth", m_txDepth);
		ctx->bindTexture("txRadar", m_txRadar);
		ctx->bindBuffer("_bfInstances", m_bfInstances);
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->setUniform("uMaxError", m_maxError);
		ctx->setUniform("uReconstructPosition", (int)m_reconstructPosition);
		ctx->draw(m_instanceCount * m_instanceCount);
		
		glAssert(glDepthFunc(GL_LESS));
		glAssert(glDepthMask(1));
	}

	ctx->blitFramebuffer(m_fbDepthColor, 0, GL_COLOR_BUFFER_BIT);

	AppBase::draw();
}

// PROTECTED

bool DepthTest::initShaders()
{
	m_shDepthOnly  = Shader::CreateVsFs("shaders/DepthTest.glsl", "shaders/DepthTest.glsl");
	if (!CheckResource(m_shDepthOnly)) return false;

	m_shDepthError = Shader::CreateVsFs("shaders/DepthTest.glsl", "shaders/DepthTest.glsl", { "DEPTH_ERROR" });
	if (!CheckResource(m_shDepthError)) return false;

	return true;
}

void DepthTest::shutdownShaders()
{
	Shader::Release(m_shDepthOnly);
	Shader::Release(m_shDepthError);
}

bool DepthTest::initTextures()
{
	shutdownTextures();

	GLenum glDepthFormat = GL_NONE;
	switch (m_depthFormat) 
	{
		default:              FRM_ASSERT(false); break;
		case DepthFormat_16:  glDepthFormat = GL_DEPTH_COMPONENT16;  break;
		case DepthFormat_24:  glDepthFormat = GL_DEPTH_COMPONENT24;  break;
		case DepthFormat_32:  glDepthFormat = GL_DEPTH_COMPONENT32;  break;
		case DepthFormat_32F: glDepthFormat = GL_DEPTH_COMPONENT32F; break;
	};

	m_txDepth = Texture::Create2d(m_resolution.x, m_resolution.y, glDepthFormat);
	if (!CheckResource(m_txDepth)) return false;
	m_txDepth->setName("txDepth");

	m_txColor = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	if (!CheckResource(m_txColor)) return false;
	m_txColor->setName("txColor");

	m_fbDepth = Framebuffer::Create(m_txDepth);
	m_fbDepthColor = Framebuffer::Create(2, m_txColor, m_txDepth);

	return true;
}

void DepthTest::shutdownTextures()
{
	Texture::Release(m_txDepth);
	Texture::Release(m_txColor);
	Framebuffer::Destroy(m_fbDepth);
	Framebuffer::Destroy(m_fbDepthColor);
}