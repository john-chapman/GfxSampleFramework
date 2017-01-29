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
		: m_selected(-1)
		, m_showHidden(false)
		, m_showTexelGrid(true)
		, m_isDragging(false)
	{
	}

	void update();
	void draw(bool* _open_);

	TextureView* findTxView(const Texture* _tx);

	static vec2 ThumbToTxView(const TextureView& _txView);

	int  m_selected;
	bool m_showHidden;
	bool m_showTexelGrid;
	bool m_isDragging;

}; // class TextureViewer

} } // namespace frm::ui

#endif // frm_TextureViewer_h
