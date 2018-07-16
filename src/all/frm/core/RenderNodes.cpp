#include "RenderNodes.h"

#include <frm/core/gl.h>
#include <frm/core/Property.h>
#include <frm/core/Buffer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/Json.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                               LuminanceMeter

*******************************************************************************/

// PUBLIC

void LuminanceMeter::setProps(Properties& _props_)
{
	PropertyGroup& propGroup = _props_.addGroup("Luminance Meter");
	//                 name           default        min             max          storage
	propGroup.addBool ("Enabled",     true,                                       &m_enabled);
	propGroup.addFloat("Rate",        1.0f,          0.0f,           16.0f,       &m_data.m_rate);
}

bool LuminanceMeter::init(int _txSize)
{
	m_shLuminanceMeter = Shader::CreateCs("shaders/LuminanceMeter_cs.glsl", 8, 8);

	m_bfData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(Data), GL_DYNAMIC_STORAGE_BIT, &m_data);
	m_bfData->setName("_bfData");

	for (int i = 0; i < kHistorySize; ++i) {
		m_txLum[i] = Texture::Create2d(_txSize, _txSize, GL_RG16F, Texture::GetMaxMipCount(_txSize, _txSize));
		if (!m_txLum[i]) {
			return false;
		}
		m_txLum[i]->setWrap(GL_CLAMP_TO_EDGE);
		m_txLum[i]->setNamef("#txLum[%d]", i);
	}
	m_current = 0;
	reset();

	return m_shLuminanceMeter && m_bfData;
}

void LuminanceMeter::shutdown()
{
	for (int i = 0; i < kHistorySize; ++i) {
		Texture::Release(m_txLum[i]);
	}
	Buffer::Destroy(m_bfData);
	Shader::Release(m_shLuminanceMeter);
}

void LuminanceMeter::reset()
{
	glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
	Framebuffer* fb = Framebuffer::Create();
	for (int i = 0; i < kHistorySize; ++i) {
	 // clear the base level
		fb->attach(m_txLum[i], GL_COLOR_ATTACHMENT0, 0);
		GlContext::GetCurrent()->setFramebufferAndViewport(fb);
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
	 // clear the max level
		fb->attach(m_txLum[i], GL_COLOR_ATTACHMENT0, m_txLum[i]->getMipCount() - 1);
		GlContext::GetCurrent()->setFramebufferAndViewport(fb);
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
	}
	Framebuffer::Destroy(fb);
}

void LuminanceMeter::draw(GlContext* _ctx_, float _dt, const Texture* _src, const Texture* _depth)
{
	PROFILER_MARKER("Luminance Meter");

	int prev = m_current;
	m_current = (m_current + 1) % kHistorySize;
	APT_ASSERT(prev != m_current);
	Texture* dst = m_txLum[m_current];

	{	PROFILER_MARKER("Luminance/Smooth");
		_ctx_->setShader  (m_shLuminanceMeter);
		_ctx_->setUniform ("uSrcLevel", -1); // indicate first pass
		_ctx_->bindBuffer (m_bfData);
		_ctx_->bindTexture("txSrc", _src);
		_ctx_->bindImage  ("txDst", dst, GL_WRITE_ONLY, 0);
		_ctx_->dispatch   (dst);
	}
 
	{	PROFILER_MARKER("Downsample");
		int wh = dst->getWidth() / 2;
		dst->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // no filtering between mips
		int lvl = 0;
		ivec3 localSize = m_shLuminanceMeter->getLocalSize();
		while (wh >= 1) {
			_ctx_->setShader  (m_shLuminanceMeter); // force reset bindings
			_ctx_->setUniform ("uDeltaTime", _dt);
			_ctx_->setUniform ("uSrcLevel", lvl);
			_ctx_->setUniform ("uMaxLevel", m_txLum[0]->getMipCount() - 1);
			_ctx_->bindBuffer (m_bfData);
			_ctx_->bindTexture("txSrc", dst);
			_ctx_->bindTexture("txSrcPrev", m_txLum[prev]);
			_ctx_->bindImage  ("txDst", dst, GL_WRITE_ONLY, ++lvl);
			_ctx_->dispatch(
				APT_MAX((wh + localSize.x - 1) / localSize.x, 1),
				APT_MAX((wh + localSize.y - 1) / localSize.y, 1)
				);
			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			wh = wh >> 1;
		}
		dst->setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
	}
}

