#pragma once
#ifndef frm_gl_h
#define frm_gl_h

#if defined(__gl_h_) || defined(__GL_H__) || defined(_GL_H) || defined(__X_GL_H)
	#error framework: Don't include GL/gl.h, included frm/gl.h
#endif

#include <frm/def.h>

#include <GL/glew.h>

#ifdef APT_DEBUG
	#define glAssert(call) \
		do { \
			(call); \
			if (frm::internal::GlAssert(#call, apt::internal::StripPath(__FILE__), __LINE__) == apt::AssertBehavior::kBreak) \
				{ APT_BREAK(); } \
			} \
		while (0)
#else
	#define glAssert(call) (call)
#endif

namespace frm { namespace internal {

const int kTextureTargetCount = 10;
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

const int kShaderStageCount = 6;
extern const GLenum kShaderStages[kShaderStageCount];
int ShaderStageToIndex(GLenum _stage);

GLenum GlDataTypeToEnum(DataType _type);

apt::AssertBehavior GlAssert(const char* _call, const char* _file, int _line);
const char* GlEnumStr(GLenum _enum);
const char* GlGetString(GLenum _name);


} } // namespace frm::internal

#endif // frm_gl_h
