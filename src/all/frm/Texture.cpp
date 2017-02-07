#include <frm/Texture.h>

#include <frm/gl.h>
#include <frm/GlContext.h>
#include <frm/Resource.h>

#include <apt/File.h>
#include <apt/FileSystem.h>
#include <apt/Image.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                 TextureView

*******************************************************************************/
TextureView::TextureView(Texture* _texture)
	: m_texture(_texture)
	, m_offset(0.0f, 0.0f)
	, m_size(0.0f, 0.0f)
	, m_mip(0)
	, m_array(0)
{
	m_rgbaMask[0] = m_rgbaMask[1] = m_rgbaMask[2] = m_rgbaMask[3] = true;
	if (m_texture) {
		m_size = vec2(_texture->getWidth(), _texture->getHeight());
		//Texture::Use(m_texture);
	}
}

TextureView::~TextureView()
{
	//Texture::Release(m_texture);
}

vec2 TextureView::getNormalizedOffset() const 
{ 
	return m_offset / vec2(m_texture->getWidth(), m_texture->getHeight()); 
}
vec2 TextureView::getNormalizedSize() const
{ 
	return m_size / vec2(m_texture->getWidth(), m_texture->getHeight()); 
}

/*******************************************************************************

                                   Texture

*******************************************************************************/

struct Texture_ScopedPixelStorei
{
	GLenum m_pname;
	GLint  m_param;

	Texture_ScopedPixelStorei(GLenum _pname, GLint _param)
		: m_pname(_pname)
	{
		glAssert(glGetIntegerv(m_pname, &m_param));
		glAssert(glPixelStorei(m_pname, _param));
	}

	~Texture_ScopedPixelStorei()
	{
		glAssert(glPixelStorei(m_pname, m_param));
	}
};
#define SCOPED_PIXELSTOREI(_pname, _param) Texture_ScopedPixelStorei APT_UNIQUE_NAME(_scopedPixelStorei)(_pname, _param)


static bool GlIsTexFormatCompressed(GLenum _format)
{
	switch (_format) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		case GL_COMPRESSED_RED_RGTC1:
		case GL_COMPRESSED_RG_RGTC2:
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
		case GL_COMPRESSED_RGBA_BPTC_UNORM:
			return true;
		default:
			return false;
	};
}

// PUBLIC

Texture* Texture::Create(const char* _path)
{
	Id id = GetHashId(_path);
	Texture* ret = Find(id);
	if (!ret) {
		ret = new Texture(id, _path);
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != kLoaded) {
	 // \todo replace with default
	}
	return ret;
}

Texture* Texture::Create1d(GLsizei _width, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_1D, _width, 1, 1, 1, _mipCount, _format);
}
Texture* Texture::Create1dArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_1D_ARRAY, _width, 1, 1, _arrayCount, _mipCount, _format);
}
Texture* Texture::Create2d(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_2D, _width, _height, 1, 1, _mipCount, _format);
}
Texture* Texture::Create2dArray(GLsizei _width, GLsizei _height, GLsizei _arrayCount, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_2D_ARRAY, _width, _height, 1, _arrayCount, _mipCount, _format);
}
Texture* Texture::Create3d(GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_3D, _width, _height, _depth, 1, _mipCount, _format);
}

Texture* Texture::CreateProxy(GLuint _handle, const char* _name)
{
	Id id = GetUniqueId();

	Texture* ret = Find(id);
	if (ret) {
		return ret;
	}
	
	ret = new Texture(id, _name);
	if (_name[0] == '\0') {
		ret->setNamef("%llu", id);
	}
	ret->m_handle = _handle;
	ret->m_ownsHandle = false;
	ret->m_width = ret->m_height = ret->m_depth = 1;
	ret->m_mipCount = 1;
	ret->m_arrayCount  = 1;
	glAssert(glGetTextureParameteriv(_handle, GL_TEXTURE_TARGET, (GLint*)&ret->m_target));
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_WIDTH, (GLint*)&ret->m_width));
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_HEIGHT, (GLint*)&ret->m_height));
	ret->m_height = APT_MAX(ret->m_height, 1);
	if (ret->m_target == GL_TEXTURE_1D_ARRAY) {
		ret->m_arrayCount = ret->m_height;
		ret->m_height = 1;
	}
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_DEPTH, (GLint*)&ret->m_depth));
	ret->m_depth = APT_MAX(ret->m_depth, 1);
	if (ret->m_target == GL_TEXTURE_2D_ARRAY) {
		ret->m_arrayCount = ret->m_depth;
		ret->m_depth = 1;
	}
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_INTERNAL_FORMAT, (GLint*)&ret->m_format));
	glAssert(glGetTextureParameteriv(_handle, GL_TEXTURE_MAX_LEVEL, (GLint*)&ret->m_mipCount));
	ret->m_mipCount = APT_MAX(ret->m_mipCount, 1);
	ret->setState(State::kLoaded);
	
	Use(ret);
	return ret;
}

