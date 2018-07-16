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
		float m_rate;
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
	static const int kHistorySize = 2;
	Texture* m_txLum[kHistorySize];
	int      m_current;
	bool     m_enabled;
	Shader*  m_shLuminanceMeter;
	Buffer*  m_bfData;
};

// Final exposure, tonemapping, color correction.
class ColorCorrection
{
public:
	struct Data
	{
		float m_exposure;
		float m_localExposureMax;
		float m_localExposureLod;
		float m_contrast;
		float m_saturation;
		float _pad[3];
		vec3  m_tint;
	};
	Data m_data;
	LuminanceMeter *m_luminanceMeter;

	void setProps(Properties& _props_);
	bool init();
	void shutdown();

	void draw(GlContext* _ctx_, const Texture* _src, const Framebuffer* _dst);

	void edit();
	bool isEnabled() const { return m_enabled; }

private:
	uint32  m_time;
	bool    m_enabled;
	Shader* m_shColorCorrection;
	Shader* m_shBlit;
	Buffer* m_bfData;


}; // class ColorCorrection

} // namespace frm
