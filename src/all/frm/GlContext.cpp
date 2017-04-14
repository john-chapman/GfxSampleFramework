#include <frm/GlContext.h>

#include <frm/gl.h>
#include <frm/math.h>
#include <frm/Buffer.h>
#include <frm/Camera.h>
#include <frm/Framebuffer.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Resource.h>
#include <frm/Shader.h>
#include <frm/Texture.h>
#include <frm/Window.h>

#include <apt/log.h>

#ifdef APT_DEBUG
 // Debug options, additional validation, troubleshooting etc.
	//#define GlContext_VALIDATE_MESH_SHADER_ON_DRAW
	//#define GlContext_ACTUALLY_CLEAR_TEXTURE_BINDINGS
	//#define GlContext_ACTUALLY_CLEAR_BUFFER_BINDINGS
#endif

using namespace frm;
using namespace apt;


// PUBLIC

void GlContext::draw(GLsizei _instances)
{
	APT_ASSERT(m_currentShader);
	APT_ASSERT(m_currentMesh);

#ifdef GlContext_VALIDATE_MESH_SHADER_ON_DRAW
 // \todo fully validate the mesh vertex descriptor against the shader
	GLint activeAttribCount;
	glAssert(glGetProgramiv(m_currentShader->getHandle(), GL_ACTIVE_ATTRIBUTES, &activeAttribCount));
	APT_ASSERT(m_currentMesh->m_desc.getVertexComponentCount() == activeAttribCount);
#endif
	const MeshData::Submesh& submesh = m_currentMesh->getSubmesh(m_currentSubmesh);

	if (m_currentMesh->getIndexBufferHandle() != 0) {
		glAssert(glDrawElementsInstanced(
			m_currentMesh->getPrimitive(), 
			(GLsizei)submesh.m_indexCount, 
			m_currentMesh->getIndexDataType(), 
			(GLvoid*)submesh.m_indexOffset, 
			_instances
			));
	} else {
		glAssert(glDrawArraysInstanced(
			m_currentMesh->getPrimitive(),
			(GLint)submesh.m_vertexOffset, 
			(GLsizei)submesh.m_vertexCount, 
			_instances
			));
	}
}

void GlContext::drawIndirect(const Buffer* _buffer, const void* _offset)
{
	APT_ASSERT(m_currentShader);
	APT_ASSERT(m_currentMesh);
	
	bindBuffer(_buffer, GL_DRAW_INDIRECT_BUFFER);

#ifdef GlContext_VALIDATE_MESH_SHADER_ON_DRAW
 // \todo fully validate the mesh vertex descriptor against the shader
	GLint activeAttribCount;
	glAssert(glGetProgramiv(m_currentShader->getHandle(), GL_ACTIVE_ATTRIBUTES, &activeAttribCount));
	APT_ASSERT(m_currentMesh->m_desc.getVertexComponentCount() == activeAttribCount);
#endif

	if (m_currentMesh->getIndexBufferHandle() != 0) {
		glAssert(glDrawElementsIndirect(m_currentMesh->getPrimitive(), m_currentMesh->getIndexDataType(), _offset));
	} else {
		glAssert(glDrawArraysIndirect(m_currentMesh->getPrimitive(), _offset));
	}
}

