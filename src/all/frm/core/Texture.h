#pragma once

#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/math.h>
#include <frm/core/Resource.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Texture
// \note When loading from a frm::Image, texture data is inverted in V.
////////////////////////////////////////////////////////////////////////////////
class Texture: public Resource<Texture>
{
public:
	// SourceLayout enum provides a hint about the layout for special cases (e.g. 2x3 cubemap).
	enum SourceLayout
	{
		SourceLayout_Default,
		SourceLayout_Cubemap2x3, // Faces arranged in a 2x3 grid, +x,-x +y,-y, +z,-z.
		SourceLayout_VolumeNx1,  // Slices are arranged in a nx1 grid.
		
		SourceLayout_Count
	};

	// Load from a file.
	static Texture* Create(const char* _path, SourceLayout _layout = SourceLayout_Default);
	// From frm::Image.
	static Texture* Create(const frm::Image& _img);
	// Init from another texture, optionally copy texture data.
	static Texture* Create(Texture* _tx, bool _copyData = true);
	// Create an empty texture (the resource name is unique).
	static Texture* Create1d(GLsizei _width, GLenum _format, GLint _mipCount = 1);
	static Texture* Create1dArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2d(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount = 1);
	static Texture* Create2dArray(GLsizei _width, GLsizei _height, GLsizei _arrayCount, GLenum _format, GLint _mipCount = 1);
	static Texture* Create3d(GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLint _mipCount = 1);
	static Texture* CreateCubemap(GLsizei _width, GLenum _format, GLint _mipcount);
	static Texture* CreateCubemapArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipcount);
	// Create a proxy for an existing texture (i.e. a texture not directly controlled by the application).
	static Texture* CreateProxy(GLuint _handle, const char* _name);
	static void     Destroy(Texture*& _inst_);

	// Reload _path.
	static void     FileModified(const char* _path);

	// Create an frm::Image (download the GPU data). This a a synchronous operation via glGetTextureImage() and will stall the gpu.
	static frm::Image* CreateImage(const Texture* _tx);
	static void        DestroyImage(frm::Image*& _img_);
	
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

	// Bias for mip selection (negative bias sharpens the image).
	void  setMipBias(float _bias);
	float getMipBias() const;
	
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
	ivec3       getDimensions() const           { return ivec3(m_width, m_height, m_depth); }
	GLint       getArrayCount() const           { return m_arrayCount; }
	GLint       getMipCount() const             { return m_mipCount;   }

	const char* getPath() const                 { return (const char*)m_path; }
	void        setPath(const char* _path)      { m_path.set(_path);          }

	bool        isCompressed() const;
	bool        isDepth() const;

	TextureView* getTextureView() const;  // get the internal texture view (owned by the texture viewer)

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
	frm::String<32> m_path;  // Empty if not from a file.
	SourceLayout    m_sourceLayout = SourceLayout_Default;

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

	// Load data from a frm::Image.
	bool loadImage(const frm::Image& _img);

	// Update the format and dimensions of the texture via glGetTexLevelParameteriv.
	// Assumes that the texture is bound to m_target.
	void updateParams();
		
}; // class Texture

class TextureSampler
{
public:

	static TextureSampler* Create();
	static TextureSampler* Create(GLenum _wrap, GLenum _filter, GLfloat _anisotropy = 1.0f, GLfloat _lodBias = 0.0f);
	static void            Destroy(TextureSampler*& _sampler_);

	GLuint  getHandle() const { return m_handle; }

	void    setWrap(GLenum _wrapUVW);
	void    setWrap(GLenum _wrapU, GLenum _wrapV, GLenum _wrapW = GL_REPEAT);
	void    setWrapU(GLenum _wrapU);
	void    setWrapV(GLenum _wrapV);
	void    setWrapW(GLenum _wrapW);
	GLenum  getWrapU() const { return m_wrap[0]; }
	GLenum  getWrapV() const { return m_wrap[1]; }
	GLenum  getWrapW() const { return m_wrap[2]; }

	void    setFilter(GLenum _filterMinMag);
	void    setFilter(GLenum _filterMin, GLenum _filterMag);
	void    setMinFilter(GLenum _filterMin);
	void    setMagFilter(GLenum _filterMag);
	GLenum  getMinFilter() const { return m_minFilter; }
	GLenum  getMagFilter() const { return m_magFilter; }

	void    setAnisotropy(GLfloat _anisotropy);
	GLfloat getAnisotropy() const { return m_anisotropy; }

	void    setLodBias(GLfloat _lodBias);
	GLfloat getLodBias() const { return m_lodBias; }

	void    setMipRange(GLfloat _min, GLfloat _max);
	GLfloat getMipRangeMin() const { return m_mipRange[0]; }
	GLfloat getMipRangeMax() const { return m_mipRange[1]; }


private:

	TextureSampler();
	~TextureSampler();

	GLuint  m_handle      = GL_NONE;

	GLenum  m_wrap[3]     = { GL_REPEAT, GL_REPEAT, GL_REPEAT };
	GLenum  m_minFilter   = GL_LINEAR;
	GLenum  m_magFilter   = GL_LINEAR;
	GLfloat m_anisotropy  = 1.0f;
	GLfloat m_lodBias     = 0.0f;
	GLfloat m_mipRange[2] = { -1000.0f, 1000.0f };

};

////////////////////////////////////////////////////////////////////////////////
// TextureView
// Represents a subregion (offset, size) of a texture mip or array layer, plus
// a color mask.
////////////////////////////////////////////////////////////////////////////////
struct TextureView
{
	Texture* m_texture;
	Shader*  m_shader;  // use a default if 0
	vec2     m_offset;
	vec2     m_size;
	GLint    m_mip;
	GLint    m_array;
	bool     m_rgbaMask[4];

	// \hack TextureView* are sometimes passed to ImGui and subsequently destroyed before ImGui derences the ptr, causing a crash.
	// To get around this we store a map of valid instances at all times and check during AppSample::ImGui_RenderDrawLists().
	static bool CheckValid(const TextureView* _txView);

	TextureView(Texture* _texture = nullptr, Shader* _shader = nullptr);
	~TextureView();

	void reset();

	vec2 getNormalizedOffset() const;
	vec2 getNormalizedSize() const;

}; // struct TextureView


} // namespace frm