void Texture::Destroy(Texture*& _inst_)
{
	delete _inst_;
}

GLint Texture::GetMaxMipCount(GLsizei _width, GLsizei _height, GLsizei _depth)
{
	const double rlog2 = 1.0 / log(2.0);
	const GLint log2Width  = (GLint)(log((double)_width)  * rlog2);
	const GLint log2Height = (GLint)(log((double)_height) * rlog2);
	const GLint log2Depth  = (GLint)(log((double)_depth)  * rlog2);
	return APT_MAX(log2Width, APT_MAX(log2Height, log2Depth)) + 1; // +1 for level 0
}

bool Texture::reload()
{
	if (m_path.isEmpty()) {
		return true;
	}
	
	File f;
	if (!FileSystem::Read(f, (const char*)m_path)) {
		setState(State::kError);
		return false;
	}

	Image img;
	if (!Image::Read(img, f)) {
		setState(State::kError);
		return false;
	}
	if (!loadImage(img)) {
		setState(State::kError);
		return false;
	}
	setState(State::kLoaded);

	return true;
}

void Texture::setData(const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip)
{
	setSubData(0, 0, 0, m_width, m_height, m_depth, _data, _dataFormat, _dataType, _mip);
	setState(State::kLoaded);
}

void Texture::setSubData(
	GLint       _offsetX, 
	GLint       _offsetY, 
	GLint       _offsetZ, 
	GLsizei     _sizeX, 
	GLsizei     _sizeY, 
	GLsizei     _sizeZ, 
	const void* _data, 
	GLenum      _dataFormat, 
	GLenum      _dataType, 
	GLint       _mip
	)
{
	APT_ASSERT(_mip <= m_mipCount);

	if (GlIsTexFormatCompressed(m_format)) {
		#ifdef APT_DEBUG
			GLsizei mipDiv = (GLsizei)pow(2.0, (double)_mip);
			GLsizei w = m_width / mipDiv;
			GLsizei h = m_height / mipDiv;
			if (w <= 4 || h <= 4) { // mip is 1 block
				bool illegal = 
					_offsetX > 0 ||
					_offsetY > 0 ||
					_sizeX != w  ||
					_sizeY != h
					;
				APT_ASSERT_MSG(!illegal, "Illegal operation, cannot upload sub data within a compressed block");
			}
		#endif
		switch (m_target) {
			case GL_TEXTURE_1D:
				glAssert(glCompressedTextureSubImage1D(m_handle, _mip, _offsetX, _sizeX, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D:
				glAssert(glCompressedTextureSubImage2D(m_handle, _mip, _offsetX, _offsetY, _sizeX, _sizeY, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;		
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glAssert(glCompressedTextureSubImage3D(m_handle, _mip, _offsetX, _offsetY, _offsetZ, _sizeX, _sizeY, _sizeZ, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;
			default:
				APT_ASSERT(false);
				setState(State::kError);
				return;
		};

	} else {
		switch (m_target) {
			case GL_TEXTURE_1D:
				glAssert(glTextureSubImage1D(m_handle, _mip, _offsetX, _sizeX, _dataFormat, _dataType, (GLvoid*)_data));
				break;
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D:
				glAssert(glTextureSubImage2D(m_handle, _mip, _offsetX, _offsetY, _sizeX, _sizeY, _dataFormat, _dataType, (GLvoid*)_data));
				break;		
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glAssert(glTextureSubImage3D(m_handle, _mip, _offsetX, _offsetY, _offsetZ, _sizeX, _sizeY, _sizeZ, _dataFormat, _dataType, (GLvoid*)_data));
				break;
			default:
				APT_ASSERT(false);
				setState(State::kError);
				return;
		};
	}
}

void Texture::generateMipmap()
{
	APT_ASSERT(m_handle);
	glAssert(glGenerateTextureMipmap(m_handle));
}

void Texture::setMipRange(GLint _base, GLint _max)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_BASE_LEVEL, (GLint)_base));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAX_LEVEL, (GLint)_base));
}

Image* Texture::downloadImage()
{
	APT_ASSERT(m_handle);

	Image::Layout layout;
	Image::DataType dataType;
	Image::CompressionType compression = Image::CompressionType::kNone;
	GLenum glFormat, glType;

	switch (m_format) {
		case GL_R:
		case GL_R8:   layout = Image::Layout::kR; dataType = Image::DataType::kUint8N;  glFormat = GL_RED; glType = GL_UNSIGNED_BYTE;  break;
		case GL_R16:  layout = Image::Layout::kR; dataType = Image::DataType::kUint16N; glFormat = GL_RED; glType = GL_UNSIGNED_SHORT; break;
		case GL_R16F: layout = Image::Layout::kR; dataType = Image::DataType::kFloat16; glFormat = GL_RED; glType = GL_HALF_FLOAT;     break;
		case GL_R32F: layout = Image::Layout::kR; dataType = Image::DataType::kFloat32; glFormat = GL_RED; glType = GL_FLOAT;          break;

		case GL_RG:
		case GL_RG8:   layout = Image::Layout::kRG; dataType = Image::DataType::kUint8N;  glFormat = GL_RG; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RG16:  layout = Image::Layout::kRG; dataType = Image::DataType::kUint16N; glFormat = GL_RG; glType = GL_UNSIGNED_SHORT; break;
		case GL_RG16F: layout = Image::Layout::kRG; dataType = Image::DataType::kFloat16; glFormat = GL_RG; glType = GL_HALF_FLOAT;     break;
		case GL_RG32F: layout = Image::Layout::kRG; dataType = Image::DataType::kFloat32; glFormat = GL_RG; glType = GL_FLOAT;          break;

		case GL_RGB:
		case GL_RGB8:   layout = Image::Layout::kRGB; dataType = Image::DataType::kUint8N;  glFormat = GL_RGB; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RGB16:  layout = Image::Layout::kRGB; dataType = Image::DataType::kUint16N; glFormat = GL_RGB; glType = GL_UNSIGNED_SHORT; break;
		case GL_RGB16F: layout = Image::Layout::kRGB; dataType = Image::DataType::kFloat16; glFormat = GL_RGB; glType = GL_HALF_FLOAT;     break;
		case GL_RGB32F: layout = Image::Layout::kRGB; dataType = Image::DataType::kFloat32; glFormat = GL_RGB; glType = GL_FLOAT;          break;
			
		case GL_RGBA:
		case GL_RGBA8:   layout = Image::Layout::kRGBA; dataType = Image::DataType::kUint8N;  glFormat = GL_RGBA; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RGBA16:  layout = Image::Layout::kRGBA; dataType = Image::DataType::kUint16N; glFormat = GL_RGBA; glType = GL_UNSIGNED_SHORT; break;
		case GL_RGBA16F: layout = Image::Layout::kRGBA; dataType = Image::DataType::kFloat16; glFormat = GL_RGBA; glType = GL_HALF_FLOAT;     break;
		case GL_RGBA32F: layout = Image::Layout::kRGBA; dataType = Image::DataType::kFloat32; glFormat = GL_RGBA; glType = GL_FLOAT;          break;

		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:       layout = Image::Layout::kRGB;  dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC1; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:      layout = Image::Layout::kRGBA; dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC1; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:      layout = Image::Layout::kRGBA; dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC2; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:      layout = Image::Layout::kRGBA; dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC3; break;
		case GL_COMPRESSED_RED_RGTC1:               layout = Image::Layout::kR;    dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC4; break;
		case GL_COMPRESSED_RG_RGTC2:                layout = Image::Layout::kR;    dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC5; break;
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: layout = Image::Layout::kRGB;  dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC6; break;
		case GL_COMPRESSED_RGBA_BPTC_UNORM:         layout = Image::Layout::kRGBA; dataType = Image::DataType::kInvalidType; compression = Image::CompressionType::kBC7; break;
	};

	Image* ret = 0;
	switch (m_target) {
		case GL_TEXTURE_1D: ret = Image::Create1d(m_width, layout, dataType, m_mipCount, compression); break;
		case GL_TEXTURE_2D: ret = Image::Create2d(m_width, m_height, layout, dataType, m_mipCount, compression); break;
		case GL_TEXTURE_3D: ret = Image::Create3d(m_width, m_height, m_depth, layout, dataType, m_mipCount, compression); break;

		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D_ARRAY:
		default: APT_ASSERT_MSG(false, "downloadImage unsupported for '%s'", internal::GlEnumStr(m_target)); break;
	};

	SCOPED_PIXELSTOREI(GL_PACK_ALIGNMENT, 1);
	for (int i = 0; i < m_arrayCount; ++i) {
		for (int j = 0; j < m_mipCount; ++j) {
			char* raw = ret->getRawImage(i, j);
			APT_ASSERT(raw);
			if (ret->isCompressed()) {
				glAssert(glGetCompressedTextureImage(m_handle, i, (GLsizei)ret->getRawImageSize(j), raw));
			} else {
				glAssert(glGetTextureImage(m_handle, i, glFormat, glType, (GLsizei)ret->getRawImageSize(j), raw));
			}
		}
	}

	return ret;
}

void Texture::setFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, (GLint)_mode));
}
void Texture::setMinFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (GLint)_mode));
}
void Texture::setMagFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, (GLint)_mode));
}
GLenum Texture::getMinFilter() const 
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_MIN_FILTER, &ret));
	return (GLenum)ret;

}
GLenum Texture::getMagFilter() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_MAG_FILTER, &ret));
	return (GLenum)ret;
}

