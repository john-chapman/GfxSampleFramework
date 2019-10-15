#pragma once

#include <frm/core/frm.h>
#include <frm/core/memory.h>

#include <algorithm>   // std::copy
#include <new>         // placement new
#include <type_traits> // std::conditional
#include <utility>     // std::move

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class PersistentVector
/// A growable array like Vector, however unlike Vector the storage is
/// persistent; elements are never moved or copied. Hence a ptr to an element
/// in a PersistentVector remains valid until the element is removed or the
/// PersistentVector is destroyed. This imposes some limitations on the
/// operations which can be performed: the container is non-movable, elements 
/// may only be added/removed at the back of the container.
/// \note Iterating over elements in the container by index is significantly
///    slower than using iterators in optimized builds.
/// \todo Move some of the larger private functions to a .cpp (use a privately
///    inherited base class).
////////////////////////////////////////////////////////////////////////////////
template <typename tType>
class PersistentVector
{
public:
	static const uint kDefaultBlockSize = 64;

	typedef tType value_type;
	typedef uint  size_type;
	
	template<bool is_const> class iterator_base;
	typedef iterator_base<false>  iterator;
	typedef iterator_base<true>   const_iterator;

	/// \param blockSize The number of elements by which the container will
	///    grow when push_back is called and the container is full.
	PersistentVector(uint _blockSize = kDefaultBlockSize);

	/// Initialize n elements by copy constructing from v.
	/// \param n Number of elements to initialize from v.
	/// \param v Value from which to copy construct elements.
	/// \param blockSize The number of elements by which the container will
	///   grow when push_back is called and the container is full.
	PersistentVector(uint _n, const tType& _v, uint _blockSize = kDefaultBlockSize);

	/// Initialize from elements in [first,last) (last is excluded).
	/// \param blockSize The number of elements by which the container will
	///   grow when push_back is called and the container is full.
	template <typename tIterator>
	PersistentVector(tIterator _first, tIterator _last, uint _blockSize = kDefaultBlockSize);

	/// Copy ctor. Elements are copy-constructed from _rhs.
	explicit PersistentVector(const PersistentVector<tType>& _rhs);

	/// Move copy/assign, swap.
	PersistentVector(PersistentVector<tType>&& _rhs);
	PersistentVector<tType>& operator=(PersistentVector<tType>&& _rhs);
	template <typename tType>
	friend void swap(PersistentVector<tType>& _a, PersistentVector<tType>& _b);

	/// Empty the container (elements are destructed).
	~PersistentVector();


	/// Add a new element at the end of the container. v is copied (or moved to the new element).
	void push_back(const tType& _v);
	void push_back(tType&& _v);
	
	/// Remove the last element from the end of the container. The element's dtor is called.
	void pop_back();

	/// Reserve enough space in the container to hold at least n elements.
	void reserve(uint _n) { while(capacity() < _n) allocBlock(); }
	
	/// Remove all elements from the container. Element dtors are called.
	void clear();
	
	/// Resize the container so that it contains n elements. If n is smaller
	/// than the current size, elements are removed from the container (and
	/// destructed). If n is greater than the current size, additional
	/// elements are added and default constructed.
	void resize(uint _n);

	/// Resize the container so that it contains n elements. If n is smaller
	/// than the current size, elements are removed from the container (and
	/// destructed). If n is greater than the current size, additional 
	/// elements are added and copy constructed from v.
	void resize(uint _n, const tType& _v);


	uint           size() const              { return m_size; }
	uint           capacity() const          { return m_blockCount * m_blockSize; }
	
	bool           empty() const             { return m_size == 0; }
	bool           full() const              { return m_size == capacity(); }
	
	tType&         front()                   { return (*this)[0]; }
	const tType&   front() const             { return (*this)[0]; }
	
	tType&         back()                    { return (*this)[size() - 1]; }
	const tType&   back() const              { return (*this)[size() - 1]; }

