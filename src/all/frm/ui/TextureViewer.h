#pragma once
#ifndef frm_TextureViewer_h
#define frm_TextureViewer_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/math.h>

namespace frm { 

namespace ui {

////////////////////////////////////////////////////////////////////////////////
/// \class TextureViewer
////////////////////////////////////////////////////////////////////////////////
class TextureViewer
{
public:

	TextureViewer() 
		: m_selected(nullptr)
		, m_showHidden(false)
		, m_showTexelGrid(true)
		, m_isDragging(false)
	{
	}

	void draw(bool* _open_);

	static vec2 ThumbToTxView(const frm::TextureView& _txView);

	Texture* m_selected;
	bool m_showHidden;
	bool m_showTexelGrid;
	bool m_isDragging;

}; // class TextureViewer

} } // namespace frm::ui

#endif // frm_TextureViewer_h
