#pragma once

#include <frm/core/gl.h>
#include <frm/core/Viewport.h>

namespace frm {

class  Buffer;
class  Framebuffer;
class  Mesh;
class  Shader;
class  Texture;
class  Window;

namespace refactor {
	class Mesh;
};

////////////////////////////////////////////////////////////////////////////////
// GlContext
////////////////////////////////////////////////////////////////////////////////
class GlContext: private frm::non_copyable<GlContext>
{
public:
	GLint kMaxComputeInvocationsPerGroup;
	GLint kMaxComputeLocalSize[3];
	GLint kMaxComputeWorkGroups[3];

	enum Vsync
	{
		Vsync_Adfrmive = -1, // swap/tear
		Vsync_Off      =  0,
		Vsync_On       =  1, // wait 1 interval
		Vsync_On2,           // wait 2 intervals
		Vsync_On3,           // wait 3 intervals

		Vsync_Count
	};

	enum CreateFlags_
	{
		CreateFlags_Compatibility = 1 << 0,
		CreateFlags_Debug         = 1 << 1,
		CreateFlags_HDR           = 1 << 2,

		CreateFlags_Count
	};
	typedef int CreateFlags;

	// Create an OpenGL context of at least version _vmaj._vmin (if available). The context is bound to _window and is current on the calling 
	// thread when this function returns. Return 0 if an error occurred.
	static GlContext* Create(const Window* _window, int _vmaj, int _vmin, CreateFlags _flags);
	
	// Destroy OpenGL context. This implicitly destroys all associated resources.
	static void       Destroy(GlContext*& _ctx_);
	
	// Get the current context on the calling thread, or nullptr if none.
	static GlContext* GetCurrent();

	// Make _ctx current on the calling thread. Return false if the operation fails.
	static bool       MakeCurrent(GlContext* _ctx);


	// Make an instanced draw call via glDrawArraysInstanced/glDrawElementsInstanced (render the current mesh with the current shader to the current framebuffer).
	void draw(GLsizei _instances = 1);
	// Make an indirect draw call via glDrawArraysIndirect/glDrawElementsIndirect, with _buffer bound as GL_DRAW_INDIRECT_BUFFER.
	void drawIndirect(const Buffer* _buffer, const void* _offset = nullptr);
	
	// Draw a quad with vertices in [-1,1]. If _cam is specified, bind the camera buffer (see shaders/Camera.glsl) or send uniforms if no buffer.
	void drawNdcQuad(const Camera* _cam = nullptr);

	// Dispatch a compute shader with the specified number of work groups.
	void dispatch(GLuint _groupsX, GLuint _groupsY = 1, GLuint _groupsZ = 1);
	// Make an indirect compute shader dispatch with _buffer bound as GL_DISPATCH_INDIRECT_BUFFER.
	void dispatchIndirect(const Buffer* _buffer, const void* _offset = nullptr);
	// Dispatch at least 1 thread per pixel (e.g. ceil(texture size/group size) groups). Note that _groupsZ can be overriden e.g. to write to a single level of an 
	// array or volume texture.
	void dispatch(const Texture* _tx, GLuint _groupsZ = 0, GLuint _lodIndex = 0);

	// Present the next image in the swapchain, increment the frame index, clear draw call counters.
	void present();

	// Get draw/dispatch counters (call before present()).
	uint32 getDrawCallCount() const { return m_drawCount; }
	uint32 getDispatchCount() const { return m_dispatchCount; }

	void      setVsync(Vsync _mode);
	Vsync     getVsync() const                   { return m_vsync;      }
	
	uint64    getFrameIndex() const              { return m_frameIndex; }


 // FRAMEBUFFER

	// Pass 0 to set the default framebuffer/viewport.
	void  setFramebuffer(const Framebuffer* _framebuffer);
	void  setFramebufferAndViewport(const Framebuffer* _framebuffer);
	const Framebuffer* getFramebuffer()          { return m_currentFramebuffer; }

	void            setViewport(int _x, int _y, int _width, int _height);
	void            setViewport(const Viewport& _viewport);
	const Viewport& getViewport() const          { return m_viewport; }

	void blitFramebuffer(const Framebuffer* _src, const Framebuffer* _dst, GLbitfield _mask = GL_COLOR_BUFFER_BIT, GLenum _filter = GL_NEAREST);
	void blitFramebuffer(const Framebuffer* _src, const Viewport& _srcViewport, const Framebuffer* _dst, const Viewport& _dstViewport, GLbitfield _mask = GL_COLOR_BUFFER_BIT, GLenum _filter = GL_NEAREST);
	
 // SHADER

