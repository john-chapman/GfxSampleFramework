#include <frm/Framebuffer.h>

#include <frm/gl.h>
#include <frm/GlContext.h>
#include <frm/Texture.h>

#include <cstdarg>
#include <cstring>

using namespace frm;
using namespace apt;

static const GLenum kAttachmentEnums[] =
{
	GL_COLOR_ATTACHMENT0,
	GL_COLOR_ATTACHMENT1,
	GL_COLOR_ATTACHMENT2,
	GL_COLOR_ATTACHMENT3,
	GL_COLOR_ATTACHMENT4,
	GL_COLOR_ATTACHMENT5,
	GL_COLOR_ATTACHMENT6,
	GL_COLOR_ATTACHMENT7,

	GL_DEPTH_ATTACHMENT,
	GL_STENCIL_ATTACHMENT,
	GL_DEPTH_STENCIL_ATTACHMENT,
};

/*******************************************************************************

                                Framebuffer

*******************************************************************************/

// PUBLIC

Framebuffer* Framebuffer::Create()
{
	Framebuffer* ret = new Framebuffer();
	return ret;
}

Framebuffer* Framebuffer::Create(int _count, ...)
{
	Framebuffer* ret = new Framebuffer();

	va_list args;
	va_start(args, _count);
	int colorCount = 0;
	for (int i = 0; i < _count; ++i) {
		Texture* tx = va_arg(args, Texture*);
		switch (tx->getFormat()) {
		case GL_DEPTH_COMPONENT:
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32F:
			ret->attachImpl(tx, GL_DEPTH_ATTACHMENT, 0);
			break;
		case GL_DEPTH_STENCIL:
		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:
			ret->attachImpl(tx, GL_DEPTH_STENCIL_ATTACHMENT, 0);
			break;
		default:
			APT_ASSERT(kAttachmentEnums[colorCount] != GL_DEPTH_ATTACHMENT); // too many color attachments
			ret->attachImpl(tx, kAttachmentEnums[colorCount++], 0);
			break;
		};
	}
	va_end(args);

	return ret;
}

void Framebuffer::Destroy(Framebuffer*& _inst_)
{
	APT_ASSERT(_inst_);
	delete _inst_;
	_inst_ = 0;
}

void Framebuffer::attach(Texture* _texture, GLenum _attachment, GLint _mip)
{
	attachImpl(_texture, _attachment, _mip);
}

void Framebuffer::attachLayer(Texture* _texture, GLenum _attachment, GLint _layer, GLint _mip)
{
	attachImpl(_texture, _attachment, _mip, _layer);
}

Texture* Framebuffer::getAttachment(GLenum _attachment) const
{
	return m_textures[GetAttachmentIndex(_attachment)];
}

GLenum Framebuffer::getStatus() const
{
	GLenum ret;
	glAssert(ret = glCheckNamedFramebufferStatus(m_handle, GL_DRAW_FRAMEBUFFER));
	return ret;
}

// PRIVATE

int Framebuffer::GetAttachmentIndex(GLenum _attachment)
{
	for (int i = 0; i < kMaxAttachments; ++i) {
		if (kAttachmentEnums[i] == _attachment) {
			return i;
		}
	}
	APT_ASSERT(false);
	return 0;
}

Framebuffer::Framebuffer()
	: m_handle(0)
	, m_width(0)
	, m_height(0)
{
	memset(m_textures, 0, sizeof(m_textures));
	glAssert(glCreateFramebuffers(1, &m_handle));
}

Framebuffer::~Framebuffer()
{
	for (uint i = 0; i < kMaxAttachments; ++i) {
		if (m_textures[i] != 0) {
			Texture::Release(m_textures[i]);
			m_textures[i] = 0;
		}
	}
	if (m_handle) {
		glAssert(glDeleteFramebuffers(1, &m_handle));
		m_handle = 0;
	}
}

void Framebuffer::attachImpl(Texture* _texture, GLenum _attachment, GLint _mip, GLint _layer)
{
	int i = GetAttachmentIndex(_attachment);
	if (m_textures[i] != 0) {
		Texture::Release(m_textures[i]);
		m_textures[i] = 0;
	}
	if (_texture) {
		m_textures[i] = _texture;
		Texture::Use(_texture);
		APT_ASSERT(_texture->getState() == Texture::State::kLoaded);
		APT_ASSERT(_texture->getMipCount() >= _mip);
		if (_layer >= 0) {
			APT_ASSERT(_texture->getDepth() >= _layer || _texture->getArrayCount() >= _layer);
			glAssert(glNamedFramebufferTextureLayer(m_handle, _attachment, _texture->getHandle(), _mip, _layer));
		} else {
			glAssert(glNamedFramebufferTexture(m_handle, _attachment, _texture->getHandle(), _mip));
		}
	}
	update();
}

void Framebuffer::update()
{
	GLenum drawBuffers[kMaxColorAttachments];
	m_width = m_height = INT_MAX;
	for (int i = 0; i < kMaxColorAttachments; ++i) {
		if (m_textures[i]) {
			drawBuffers[i] = kAttachmentEnums[i];
			m_width  = APT_MIN(m_width,  m_textures[i]->getWidth());
			m_height = APT_MIN(m_height, m_textures[i]->getHeight());
		} else {
			drawBuffers[i] = GL_NONE;
		}
	}
	for (int i = kMaxColorAttachments; i < kMaxAttachments; ++i) {
		if (m_textures[i]) {
			m_width  = APT_MIN(m_width,  m_textures[i]->getWidth());
			m_height = APT_MIN(m_height, m_textures[i]->getHeight());
		}
	}
	glAssert(glNamedFramebufferDrawBuffers(m_handle, kMaxColorAttachments, drawBuffers));
}