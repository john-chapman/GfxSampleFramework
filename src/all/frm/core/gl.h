#pragma once

#if defined(__gl_h_) || defined(__GL_H__) || defined(_GL_H) || defined(__X_GL_H)
	#error framework: Don't include GL/gl.h, included frm/gl.h
#endif

#include <frm/core/frm.h>

#include <GL/glew.h>

#ifdef FRM_DEBUG
	#define glAssert(call) \
		do { \
			(call); \
			if (frm::internal::GlAssert(#call, __FILE__, __LINE__) == frm::AssertBehavior_Break) \
				{ FRM_BREAK(); } \
			} \
		while (0)
#else
	#define glAssert(call) (call)
#endif

namespace frm { namespace internal {

const int kTextureTargetCount = 11;
extern const GLenum kTextureTargets[kTextureTargetCount];
int TextureTargetToIndex(GLenum _target);

const int kTextureWrapModeCount = 5;
extern const GLenum kTextureWrapModes[kTextureWrapModeCount];
int TextureWrapModeToIndex(GLenum _wrapMode);

const int kTextureFilterModeCount = 6;
const int kTextureMinFilterModeCount = kTextureFilterModeCount;
const int kTextureMagFilterModeCount = 2;
extern const GLenum kTextureFilterModes[kTextureFilterModeCount];
int TextureFilterModeToIndex(GLenum _filterMode);

const int kBufferTargetCount = 14;
extern const GLenum kBufferTargets[kBufferTargetCount];
int BufferTargetToIndex(GLenum _stage);
bool IsBufferTargetIndexed(GLenum _target);

const int kShaderStageCount = 6;
extern const GLenum kShaderStages[kShaderStageCount];
int ShaderStageToIndex(GLenum _stage);

GLenum DataTypeToGLenum(frm::DataType _type);

frm::AssertBehavior GlAssert(const char* _call, const char* _file, int _line);
const char* GlEnumStr(GLenum _enum);
const char* GlGetString(GLenum _name);

} } // namespace frm::internal


namespace frm { 

// Scoped state modifiers. These restore the previous state in the dtor.

struct GLScopedPixelStorei
{
	GLenum m_pname;
	GLint  m_param;

	GLScopedPixelStorei(GLenum _pname, GLint _param)
		: m_pname(_pname)
	{
		glAssert(glGetIntegerv(m_pname, &m_param));
		glAssert(glPixelStorei(m_pname, _param));
	}

	~GLScopedPixelStorei()
	{
		glAssert(glPixelStorei(m_pname, m_param));
	}
};
#define glScopedPixelStorei(_pname, _param) frm::GLScopedPixelStorei FRM_UNIQUE_NAME(_GLScopedPixelStorei)(_pname, _param)


struct GLScopedEnable
{
	GLenum    m_cap;
	bool      m_val;

	GLScopedEnable(GLenum _cap, bool _val)
		: m_cap(_cap)
	{
		glAssert(m_val = (glIsEnabled(m_cap) == GL_TRUE));
		apply(_val);
	}

	~GLScopedEnable()
	{
		apply(m_val);
	}

	void apply(bool _val)
	{
		if (_val) {
			glAssert(glEnable(m_cap));
		} else {
			glAssert(glDisable(m_cap));
		}
	}
};
#define glScopedEnable(_cap, _val) frm::GLScopedEnable FRM_UNIQUE_NAME(_GLScopedEnable)(_cap, _val)

} // namespace frm
