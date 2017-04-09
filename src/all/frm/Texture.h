#pragma once
#ifndef frm_Texture_h
#define frm_Texture_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/math.h>
#include <frm/Resource.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Texture
// \note When loading from a apt::Image, texture data is inverted in V.
////////////////////////////////////////////////////////////////////////////////
class Texture: public Resource<Texture>
{
public:
	// Load from a file.
	static Texture* Create(const char* _path);
	static Texture* CreateCubemap2x3(const char* _path); // faces arranged in a 2x3 grid, +x,-x +y,-y, +z,-z
	// Create an empty texture (the resource name is unique).
	static Texture* Create1d(GLsizei _width, GLenum _format, GLint _mipCount = 1);
	static Texture* Create1dArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2d(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2dArray(GLsizei _width, GLsizei _height, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create3d(GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLint _mipCount = 1);
	static Texture* CreateCubemap(GLsizei _width, GLenum _format, GLint _mipcount);
	// Create a proxy for an existing texture (i.e. a texture not directly controlled by the application).
	static Texture* CreateProxy(GLuint _handle, const char* _name);
	static void     Destroy(Texture*& _inst_);
	
	static GLint GetMaxMipCount(GLsizei _width, GLsizei _height, GLsizei _depth = 1);

	// Convert between environment map projections.
	static bool ConvertSphereToCube(Texture& _sphere, GLsizei _width);
	static bool ConvertCubeToSphere(Texture& _cube, GLsizei _width);

	static void ShowTextureViewer(bool* _open_);

	bool load()   { return reload(); }
	bool reload();

	// Upload data to the GPU. The image dimensions and mip count must exactly match those 
	// used to create the texture (texture storage is immutable).
	void setData(const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip = 0);

	// Upload data to a subregion of the texture.
	void setSubData(
		GLint       _offsetX, 
		GLint       _offsetY, 
		GLint       _offsetZ,
		GLsizei     _sizeX, 
		GLsizei     _sizeY, 
		GLsizei     _sizeZ,
		const void* _data, 
		GLenum      _dataFormat, 
		GLenum      _dataType, // or the compressed data size
		GLint       _mip = 0
		);
	
	// Auto generate mipmap.
	void generateMipmap();

	// Set base/max level for mipmap access.
	void setMipRange(GLint _base, GLint _max);

	// Download the all mips/levels of the texture to image_. This a a synchronous operation 
	// via glGetTextureImage() and will stall the gpu. The apt::Image* must be released via 
	// apt::Image::Destroy().
	apt::Image* downloadImage();

	// Filter mode.
	void        setFilter(GLenum _mode);    // mipmap filter modes cannot be applied globally
	void        setMinFilter(GLenum _mode);
	void        setMagFilter(GLenum _mode);
	GLenum      getMinFilter() const;
	GLenum      getMagFilter() const;

	// Anisotropy (values >1 enable anisotropic filtering).
	void        setAnisotropy(GLfloat _anisotropy);
	GLfloat     getAnisotropy() const;

	void        setWrap(GLenum _mode);
	void        setWrapU(GLenum _mode);
	void        setWrapV(GLenum _mode);
	void        setWrapW(GLenum _mode);
	GLenum      getWrapU() const;
	GLenum      getWrapV() const;
	GLenum      getWrapW() const;

	GLuint      getHandle() const               { return m_handle;     }
	GLenum      getTarget() const               { return m_target;     }
	GLint       getFormat() const               { return m_format;     }

	GLsizei     getWidth() const                { return m_width;      }
	GLsizei     getHeight() const               { return m_height;     }
	GLsizei     getDepth() const                { return m_depth;      }
	GLint       getArrayCount() const           { return m_arrayCount; }
	GLint       getMipCount() const             { return m_mipCount;   }

	const char* getPath() const                 { return m_path;       }
	void        setPath(const char* _path)      { m_path.set(_path);   }

	bool        isCompressed() const;
	bool        isDepth() const;

	friend void swap(Texture& _a, Texture& _b);

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

private:
	apt::String<32> m_path;  // Empty if not from a file.

	GLuint  m_handle;
	bool    m_ownsHandle;    // False if this is a proxy.
	GLenum  m_target;        // GL_TEXTURE_2D, GL_TEXTURE_3D, etc.
	GLint   m_format;        // Internal format (as used by the implementation, not necessarily the same as the requested format).
	GLsizei m_width;
	GLsizei m_height;        // Min is 1.
	GLsizei m_depth;         //    "
	GLint   m_arrayCount;    //    "
	GLint   m_mipCount;      //    "

	// Common code for Create* methods. 
	static Texture* Create(
		GLenum  _target,
		GLsizei _width,
		GLsizei _height,
		GLsizei _depth, 
		GLsizei _arrayCount,
		GLsizei _mipCount,
		GLenum  _format
		);

	// Load data from a apt::Image.
	bool loadImage(const apt::Image& _img);

	// Update the format and dimensions of the texture via glGetTexLevelParameteriv.
	// Assumes that the texture is bound to m_target.
	void updateParams();
		
}; // class Texture


////////////////////////////////////////////////////////////////////////////////
// TextureView
// Represents a subregion (offset, size) of a texture mip or array layer, plus
// a color mask.
////////////////////////////////////////////////////////////////////////////////
struct TextureView
{
	Texture* m_texture;
	vec2     m_offset;
	vec2     m_size;
	GLint    m_mip;
	GLint    m_array;
	bool     m_rgbaMask[4];

	TextureView(Texture* _texture = 0);
	~TextureView();

	void reset();

	vec2 getNormalizedOffset() const;
	vec2 getNormalizedSize() const;

}; // struct TextureView


} // namespace frm

#endif // frm_Texture_h