void Texture::setAnisotropy(GLfloat _anisotropy)
{
	APT_ASSERT(m_handle);
	//if (GLEW_EXT_texture_filter_anisotropic) {
		float mx;
		glAssert(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &mx));
		glAssert(glTextureParameterf(m_handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, APT_CLAMP(_anisotropy, 1.0f, mx)));
	//}
}

GLfloat Texture::getAnisotropy() const
{
	APT_ASSERT(m_handle);
	GLfloat ret = -1.0f;
	//if (GLEW_EXT_texture_filter_anisotropic) {
		glAssert(glGetTextureParameterfv(m_handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, &ret));
	//}
	return ret;
}

void Texture::setWrap(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_R, (GLint)_mode));
}
void Texture::setWrapU(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, (GLint)_mode));
}
void Texture::setWrapV(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, (GLint)_mode));
}
void Texture::setWrapW(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_R, (GLint)_mode));
}
GLenum Texture::getWrapU() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_S, &ret));
	return (GLenum)ret;

}
GLenum Texture::getWrapV() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_T, &ret));
	return (GLenum)ret;
}
GLenum Texture::getWrapW() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_R, &ret));
	return (GLenum)ret;
}


// PROTECTED

Texture::Texture(uint64 _id, const char* _name)
	: Resource(_id, _name)
	, m_handle(0)
	, m_ownsHandle(true)
	, m_target(GL_NONE)
	, m_format((GLint)GL_NONE)
	, m_width(0), m_height(0), m_depth(0)
	, m_mipCount(0)
{
	APT_ASSERT(GlContext::GetCurrent());
}

