#include <frm/RenderNodes.h>

#include <frm/gl.h>
#include <frm/Property.h>
#include <frm/Buffer.h>
#include <frm/GlContext.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  ColorCorrection

*******************************************************************************/

// PUBLIC

bool ColorCorrection::init(Properties& _props_)
{
	m_shader = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/ColorCorrection_fs.glsl");
	
	PropertyGroup& propGroup = _props_.addGroup("Color Correction");
	//                 name           default        min             max          storage
	propGroup.addFloat("Exposure",    exp2(0.0f),    exp2(-12.0f),   exp2(12.0f), &m_data.m_exposure);
	propGroup.addFloat("Saturation",  1.0f,          0.0f,           8.0f,        &m_data.m_saturation);
	propGroup.addFloat("Contrast",    1.0f,          0.0f,           8.0f,        &m_data.m_contrast);
	propGroup.addRgb  ("Tint",        vec3(1.0f),    0.0f,           1.0f,        &m_data.m_tint);
	
	m_data.m_exposure    = exp2(0.0f);
	m_data.m_saturation  = 1.0f;
	m_data.m_contrast    = 1.0f;
	m_data.m_tint        = vec3(1.0f);

	m_bfData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(Data), GL_DYNAMIC_STORAGE_BIT, &m_data);
	m_bfData->setName("_bfData");

	return m_shader && m_bfData;
}

void ColorCorrection::shutdown()
{
	Shader::Release(m_shader);
}

void ColorCorrection::draw(GlContext* _ctx_, const Texture* _src, const Framebuffer* _dst)
{
	AUTO_MARKER("Color Correction");
	_ctx_->setFramebufferAndViewport(_dst);
	_ctx_->setShader(m_shader);
	_ctx_->bindTexture("txInput", _src);
	_ctx_->bindBuffer(m_bfData);
	_ctx_->drawNdcQuad();
}
