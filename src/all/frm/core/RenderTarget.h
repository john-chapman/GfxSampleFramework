#pragma once

#include <frm/core/frm.h>
#include <frm/core/Texture.h>

#include <eastl/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// RenderTarget
// N-buffered render target. 
////////////////////////////////////////////////////////////////////////////////
struct RenderTarget
{
	RenderTarget() = default;
	~RenderTarget();

	bool init(int _width, int _height, GLenum _format, GLenum _wrap, GLenum _filter, int _bufferCount, int _mipCount = 1);
	void shutdown();

	void setName(const char* _name);

	Texture* getTexture(int _offset)
	{
		if (m_textures.empty())
		{
			return nullptr;
		}
		
		const int size = (int)m_textures.size();
		int i = ((m_current + _offset) % size + size) % size;
		FRM_ASSERT(i >= 0 && i < m_textures.size());
		return m_textures[i];
	}

	void nextFrame()
	{
		size_t size = FRM_MAX((size_t)1, m_textures.size()); // prevent DBZ in case m_textures is empty
		m_current = (m_current + 1) % size;
	}

private:

	eastl::vector<Texture*> m_textures;
	int                     m_current;
};

} // namespace frm
