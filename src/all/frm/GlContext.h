#pragma once
#ifndef frm_GlContext_h
#define frm_GlContext_h

#include <frm/gl.h>

namespace frm {

class Buffer;
class Framebuffer;
class Mesh;
class Shader;
class Texture;
class Window;

////////////////////////////////////////////////////////////////////////////////
/// \class GlContext
////////////////////////////////////////////////////////////////////////////////
class GlContext: private apt::non_copyable<GlContext>
{
public:
	GLint kMaxComputeInvocations;   //< Total maximum invocations per work group.
	GLint kMaxComputeLocalSize[3];  //< Maximum local group size.
	GLint kMaxComputeWorkGroups[3]; //< Maximum number of work groups.

	enum class VsyncMode: int
	{
		kAdaptive = -1, // swap/tear
		kOff      =  0,
		kOn       =  1, // wait 1 interval
		kOn2,           // wait 2 intervals
		kOn3,           // wait 3 intervals
	};

	/// Create an OpenGL context of at least version _vmaj._vmin, optionally
	/// with the comptaibility profile enabled. The context is bound to _window
	/// and is current on the calling thread when this function returns.
	/// \return OpenGL context, or 0 if an error occurred.
	static GlContext* Create(const Window* _window, int _vmaj, int _vmin, bool _compatibility);
	
	/// Destroy OpenGL context. This implicitly destroys all associated resources.
	static void Destroy(GlContext*& _ctx_);
	
	/// \return Current context on the calling thread, or 0 if none.
	static GlContext* GetCurrent();

	/// Make _ctx current on the calling thread.
	/// \return false if the operation fails.
	static bool MakeCurrent(GlContext* _ctx);


	/// Make an instanced draw call via glDrawArraysInstanced/glDrawElementsInstanced
	/// (render the current mesh with the current shader to the current framebuffer).
	void draw(GLsizei _instances = 1);
	/// Make an indirect draw call via glDrawArraysIndirect/glDrawElementsIndirect,
	/// with _buffer bound as GL_DRAW_INDIRECT_BUFFER.
	void drawIndirect(const Buffer* _buffer, const void* _offset = 0);

	/// Dispatch a compute shader with the specified number of work groups.
	void dispatch(GLuint _groupsX, GLuint _groupsY = 1, GLuint _groupsZ = 1);
	/// Make an indirect compute shader dispatch with _buffer bound as 
	/// GL_DISPATCH_INDIRECT_BUFFER.
	void dispatchIndirect(const Buffer* _buffer, const void* _offset = 0);

	/// Present the next image in the swapchain, increment the frame index.
	void present();


	void      setVsyncMode(VsyncMode _mode);
	VsyncMode getVsyncMode() const               { return m_vsyncMode; }
	
	uint64    getFrameIndex() const              { return m_frameIndex; }


 // FRAMEBUFFER

	/// Pass 0 to set the default framebuffer/viewport.
	void  setFramebuffer(const Framebuffer* _framebuffer);
	void  setFramebufferAndViewport(const Framebuffer* _framebuffer);
	const Framebuffer* getFramebuffer()          { return m_currentFramebuffer; }

	void setViewport(int _x, int _y, int _width, int _height);
	int  getViewportX() const                    { return m_viewportX; }
	int  getViewportY() const                    { return m_viewportY; }
	int  getViewportWidth() const                { return m_viewportWidth; }
	int  getViewportHeight() const               { return m_viewportHeight; }

	void blitFramebuffer(Framebuffer* _src, Framebuffer* _dst, GLbitfield _mask = GL_COLOR_BUFFER_BIT, GLenum _filter = GL_NEAREST);
	
 // SHADER

	void setShader(const Shader* _shader);
	const Shader* getShader()                    { return m_currentShader; }	
	/// Set uniform values on the currently bound shader. If there is no current
	/// shader or if _name is not an active uniform, the call does nothing.
	template <typename tType>
	void setUniformArray(const char* _name, const tType* _val, GLsizei _count);
	template <typename tType>
	void setUniform(const char* _name, const tType& _val) { setUniformArray<tType>(_name, &_val, 1); }

 // MESH

	void setMesh(const Mesh* _mesh, int _submeshId = -1);
	const Mesh* getMesh()                        { return m_currentMesh; }

 // BUFFER
	
	/// Bind _buffer to a named _location on the current shader. The target
	/// is chosen from the _buffer's target hint; only atomic, tranform-feedback, 
	/// uniform and storage buffers are allowed. Binding indices are managed 
	/// automatically; they are reset only when the current shader changes. 
	/// If _location is not active on the current shader, do nothing.
	void bindBuffer(const char* _location, const Buffer* _buffer);
	void bindBufferRange(const char* _location, const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);

	/// As bindBuffer()/bindBufferRange() but use _buffer->getName() as the
	/// location.
	void bindBuffer(const Buffer* _buffer);
	void bindBufferRange(const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);
	
	/// Bind _buffer to _target, or to _buffer's target hint by default.
	/// This is intended for non-indexed targets e.g. GL_DRAW_INDIRECT_BUFFER.
	void bindBuffer(const Buffer* _buffer, GLenum _target);

	/// Clear all current buffer bindings. 
	void clearBufferBindings();

 // TEXTURE

	/// Bind _texture to a named _location on the current shader. Binding 
	/// indices are managed automatically; they are reset only when the current
	/// shader changes. If _location is not active on the current shader, do 
	/// nothing.
	void bindTexture(const char* _location, const Texture* _texture);

	/// As bindTexture() but use _texture->getName() as the location.
	void bindTexture(const Texture* _texture);

	/// Clear all texture bindings. 
	void clearTextureBindings();

 // IMAGE

	/// Bind _texture as an image to a named _location on the current shader. _access
	/// is one of GL_READ_ONLY, GL_WRITE_ONLY or GL_READ_WRITE.
	void bindImage(const char* _location, const Texture* _texture, GLenum _access);

	/// Clear all image bindings. 
	void clearImageBindings();
	
private:
	const Window*       m_window;
	VsyncMode           m_vsyncMode;
	uint64              m_frameIndex;

	int                 m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight;

 	const Framebuffer*  m_currentFramebuffer;
	const Shader*       m_currentShader;
	const Mesh*         m_currentMesh;
	int                 m_currentSubmesh;

	// \note Tracking state for all targets is redundant as only a subset use an indexed binding model
	static const int    kBufferSlotCount  = 16;
	const Buffer*       m_currentBuffers [internal::kBufferTargetCount][kBufferSlotCount];
	GLint               m_nextBufferSlots[internal::kBufferTargetCount];

	static const int    kTextureSlotCount = 24;
	const Texture*      m_currentTextures[kTextureSlotCount];
	GLint               m_nextTextureSlot;

	static const int    kImageSlotCount = 8;
	const Texture*      m_currentImages[kImageSlotCount];
	GLint               m_nextImageSlot;
	
	struct Impl;
	Impl* m_impl;
	

	GlContext();
	~GlContext();

	void queryLimits();

	
}; // class GlContext

} // namespace frm

#endif // frm_GlContext_h