Texture::Texture(
	uint64      _id, 
	const char* _name,
	GLenum      _target,
	GLsizei     _width,
	GLsizei     _height,
	GLsizei     _depth, 
	GLsizei     _arrayCount,
	GLsizei     _mipCount,
	GLenum      _format
	)
	: Resource(_id, _name)
{
	m_target     = _target;
	m_format     = _format;
	m_width      = _width;
	m_height     = _height;
	m_height     = _depth;
	m_arrayCount = _arrayCount;
	m_mipCount   = APT_MIN(_mipCount, GetMaxMipCount(_width, _height));
	glAssert(glCreateTextures(m_target, 1, &m_handle));

	switch (_target) {
		case GL_TEXTURE_1D:       glAssert(glTextureStorage1D(m_handle, _mipCount, _format, _width)); break;
		case GL_TEXTURE_1D_ARRAY: glAssert(glTextureStorage2D(m_handle, _mipCount, _format, _width, _arrayCount)); break;
		case GL_TEXTURE_2D:       glAssert(glTextureStorage2D(m_handle, _mipCount, _format, _width, _height)); break;
		case GL_TEXTURE_2D_ARRAY: glAssert(glTextureStorage3D(m_handle, _mipCount, _format, _width, _height, _arrayCount)); break;
		case GL_TEXTURE_3D:       glAssert(glTextureStorage3D(m_handle, _mipCount, _format, _width, _height, _depth)); break;
		default:                  APT_ASSERT(false); setState(State::kError); return;
	};
	
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (_mipCount > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_BASE_LEVEL, 0));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAX_LEVEL, _mipCount - 1));
	updateParams();
	
	setState(State::kLoaded);
}

