#include <frm/RenderNodes.h>

#include <frm/gl.h>
#include <frm/Property.h>
#include <frm/Buffer.h>
#include <frm/Framebuffer.h>
#include <frm/GlContext.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Texture.h>

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
		m_txLogLum[i] = Texture::Create2d(_txSize, _txSize, GL_R16F, Texture::GetMaxMipCount(_txSize, _txSize));
		if (!m_txLogLum[i]) {
			return false;
		}
		m_txLogLum[i]->setWrap(GL_CLAMP_TO_EDGE);
		m_txLogLum[i]->setNamef("#txLogLum[%d]", i);
	}
	m_current = 0;

	return m_shLuminanceMeter && m_bfData;
}

void LuminanceMeter::shutdown()
{
	for (int i = 0; i < kHistorySize; ++i) {
		Texture::Release(m_txLogLum[i]);
	}
	Buffer::Destroy(m_bfData);
	Shader::Release(m_shLuminanceMeter);
}

void LuminanceMeter::reset()
{
	Framebuffer* fb = Framebuffer::Create();
	for (int i = 0; i < kHistorySize; ++i) {
		fb->attach(m_txLogLum[i], GL_COLOR_ATTACHMENT0);
		GlContext::GetCurrent()->setFramebufferAndViewport(fb);
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
	}
	Framebuffer::Destroy(fb);
}

void LuminanceMeter::draw(GlContext* _ctx_, float _dt, const Texture* _src, const Texture* _depth)
{
	AUTO_MARKER("Luminance Meter");

	int prev = m_current;
	m_current = (m_current + 1) % kHistorySize;
	APT_ASSERT(prev != m_current);
	Texture* dst = m_txLogLum[m_current];

	{	AUTO_MARKER("Luminance/Smooth");
		_ctx_->setShader  (m_shLuminanceMeter);
		_ctx_->setUniform ("uDeltaTime", _dt);
		_ctx_->setUniform ("uSrcLevel", -1); // indicate first pass
		_ctx_->bindBuffer (m_bfData);
		_ctx_->bindTexture("txSrc", _src);
		_ctx_->bindTexture("txSrcPrev", m_txLogLum[prev]);
		_ctx_->bindImage  ("txDst", dst, GL_WRITE_ONLY, 0);
		_ctx_->dispatch   (dst);
	}
 
	{	AUTO_MARKER("Downsample");
		int wh = dst->getWidth() / 2;
		dst->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // no filtering between mips
		int lvl = 0;
		while (wh >= 1) {
			_ctx_->setShader  (m_shLuminanceMeter); // force reset bindings
			_ctx_->setUniform ("uSrcLevel", lvl);
			_ctx_->bindTexture("txSrc", dst);
			_ctx_->bindImage  ("txDst", dst, GL_WRITE_ONLY, ++lvl);
			_ctx_->dispatch(
				max((wh + m_shLuminanceMeter->getLocalSizeX() - 1) / m_shLuminanceMeter->getLocalSizeX(), 1),
				max((wh + m_shLuminanceMeter->getLocalSizeY() - 1) / m_shLuminanceMeter->getLocalSizeY(), 1)
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
	//                 name                        default        min             max          storage
	propGroup.addBool ("Enabled",                  true,                                       &m_enabled);
	propGroup.addFloat("Exposure Compensation",    exp2(0.0f),    exp2(-12.0f),   exp2(12.0f), &m_data.m_exposureCompensation);
	propGroup.addFloat("Aperture",                 4.0f,          1.0f,           24.0f,       &m_data.m_aperture);
	propGroup.addFloat("Shutter Speed",            1.0f/60.0f,    1.0f/100.0f,    1.0f/0.1f,   &m_data.m_shutterSpeed);
	propGroup.addFloat("ISO",                      100.0f,        64.0f,          6400.0f,     &m_data.m_iso);
	propGroup.addFloat("Saturation",               1.0f,          0.0f,           8.0f,        &m_data.m_saturation);
	propGroup.addFloat("Contrast",                 1.0f,          0.0f,           8.0f,        &m_data.m_contrast);
	propGroup.addRgb  ("Tint",                     vec3(1.0f),    0.0f,           1.0f,        &m_data.m_tint);
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
	AUTO_MARKER("Color Correction");
	_ctx_->setFramebufferAndViewport(_dst);
	if (m_enabled) {
		_ctx_->setShader  (m_shColorCorrection);
		_ctx_->setUniform ("uTime", m_time++);
		_ctx_->bindTexture("txInput", _src);
		if (m_luminanceMeter) {
			_ctx_->bindTexture("txAvgLogLuminance", m_luminanceMeter->getAvgLogLuminanceTexture());
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
		float exposure = log2(m_data.m_exposureCompensation);
		update |= ImGui::SliderFloat("Exposure Compensation", &exposure, -16.0f, 16.0f);
		m_data.m_exposureCompensation = exp2(exposure);
		update |= ImGui::SliderFloat("Aperture", &m_data.m_aperture, 1.0f, 24.0f);
		float shutterSpeed = 1.0f / m_data.m_shutterSpeed;
		update |= ImGui::SliderFloat("Shutter Speed", &shutterSpeed, 0.1f, 100.0f);
		m_data.m_shutterSpeed = 1.0f / shutterSpeed;
		update |= ImGui::SliderFloat("ISO", &m_data.m_iso, 64.0f, 6400.0f);

		ImGui::Spacing();
		update |= ImGui::SliderFloat("Saturation", &m_data.m_saturation, 0.0f, 8.0f);
		update |= ImGui::SliderFloat("Contrast", &m_data.m_contrast, 0.0f, 8.0f);
		update |= ImGui::ColorEdit3("Tint", &m_data.m_tint.x);
		if (update) {
			m_bfData->setData(sizeof(Data), &m_data);
		}
	}
}
