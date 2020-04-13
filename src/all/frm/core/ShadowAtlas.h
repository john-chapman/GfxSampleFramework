#pragma once

#include <frm/core/gl.h>
#include <frm/core/frm.h>
#include <frm/core/Pool.h>
#include <frm/core/Quadtree.h>

#include <eastl/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// ShadowAtlas
////////////////////////////////////////////////////////////////////////////////
class ShadowAtlas
{
public:

	static ShadowAtlas* Create(GLsizei _maxSize, GLsizei _minSize, GLenum _format, GLsizei _arrayCount = 1);
	static void         Destroy(ShadowAtlas*& _inst_);

	struct ShadowMap
	{
		float  uvScale    = 0.0f;
		vec2   uvBias     = vec2(0.0f);
		int    size       = 0;
		ivec2  origin     = ivec2(0);
		uint32 arrayIndex = 0;
		uint16 nodeIndex  = 0;
	};

	ShadowMap*   alloc(float _lod);
	ShadowMap*   alloc(GLsizei _size);

	void         free(ShadowMap*& _shadowMap_);
	

	Texture*     getTexture()              { return m_texture; }
	Framebuffer* getFramebuffer(int i = 0) { return m_framebuffers[i]; }

private:
	using Quadtree = Quadtree<uint16, bool>; // 16 bit index = 8 levels, bool determines if a node is empty (false if any children were allocated)

	GLsizei                     m_minSize       = 0;
	GLsizei                     m_maxSize       = 0;
	GLsizei                     m_arrayCount    = 0;
	GLenum                      m_format        = GL_NONE;
	Texture*                    m_texture       = nullptr;
	Pool<ShadowMap>             m_shadowMapPool;
	eastl::vector<Quadtree*>    m_quadtrees;    // per array layer
	eastl::vector<Framebuffer*>	m_framebuffers; // per array layer

	ShadowAtlas(GLsizei _maxSize, GLsizei _minSize, GLenum _format, GLsizei _arrayCount);
	~ShadowAtlas();

	bool init();
	void shutdown();

	ShadowMap* allocRecursive(uint32 _arrayIndex, int _targetLevel, uint16 _nodeIndex, int _nodeLevel, GLsizei _size);
	void       freeRecursive(uint32 _arrayIndex, uint16 _nodeIndex, int _nodeLevel);

};

} // namespace frm
