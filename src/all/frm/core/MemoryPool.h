#pragma once

#include <frm/core/frm.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// MemoryPool
// See Pool.h for a more user-friendly, templated version of this class.
// Provides (de)allocations of objects with O(1) complexity given a fixed object 
// size/alignment.
// Usage:	
//
//    MemoryPool mp(sizeof(Foo), FRM_ALIGNOF(Foo), 128);
//    Foo* f = new(mp.alloc()) Foo; // placement new
//    f->~Foo(); // explicit dtor call
//    mp.free(f);
//
// Any allocated objects should be released via free() before the MemoryPool is 
// destroyed.
////////////////////////////////////////////////////////////////////////////////
class MemoryPool: private non_copyable<MemoryPool>
{
public:
	// _objectSize must be at least sizeof(void*). _blockSize is the number of new unused objects to allocate when alloc() cannot service a new request.
	MemoryPool(uint _objectSize, uint _objectAlignment, uint _blockSize);

	// Free all allocated memory. Any allocated objects should be released via free() before the MemoryPool is destroyed.
	~MemoryPool();

	void* alloc();
	void  free(void* _object);

	// Return true if _ptr was allocated from the pool.
	bool isFromPool(const void* _ptr) const;

	// Return true if # used objects is consistent with # accessible free objects.
	bool validate() const;

	uint getCapacity() const  { return m_blockSize * m_blockCount; }
	uint getUsedCount() const { return m_usedCount; }
	uint getFreeCount() const { return getCapacity() - m_usedCount; }

	friend void swap(MemoryPool& _a, MemoryPool& _b);
	
private:
	uint   m_objectSize, m_objectAlignment, m_blockSize;
	void*  m_nextFree;
	uint   m_usedCount;
	void** m_blocks;
	uint   m_blockCount;

	void allocBlock();

};

void swap(MemoryPool& _a, MemoryPool& _b);

} // namespace frm
