#pragma once
#ifndef frm_ShaderViewer_h
#define frm_ShaderViewer_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/math.h>

namespace frm { 

class Shader;

namespace ui {

////////////////////////////////////////////////////////////////////////////////
/// \class ShaderViewer
////////////////////////////////////////////////////////////////////////////////
class ShaderViewer
{
public:

	ShaderViewer() 
		: m_showHidden(false)
		, m_selectedShader(-1)
		, m_selectedStage(-1)
	{
	}

	void update();
	void draw(bool* _open_);

	bool   m_showHidden;
	int    m_selectedShader;
	GLenum m_selectedStage;

}; // class ShaderViewer

} } // namespace frm::ui

#endif // frm_ShaderViewer_h
