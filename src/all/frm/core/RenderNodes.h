#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

namespace frm {

// \todo Render 'nodes' as a basis for script-based pipelines? Currently just dump common stuff into small classes which wrap boilerplate/shader loading etc
//	- Probably make Data private, have edit/serialize methods (or rely on Properties); this way you can know exactly if the data changed and reload the buffer when required


// Luminance meter with history/weighted average.
// \todo Switchable weight/averaging modes, enable/disable history.
class LuminanceMeter
{
public:
	struct Data
	{
		float m_rate = 0.25f;
	};
	Data m_data;

	void setProps(Properties& _props_);
	bool init(int _txSize = 1024);
	void shutdown();
	void reset();

	void draw(GlContext* _ctx_, float _dt, const Texture* _src, const Texture* _depth = nullptr);

	void edit();
	bool isEnabled() const { return m_enabled; }

	const Texture* getLuminanceTexture() { return m_txLum[m_current]; }
private:
	static const int kHistorySize  = 2;
	Texture* m_txLum[kHistorySize] = { nullptr };
	int      m_current             = 0;
	bool     m_enabled             = true;
	Shader*  m_shLuminanceMeter    = nullptr;
	Buffer*  m_bfData              = nullptr;
};

// Final exposure, tonemapping, color correction.
class ColorCorrection
{
public:
	struct Data
	{
		float m_exposure          = 0.0f;
		float m_localExposureMax  = 0.0f;
		float m_localExposureLod  = 1.0f;
		float m_contrast          = 1.0f;
		float m_saturation        = 1.0f;
		float _pad[3];
		vec3  m_tint              = vec3(1.0f);
	};
	Data m_data;
	LuminanceMeter* m_luminanceMeter = nullptr;

	void setProps(Properties& _props_);
	bool init();
	void shutdown();

	void draw(GlContext* _ctx_, const Texture* _src, const Framebuffer* _dst);

	void edit();
	bool isEnabled() const { return m_enabled; }

private:
	uint32  m_time                = 0;
	bool    m_enabled             = true;
	Shader* m_shColorCorrection   = nullptr;
	Shader* m_shBlit              = nullptr;
	Buffer* m_bfData              = nullptr;


}; // class ColorCorrection

} // namespace frm
