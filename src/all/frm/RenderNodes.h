#pragma once
#ifndef frm_render_Nodes
#define frm_render_Nodes

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

// \todo Render 'nodes' as a basis for script-based pipelines? Currently just dump common stuff into small classes which wrap boilerplate/shader loading etc
//	- Probably make Data private, have edit/serialize methods (or rely on Properties); this way you can know exactly if the data changed and reload the buffer when required


// Log luminance history buffer (use for auto-exposure).
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

	void draw(GlContext* _ctx_, float _dt, const Texture* _src, const Texture* _depth = nullptr);

	void edit();
	bool isEnabled() const { return m_enabled; }

	const Texture* getAvgLogLuminanceTexture() { return m_txLogLum[m_current]; }
private:
	static const int kHistorySize = 2;
	Texture* m_txLogLum[kHistorySize];
	int      m_current;
	bool     m_enabled;
	Shader*  m_shLuminanceMeter;
	Buffer*  m_bfData;
};

// Final exposure, tonemapping and color correction.
class ColorCorrection
{
public:
	struct Data
	{
		float m_saturation;
		float m_contrast;
		float m_exposureCompensation;
		float m_aperture;
		float m_shutterSpeed;
		float m_iso;
		float pad0;
		float pad1;
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

#endif // frm_render_Nodes
