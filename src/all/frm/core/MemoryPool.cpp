#include "MemoryPool.h"

#include <frm/core/memory.h>

#include <algorithm>
#include <new>
#include <utility>

using namespace frm;

void frm::swap(MemoryPool& _a, MemoryPool& _b)
{
	using std::swap;
	swap(_a.m_objectSize,      _b.m_objectSize);
	swap(_a.m_objectAlignment, _b.m_objectAlignment);
	swap(_a.m_blockSize,       _b.m_blockSize);
	swap(_a.m_nextFree,        _b.m_nextFree);
	swap(_a.m_usedCount,       _b.m_usedCount);
	swap(_a.m_blocks,          _b.m_blocks);
	swap(_a.m_blockCount,      _b.m_blockCount);
}

// PUBLIC

MemoryPool::MemoryPool(uint _objectSize, uint _objectAlignment, uint _blockSize)
	: m_objectSize(_objectSize)
	, m_objectAlignment(_objectAlignment)
	, m_blockSize(_blockSize)
	, m_nextFree(0)
	, m_usedCount(0)
	, m_blocks(0)
	, m_blockCount(0)
{
	FRM_ASSERT(m_objectSize >= sizeof(void*)); // objects must be at least the size of a ptr
}

MemoryPool::~MemoryPool()
{
	FRM_ASSERT(m_usedCount == 0); // not all objects were freed
	for (uint i = 0; i < m_blockCount; ++i)
	{
		FRM_FREE_ALIGNED(m_blocks[i]);
	}
	FRM_FREE_ALIGNED(m_blocks);
}

void* MemoryPool::alloc()
{
	if (m_nextFree == 0)
	{
		allocBlock();
	}
	void* ret = m_nextFree;
	m_nextFree = *((void**)m_nextFree);
	++m_usedCount;
	return ret;
}

void MemoryPool::free(void* _object)
{
	FRM_ASSERT(_object);
	FRM_ASSERT(m_usedCount > 0);
	*((void**)_object) = m_nextFree;
	m_nextFree = _object;
	--m_usedCount;
}


// PRIVATE

void MemoryPool::allocBlock()
{
	m_blocks = (void**)FRM_REALLOC_ALIGNED(m_blocks, sizeof(void*) * (m_blockCount + 1), alignof(void*));
	m_blocks[m_blockCount] = FRM_MALLOC_ALIGNED(m_objectSize * m_blockSize, m_objectAlignment);

 // init free ptrs; if m_nextFree initially points to locX, after initializing the new block (starting new0) is initialized as follows:
 //  m_nextFree -> new0 -> new1 -> new2 -> new3 -> locX
	uint p = (uint)m_blocks[m_blockCount];
	for (uint i = 0, n = m_blockSize - 1; i < n; ++i)
	{
		*((uint*)p) = p + m_objectSize;
		p += m_objectSize;
	}
	*((uint*)p) = (uint)m_nextFree;
	m_nextFree = m_blocks[m_blockCount];
	
	++m_blockCount;
}

bool MemoryPool::isFromPool(const void* _ptr) const
{
	uint p = (uint)_ptr;
	for (uint i = 0; i < m_blockCount; ++i)
	{
		if (p >= (uint)m_blocks[i] && p < ((uint)m_blocks[i] + m_blockSize * m_objectSize))
		{
			return true;
		}
	}
	return false;
}

bool MemoryPool::validate() const
{
	uint freeCount = 0;
	void* p = m_nextFree;
	while (p != 0)
	{
		++freeCount;
		p = *((void**)p);
	}
	uint totalObjects = m_blockSize * m_blockCount;
	return m_usedCount == totalObjects - freeCount;
}