Texture::~Texture()
{
	if (m_ownsHandle && m_handle) {
		glAssert(glDeleteTextures(1, &m_handle));
		m_handle = 0;
	}
	setState(State::kUnloaded);
}


bool Texture::isCompressed() const
{
	return GlIsTexFormatCompressed(m_format);
}

// PRIVATE

Texture* Texture::Create(
	GLenum  _target,
	GLsizei _width,
	GLsizei _height,
	GLsizei _depth, 
	GLsizei _arrayCount,
	GLsizei _mipCount,
	GLenum  _format
	)
{
	uint64 id = GetUniqueId();
	Texture* ret = new Texture(id, "", _target, _width, _height, _depth, _arrayCount, _mipCount, _format);
	ret->setNamef("%llu", id);
	Use(ret);
	return ret;
}



static void Alloc1d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage1D(_tx.getHandle(), (GLsizei)_img.getMipmapCount(), _tx.getFormat(), _tx.getWidth()));
}
static void Alloc1dArray(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage2D(_tx.getHandle(), (GLsizei)_img.getMipmapCount(), _tx.getFormat(), _tx.getWidth(), _tx.getArrayCount()));
}
static void Alloc2d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage2D(_tx.getHandle(), (GLsizei)_img.getMipmapCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight()));
}
static void Alloc2dArray(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage3D(_tx.getHandle(), (GLsizei)_img.getMipmapCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight(), _tx.getArrayCount()));
}
static void Alloc3d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage3D(_tx.getHandle(), (GLsizei)_img.getMipmapCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight(), _tx.getDepth()));
}

#define Texture_COMPUTE_WHD() \
	GLsizei div = (GLsizei)pow(2.0, (double)_mip); \
	GLsizei w = APT_MAX(_tx.getWidth()  / div, (GLsizei)1); \
	GLsizei h = APT_MAX(_tx.getHeight() / div, (GLsizei)1);\
	GLsizei d = APT_MAX(_tx.getDepth()  / div, (GLsizei)1); 

