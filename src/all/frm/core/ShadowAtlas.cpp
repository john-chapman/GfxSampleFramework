#include "ShadowAtlas.h"

#include <frm/core/interpolation.h>
#include <frm/core/memory.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/Texture.h>


static int RoundUpToPow2(int x)
{
	if (!frm::IsPow2(x))
	{		
		x--;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		x++;
	}

	return x;
}

static int FindFirstBit(int x)
{
	int ret = 0;

	int y = 1;
	while (!(x & y))
	{
		y = y << 1;
		++ret;
	}

	return ret;
}

namespace frm {

// PUBLIC

ShadowAtlas* ShadowAtlas::Create(GLsizei _maxSize, GLsizei _minSize, GLenum _format, GLsizei _arrayCount)
{
	ShadowAtlas* ret = FRM_NEW(ShadowAtlas(_maxSize, _minSize, _format, _arrayCount));
	return ret;
}

void ShadowAtlas::Destroy(ShadowAtlas*& _inst_)
{
	FRM_DELETE(_inst_);
	_inst_ = nullptr;
}

ShadowAtlas::ShadowMap* ShadowAtlas::alloc(float _lod)
{
	GLsizei size = (GLsizei)lerp((float)m_minSize, (float)m_maxSize, _lod);
	return alloc(size);
}

ShadowAtlas::ShadowMap* ShadowAtlas::alloc(GLsizei _size)
{
	_size = RoundUpToPow2(_size);
	_size = Min(m_maxSize, _size);

	if (_size < m_minSize)
	{
		return nullptr;
	}

	// convert _size to a quadtree level index
	int targetLevel = FindFirstBit((m_texture->getWidth() / _size));

	ShadowMap* ret = nullptr;
	for (GLsizei arrayIndex = 0; arrayIndex < m_arrayCount; ++arrayIndex)
	{
		ret = allocRecursive(arrayIndex, targetLevel, 0, 0, _size);
		if (ret)
		{
			break;
		}
	}
	return ret;
}

void ShadowAtlas::free(ShadowMap*& _shadowMap_)
{
	FRM_ASSERT(_shadowMap_);

	Quadtree& quadtree = *m_quadtrees[_shadowMap_->arrayIndex];
	const Quadtree::Index nodeIndex = _shadowMap_->nodeIndex;
	const int nodeLevel = Quadtree::FindLevel(nodeIndex);
	const Quadtree::Index parentIndex = quadtree.getParentIndex(nodeIndex, nodeLevel);

	quadtree[nodeIndex] = true;
	freeRecursive(_shadowMap_->arrayIndex, parentIndex, nodeLevel - 1);

	m_shadowMapPool.free(_shadowMap_);
	_shadowMap_ = nullptr;
}

// PRIVATE

ShadowAtlas::ShadowAtlas(GLsizei _maxSize, GLsizei _minSize, GLenum _format, GLsizei _arrayCount)
	: m_maxSize(RoundUpToPow2(_maxSize))
	, m_minSize(RoundUpToPow2(_minSize))
	, m_format(_format)
	, m_arrayCount(_arrayCount)
	, m_shadowMapPool(16)
{
	FRM_ASSERT(m_maxSize >= m_minSize);
	FRM_ASSERT(m_arrayCount > 0);
	FRM_VERIFY(init());
}

ShadowAtlas::~ShadowAtlas()
{
	shutdown();
}

bool ShadowAtlas::init()
{
	FRM_ASSERT(m_shadowMapPool.getUsedCount() == 0); // can't init while shadow maps are allocated (can't invalidate externally held ShadowMap*)

	shutdown();

	bool ret = true;

	GLsizei textureSize = m_maxSize * 2; // reduce the chance of filling up the atlas with max size allocations
	m_texture = Texture::Create2dArray(textureSize, textureSize, m_arrayCount, m_format);
	if (!m_texture || m_texture->getState() != Texture::State_Loaded)
	{
		return false;
	}
	m_texture->setFilter(GL_NEAREST);
	m_texture->setName("txShadowAtlas");

	m_framebuffers.resize(m_arrayCount);
	m_quadtrees.resize(m_arrayCount);
	for (GLsizei i = 0; i < m_arrayCount; ++i)
	{
		Framebuffer*& fb = m_framebuffers[i];
		fb = Framebuffer::Create();
		fb->attachLayer(m_texture, GL_DEPTH_ATTACHMENT, i);

		Quadtree*& qt = m_quadtrees[i];
		qt = FRM_NEW(Quadtree(Quadtree::GetAbsoluteMaxLevelCount(), true)); // \todo compute the actual number of levels required
	}

	return true;
}

void ShadowAtlas::shutdown()
{
	Texture::Release(m_texture);

	for (Framebuffer*& fb : m_framebuffers)
	{
		Framebuffer::Destroy(fb);
	}
	m_framebuffers.clear();

	while (!m_quadtrees.empty())
	{
		FRM_DELETE(m_quadtrees.back());
		m_quadtrees.pop_back();
	}
}

ShadowAtlas::ShadowMap* ShadowAtlas::allocRecursive(uint32 _arrayIndex, int _targetLevel, uint16 _nodeIndex, int _nodeLevel, GLsizei _size)
{
	Quadtree& quadtree = *m_quadtrees[_arrayIndex];
	
	ShadowMap* ret = nullptr;

	if (_nodeLevel == _targetLevel)
	{
		if (quadtree[_nodeIndex]) // node is empty
		{
			quadtree[_nodeIndex] = false;
			
			ret             = m_shadowMapPool.alloc();
			ret->arrayIndex = _arrayIndex;
			ret->nodeIndex  = _nodeIndex;
			ret->size       = _size;
			ret->origin     = ivec2(Quadtree::ToCartesian(_nodeIndex, _nodeLevel) * (uint32)_size);
			ret->uvBias     = vec2(ret->origin) / (float)m_texture->getWidth();
			ret->uvScale    = (float)_size / (float)m_texture->getWidth();
		}
	}
	else
	{
		Quadtree::Index firstChildIndex = quadtree.getFirstChildIndex(_nodeIndex, _nodeLevel);
		for (Quadtree::Index childIndex = 0; childIndex < 4; ++childIndex)
		{
			ret = allocRecursive(_arrayIndex, _targetLevel, firstChildIndex + childIndex, _nodeLevel + 1, _size);
			if (ret)
			{
				quadtree[_nodeIndex] = false;					
				break;
			}
		}
	}

	return ret;
}

void ShadowAtlas::freeRecursive(uint32 _arrayIndex, uint16 _nodeIndex, int _nodeLevel)
{
	if (_nodeIndex == Quadtree::Index_Invalid)
	{
		return;
	}

	Quadtree& quadtree = *m_quadtrees[_arrayIndex];
	
	const Quadtree::Index firstChildIndex = quadtree.getFirstChildIndex(_nodeIndex, _nodeLevel);
	FRM_ASSERT(firstChildIndex != Quadtree::Index_Invalid);
	
	quadtree[_nodeIndex] = true
		&& quadtree[firstChildIndex + 0]
		&& quadtree[firstChildIndex + 1]
		&& quadtree[firstChildIndex + 2]
		&& quadtree[firstChildIndex + 3]
		;

	const Quadtree::Index parentIndex = quadtree.getParentIndex(_nodeIndex, _nodeLevel);
	if (parentIndex != Quadtree::Index_Invalid)
	{
		freeRecursive(_arrayIndex, parentIndex, _nodeLevel - 1);
	}
}


} // namespace frm
