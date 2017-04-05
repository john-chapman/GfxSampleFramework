#include <frm/Buffer.h>

#include <frm/gl.h>

using namespace frm;
using namespace apt;

// PUBLIC

Buffer* Buffer::Create(GLenum _target, GLsizei _size, GLbitfield _flags, GLvoid* _data)
{
	Buffer* ret = new Buffer(_target, _size, _flags);
	glAssert(glNamedBufferStorage(ret->m_handle, _size, _data, _flags));
	return ret;
}

void Buffer::Destroy(Buffer*& _inst_)
{
	delete _inst_;
	_inst_ = nullptr;
}

void Buffer::setData(GLsizeiptr _size, GLvoid* _data, GLintptr _offset)
{
	APT_ASSERT(m_handle);
	APT_ASSERT(m_flags & GL_DYNAMIC_STORAGE_BIT);
	APT_ASSERT(_offset + _size <= m_size);
	glAssert(glNamedBufferSubData(m_handle, _offset, _size, _data));
}

template <>
void Buffer::clearDataRange<float>(float _value, GLenum _internalFormat, GLintptr _offset, GLsizei _size)
{
	APT_ASSERT(m_handle);
	glAssert(glClearNamedBufferSubData(m_handle, _internalFormat, _offset, _size, GL_RED, GL_FLOAT, &_value));
}

template <>
void Buffer::clearDataRange<int>(int _value, GLenum _internalFormat, GLintptr _offset, GLsizei _size)
{
	APT_ASSERT(m_handle);
	glAssert(glClearNamedBufferSubData(m_handle, _internalFormat, _offset, _size, GL_RED, GL_INT, &_value));
}

void* Buffer::map(GLenum _access)
{
	APT_ASSERT(m_handle);
	APT_ASSERT(!m_isMapped);
	void* ret;
	glAssert(ret = glMapNamedBuffer(m_handle, _access));
	m_isMapped = true;
	return ret;
}

void* Buffer::mapRange(GLintptr _offset, GLsizei _size, GLenum _access)
{
	APT_ASSERT(m_handle);
	APT_ASSERT(!m_isMapped);
	void* ret;
	glAssert(ret = glMapNamedBufferRange(m_handle, _offset, _size, _access));
	m_isMapped = true;
	return ret;
}

void Buffer::unmap()
{
	APT_ASSERT(m_handle);
	APT_ASSERT(m_isMapped);
	glAssert(glUnmapNamedBuffer(m_handle));
	m_isMapped = false;
}

// PRIVATE

Buffer::Buffer(GLenum _target, GLsizei _size, GLbitfield _flags)
	: m_handle(0)
	, m_target(_target)
	, m_size(_size)
	, m_flags(_flags)
	, m_isMapped(false)
	, m_name(0)
{
	glAssert(glCreateBuffers(1, &m_handle));
}

Buffer::~Buffer()
{
	if (m_handle) {
		glAssert(glDeleteBuffers(1, &m_handle));
		m_handle = 0;
	}
	m_target = GL_NONE;
}