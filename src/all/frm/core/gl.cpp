#include "gl.h"

#include <apt/log.h>

using namespace frm;
using namespace apt;

template <size_t kListSize>
static int FindIndex(const GLenum (&_list)[kListSize], GLenum _find)
{
	for (int i = 0; i < kListSize; ++i) {
		if (_list[i] == _find) {
			return i;
		}
	}
	APT_ASSERT(false);
	return -1;
}

static const GLenum frm::internal::kTextureTargets[] =
{
	GL_TEXTURE_2D,
	GL_TEXTURE_2D_MULTISAMPLE,
	GL_TEXTURE_2D_ARRAY,
	GL_TEXTURE_CUBE_MAP,
	GL_TEXTURE_3D,
	GL_TEXTURE_CUBE_MAP_ARRAY,
	GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
	GL_TEXTURE_1D,
	GL_TEXTURE_1D_ARRAY,
	GL_TEXTURE_BUFFER
};
int frm::internal::TextureTargetToIndex(GLenum _target)
{
	return FindIndex(kTextureTargets, _target);
}

static const GLenum frm::internal::kTextureWrapModes[] =
{
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE,
	GL_MIRROR_CLAMP_TO_EDGE,
	GL_CLAMP_TO_BORDER
};
int frm::internal::TextureWrapModeToIndex(GLenum _wrapMode)
{
	return FindIndex(kTextureWrapModes, _wrapMode);
}

static const GLenum frm::internal::kTextureFilterModes[] =
{
 // min + mag
	GL_NEAREST,
	GL_LINEAR,

 // min only
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR
};
int frm::internal::TextureFilterModeToIndex(GLenum _filterMode)
{
	return FindIndex(kTextureFilterModes, _filterMode);
}

static const GLenum frm::internal::kBufferTargets[] =
{
 // indexed targets
	GL_UNIFORM_BUFFER,
	GL_SHADER_STORAGE_BUFFER,
	GL_ATOMIC_COUNTER_BUFFER,
	GL_TRANSFORM_FEEDBACK_BUFFER,

 // non-indexed targets
	GL_DRAW_INDIRECT_BUFFER,
	GL_DISPATCH_INDIRECT_BUFFER,
	GL_COPY_READ_BUFFER,
	GL_COPY_WRITE_BUFFER,
	GL_QUERY_BUFFER,
	GL_PIXEL_PACK_BUFFER,
	GL_PIXEL_UNPACK_BUFFER,
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER,
	GL_TEXTURE_BUFFER
};
int frm::internal::BufferTargetToIndex(GLenum _target)
{
	return FindIndex(kBufferTargets, _target);
}
bool frm::internal::IsBufferTargetIndexed(GLenum _target)
{
	return false
		|| _target == GL_UNIFORM_BUFFER
		|| _target == GL_SHADER_STORAGE_BUFFER
		|| _target == GL_ATOMIC_COUNTER_BUFFER
		|| _target == GL_TRANSFORM_FEEDBACK_BUFFER
		;
}

static const GLenum frm::internal::kShaderStages[] =
{
	GL_COMPUTE_SHADER,
	GL_VERTEX_SHADER,
	GL_TESS_CONTROL_SHADER,
	GL_TESS_EVALUATION_SHADER,
	GL_GEOMETRY_SHADER,
	GL_FRAGMENT_SHADER
};
int frm::internal::ShaderStageToIndex(GLenum _stage)
{
	return FindIndex(kShaderStages, _stage);
}

GLenum frm::internal::DataTypeToGLenum(DataType _type)
{
	switch (_type) {
		case DataType_Sint8:
		case DataType_Sint8N:   return GL_BYTE;
		case DataType_Sint16:
		case DataType_Sint16N:  return GL_SHORT;
		case DataType_Sint32:
		case DataType_Sint32N:  return GL_INT;
		case DataType_Uint8: 
		case DataType_Uint8N:   return GL_UNSIGNED_BYTE;
		case DataType_Uint16:
		case DataType_Uint16N:  return GL_UNSIGNED_SHORT;
		case DataType_Uint32:
		case DataType_Uint32N:  return GL_UNSIGNED_INT;
		case DataType_Float16:  return GL_HALF_FLOAT;
		case DataType_Float32:  return GL_FLOAT;
		default:                APT_ASSERT(false); return GL_INVALID_VALUE;
	};
}