	iterator       begin()                   { return iterator(this, 0, 0); }
	const_iterator begin() const             { return const_iterator(this, 0, 0); }
	iterator       end()                     { return iterator(this, m_back, m_backSize); }
	const_iterator end() const               { return const_iterator(this, m_back, m_backSize); }


	tType&         operator[](uint _i)       { return getElement(_i); }
	const tType&   operator[](uint _i) const { return getElement(_i); }

private:

	tType** m_blocks;
	uint    m_blockCount; ///< Number of allocated blocks
	uint    m_blockSize;  ///< Elements per block
	uint    m_size;       ///< Number of stored elements
	uint    m_back;       ///< Index of back block
	uint    m_backSize;   ///< Size of back block


	/// \return reference to the ith element;
	tType& getElement(uint _i) const;

	/// Allocate a new element (common code for push_back variants).
	void allocElement();

	/// Allocate a new block (increase capacity by m_blockSize).
	void allocBlock();


}; // class PersistentVector


/*******************************************************************************

                              PersistentVector

*******************************************************************************/

//	PUBLIC

template <typename tType>
inline PersistentVector<tType>::PersistentVector(uint _blockSize)
	: m_blocks(0)
	, m_blockCount(0)
	, m_size(0)
	, m_blockSize(_blockSize)
	, m_back(0)
	, m_backSize(0)
{
	// Can avoid allocBlock() here if you set m_backsSize = m_blockSize (so that
	// the first allocation happens during the first call to push_back). HOWEVER 
	// this requires end() to be more complex as it must explicitly detect an 
	// empty container.
	allocBlock();
}

template <typename tType>
inline PersistentVector<tType>::PersistentVector(uint _n, const tType& _v, uint _blockSize)
	: m_blocks(0)
	, m_blockCount(0)
	, m_size(0)
	, m_blockSize(_blockSize)
	, m_back(0)
	, m_backSize(0)
{
	reserve(_n);
	while (_n) {
		push_back(_v);
		--_n;
	}
}

template <typename tType>
template <typename tIterator>
inline PersistentVector<tType>::PersistentVector(tIterator _first, tIterator _last, uint _blockSize)
	: m_blocks(0)
	, m_blockCount(0)
	, m_size(0)
	, m_blockSize(_blockSize)
	, m_back(0)
	, m_backSize(0)
{
	FRM_ASSERT(_last > _first);
	reserve(_last - _first);
	while (_first != _last) {
		push_back(*_first);
		++_first;
	}
}

template <typename tType>
inline PersistentVector<tType>::PersistentVector(const PersistentVector<tType>& _rhs)
	: m_blockCount(_rhs.m_blockCount)
	, m_blockSize(_rhs.m_blockSize)
	, m_size(_rhs.m_size)
	, m_back(_rhs.m_back)
	, m_backSize(_rhs.m_backSize)
{
	m_blocks = new tType*[m_blockCount];
	for (uint i = 0; i < m_blockCount; ++i) {
		m_blocks[i] = (tType*)frm::malloc_aligned(sizeof(tType) * m_blockSize, FRM_ALIGNOF(tType));
		std::copy(_rhs.m_blocks[i], _rhs.m_blocks[i] + m_blockSize, m_blocks[i]);
	}
}

template <typename tType>
inline PersistentVector<tType>::PersistentVector(PersistentVector<tType>&& _rhs)
	: m_blocks(0)
	, m_blockCount(0)
	, m_size(0)
	, m_blockSize(0)
	, m_back(0)
	, m_backSize(0)
{
	swap(*this, _rhs);
}
template <typename tType>
inline PersistentVector<tType>& PersistentVector<tType>::operator=(PersistentVector<tType>&& _rhs)
{
	swap(*this, _rhs);
	return *this;
}
template <typename tType>
inline void swap(PersistentVector<tType>& _a, PersistentVector<tType>& _b)
{
	using std::swap;
	swap(_a.m_blocks, _b.m_blocks);
	swap(_a.m_blockCount, _b.m_blockCount);
	swap(_a.m_size, _b.m_size);
	swap(_a.m_blockSize, _b.m_blockSize);
	swap(_a.m_back, _b.m_back);
	swap(_a.m_backSize, _b.m_backSize);
}