void GlContext::drawNdcQuad(const Camera* _cam)
{
	if_unlikely (!m_ndcQuadMesh) {
		MeshDesc quadDesc;
		quadDesc.setPrimitive(MeshDesc::Primitive_TriangleStrip);
		quadDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType::Float32, 2);
		//quadDesc.addVertexAttr(VertexAttr::Semantic_Texcoords, 2, DataType::Float32);
		float quadVertexData[] = { 
			-1.0f, -1.0f, //0.0f, 0.0f,
			 1.0f, -1.0f, //1.0f, 0.0f,
			-1.0f,  1.0f, //0.0f, 1.0f,
			 1.0f,  1.0f, //1.0f, 1.0f
		};
		MeshData* quadData = MeshData::Create(quadDesc, 4, 0, quadVertexData);
		m_ndcQuadMesh = Mesh::Create(*quadData);
		MeshData::Destroy(quadData);
	}

	if (_cam) {
		setUniform("uCameraNear",        _cam->m_near);
		setUniform("uCameraFar",         _cam->m_far);
		setUniform("uCameraTanHalfFov",  _cam->m_up);
		setUniform("uCameraAspectRatio", _cam->m_aspectRatio);
		setUniform("uCameraWorldMatrix", _cam->m_world);
	}

	const Mesh* prevMesh = m_currentMesh;
	setMesh(m_ndcQuadMesh);
	draw();
	setMesh(prevMesh);
}

void GlContext::dispatch(GLuint _groupsX, GLuint _groupsY, GLuint _groupsZ)
{
	APT_ASSERT(m_currentShader);
	APT_ASSERT(_groupsX < (GLuint)kMaxComputeWorkGroups[0]);
	APT_ASSERT(_groupsY < (GLuint)kMaxComputeWorkGroups[1]);
	APT_ASSERT(_groupsZ < (GLuint)kMaxComputeWorkGroups[2]);
	glAssert(glDispatchCompute(_groupsX, _groupsY, _groupsZ));
}

void GlContext::dispatchIndirect(const Buffer* _buffer, const void* _offset)
{
	APT_ASSERT(m_currentShader);
	bindBuffer(_buffer, GL_DISPATCH_INDIRECT_BUFFER);
	glAssert(glDispatchComputeIndirect((GLintptr)_offset));
}

void GlContext::setFramebuffer(const Framebuffer* _framebuffer)
{
	if (_framebuffer == m_currentFramebuffer) {
		return;
	}
	if (!_framebuffer) {
		glAssert(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	} else {
	 // validate framebuffer completeness
		GLenum status = _framebuffer->getStatus();
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			APT_LOG_ERR("GlContext: Framebuffer %u incomplete (status %s)", _framebuffer->getHandle(), internal::GlEnumStr(status));
			APT_ASSERT(false);
		}

		glAssert(glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer->getHandle()));
	}
	m_currentFramebuffer = _framebuffer;
}

void GlContext::setFramebufferAndViewport(const Framebuffer* _framebuffer)
{
	setFramebuffer(_framebuffer);
	if (_framebuffer) {
		setViewport(0, 0, _framebuffer->getWidth(), _framebuffer->getHeight());
	} else {
		setViewport(0, 0, m_window->getWidth(), m_window->getHeight());
	}
}

void GlContext::blitFramebuffer(Framebuffer* _src, Framebuffer* _dst, GLbitfield _mask, GLenum _filter)
{
	glAssert(glBlitNamedFramebuffer(
		_src ? _src->getHandle() : 0,
		_dst ? _dst->getHandle() : 0,
		0, 0, _src ? _src->getWidth() : m_window->getWidth(), _src ? _src->getHeight() : m_window->getHeight(),
		0, 0, _dst ? _dst->getWidth() : m_window->getWidth(), _dst ? _dst->getHeight() : m_window->getHeight(),
		_mask,
		_filter
		));
}

void GlContext::setViewport(int _x, int _y, int _width, int _height)
{
	m_viewportX = _x;
	m_viewportY = _y;
	m_viewportWidth = _width;
	m_viewportHeight = _height;
	glAssert(glViewport(_x, _y, _width, _height));
}

void GlContext::setShader(const Shader* _shader)
{
 // must at least clear the slot counters between shaders
	clearBufferBindings();
	clearTextureBindings(); 
	clearImageBindings();

	if (_shader == m_currentShader) {
		return;
	}
	if (!_shader || _shader->getState() != Shader::State_Loaded) {
		glAssert(glUseProgram(0));
	} else {
		glAssert(glUseProgram(_shader->getHandle()));
	}
	m_currentShader = _shader;
}

