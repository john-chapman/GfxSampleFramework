#pragma once

#include <frm/core/frm.h>

#include <cstring>

#define FRM_MALLOC(size)                        (frm::internal::malloc(size))
#define FRM_REALLOC(ptr, size)                  (frm::internal::realloc(ptr, size))
#define FRM_FREE(ptr)                           (frm::internal::free(ptr))
#define FRM_MALLOC_ALIGNED(size, align)         (frm::internal::malloc_aligned(size, align))
#define FRM_REALLOC_ALIGNED(ptr, size, align)   (frm::internal::realloc_aligned(ptr, size, align))
#define FRM_FREE_ALIGNED(ptr)                   (frm::internal::free_aligned(ptr))
#define FRM_NEW(type)                           (new type)
#define FRM_NEW_ARRAY(type, count)              (new type[count])
#define FRM_DELETE(ptr)                         (delete ptr)
#define FRM_DELETE_ARRAY(ptr)                   (delete[] ptr)

namespace frm { namespace internal {

void* malloc(size_t _size);
void* realloc(void* _ptr, size_t _size);
void  free(void* _ptr);
void* malloc_aligned(size_t _size, size_t _align);
void* realloc_aligned(void* _ptr, size_t _size, size_t _align);
void  free_aligned(void* _ptr);

template <size_t kAlignment> struct aligned_base;
	template<> struct alignas(1)   aligned_base<1>   {};
	template<> struct alignas(2)   aligned_base<2>   {};
	template<> struct alignas(4)   aligned_base<4>   {};
	template<> struct alignas(8)   aligned_base<8>   {};
	template<> struct alignas(16)  aligned_base<16>  {};
	template<> struct alignas(32)  aligned_base<32>  {};
	template<> struct alignas(64)  aligned_base<64>  {};
	template<> struct alignas(128) aligned_base<128> {};

} } // namespace frm::internal

namespace frm {

// Call tType() on elements in [from, to[.
template <typename tType>
inline void Construct(tType* from, const tType* to)
{
	while (from < to)
	{
		new(from) tType();
		++from;
	}
}

// Call ~tType() on elements in [from, to[.
template <typename tType>
inline void Destruct(tType* from, const tType* to)
{
	while (from < to)
	{
		from->~tType();
		++from;
	}
}

////////////////////////////////////////////////////////////////////////////////
// aligned
// Mixin class, provides template-based memory alignment for deriving classes.
// Use cautiously, especially with multiple inheritance.
// Usage:
//
//    class Foo: public aligned<Foo, 16>
//    { // ...
//
// Alignment can only be increased. If the deriving class has a higher natural 
// alignment than kAlignment, the higher alignment is used.
////////////////////////////////////////////////////////////////////////////////
template <typename tType, size_t kAlignment>
struct aligned: private internal::aligned_base<kAlignment>
{
	aligned()                                           { FRM_STRICT_ASSERT((size_t)this % kAlignment == 0); }

	// malloc_aligned is called with alignof(tType) which will be the min of the natural alignment of tType and kAlignment
	void* operator new(size_t _size)                    { return FRM_MALLOC_ALIGNED(_size, alignof(tType)); }
	void  operator delete(void* _ptr)                   { FRM_FREE_ALIGNED(_ptr); }
	void* operator new[](size_t _size)                  { return FRM_MALLOC_ALIGNED(_size, alignof(tType)); }
	void  operator delete[](void* _ptr)                 { FRM_FREE_ALIGNED(_ptr); }
	void* operator new(size_t _size, void* _ptr)        { FRM_STRICT_ASSERT((size_t)_ptr % kAlignment == 0); return _ptr; }
	void  operator delete(void*, void*)                 { ; } // dummy, matches placement new

};

////////////////////////////////////////////////////////////////////////////////
// storage
// Provides aligned storage for kCount objects of type tType. Suitable for 
// allocating static blocks of uninitialized memory for use with placement
// new.
////////////////////////////////////////////////////////////////////////////////
template <typename tType, size_t kCount = 1>
class storage: private aligned< storage<tType, kCount>, alignof(tType) >
{
	char m_buf[sizeof(tType) * kCount];
public:
	storage(): aligned< storage<tType, kCount>, alignof(tType) >() {}

	operator       tType*()                                        { return (tType*)m_buf; }
	operator const tType*() const                                  { return (tType*)m_buf; }

	tType*         operator->()                                    { return (tType*)m_buf; }
	const tType*   operator->() const                              { return (tType*)m_buf; }
};

} // namespace frm
