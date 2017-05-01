#include <frm/RenderNodes.h>

#include <frm/gl.h>
#include <frm/Property.h>
#include <frm/Buffer.h>
#include <frm/Framebuffer.h>
#include <frm/GlContext.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>

#include <apt/Json.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                  ColorCorrection

*******************************************************************************/

// PUBLIC

void ColorCorrection::setProps(Properties& _props_)
{
	PropertyGroup& propGroup = _props_.addGroup("Color Correction");
	//                 name           default        min             max          storage
	propGroup.addBool ("Enabled",     true,                                       &m_enabled);
	propGroup.addFloat("Exposure",    exp2(0.0f),    exp2(-12.0f),   exp2(12.0f), &m_data.m_exposure);
	propGroup.addFloat("Saturation",  1.0f,          0.0f,           8.0f,        &m_data.m_saturation);
	propGroup.addFloat("Contrast",    1.0f,          0.0f,           8.0f,        &m_data.m_contrast);
	propGroup.addRgb  ("Tint",        vec3(1.0f),    0.0f,           1.0f,        &m_data.m_tint);
}

bool ColorCorrection::init()
{
	m_shColorCorrection = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/ColorCorrection_fs.glsl");
	m_shBlit = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/Basic_fs.glsl");
	m_bfData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(Data), GL_DYNAMIC_STORAGE_BIT, &m_data);
	m_bfData->setName("_bfData");

	return m_shColorCorrection && m_shBlit && m_bfData;
}

void ColorCorrection::shutdown()
{
	Shader::Release(m_shColorCorrection);
	Shader::Release(m_shBlit);
}

void ColorCorrection::draw(GlContext* _ctx_, const Texture* _src, const Framebuffer* _dst)
{
	AUTO_MARKER("Color Correction");
	_ctx_->setFramebufferAndViewport(_dst);
	if (m_enabled) {
		_ctx_->setShader(m_shColorCorrection);
		_ctx_->setUniform("uTime", m_time++);
		_ctx_->bindTexture("txInput", _src);
		_ctx_->bindBuffer(m_bfData);
	} else {
		_ctx_->setShader(m_shBlit);
		_ctx_->bindTexture("txTexture2d", _src);
	}
	_ctx_->drawNdcQuad();
}

void ColorCorrection::edit()
{
	ImGui::Checkbox("Enabled", &m_enabled);
	if (m_enabled) {
		bool update = false;
		float exposure = log2(m_data.m_exposure);
		update |= ImGui::SliderFloat("Exposure", &exposure, -16.0f, 16.0f);
		m_data.m_exposure = exp2(exposure);
		update |= ImGui::SliderFloat("Saturation", &m_data.m_saturation, 0.0f, 8.0f);
		update |= ImGui::SliderFloat("Contrast", &m_data.m_contrast, 0.0f, 8.0f);
		update |= ImGui::ColorEdit3("Tint", &m_data.m_tint.x);
		if (update) {
			m_bfData->setData(sizeof(Data), &m_data);
		}
	}
}
