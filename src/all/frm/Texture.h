#pragma once
#ifndef frm_Texture_h
#define frm_Texture_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/math.h>
#include <frm/Resource.h>

namespace apt {
	class Image;
}

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class Texture
/// \note When loading from a apt::Image, texture data is inverted in V.
////////////////////////////////////////////////////////////////////////////////
class Texture: public Resource<Texture>
{
public:

	/// Load from a file.
	static Texture* Create(const char* _path);

	/// Create an empty texture (the resource name is unique).
	static Texture* Create1d(GLsizei _width, GLenum _format, GLint _mipCount = 1);
	static Texture* Create1dArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2d(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2dArray(GLsizei _width, GLsizei _height, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create3d(GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLint _mipCount = 1);

	/// Create a proxy for an existing texture (i.e. a texture not directly
	/// controlled by the application).
	static Texture* CreateProxy(GLuint _handle, const char* _name);
	
	static void Destroy(Texture*& _inst_);

	static GLint GetMaxMipCount(GLsizei _width, GLsizei _height, GLsizei _depth = 1);


	bool load()   { return reload(); }
	bool reload();

	/// Upload data to the GPU. The image dimensions and mip count must exactly 
	/// match those used to create the texture (texture storage is immutable).
	void setData(const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip = 0);

	/// Upload data to a subregion of the texture.
	void setSubData(
		GLint       _offsetX, 
		GLint       _offsetY, 
		GLint       _offsetZ,
		GLsizei     _sizeX, 
		GLsizei     _sizeY, 
		GLsizei     _sizeZ,
		const void* _data, 
		GLenum      _dataFormat, 
		GLenum      _dataType,    // \hack or compressed size for compressed data
		GLint       _mip = 0
		);
	
	/// Auto generate mipmap.
	void generateMipmap();

	/// Set base/max level for mipmap access.
	void setMipRange(GLint _base, GLint _max);

	/// Download the all mips/levels of the texture to image_. This a a 
	/// synchronous operation via glGetTextureImage() and will stall the gpu.
	/// The apt::Image* must be released via Image::Destroy()
	apt::Image* downloadImage();

	/// Filter mode.
	void    setFilter(GLenum _mode);    //< \note MIPMAP filter modes cannot be applied globally
	void    setMinFilter(GLenum _mode);
	void    setMagFilter(GLenum _mode);
	GLenum  getMinFilter() const;
	GLenum  getMagFilter() const;

	/// Anisotropy (values >1 enable anisotropic filtering).
	void    setAnisotropy(GLfloat _anisotropy);
	GLfloat getAnisotropy() const;

	/// Wrap mode. UVW = STR.
	void    setWrap(GLenum _mode);
	void    setWrapU(GLenum _mode);
	void    setWrapV(GLenum _mode);
	void    setWrapW(GLenum _mode);
	GLenum  getWrapU() const;
	GLenum  getWrapV() const;
	GLenum  getWrapW() const;


	GLuint  getHandle() const     { return m_handle; }
	GLenum  getTarget() const     { return m_target; }
	GLint   getFormat() const     { return m_format; }

	GLsizei getWidth() const      { return m_width; }
	GLsizei getHeight() const     { return m_height; }
	GLsizei getDepth() const      { return m_depth; }
	GLint   getArrayCount() const { return m_arrayCount; }
	GLint   getMipCount() const   { return m_mipCount; }

	const char* getPath() const             { return m_path; }
	void        setPath(const char* _path)  { m_path.set(_path); }

protected:
	Texture(uint64 _id, const char* _name);
	Texture(
		uint64 _id, 
		const char* _name, 
		GLenum _target, 
		GLsizei _width, 
		GLsizei _height, 
		GLsizei _depth,
		GLsizei _arrayCount,
		GLsizei _mipCount,
		GLenum _format
		);
	~Texture();

	bool isCompressed() const;

private:
	apt::String<32> m_path;  //< Empty if not from a file.

	GLuint  m_handle;
	bool    m_ownsHandle;    //< False if this is a proxy.
	GLenum  m_target;        //< GL_TEXTURE_2D, GL_TEXTURE_3D, etc.
	GLint   m_format;        //< Internal format (as used by the implementation, not necessarily the same as the requested format).

	GLsizei m_width, m_height, m_depth;
	GLint   m_arrayCount;
	GLint   m_mipCount;

	/// Common code for Create* methods. 
	static Texture* Create(
		GLenum  _target,
		GLsizei _width,
		GLsizei _height,
		GLsizei _depth, 
		GLsizei _arrayCount,
		GLsizei _mipCount,
		GLenum  _format
		);

	/// Load data from a apt::Image.
	bool loadImage(const apt::Image& _img);

	/// Update the format and dimensions of the texture via glGetTexLevelParameteriv.
	/// Assumes that the texture is bound to m_target.
	void updateParams();
		
}; // class Texture

struct TextureView
{
	const Texture* m_texture;
	vec2           m_offset;
	vec2           m_size;
	GLint          m_mip;
	GLint          m_array;
	bool           m_rgbaMask[4];

	// \todo correct Use/Unuse here
	TextureView(const Texture* _texture = 0)
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

	~TextureView()
	{
		if (m_texture) {
			//Texture::Unuse(m_texture)
		}
	}

	vec2 getNormalizedOffset() const { return m_offset / vec2(m_texture->getWidth(), m_texture->getHeight()); }
	vec2 getNormalizedSize() const   { return m_size / vec2(m_texture->getWidth(), m_texture->getHeight()); }

}; // struct TextureView


} // namespace frm

#endif // frm_Texture_h