template <typename tType>
inline PersistentVector<tType>::~PersistentVector()
{
	clear(); // call dtors on elements
	for (uint i = 0; i < m_blockCount; ++i) {
		frm::free_aligned(m_blocks[i]);
	}
	delete[] m_blocks;
}

template <typename tType>
inline void PersistentVector<tType>::push_back(const tType& _v)
{
	allocElement();
	new(&m_blocks[m_back][m_backSize]) tType(_v);
	++m_backSize;
	++m_size;
}

template <typename tType>
inline void PersistentVector<tType>::push_back(tType&& _v)
{
	allocElement();
	new(&m_blocks[m_back][m_backSize]) tType(std::move(_v));
	++m_backSize;
	++m_size;
}

template <typename tType>
inline void PersistentVector<tType>::pop_back()
{
	FRM_ASSERT(m_size != 0);
	if (m_size == 0) {
		return;
	}

	back().~tType();
	--m_size;
	--m_backSize;
	if (m_backSize == 0 && m_back != 0) {
		--m_back;
		if (m_backSize == 0) {
			m_backSize = m_blockSize;
		}
	}
}

template <typename tType>
inline void PersistentVector<tType>::clear()
{
	if (m_size == 0) {
		return;
	}
#ifndef FRM_DEBUG
 // in release, the operator[] can be very well optimized
	for (uint i = 0; i < m_size; ++i) {
		(*this)[i].~tType();
	}
#else
 // in debug, using a double loop is faster
	for (uint i = 0, j = 0; i < m_blockCount; ++i) {
		for (uint k = 0; k < m_blockSize && j < m_size; ++k, ++j) {
			m_blocks[i][k].~tType();
		}
	}
#endif
	m_size     = 0;
	m_back     = 0;
	m_backSize = 0;
}

template <typename tType>
inline void PersistentVector<tType>::resize(uint _n)
{
	uint s = size();
	for (uint i = s; i < _n; ++i) {
		push_back(tType());
	}
	for (uint i = _n; i < s; ++i) {
		pop_back();
	}
}

template <typename tType>
inline void PersistentVector<tType>::resize(uint _n, const tType& _v)
{
	uint s = size();
	for (uint i = s; i < _n; ++i) {
		push_back(_v);
	}
	for (uint i = _n; i < s; ++i) {
		pop_back();
	}
}


//	PRIVATE

template <typename tType>
inline tType& PersistentVector<tType>::getElement(uint _i) const
{
	FRM_ASSERT(_i < size());
	return m_blocks[_i / m_blockSize][_i % m_blockSize];
}

template <typename tType>
inline void PersistentVector<tType>::allocElement()
{
	if (m_backSize == m_blockSize) {
		if (m_blockCount != 0) {
			++m_back;
		}
		m_backSize = 0;
		if (m_back == m_blockCount) {
			allocBlock();
		}
	}
}

template <typename tType>
inline void PersistentVector<tType>::allocBlock()
{
	tType** tmp = new tType*[m_blockCount + 1];
	if (m_blocks) {
		std::copy(m_blocks, m_blocks + m_blockCount, tmp);
		delete[] m_blocks;
	}
	m_blocks = tmp;
	
	m_blocks[m_blockCount] = (tType*)frm::malloc_aligned(sizeof(tType) * m_blockSize, FRM_ALIGNOF(tType));
	++m_blockCount;
}


/*******************************************************************************

                      PersistentVector::iterator_base

*******************************************************************************/

template <typename tType>
template <bool is_const>
class PersistentVector<tType>::iterator_base
{
	friend class PersistentVector<tType>;
public:
	typedef typename std::conditional<is_const, const tType, tType>::type value_type;
	typedef sint         difference_type;
	typedef value_type*  pointer;
	typedef value_type&  reference;
	typedef std::random_access_iterator_tag iterator_category;