template <>
void GlContext::setUniformArray<int>(const char* _name, const int* _val, GLsizei _count)
{
	glAssert(glUniform1iv(m_currentShader->getUniformLocation(_name), _count, (const GLint*)_val));
}
template <>
void GlContext::setUniformArray<uint32>(const char* _name, const uint32* _val, GLsizei _count)
{
	glAssert(glUniform1uiv(m_currentShader->getUniformLocation(_name), _count, (const GLuint*)_val));
}
template <>
void GlContext::setUniformArray<float>(const char* _name, const float* _val, GLsizei _count)
{
	glAssert(glUniform1fv(m_currentShader->getUniformLocation(_name), _count, (const GLfloat*)_val));
}
template <>
void GlContext::setUniformArray<vec2>(const char* _name, const vec2* _val, GLsizei _count)
{
	glAssert(glUniform2fv(m_currentShader->getUniformLocation(_name), _count, (const GLfloat*)_val));
}
template <>
void GlContext::setUniformArray<vec3>(const char* _name, const vec3* _val, GLsizei _count)
{
	glAssert(glUniform3fv(m_currentShader->getUniformLocation(_name), _count, (const GLfloat*)_val));
}
template <>
void GlContext::setUniformArray<vec4>(const char* _name, const vec4* _val, GLsizei _count)
{
	glAssert(glUniform4fv(m_currentShader->getUniformLocation(_name), _count, (const GLfloat*)_val));
}
template <>
void GlContext::setUniformArray<ivec2>(const char* _name, const ivec2* _val, GLsizei _count)
{
	glAssert(glUniform2iv(m_currentShader->getUniformLocation(_name), _count, (const GLint*)_val));
}
template <>
void GlContext::setUniformArray<ivec3>(const char* _name, const ivec3* _val, GLsizei _count)
{
	glAssert(glUniform3iv(m_currentShader->getUniformLocation(_name), _count, (const GLint*)_val));
}
template <>
void GlContext::setUniformArray<ivec4>(const char* _name, const ivec4* _val, GLsizei _count)
{
	glAssert(glUniform4iv(m_currentShader->getUniformLocation(_name), _count, (const GLint*)_val));
}
template <>
void GlContext::setUniformArray<uvec2>(const char* _name, const uvec2* _val, GLsizei _count)
{
	glAssert(glUniform2uiv(m_currentShader->getUniformLocation(_name), _count, (const GLuint*)_val));
}
template <>
void GlContext::setUniformArray<uvec3>(const char* _name, const uvec3* _val, GLsizei _count)
{
	glAssert(glUniform3uiv(m_currentShader->getUniformLocation(_name), _count, (const GLuint*)_val));
}
template <>
void GlContext::setUniformArray<uvec4>(const char* _name, const uvec4* _val, GLsizei _count)
{
	glAssert(glUniform4uiv(m_currentShader->getUniformLocation(_name), _count, (const GLuint*)_val));
}
template <>
void GlContext::setUniformArray<mat4>(const char* _name, const mat4* _val, GLsizei _count)
{
	glAssert(glUniformMatrix4fv(m_currentShader->getUniformLocation(_name), _count, false, (const GLfloat*)_val));
}


void GlContext::setMesh(const Mesh* _mesh, int _submeshId)
{
	APT_ASSERT(_submeshId < _mesh->getSubmeshCount());
	m_currentSubmesh = _submeshId;
	if (_mesh == m_currentMesh) {
		return;
	}
	if (!_mesh || _mesh->getState() != Mesh::State_Loaded) {
		glAssert(glBindVertexArray(0));
	} else {
		glAssert(glBindVertexArray(_mesh->getVertexArrayHandle()));
	}
	m_currentMesh = _mesh;
}

void GlContext::bindBuffer(const char* _location, const Buffer* _buffer)
{
	bindBufferRange(_location, _buffer, 0, _buffer->getSize());
}