const char* frm::internal::GlEnumStr(GLenum _enum)
{
	#define CASE_ENUM(e) case e: return #e
	switch (_enum) {
	 // errors
		CASE_ENUM(GL_NONE);
		CASE_ENUM(GL_INVALID_ENUM);
		CASE_ENUM(GL_INVALID_VALUE);
		CASE_ENUM(GL_INVALID_OPERATION);
		CASE_ENUM(GL_INVALID_FRAMEBUFFER_OPERATION);
		CASE_ENUM(GL_OUT_OF_MEMORY);

	 // framebuffer states
		CASE_ENUM(GL_FRAMEBUFFER_UNDEFINED);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
		CASE_ENUM(GL_FRAMEBUFFER_UNSUPPORTED);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
		CASE_ENUM(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);

	 // shader stages
		CASE_ENUM(GL_COMPUTE_SHADER);
		CASE_ENUM(GL_VERTEX_SHADER);
		CASE_ENUM(GL_TESS_CONTROL_SHADER);
		CASE_ENUM(GL_TESS_EVALUATION_SHADER);
		CASE_ENUM(GL_GEOMETRY_SHADER);
		CASE_ENUM(GL_FRAGMENT_SHADER);

	 // buffer targets
		CASE_ENUM(GL_ARRAY_BUFFER);
		CASE_ENUM(GL_ELEMENT_ARRAY_BUFFER);
		CASE_ENUM(GL_UNIFORM_BUFFER);
		CASE_ENUM(GL_SHADER_STORAGE_BUFFER);
		CASE_ENUM(GL_DRAW_INDIRECT_BUFFER);
		CASE_ENUM(GL_DISPATCH_INDIRECT_BUFFER);
		CASE_ENUM(GL_ATOMIC_COUNTER_BUFFER);
		CASE_ENUM(GL_COPY_READ_BUFFER);
		CASE_ENUM(GL_COPY_WRITE_BUFFER);
		CASE_ENUM(GL_QUERY_BUFFER);
		CASE_ENUM(GL_TRANSFORM_FEEDBACK_BUFFER);
		CASE_ENUM(GL_TEXTURE_BUFFER);
		CASE_ENUM(GL_PIXEL_PACK_BUFFER);
		CASE_ENUM(GL_PIXEL_UNPACK_BUFFER);

	 // texture targets
		CASE_ENUM(GL_TEXTURE_1D);
		CASE_ENUM(GL_TEXTURE_2D);
		CASE_ENUM(GL_TEXTURE_2D_MULTISAMPLE);
		CASE_ENUM(GL_TEXTURE_3D);
		CASE_ENUM(GL_TEXTURE_CUBE_MAP);
		CASE_ENUM(GL_TEXTURE_1D_ARRAY);
		CASE_ENUM(GL_TEXTURE_2D_ARRAY);
		CASE_ENUM(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_TEXTURE_CUBE_MAP_ARRAY);
		CASE_ENUM(GL_PROXY_TEXTURE_1D);
		CASE_ENUM(GL_PROXY_TEXTURE_2D);
		CASE_ENUM(GL_PROXY_TEXTURE_2D_MULTISAMPLE);
		CASE_ENUM(GL_PROXY_TEXTURE_3D);
		CASE_ENUM(GL_PROXY_TEXTURE_CUBE_MAP);
		CASE_ENUM(GL_PROXY_TEXTURE_1D_ARRAY);
		CASE_ENUM(GL_PROXY_TEXTURE_2D_ARRAY);
		CASE_ENUM(GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_PROXY_TEXTURE_CUBE_MAP_ARRAY);

	 // texture wrap modes
		CASE_ENUM(GL_REPEAT);
		CASE_ENUM(GL_MIRRORED_REPEAT);
		CASE_ENUM(GL_CLAMP_TO_EDGE);
		CASE_ENUM(GL_MIRROR_CLAMP_TO_EDGE);

	 // texture filter modes
		CASE_ENUM(GL_NEAREST);
		CASE_ENUM(GL_LINEAR);
		CASE_ENUM(GL_NEAREST_MIPMAP_NEAREST);
		CASE_ENUM(GL_LINEAR_MIPMAP_NEAREST);
		CASE_ENUM(GL_NEAREST_MIPMAP_LINEAR);
		CASE_ENUM(GL_LINEAR_MIPMAP_LINEAR);

	 // texture formats
		CASE_ENUM(GL_DEPTH_COMPONENT);
		CASE_ENUM(GL_DEPTH_STENCIL);
		CASE_ENUM(GL_RED);
		CASE_ENUM(GL_RG);
		CASE_ENUM(GL_RGB);
		CASE_ENUM(GL_RGBA);
		CASE_ENUM(GL_DEPTH_COMPONENT16);
		CASE_ENUM(GL_DEPTH_COMPONENT24);
		CASE_ENUM(GL_DEPTH_COMPONENT32);
		CASE_ENUM(GL_DEPTH_COMPONENT32F);
		CASE_ENUM(GL_DEPTH24_STENCIL8);
		CASE_ENUM(GL_DEPTH32F_STENCIL8);
		CASE_ENUM(GL_R8);
		CASE_ENUM(GL_R8_SNORM);
		CASE_ENUM(GL_R16);
		CASE_ENUM(GL_R16_SNORM);
		CASE_ENUM(GL_RG8);
		CASE_ENUM(GL_RG8_SNORM);
		CASE_ENUM(GL_RG16);
		CASE_ENUM(GL_RG16_SNORM);
		CASE_ENUM(GL_R3_G3_B2);
		CASE_ENUM(GL_RGB4);
		CASE_ENUM(GL_RGB5);
		CASE_ENUM(GL_RGB8);
		CASE_ENUM(GL_RGB8_SNORM);
		CASE_ENUM(GL_RGB10);
		CASE_ENUM(GL_RGB12);
		CASE_ENUM(GL_RGB16_SNORM);
		CASE_ENUM(GL_RGBA2);
		CASE_ENUM(GL_RGBA4);
		CASE_ENUM(GL_RGB5_A1);
		CASE_ENUM(GL_RGBA8);
		CASE_ENUM(GL_RGBA8_SNORM);
		CASE_ENUM(GL_RGB10_A2);
		CASE_ENUM(GL_RGB10_A2UI);
		CASE_ENUM(GL_RGBA12);
		CASE_ENUM(GL_RGBA16);
		CASE_ENUM(GL_SRGB8);
		CASE_ENUM(GL_SRGB8_ALPHA8);
		CASE_ENUM(GL_R16F);
		CASE_ENUM(GL_RG16F);
		CASE_ENUM(GL_RGB16F);
		CASE_ENUM(GL_RGBA16F);
		CASE_ENUM(GL_R32F);
		CASE_ENUM(GL_RG32F);
		CASE_ENUM(GL_RGB32F);
		CASE_ENUM(GL_RGBA32F);
		CASE_ENUM(GL_R11F_G11F_B10F);
		CASE_ENUM(GL_RGB9_E5);
		CASE_ENUM(GL_R8I);
		CASE_ENUM(GL_R8UI);
		CASE_ENUM(GL_R16I);
		CASE_ENUM(GL_R16UI);
		CASE_ENUM(GL_R32I);
		CASE_ENUM(GL_R32UI);
		CASE_ENUM(GL_RG8I);
		CASE_ENUM(GL_RG8UI);
		CASE_ENUM(GL_RG16I);
		CASE_ENUM(GL_RG16UI);
		CASE_ENUM(GL_RG32I);
		CASE_ENUM(GL_RG32UI);
		CASE_ENUM(GL_RGB8I);
		CASE_ENUM(GL_RGB8UI);
		CASE_ENUM(GL_RGB16I);
		CASE_ENUM(GL_RGB16UI);
		CASE_ENUM(GL_RGB32I);
		CASE_ENUM(GL_RGB32UI);
		CASE_ENUM(GL_RGBA8I);
		CASE_ENUM(GL_RGBA8UI);
		CASE_ENUM(GL_RGBA16I);
		CASE_ENUM(GL_RGBA16UI);
		CASE_ENUM(GL_RGBA32I);
		CASE_ENUM(GL_RGBA32UI);
		CASE_ENUM(GL_COMPRESSED_RED);
		CASE_ENUM(GL_COMPRESSED_RG);
		CASE_ENUM(GL_COMPRESSED_RGB);
		CASE_ENUM(GL_COMPRESSED_RGBA);
		CASE_ENUM(GL_COMPRESSED_SRGB);
		CASE_ENUM(GL_COMPRESSED_SRGB_ALPHA);
		CASE_ENUM(GL_COMPRESSED_RED_RGTC1);
		CASE_ENUM(GL_COMPRESSED_SIGNED_RED_RGTC1);
		CASE_ENUM(GL_COMPRESSED_RG_RGTC2);
		CASE_ENUM(GL_COMPRESSED_SIGNED_RG_RGTC2);
		CASE_ENUM(GL_COMPRESSED_RGBA_BPTC_UNORM);
		CASE_ENUM(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM);
		CASE_ENUM(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT);
		CASE_ENUM(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT);
		CASE_ENUM(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
		CASE_ENUM(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
		CASE_ENUM(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
		CASE_ENUM(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);

	 // uniform types
		CASE_ENUM(GL_FLOAT);
		CASE_ENUM(GL_FLOAT_VEC2);
		CASE_ENUM(GL_FLOAT_VEC3);
		CASE_ENUM(GL_FLOAT_VEC4);
		CASE_ENUM(GL_DOUBLE);
		CASE_ENUM(GL_DOUBLE_VEC2);
		CASE_ENUM(GL_DOUBLE_VEC3);
		CASE_ENUM(GL_DOUBLE_VEC4);
		CASE_ENUM(GL_INT);
		CASE_ENUM(GL_INT_VEC2);
		CASE_ENUM(GL_INT_VEC3);
		CASE_ENUM(GL_INT_VEC4);
		CASE_ENUM(GL_UNSIGNED_INT);
		CASE_ENUM(GL_UNSIGNED_INT_VEC2);
		CASE_ENUM(GL_UNSIGNED_INT_VEC3);
		CASE_ENUM(GL_UNSIGNED_INT_VEC4);
		CASE_ENUM(GL_BOOL);
		CASE_ENUM(GL_BOOL_VEC2);
		CASE_ENUM(GL_BOOL_VEC3);
		CASE_ENUM(GL_BOOL_VEC4);
		CASE_ENUM(GL_FLOAT_MAT2);
		CASE_ENUM(GL_FLOAT_MAT3);
		CASE_ENUM(GL_FLOAT_MAT4);
		CASE_ENUM(GL_FLOAT_MAT2x3);
		CASE_ENUM(GL_FLOAT_MAT2x4);
		CASE_ENUM(GL_FLOAT_MAT3x2);
		CASE_ENUM(GL_FLOAT_MAT3x4);
		CASE_ENUM(GL_FLOAT_MAT4x2);
		CASE_ENUM(GL_FLOAT_MAT4x3);
		CASE_ENUM(GL_DOUBLE_MAT2);
		CASE_ENUM(GL_DOUBLE_MAT3);
		CASE_ENUM(GL_DOUBLE_MAT4);
		CASE_ENUM(GL_DOUBLE_MAT2x3);
		CASE_ENUM(GL_DOUBLE_MAT2x4);
		CASE_ENUM(GL_DOUBLE_MAT3x2);
		CASE_ENUM(GL_DOUBLE_MAT3x4);
		CASE_ENUM(GL_DOUBLE_MAT4x2);
		CASE_ENUM(GL_DOUBLE_MAT4x3);
		CASE_ENUM(GL_SAMPLER_1D);
		CASE_ENUM(GL_SAMPLER_2D);
		CASE_ENUM(GL_SAMPLER_3D);
		CASE_ENUM(GL_SAMPLER_CUBE);
		CASE_ENUM(GL_SAMPLER_1D_SHADOW);
		CASE_ENUM(GL_SAMPLER_2D_SHADOW);
		CASE_ENUM(GL_SAMPLER_1D_ARRAY);
		CASE_ENUM(GL_SAMPLER_2D_ARRAY);
		CASE_ENUM(GL_SAMPLER_1D_ARRAY_SHADOW);
		CASE_ENUM(GL_SAMPLER_2D_ARRAY_SHADOW);
		CASE_ENUM(GL_SAMPLER_2D_MULTISAMPLE);
		CASE_ENUM(GL_SAMPLER_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_SAMPLER_CUBE_SHADOW);
		CASE_ENUM(GL_SAMPLER_BUFFER);
		CASE_ENUM(GL_SAMPLER_2D_RECT);
		CASE_ENUM(GL_SAMPLER_2D_RECT_SHADOW);
		CASE_ENUM(GL_INT_SAMPLER_1D);
		CASE_ENUM(GL_INT_SAMPLER_2D);
		CASE_ENUM(GL_INT_SAMPLER_3D);
		CASE_ENUM(GL_INT_SAMPLER_CUBE);
		CASE_ENUM(GL_INT_SAMPLER_1D_ARRAY);
		CASE_ENUM(GL_INT_SAMPLER_2D_ARRAY);
		CASE_ENUM(GL_INT_SAMPLER_2D_MULTISAMPLE);
		CASE_ENUM(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_INT_SAMPLER_BUFFER);
		CASE_ENUM(GL_INT_SAMPLER_2D_RECT);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_1D);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_2D);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_3D);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_CUBE);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_BUFFER);
		CASE_ENUM(GL_UNSIGNED_INT_SAMPLER_2D_RECT);
		CASE_ENUM(GL_IMAGE_1D);
		CASE_ENUM(GL_IMAGE_2D);
		CASE_ENUM(GL_IMAGE_3D);
		CASE_ENUM(GL_IMAGE_2D_RECT);
		CASE_ENUM(GL_IMAGE_CUBE);
		CASE_ENUM(GL_IMAGE_BUFFER);
		CASE_ENUM(GL_IMAGE_1D_ARRAY);
		CASE_ENUM(GL_IMAGE_2D_ARRAY);
		CASE_ENUM(GL_IMAGE_2D_MULTISAMPLE);
		CASE_ENUM(GL_IMAGE_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_INT_IMAGE_1D);
		CASE_ENUM(GL_INT_IMAGE_2D);
		CASE_ENUM(GL_INT_IMAGE_3D);
		CASE_ENUM(GL_INT_IMAGE_2D_RECT);
		CASE_ENUM(GL_INT_IMAGE_CUBE);
		CASE_ENUM(GL_INT_IMAGE_BUFFER);
		CASE_ENUM(GL_INT_IMAGE_1D_ARRAY);
		CASE_ENUM(GL_INT_IMAGE_2D_ARRAY);
		CASE_ENUM(GL_INT_IMAGE_2D_MULTISAMPLE);
		CASE_ENUM(GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_1D);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_2D);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_3D);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_2D_RECT);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_CUBE);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_BUFFER);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_1D_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_2D_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE);
		CASE_ENUM(GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY);
		CASE_ENUM(GL_UNSIGNED_INT_ATOMIC_COUNTER);

	 // debug source
		CASE_ENUM(GL_DEBUG_SOURCE_API);
		CASE_ENUM(GL_DEBUG_SOURCE_WINDOW_SYSTEM);
		CASE_ENUM(GL_DEBUG_SOURCE_SHADER_COMPILER);
		CASE_ENUM(GL_DEBUG_SOURCE_THIRD_PARTY);
		CASE_ENUM(GL_DEBUG_SOURCE_APPLICATION);
		CASE_ENUM(GL_DEBUG_SOURCE_OTHER);

	 // debug type
		CASE_ENUM(GL_DEBUG_TYPE_ERROR);
		CASE_ENUM(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR);
		CASE_ENUM(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR);
		CASE_ENUM(GL_DEBUG_TYPE_PORTABILITY);
		CASE_ENUM(GL_DEBUG_TYPE_PERFORMANCE);
		CASE_ENUM(GL_DEBUG_TYPE_MARKER);
		CASE_ENUM(GL_DEBUG_TYPE_PUSH_GROUP);
		CASE_ENUM(GL_DEBUG_TYPE_POP_GROUP);
		CASE_ENUM(GL_DEBUG_TYPE_OTHER);

	 // debug severity		
		CASE_ENUM(GL_DEBUG_SEVERITY_HIGH);
		CASE_ENUM(GL_DEBUG_SEVERITY_MEDIUM);
		CASE_ENUM(GL_DEBUG_SEVERITY_LOW);
		CASE_ENUM(GL_DEBUG_SEVERITY_NOTIFICATION);

		default: break;
	};
	#undef CASE_ENUM
	return "Unknown enum";
}

apt::AssertBehavior frm::internal::GlAssert(const char* _call, const char* _file, int _line)
{
	GLenum err = glGetError(); 
	if (err != GL_NO_ERROR) {
		APT_LOG_ERR("GL_ASSERT (%s, line %d)\n\t'%s' %s", apt::internal::StripPath(_file), _line, _call ? _call : "", GlEnumStr(err));
		return apt::AssertBehavior_Break;
	}
	return apt::AssertBehavior_Continue;
}

const char* frm::internal::GlGetString(GLenum _name)
{
	const char* ret;
	glAssert(ret = (const char*)glGetString(_name));
	return ret ? ret : "";
}
