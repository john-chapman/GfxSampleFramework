#pragma once
#ifndef frm_TextureViewer_h
#define frm_TextureViewer_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/math.h>

namespace frm { 

class Texture;
struct TextureView;
class Shader;

namespace ui {

////////////////////////////////////////////////////////////////////////////////
/// \class TextureViewer
////////////////////////////////////////////////////////////////////////////////
class TextureViewer
{
public:

	TextureViewer() 
		: m_showHidden(false)
		, m_selected(-1)
		, m_showTexelGrid(true)
	{
	}

	void update();
	void draw(bool* _open_);

	TextureView* findTxView(const Texture* _tx);

	bool m_showHidden;
	int m_selected;
	bool m_showTexelGrid;

}; // class TextureViewer

} } // namespace frm::ui

#endif // frm_TextureViewer_h