	iterator_base(const iterator_base& _rhs)
		: m_parent(_rhs.m_parent)
		, m_block(_rhs.m_block)
		, m_offset(_rhs.m_offset)
	{
	}

	iterator_base& operator+=(sint _rhs)
	{
		if (_rhs >= 0) {
			m_offset += (uint)_rhs;
			m_block  += m_offset / m_parent->m_blockSize;
			m_offset  = m_offset % m_parent->m_blockSize;
		} else {
			m_block -= (uint)-_rhs / m_parent->m_blockSize;
			uint off = (uint)-_rhs % m_parent->m_blockSize;
			if (m_offset || !off) {
				m_offset -= off;
			} else {
				--m_block;
				m_offset = m_parent->m_blockSize - off;
			}
		}
		return *this;
	}
	iterator_base operator+(sint _rhs)
	{
		iterator_base ret = *this;
		ret += _rhs;
		return ret;
	}

	iterator_base operator-=(sint _rhs)
	{
		return operator+=(-_rhs);
	}
	iterator_base operator-(sint _rhs)
	{
		iterator_base ret = *this;
		ret -= _rhs;
		return ret;
	}

	sint operator-(iterator_base _rhs)
	{
		sint ret = ((sint)m_block - (sint)_rhs.m_block) * (sint)m_parent->m_blockSize;
		ret += (sint)m_offset - (sint)_rhs.m_offset;
		return ret;
	}

	iterator_base& operator++()
	{
		FRM_ASSERT(*this != m_parent->end()); // can't increment the end iterator
		++m_offset;
		if (m_offset == m_parent->m_blockSize) {
			++m_block;
			m_offset = 0;
		}
		return *this;
	}
	iterator_base operator++(int)
	{
		iterator_base ret = *this;
		operator++();
		return ret;
	}

	iterator_base& operator--()
	{
		FRM_ASSERT(*this != m_parent->begin()); // can't decrement the begin iterator
		if (m_offset == 0) {
			--m_block;
			m_offset = m_parent->m_blockSize;
		}
		--m_offset;
		return *this;
	}
	iterator_base operator--(int)
	{
		iterator_base ret = *this;
		operator--();
		return ret;
	}

	bool operator==(const iterator_base& _rhs)
	{
		return m_block == _rhs.m_block && m_offset == _rhs.m_offset;
	}
	bool operator!=(const iterator_base& _rhs)
	{
		return !(*this == _rhs);
	}

	bool operator> (const iterator_base& _rhs)
	{
		return m_block >  _rhs.m_block || (m_block == _rhs.m_block && m_offset >  _rhs.m_offset);
	}
	bool operator>=(const iterator_base& _rhs) { return !(*this < _rhs); }


	bool operator< (const iterator_base& _rhs)
	{
		return m_block <  _rhs.m_block || (m_block == _rhs.m_block && m_offset <  _rhs.m_offset);
	}
	bool operator<=(const iterator_base& _rhs) { return !(*this > _rhs); }


	value_type& operator*() const 
	{
		FRM_ASSERT(m_block < m_parent->m_blockCount && m_offset < m_parent->m_blockSize);
		return m_parent->m_blocks[m_block][m_offset]; 
	}

	value_type& operator[](uint i) const
	{
		return *(*this + i);
	}

private:
	typedef PersistentVector<tType> PersistentVector;
	typedef typename std::conditional<is_const, const PersistentVector, PersistentVector>::type parent_type;
	parent_type* m_parent; // access to constraints

	uint m_block;          // index to current block
	uint m_offset;         // offset into current block

	iterator_base(parent_type* _parent, uint _block, uint _offset)
		: m_parent(_parent)
		, m_block(_block)
		, m_offset(_offset)
	{
		if (m_offset == m_parent->m_blockSize) {
			++m_block;
			m_offset = 0;
		}
	}

}; // class iterator_base
	

} // namespace frm
