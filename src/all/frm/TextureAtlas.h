#pragma once
#ifndef frm_TextureAtlas_h
#define frm_TextureAtlas_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/Texture.h>

#include <apt/Pool.h>
#include <apt/StringHash.h>

#include <vector>

#ifdef APT_DEBUG
	#define frm_TextureAtlas_DEBUG
#endif

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class TextureAtlas
/// \todo Pass an ID when uploading an image (detect if you already uploaded the
///   the image). Requires refcounting for regions!
/// \todo Select between quadtree/BSP allocator, more efficient implementation
///   for both. See commit 7c6ac7e for a working version of the BSP.
////////////////////////////////////////////////////////////////////////////////
class TextureAtlas: public Texture
{
public:
	typedef apt::StringHash::HashType RegionId;

	struct Region
	{
		vec2 m_uvScale;
		vec2 m_uvBias;
		int  m_lodMax;
	};

	static TextureAtlas* Create(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount = 1);
	static void Destroy(TextureAtlas*& _inst_);

	/// Alloc an uninitialized _width * _height region.
	/// \return 0 if the allocation failed.
	Region* alloc(GLsizei _width, GLsizei _height);
	/// Alloc a region large enough to fit _img and upload data (to all mips). Optionally 
	/// set the region id (e.g. a hash of the image path), see findUse()/unuseFree().
	Region* alloc(const apt::Image& _img, RegionId _id = 0);

	/// Free a previously allocated region.
	void free(Region*& _region_);

	/// Increment the ref count of a named region.
	/// \return 0 if _id was not found.
	Region* findUse(RegionId _id);
	/// Decrement the ref count of named region, free the region if ref count == 0.
	void unuseFree(Region*& _region_);

	/// Upload data to a previously allocated region.
	void upload(const Region& _region, const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip = 0);


protected:
	TextureAtlas(
		uint64      _id, 
		const char* _name,
		GLenum      _format,
		GLsizei     _width, 
		GLsizei     _height,
		GLsizei     _mipCount
		);
	~TextureAtlas();

private:
	vec2 m_rsize;
	apt::Pool<Region> m_regionPool;

	struct RegionRef { RegionId m_id; Region* m_region; int m_refCount; };
	std::vector<RegionRef> m_regionMap;

 // quad tree allocator
 	struct Node;
	Node* m_root;
	apt::Pool<Node>* m_nodePool;	

	Node* insert(Node* _root, uint16 _sizeX, uint16 _sizeY);
	void  remove(Node*& _node_);
	Node* find(const Region& _region);
	Node* find(Node* _root, uint16 _originX, uint16 _originY);

#ifdef frm_TextureAtlas_DEBUG
public:
	void debug();
	void debugDrawNode(const Node* _node, const vec2& _drawStart, const vec2& _drawSize);
#endif

}; // class TextureAtlas

} // namespace frm

#endif // frm_TextureAtlas_h