void GlContext::bindBufferRange(const char* _location, const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size)
{
	APT_ASSERT(_location);
	APT_ASSERT(m_currentShader);
	APT_ASSERT(_buffer->getTarget() == GL_UNIFORM_BUFFER || 
	           _buffer->getTarget() == GL_SHADER_STORAGE_BUFFER ||
			   _buffer->getTarget() == GL_ATOMIC_COUNTER_BUFFER ||
			   _buffer->getTarget() == GL_TRANSFORM_FEEDBACK_BUFFER
			   );
	APT_ASSERT((_offset + _size) <= _buffer->getSize());

	GLint loc = m_currentShader->getResourceIndex(_buffer->getTarget() == GL_SHADER_STORAGE_BUFFER ? GL_SHADER_STORAGE_BLOCK : GL_UNIFORM_BLOCK, _location);
	if (loc != GL_INVALID_INDEX) {
		int t = internal::BufferTargetToIndex(_buffer->getTarget());
		APT_ASSERT(m_nextBufferSlots[t] < kBufferSlotCount);
		switch (_buffer->getTarget()) {
			case GL_SHADER_STORAGE_BUFFER: glShaderStorageBlockBinding(m_currentShader->getHandle(), loc, m_nextBufferSlots[t]); break;
			case GL_UNIFORM_BUFFER:
			default:                       glUniformBlockBinding(m_currentShader->getHandle(), loc, m_nextBufferSlots[t]); break;
		};
		glAssert(glBindBufferRange(_buffer->getTarget(), m_nextBufferSlots[t], _buffer->getHandle(), _offset, _size));
		m_currentBuffers[t][m_nextBufferSlots[t]] = _buffer;
		++m_nextBufferSlots[t];
	}
}

void GlContext::bindBuffer(const Buffer* _buffer)
{
	bindBufferRange(_buffer->getName(), _buffer, 0, _buffer->getSize());
}
void GlContext::bindBufferRange(const Buffer* _buffer, GLintptr _offset, GLsizeiptr _size)
{
	bindBufferRange(_buffer->getName(), _buffer, 0, _buffer->getSize());
}

void GlContext::bindBuffer(const Buffer* _buffer, GLenum _target)
{
	_target = _target == GL_NONE ? _buffer->getTarget() : _target;

	APT_ASSERT(_target == GL_DRAW_INDIRECT_BUFFER || 
	           _target == GL_DISPATCH_INDIRECT_BUFFER ||
			   _target == GL_COPY_READ_BUFFER ||
			   _target == GL_COPY_WRITE_BUFFER ||
			   _target == GL_QUERY_BUFFER ||
			   _target == GL_PIXEL_PACK_BUFFER ||
			   _target == GL_PIXEL_UNPACK_BUFFER ||
			   _target == GL_ARRAY_BUFFER ||
			   _target == GL_ELEMENT_ARRAY_BUFFER ||
			   _target == GL_TEXTURE_BUFFER
			   );

 // always bind to slot 0 \todo modify this behaviour if it's an indexed target?
	int t = internal::BufferTargetToIndex(_target);
	m_currentBuffers[t][0] = _buffer;
	glAssert(glBindBuffer(_target, _buffer->getHandle()));
}

void GlContext::clearBufferBindings()
{
	memset(m_nextBufferSlots, 0, sizeof(m_nextBufferSlots));
	memset(m_currentBuffers,  0, sizeof(m_currentBuffers)); // not necessary but nice for debugging

#ifdef GlContext_ACTUALLY_CLEAR_BUFFER_BINDINGS
 // for debugging only: actually unbind the textures via GL
	for (int slot = 0; slot < kBufferSlotCount; ++slot) {
		glAssert(glActiveTexture(GL_TEXTURE0 + slot));
		for (int t = 0; t < kBufferSlotCount; ++t) {
			glAssert(glBindBufferBase(kBufferTargets[t], slot, 0));
		}
	}
#endif
}


