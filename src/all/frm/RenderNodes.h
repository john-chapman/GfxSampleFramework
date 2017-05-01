#pragma once
#ifndef frm_render_Nodes
#define frm_render_Nodes

#include <frm/def.h>
#include <frm/math.h>

namespace frm {

// \todo Render 'nodes' as a basis for script-based pipelines? Currently just dump common stuff into small classes which wrap boilerplate/shader loading etc

class ColorCorrection
{
public:
 // \todo probably make the data private, have edit/serialize methods (or rely on AppProperties?)
 // this way you can know exactly if the data changed and reload the buffer when required
	struct Data
	{
		float m_exposure;
		float m_saturation;
		float m_contrast;
		float pad0;
		vec3  m_tint;
	};
	Data m_data;

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
