#pragma once

#include <frm/core/MemoryPool.h>

#include <utility> // std::move

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Pool
// Templated MemoryPool.
////////////////////////////////////////////////////////////////////////////////
template <typename tType>
class Pool: public MemoryPool
{
public:
	Pool(uint _blockSize)
		: MemoryPool(sizeof(tType), alignof(tType), _blockSize)
	{
	}

	tType* alloc()
	{
		tType* ret = (tType*)MemoryPool::alloc();
		new(ret) tType();
		return ret;
	}

	tType* alloc(const tType& _v)
	{
		tType* ret = (tType*)MemoryPool::alloc();
		new(ret) tType(_v);
		return ret;
	}

	tType* alloc(tType&& _v)
	{
		tType* ret = (tType*)MemoryPool::alloc();
		new(ret) tType(std::move(_v));
		return ret;
	}

	void free(tType* _object)
	{
		_object->~tType();
		MemoryPool::free(_object);
	}

}; // class Pool

} // namespace frm