	void setShader(const Shader* _shader);
	const Shader* getShader()                    { return m_currentShader; }	
	// Set uniform values on the currently bound shader. If _name is not an active uniform, do nothing.
	template <typename tType>
	void setUniformArray(const char* _name, const tType* _val, GLsizei _count);
	template <typename tType>
	void setUniform(const char* _name, const tType& _val) { setUniformArray<tType>(_name, &_val, 1); }

 // MESH

	void setMesh(const DrawMesh* _mesh, int _lodIndex = 0, int _submeshIndex = 0, uint16 _bindHandleKey = ~0);
	const DrawMesh* getMesh() { return m_currentMesh.mesh; }

 // BUFFER
	
	// Bind _buffer to a named _location on the current shader. The target is chosen from the _buffer's target hint; only atomic, tranform-feedback, 
	// uniform and storage buffers are allowed. Binding indices are managed automatically; they are reset only when the current shader changes. If 
	// _location is not active on the current shader, do nothing.
	void bindBuffer(const char* _location, const Buffer* _buffer);
	void bindBufferRange(const char* _location, const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);

	// As bindBuffer()/bindBufferRange() but use _buffer->getName() as the location.
	void bindBuffer(const Buffer* _buffer);
	void bindBufferRange(const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);
	
	// Bind _buffer to _target, or to _buffer's target hint by default. This is intended for non-indexed targets e.g. GL_DRAW_INDIRECT_BUFFER.
	void bindBuffer(const Buffer* _buffer, GLenum _target);

	// Clear all current buffer bindings. 
	void clearBufferBindings();

 // TEXTURE

	// Bind _texture to a named _location on the current shader. Binding indices are managed automatically; they are reset only when the current shader 
	// changes. If _location is not active on the current shader, do nothing. _sampler is an optional TextureSampler which overrides the texture's internal
	// sampling parameters.
	void bindTexture(const char* _location, const Texture* _texture, const TextureSampler* _sampler = nullptr);

	// As bindTexture() but use _texture->getName() as the location.
	void bindTexture(const Texture* _texture, const TextureSampler* _sampler = nullptr);

	// Clear all texture bindings. 
	void clearTextureBindings();

 // IMAGE

	// Bind _texture as an image to a named _location on the current shader. _access is one of GL_READ_ONLY, GL_WRITE_ONLY or GL_READ_WRITE.
	void bindImage(const char* _location, const Texture* _texture, GLenum _access, GLint _level = 0);

	// Clear all image bindings. 
	void clearImageBindings();
	
private:
	const Window*         m_window                                                          = nullptr;
	Vsync                 m_vsync                                                           = Vsync_On;
	uint64                m_frameIndex                                                      = 0;
	uint32                m_drawCount                                                       = 0;
	uint32                m_dispatchCount                                                   = 0;

 	const Framebuffer*    m_currentFramebuffer                                              = nullptr;
	Viewport              m_viewport                                                        = { 0 };
	const Shader*         m_currentShader                                                   = nullptr;
	int                   m_currentSubmesh                                                  = -1;

	struct MeshBindData
	{
		const DrawMesh* mesh             = nullptr;
		int             lodIndex         = -1;
		int             submeshIndex     = -1;
		uint16          bindHandleKey    = 0;
		GLuint          bindHandle       = 0;

		bool operator==(const MeshBindData& _rhs) const
		{
			// Note that we don't check the bind handle itself to avoid having to resolve it during setMesh() when checking if the mesh is already bound.
			return mesh == _rhs.mesh && lodIndex == _rhs.lodIndex && submeshIndex == _rhs.submeshIndex && bindHandleKey == _rhs.bindHandleKey;
		}
	};
	MeshBindData          m_currentMesh;

	// Tracking state for all targets is redundant as only a subset use an indexed binding model
	static const int      kBufferSlotCount                                                  = 16;
	const Buffer*         m_currentBuffers [internal::kBufferTargetCount][kBufferSlotCount] = { { nullptr } };
	GLint                 m_nextBufferSlots[internal::kBufferTargetCount]                   = { 0 };
	GLint                 kMaxBufferSlots  [internal::kBufferTargetCount]                   = { 0 };

	static const int      kTextureSlotCount                                                 = 24;
	const Texture*        m_currentTextures[kTextureSlotCount]                              = { nullptr };
	const TextureSampler* m_currentTextureSamplers[kTextureSlotCount]                       = { nullptr };
	GLint                 m_nextTextureSlot                                                 = 0;

	static const int      kImageSlotCount                                                   = 8;
	const Texture*        m_currentImages[kImageSlotCount]                                  = { nullptr };
	GLint                 m_nextImageSlot                                                   = 0;
	
	DrawMesh*             m_ndcQuadMesh                                                     = nullptr;

	struct Impl;
	Impl* m_impl;
	

	GlContext();
	~GlContext();

	bool init();
	void shutdown();

	void queryLimits();

	
}; // class GlContext

} // namespace frm