void GlContext::bindTexture(const char* _location, const Texture* _texture)
{
	APT_ASSERT(_location);
	APT_ASSERT(m_currentShader);
	GLint loc = m_currentShader->getUniformLocation(_location);
	if (loc != GL_INVALID_INDEX) {
		APT_ASSERT(m_nextTextureSlot < kTextureSlotCount);
		glAssert(glUniform1i(loc, m_nextTextureSlot));
		glAssert(glActiveTexture(GL_TEXTURE0 + m_nextTextureSlot));
		glAssert(glBindTexture(_texture->getTarget(), _texture->getHandle()));
		m_currentTextures[m_nextTextureSlot] = _texture;
		++m_nextTextureSlot;
	}
}

void GlContext::bindTexture(const Texture* _texture)
{
	bindTexture(_texture->getName(), _texture);
}

void GlContext::clearTextureBindings()
{
	m_nextTextureSlot = 0;
	memset(m_currentTextures,  0, sizeof(m_currentTextures)); // not necessary but nice for debugging

#ifdef GlContext_ACTUALLY_CLEAR_TEXTURE_BINDINGS
 // for debugging only: actually unbind the textures via GL
	for (int slot = 0; slot < kTextureSlotCount; ++slot) {
		glAssert(glActiveTexture(GL_TEXTURE0 + slot));
		for (int target = 0; target < internal::kTextureTargetCount; ++target) {
			glAssert(glBindTexture(internal::kTextureTargets[target], 0));
		}
	}
#endif
}

void GlContext::bindImage(const char* _location, const Texture* _texture, GLenum _access)
{
	APT_ASSERT(_location);
	APT_ASSERT(m_currentShader);
	GLint loc = m_currentShader->getUniformLocation(_location);
	if (loc != GL_INVALID_INDEX) {
		APT_ASSERT(m_nextImageSlot < kImageSlotCount);
		glAssert(glUniform1i(loc, m_nextImageSlot));
		
		GLboolean layered = 
			_texture->getTarget() == GL_TEXTURE_2D_ARRAY ||
			_texture->getTarget() == GL_TEXTURE_3D ||
			_texture->getTarget() == GL_TEXTURE_CUBE_MAP
			;
		glAssert(glBindImageTexture(m_nextImageSlot, _texture->getHandle(), 0, layered, 0, _access, _texture->getFormat()));
		m_currentImages[m_nextImageSlot] = _texture;
		++m_nextImageSlot;
	}
}

void GlContext::clearImageBindings()
{
	m_nextImageSlot = 0;
	memset(m_currentImages,  0, sizeof(m_currentImages)); // not necessary but nice for debugging

#ifdef GlContext_ACTUALLY_CLEAR_TEXTURE_BINDINGS
 // for debugging only: actually unbind the textures via GL
	for (int slot = 0; slot < kImageSlotCount; ++slot) {
		glAssert(glBindImageTexture(slot, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8));
	}
#endif
}


// PRIVATE

GlContext::GlContext()
	: m_impl(nullptr)
	, m_window(nullptr)
	, m_vsync(Vsync_On)
	, m_frameIndex(0)
	, m_currentFramebuffer(nullptr)
	, m_currentShader(nullptr)
	, m_currentMesh(nullptr)
	, m_ndcQuadMesh(nullptr)
{
}

GlContext::~GlContext()
{
	APT_ASSERT(m_impl == 0);
}

bool GlContext::init()
{
	setVsync(m_vsync);
	queryLimits();
	return true;
}
void GlContext::shutdown()
{
	Mesh::Release(m_ndcQuadMesh);
}

void GlContext::queryLimits()
{
	glAssert(glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &kMaxComputeInvocations));

	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &kMaxComputeWorkGroups[0]));
	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &kMaxComputeWorkGroups[1]));
	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &kMaxComputeWorkGroups[2]));

	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &kMaxComputeLocalSize[0]));
	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &kMaxComputeLocalSize[1]));
	glAssert(glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &kMaxComputeLocalSize[2]));
}
