#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// BasicRenderer
// \todo
// - Init all textures to defaults (can't have empty slots).
////////////////////////////////////////////////////////////////////////////////
struct BasicRenderer
{
	static BasicRenderer* Create(int _resolutionX, int _resolutionY);
	static void Destroy(BasicRenderer*& _inst_);

	void draw(Camera* _camera);

	Texture*     m_txGBuffer0     = nullptr;
	Texture*     m_txGBuffer1     = nullptr;
	Texture*     m_txGBuffer2     = nullptr;
	Texture*     m_txGBufferDepth = nullptr;
	Framebuffer* m_fbGBuffer      = nullptr;
	Shader*      m_shGBuffer      = nullptr;
	Texture*     m_txScene        = nullptr;
	Framebuffer* m_fbScene        = nullptr;

private:
	BasicRenderer(int _resolutionX, int _resolutionY);
	~BasicRenderer();

}; // class BasicRenderer

} // namespace frm
