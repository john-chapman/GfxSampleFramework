#pragma once
#ifndef frm_Framebuffer_h
#define frm_Framebuffer_h

#include <frm/def.h>
#include <frm/gl.h>

namespace frm {

class GlContext;
class Texture;

////////////////////////////////////////////////////////////////////////////////
/// \class Framebuffer
/// Framebuffer to which textures may be attached for rendering.
/// \note Color attachment order is constrained such that GL_COLOR_ATTACHMENTi
///    always receives the ith color output of a fragment shader.
/// \note Framebuffers are not resources (they don't need to be shared). It may
///    be useful to allow them to be globally named (e.g. for scripted 
///    pipelines).
////////////////////////////////////////////////////////////////////////////////
class Framebuffer: public apt::non_copyable<Framebuffer>
{
public:

	static Framebuffer* Create();
	
	/// Argument list is a set of _count texture pointers to attach (in order).
	static Framebuffer* Create(int _count, ...);

	static void Destroy(Framebuffer*& _inst_);

	/// Attach _mip level of _texture to _attachment. _texture may not be an
	/// array, 3d or cubemap texture (use attachLayer() or attachFace()).
	/// _texture may be 0 in which case the attachment is removed.
	void attach(Texture* _texture, GLenum _attachment, GLint _mip = 0);

	/// Attach _mip level of _layer of _texture to _attachment. _texture must be 
	/// an array or 3d texture. _texture may be 0 in which case the attachment is 
	/// removed.
	void attachLayer(Texture* _texture, GLenum _attachment, GLint _layer, GLint _mip = 0);
	
	/// \return Texture at _attachment or 0 if no attachment was present.
	Texture* getAttachment(GLenum _attachment) const;

	/// \return the framebuffer status, which should be GL_FRAMEBUFFER_COMPLETE
	///   in order for the framebuffer to be useable.
	GLenum getStatus() const;

	GLuint getHandle() const   { return m_handle; }
	GLint  getWidth() const    { return m_width; }
	GLint  getHeight() const   { return m_height; }

private:

	GLuint m_handle;

	static const int kMaxColorAttachments = 8;
	static const int kMaxAttachments = kMaxColorAttachments + 3; // +3 for depth, stencil & depth-stencil
	Texture* m_textures[kMaxAttachments]; //< Attached textures

	GLint m_width, m_height; //< Min of all attachments.


	/// Convert an attachment enum (e.g. GL_COLOR_ATTACHMENT0) to an index into the
	/// attachment list.
	static int GetAttachmentIndex(GLenum _attachment);

	Framebuffer();
	~Framebuffer();

	/// Common code for attach* variants.
	void attachImpl(Texture* _texture, GLenum _attachment, GLint _mip, GLint _layer = -1);

	/// Call when adding/removing an attachment; sets metadata and calls glDrawBuffers.
	void update();
	

}; // Framebuffer

} // namespace frm

#endif // frm_Framebuffer_h