void LuminanceMeter::edit()
{
	ImGui::Checkbox("Enabled", &m_enabled);
	if (m_enabled) {
		bool update = false;
		update |= ImGui::SliderFloat("Rate", &m_data.m_rate, 0.0f, 8.0f);
		if (update) {
			m_bfData->setData(sizeof(Data), &m_data);
		}
		if (ImGui::Button("Reset")) {
			reset();
		}
	}
}

/*******************************************************************************

                                  ColorCorrection

*******************************************************************************/

// PUBLIC

void ColorCorrection::setProps(Properties& _props_)
{
	PropertyGroup& propGroup = _props_.addGroup("Color Correction");
	//                  name                     default                         min             max           storage
	propGroup.addBool  ("Enabled",               true,                                                         &m_enabled);
	propGroup.addFloat ("Exposure",              0.0f,                          -16.0f,          16.0f,        &m_data.m_exposure);
	propGroup.addFloat ("Local Exposure Max",    0.25f,                          0.0f,           1.0f,         &m_data.m_localExposureMax);
	propGroup.addFloat ("Local Exposure Lod",    log2(4.0f),                     log2(1.0f),     log2(512.0f), &m_data.m_localExposureLod);
	propGroup.addFloat ("Saturation",            1.0f,                           0.0f,           8.0f,         &m_data.m_saturation);
	propGroup.addFloat ("Contrast",              1.0f,                           0.0f,           8.0f,         &m_data.m_contrast);
	propGroup.addRgb   ("Tint",                  vec3(1.0f),                     0.0f,           1.0f,         &m_data.m_tint);
}

bool ColorCorrection::init()
{
	const char* defines = "\0";
	if (m_luminanceMeter) {
		defines = "AUTO_EXPOSURE\0";
	}
	m_shColorCorrection = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/ColorCorrection_fs.glsl", defines);
	m_shBlit = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/Basic_fs.glsl");
	m_bfData = Buffer::Create(GL_UNIFORM_BUFFER, sizeof(Data), GL_DYNAMIC_STORAGE_BIT, &m_data);
	m_bfData->setName("_bfData");

	return m_shColorCorrection && m_shBlit && m_bfData;
}

void ColorCorrection::shutdown()
{
	Shader::Release(m_shColorCorrection);
	Shader::Release(m_shBlit);
	Buffer::Destroy(m_bfData);
}

void ColorCorrection::draw(GlContext* _ctx_, const Texture* _src, const Framebuffer* _dst)
{
	PROFILER_MARKER("Color Correction");
	_ctx_->setFramebufferAndViewport(_dst);
	if (m_enabled) {
		_ctx_->setShader  (m_shColorCorrection);
		_ctx_->setUniform ("uTime", m_time++);
		_ctx_->bindTexture("txInput", _src);
		if (m_luminanceMeter) {
			_ctx_->bindTexture("txLuminance", m_luminanceMeter->getLuminanceTexture());
		}
		_ctx_->bindBuffer (m_bfData);
	} else {
		_ctx_->setShader  (m_shBlit);
		_ctx_->bindTexture("txTexture2d", _src);
	}
	_ctx_->drawNdcQuad();
}

void ColorCorrection::edit()
{
	ImGui::Checkbox("Enabled", &m_enabled);
	if (m_enabled) {
		if (m_luminanceMeter) {
			if (ImGui::TreeNode("Luminance Meter")) {
				m_luminanceMeter->edit();
				ImGui::TreePop();
			}
		}

		bool update = false;

		update |= ImGui::SliderFloat("Exposure", &m_data.m_exposure, -16.0f, 16.0f);
		if (m_luminanceMeter) {
			update |= ImGui::SliderFloat("Local Exposure Max",  &m_data.m_localExposureMax, 0.0f, 1.0f);

			float pixels = exp2(m_data.m_localExposureLod);
			update |= ImGui::SliderFloat("Local Exposure Radius", &pixels, 1.0f, 512.0f);
			m_data.m_localExposureLod = log2(pixels);
		}
		
		ImGui::Spacing();
		update |= ImGui::SliderFloat("Saturation", &m_data.m_saturation,   0.0f,  8.0f);
		update |= ImGui::SliderFloat("Contrast",   &m_data.m_contrast,     0.0f,  8.0f);
		
		vec3 tintGamma = pow(m_data.m_tint, vec3(1.0f/2.2f));
		if (ImGui::ColorEdit3("Tint", &tintGamma.x)) {
			m_data.m_tint = pow(tintGamma, vec3(2.2f));
			update = true;
		}

		if (update) {
			m_bfData->setData(sizeof(Data), &m_data);
		}

	}
}
