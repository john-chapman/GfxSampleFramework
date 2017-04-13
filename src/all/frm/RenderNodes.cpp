#include <frm/RenderNodes.h>

#include <frm/gl.h>
#include <frm/AppProperty.h>
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

bool ColorCorrection::init(AppProperties& _props_)
{
	m_shader = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/ColorCorrection_fs.glsl");
	
 // \todo refactor app properties, set property pointers here
	AppPropertyGroup& group = _props_.addGroup("ColorCorrection");

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
