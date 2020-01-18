#include "RenderTarget.h"

#include <frm/core/GlContext.h>
#include <frm/core/Texture.h>

namespace frm {

// PUBLIC

RenderTarget::~RenderTarget()
{
	shutdown();
}

bool RenderTarget::init(int _width, int _height, GLenum _format, GLenum _wrap, GLenum _filter, int _bufferCount)
{
	shutdown();

	for (int i = 0; i < _bufferCount; ++i)
	{
		Texture*& tx = m_textures.push_back();
		tx = Texture::Create2d(_width, _height, _format);
		tx->setWrap(_wrap);
		tx->setFilter(_filter);
	}

	m_current = 0;

	return true;
}

void RenderTarget::setName(const char* _name)
{
	if (m_textures.size() == 1)
	{
		m_textures[0]->setName(_name);
	}
	else
	{
		for (size_t i = 0; i < m_textures.size(); ++i)
		{
			m_textures[i]->setName(String<64>("%s[%u]", _name, i).c_str());
		}
	}
	
}

void RenderTarget::shutdown()
{
	for (Texture* tx : m_textures)
	{
		Texture::Release(tx);
	}
	m_textures.clear();
}

} // namespace frm

