#pragma once

#include <frm/core/frm.h>

#include <cstdarg> // va_list

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// StringBase
// Base for string class with an optional local buffer. If/when the local 
// buffer overflows it is replaced with a heap-allocated buffer. Once the 
// buffer is heap-allocated it never returns to using the local buffer.
// All const char* interfaces expect null-terminated strings.
////////////////////////////////////////////////////////////////////////////////
class StringBase
{
public:
	// Copy _count characters from _src. If _count == 0, all of _src is copied. If the end of 
	// _src is found  before _count characters have been copied, the result is padded with zeros 
	// until _count characters have been written (like strncpy). Unlike strncpy, an implicit null 
	// char is appended to the end of the result. 
	// Return the new length of the string (excluding the null terminator).
	uint set(const char* _src, uint _count = 0);
	// Set formatted content. Return the new length of the string (excluding the null terminator).
	uint setf(const char* _fmt, ...);
	uint setfv(const char* _fmt, va_list _args);

	// Append _count characters from _src. If _count == 0, all of _src is appended. If the end of 
	// _src is found  before _count characters have been appended, the result is padded with zeros 
	// until _count characters have been  written (like strncpy). Unlike strncpy, an implicit null 
	// char is appended to the end of the result. 
	// Return the new length of the string (excluding the null terminator).
	uint append(const char* _src, uint _count = 0);
	// Append formatted content. Return the new length of the string (excluding the null terminator).
	uint appendf(const char* _fmt, ...);
	uint appendfv(const char* _fmt, va_list _args);

	// Find the first (or last) occurence of any character in _list (null terminated). If no match is found return 0.
	const char* findFirst(const char* _list) const;
	const char* findLast(const char* _list) const;

	// Find the first occurence of the substring _str. If not found return 0.
	const char* find(const char* _str) const;

	// Replace all instances of _find with _replace. Return the number of instances replaced.
	uint replace(char _find, char _replace); // single char (faster, in-place)
	uint replace(const char* _find, const char* _replace); // substring
	uint replacef(const char* _find, const char* _fmt, ...);
	uint replacefv(const char* _find, const char* _fmt, va_list _args);

	// Replace all instances of _find with _replace, ignoring case. Return the number of instances replaced.
	uint replacei(char _find, char _replace); // single char (faster, in-place)
	uint replacei(const char* _find, const char* _replace); // substring
	uint replaceif(const char* _find, const char* _fmt, ...);
	uint replaceifv(const char* _find, const char* _fmt, va_list _args);

	// Convert to upper/lower case.
	void toUpperCase();
	void toLowerCase();

	// String length (excluding the null terminator).
	uint getLength() const                          { return m_length; }
	void setLength(uint _length);

	void clear()                                    { if (m_buf) { *m_buf = '\0'; } m_length = 0; }
	bool isEmpty() const                            { return m_length == 0; }
	bool isLocal() const                            { return m_buf == getLocalBuf(); }
	uint getCapacity() const                        { return m_capacity; }
	void setCapacity(uint _capacity);

	bool  operator==(const char* _rhs) const;
	bool  operator!=(const char* _rhs) const        { return !(*this == _rhs); }
	bool  operator==(const StringBase& _rhs) const  { return this->operator==((const char*)_rhs); }
	bool  operator!=(const StringBase& _rhs) const  { return !(*this == _rhs); }
	bool  operator<(const char* _rhs) const;
	bool  operator<(const StringBase& _rhs) const   { return this->operator<((const char*)_rhs); }
	bool  operator>(const char* _rhs) const;
	bool  operator>(const StringBase& _rhs) const   { return this->operator>((const char*)_rhs); }
	char& operator[](int _i)                        { return m_buf[_i]; }
	char  operator[](int _i) const                  { return m_buf[_i]; }

	// Cast to char*/const char* is explicit to avoid conflicts with the operator overloads above.
	explicit operator const char*() const           { return m_buf; }
	explicit operator char*()                       { return m_buf; }
	const char* c_str() const                       { return m_buf; }
	const char* begin() const                       { return m_buf; }
	const char* end() const                         { return begin() + m_length; }
	
	friend void swap(StringBase& _a_, StringBase& _b_);

protected:
	// String always heap-allocated.
	StringBase();
	// String has a local buffer of _localBufferSize chars.
	StringBase(uint _localBufferSize);
	// Move ctor. If _rhs_ is local it *must* have the same capacity as this (because the local buffer 
	// capacity isn't stored). This is enforced by the deriving String class move ctors.
	StringBase(StringBase&& _rhs_);
	// Move assignment. As move ctor.
	StringBase& operator=(StringBase&& _rhs_);

	~StringBase();

	char* getLocalBuf()             { return (char*)this + sizeof(StringBase); }
	const char* getLocalBuf() const { return (char*)this + sizeof(StringBase); }

private:
	char* m_buf;       // Ptr to local buffer, or heap allocated.
	uint  m_length;    // String length (excluding the null terminator).
	uint  m_capacity;  // Local buffer/allocated size.

	// Resize m_buf to _capacity, discard contents.
	void alloc(uint _capacity);

	// Resize m_buf to _capaicty, maintain contents.
	void realloc(uint _capacity);

};

void swap(StringBase& _a_, StringBase& _b_);


template <uint kCapacity>
class String: public StringBase
{
	char m_localBuf[kCapacity];

public:
	String():                              StringBase(kCapacity)           {}
	String(const String<kCapacity>& _rhs): StringBase(kCapacity)           { set((const char*)_rhs); }
	String<kCapacity>& operator=(const String<kCapacity>& _rhs)            { if (&_rhs != this) set((const char*)_rhs); return *this; }
	String(String<kCapacity>&& _rhs):      StringBase((StringBase&&)_rhs)  {}
	String<kCapacity>& operator=(String<kCapacity>&& _rhs)                 { StringBase::operator=((StringBase&&)_rhs); return *this; }
	String(const char* _fmt, ...):         StringBase(kCapacity)
	{
		if (_fmt) {
			va_list args;
			va_start(args, _fmt);
			setfv(_fmt, args);
			va_end(args);
		}
	}
};

template <>
class String<0>: public StringBase
{
public:
	String():                      StringBase()                   {}
	String(const String<0>& _rhs): StringBase()                   { set((const char*)_rhs); }
	String<0>& operator=(const String<0>& _rhs)                   { if (&_rhs != this) set((const char*)_rhs); return *this; }
	String(String<0>&& _rhs):      StringBase((StringBase&&)_rhs) {}
	String<0>& operator=(String<0>&& _rhs)                        { StringBase::operator=((StringBase&&)_rhs); return *this; }
	String(const char* _fmt, ...): StringBase()
	{
		if (_fmt) {
			va_list args;
			va_start(args, _fmt);
			setfv(_fmt, args);
			va_end(args);
		}
	}
};

} // namespace frm
