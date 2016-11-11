#pragma once
#ifndef frm_Buffer_h
#define frm_Buffer_h

#include <frm/def.h>
#include <frm/gl.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Buffer
/// \note Buffers are not resources (they don't need to be shared).
////////////////////////////////////////////////////////////////////////////////
class Buffer
{
public:

	struct DrawArraysIndirectCommand
	{
		uint32 m_vertexCount;
		uint32 m_instanceCount;
		uint32 m_first;
		uint32 m_baseInstance;
	};

	struct DrawElementsIndirectCommand
	{
		uint32 m_indexCount;
		uint32 m_instanceCount;
		uint32 m_firstIndex;
		uint32 m_baseVertex;
		uint32 m_baseInstance;
	};

	/// Create a buffer object. _data is optional. 
	/// \note _target is only used as a hint for GlContext::bindBuffer() functions.
	static Buffer* Create(GLenum _target, GLsizei _size, GLbitfield _flags = 0, GLvoid* _data = 0);

	static void Destroy(Buffer*& _inst_);

	/// Set buffer data. The buffer must have been created with GL_DYNAMIC_STORAGE_BIT set.
	void setData(GLsizeiptr _size, GLvoid* _data, GLintptr _offset = 0);

	/// Fill the buffer with _value. _internalFormat describes how _value should be
	/// converted for storage in the buffer. This is legal even if the buffer was not
	/// created with GL_DYNAMIC_STORAGE_BIT set.
	template <typename tType>
	void clearData(tType _value, GLenum _internalFormat) { clearDataRange(_value, _internalFormat, 0, m_size); }
	template <typename tType>
	void clearDataRange(tType _value, GLenum _internalFormat, GLintptr _offset, GLsizei _size);

	/// Map the buffer for cpu-side access. _access is GL_READ_ONLY, GL_WRITE_ONLY or
	/// GL_READ_WRITE.
	void* map(GLenum _access);
	void* mapRange(GLintptr _offset, GLsizei _size, GLbitfield _access);

	/// Unmap the buffer, previously bound by map() or mapRange().
	void  unmap();

	GLuint      getHandle() const          { return m_handle; }
	GLenum      getTarget() const          { return m_target; }
	GLsizeiptr  getSize() const            { return m_size; }
	GLbitfield  getFlags()  const          { return m_flags; }
	bool        isMapped() const           { return m_isMapped; }
	const char* getName() const            { return m_name; }
	void        setName(const char* _name) { m_name = _name; }

private:
	GLuint      m_handle;
	GLenum      m_target;   //< Target passed to Create(), however the buffer is not restricted to this target.
	GLsizei     m_size;
	GLbitfield  m_flags;
	bool        m_isMapped;
	const char* m_name;     //< Useful to store a shader interface name.

	Buffer(GLenum _target, GLsizei _size, GLbitfield _flags);
	~Buffer();

}; // class Buffer

} // namespace frm

#endif // frm_Buffer_h