static void Upload1d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage1D(_tx.getHandle(), _mip, 0, w, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage1D(_tx.getHandle(), _mip, 0, w, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload1dArray(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, 1, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, 1, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload2d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, h, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, h, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload2dArray(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, 1, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, 1, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload3d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, d, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, d, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}

#undef Texture_COMPUTE_WHD

bool Texture::loadImage(const Image& _img)
{
	SCOPED_PIXELSTOREI(GL_UNPACK_ALIGNMENT, 1);

 // metadata
	m_width      = (GLint)_img.getWidth();
	m_height     = (GLint)_img.getHeight();
	m_depth      = (GLint)_img.getDepth();
	m_arrayCount = (GLint)_img.getArrayCount();
	m_mipCount   = (GLint)_img.getMipmapCount();

 // target, alloc/upload dispatch functions
	void (*alloc)(Texture& _tx, const Image& _img);
	void (*upload)(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType);
	switch (_img.getType()) {
		case Image::Type::k1d:           m_target = GL_TEXTURE_1D;        alloc = Alloc1d;      upload = Upload1d; break;
		case Image::Type::k1dArray:      m_target = GL_TEXTURE_1D_ARRAY;  alloc = Alloc1dArray; upload = Upload1dArray; break;
		case Image::Type::k2d:           m_target = GL_TEXTURE_2D;        alloc = Alloc2d;      upload = Upload2d; break;
		case Image::Type::k2dArray:      m_target = GL_TEXTURE_2D_ARRAY;  alloc = Alloc2dArray; upload = Upload2dArray; break;
		case Image::Type::k3d:           m_target = GL_TEXTURE_3D;        alloc = Alloc3d;      upload = Upload3d; break;
	 // \todo implement cubemaps
		case Image::Type::kCubemap:      APT_ASSERT(false);//m_target = GL_TEXTURE_CUBE_MAP;       upload = Upload2d; break;
		case Image::Type::kCubemapArray: APT_ASSERT(false);//m_target = GL_TEXTURE_CUBE_MAP_ARRAY; upload = Upload3d; break;
		default:                         APT_ASSERT(false); return false;
	};

 // src format
	GLenum srcFormat;
	switch (_img.getLayout()) {
		case Image::Layout::kR:          srcFormat = m_format = GL_RED;  break;
		case Image::Layout::kRG:         srcFormat = m_format = GL_RG;   break;
		case Image::Layout::kRGB:        srcFormat = m_format = GL_RGB;  break;
		case Image::Layout::kRGBA:       srcFormat = m_format = GL_RGBA; break;
		default:                         APT_ASSERT(false); return false;
	};

 // internal format (request only, we read back the actual format the implementation used later)
	if (_img.isCompressed()) {
		switch (_img.getCompressionType()) {
			case Image::CompressionType::kBC1: 
				switch (_img.getLayout()) {
					case Image::Layout::kRGB:  m_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;       break;
					case Image::Layout::kRGBA: m_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;      break;
					default:                   APT_ASSERT(false); return false;
				};
				break;
			case Image::CompressionType::kBC2: m_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;      break;
			case Image::CompressionType::kBC3: m_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;      break;
			case Image::CompressionType::kBC4: m_format = GL_COMPRESSED_RED_RGTC1;               break;
			case Image::CompressionType::kBC5: m_format = GL_COMPRESSED_RG_RGTC2;                break;
			case Image::CompressionType::kBC6: m_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT; break;
			case Image::CompressionType::kBC7: m_format = GL_COMPRESSED_RGBA_BPTC_UNORM;         break;
		};
	} else {
		switch (_img.getLayout()) {
			case Image::Layout::kR:
				switch (_img.getImageDataType()) {
					case DataType::kFloat32: m_format = GL_R32F; break;
					case DataType::kFloat16: m_format = GL_R16F; break;
					case DataType::kUint16N: m_format = GL_R16;  break;
					default:                 m_format = GL_R8;   break;
				};
				break;
			case Image::Layout::kRG:
				switch (_img.getImageDataType()) {
					case DataType::kFloat32: m_format = GL_RG32F; break;
					case DataType::kFloat16: m_format = GL_RG16F; break;
					case DataType::kUint16N: m_format = GL_RG16;  break;
					default:                 m_format = GL_RG8;   break;
				};
				break;
			case Image::Layout::kRGB:			
				switch (_img.getImageDataType()) {
					case DataType::kFloat32: m_format = GL_RGB32F; break;
					case DataType::kFloat16: m_format = GL_RGB16F; break;
					case DataType::kUint16N: m_format = GL_RGB16;  break;
					default:                 m_format = GL_RGB8;   break;
				};
				break;
			case Image::Layout::kRGBA:
				switch (_img.getImageDataType()) {
					case DataType::kFloat32: m_format = GL_RGBA32F; break;
					case DataType::kFloat16: m_format = GL_RGBA16F; break;
					case DataType::kUint16N: m_format = GL_RGBA16;  break;
					default:                 m_format = GL_RGBA8;   break;
				};
				break;
			default: break;
		};
	}
	
	GLenum srcType = _img.isCompressed() ? GL_UNSIGNED_BYTE : internal::GlDataTypeToEnum(_img.getImageDataType());

 // delete old handle, gen new handle (required since we use immutable storage)
	if (m_handle) {
		glAssert(glDeleteTextures(1, &m_handle));
	}

 // upload data; apt::Image stores each array layer contiguously with its mip chain, so we need to call
 // glTexSubImage* to upload each layer/mip separately
	glAssert(glCreateTextures(m_target, 1, &m_handle));
	alloc(*this, _img);
	for (GLint i = 0; i < m_arrayCount; ++i) {		
		for (GLint j = 0; j < m_mipCount; ++j) {
			upload(*this, _img, i, j, srcFormat, srcType);
		}
	}
	updateParams();

	setWrap(GL_REPEAT);
	setMagFilter(GL_LINEAR);
	setMinFilter(m_mipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

	return true;
}

void Texture::updateParams()
{
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_INTERNAL_FORMAT, &m_format));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_WIDTH,  &m_width));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_HEIGHT, &m_height));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_DEPTH, m_arrayCount > 1 ? &m_arrayCount : &m_depth));
	m_defaultTextureView = TextureView(this);
}