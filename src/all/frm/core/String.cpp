#include "String.h"

#include <frm/core/memory.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifndef va_copy
	#define va_copy(_dst, _src) (_dst = _src)
#endif

static char* _stristr(const char* _str1, const char* _str2)
{
	const char* p1 = _str1;
	const char* p2 = _str2;
	const char* r = *p2 == 0 ? _str1 : 0;

	while (*p1 != 0 && *p2 != 0)
	{
		if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2))
		{
			if (r == 0)
			{
				r = p1;
			}
			p2++;
		}
		else
		{
			p2 = _str2;
			if(r != 0)
			{
				p1 = r + 1;
			}

			if (tolower((unsigned char)*p1 ) == tolower((unsigned char)*p2 ))
			{
				r = p1;
				p2++;
			}
			else
			{
				r = 0;
			}
		}
		p1++;
	}

	return *p2 == 0 ? (char*)r : 0;
}

using namespace frm;

// PUBLIC

uint StringBase::set(const char* _src, uint _count)
{
	if (!_src) {
		return m_length;
	}
	uint srclen = _count;
	if (srclen == 0) {
		srclen = strlen(_src);
	}
	if (m_capacity < srclen + 1) {
		alloc(srclen + 1);
	}
	memcpy(m_buf, _src, srclen);
	m_buf[srclen] = '\0';
	m_length = srclen;
	return srclen;
}

uint StringBase::setf(const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	uint ret = setfv(_fmt, args);
	va_end(args);
	return ret;
}

uint StringBase::setfv(const char* _fmt, va_list _args)
{
	va_list args;
	va_copy(args, _args);

#ifdef FRM_COMPILER_MSVC
 // vsnprintf returns -1 on overflow, requires 2 passes
	int len = vsnprintf(0, 0, _fmt, args);
	FRM_STRICT_ASSERT(len >= 0);
	if (m_capacity < (uint)len + 1) {
		alloc(len + 1);
	}
	FRM_VERIFY(vsnprintf(m_buf, m_capacity, _fmt, args) >= 0);
#else
	int len = vsnprintf(m_buf, m_capacity, _fmt, args);
	FRM_STRICT_ASSERT(len >= 0);
	if (m_capacity < len + 1) {
		alloc(len + 1);
		FRM_VERIFY(vsnprintf(m_buf, m_capacity, _fmt, args) >= 0);
	}
#endif
	m_length = (uint)len;
	return m_length;
}

uint StringBase::append(const char* _src, uint _count)
{
	if (!_src) {
		return m_length;
	}
	uint srclen = _count;
	if (srclen == 0) {
		srclen = strlen(_src);
	}
	uint len = getLength();
	if (m_capacity < len + srclen + 1) {
		realloc(len + srclen + 1);
	}
	memcpy(m_buf + len, _src, srclen);
	len += srclen;
	m_buf[len] = '\0';
	m_length = len;
	return len;
}

uint StringBase::appendf(const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	uint ret = appendfv(_fmt, args);
	va_end(args);
	return ret;
}

uint StringBase::appendfv(const char* _fmt, va_list _args)
{
	va_list args;
	va_copy(args, _args);

	uint len = getLength();
	int srclen = vsnprintf(0, 0, _fmt, args);
	FRM_ASSERT(srclen > 0);
	if (m_capacity < len + srclen + 1) {
		realloc(len + srclen + 1);
	}
	FRM_VERIFY(vsnprintf(m_buf + len, m_capacity, _fmt, args) >= 0);
	m_length = (uint)srclen + len;
	return m_length;
}

const char* StringBase::findFirst(const char* _list) const
{
	return strpbrk(m_buf, _list);
}
const char* StringBase::findLast(const char* _list) const
{
#if 0
 // rather than call strlen (which has to search the whole string anyway) we just make several calls to strpbrk
	const char* ret = 0;
	const char* tmp = m_buf;
	do {
		tmp = strpbrk(tmp + 1, _list);
		ret = tmp == 0 ? ret : tmp;
	} while (tmp != 0);
	return ret;
#else
	uint i, j;
	for (i = 0, j = m_length - 1; i < m_length; ++i, --j) {
		const char* s = _list;
		while (*s) {
			if (*s == m_buf[j]) {
				return &m_buf[j];
			}
			++s;
		}
	}
	return nullptr;
#endif
}

const char* StringBase::find(const char* _str) const
{
	return strstr(m_buf, _str);
}

uint StringBase::replace(char _find, char _replace)
{
	char* tmp = m_buf;
	uint ret = 0;
	for (; *tmp != '\0'; ++tmp) {
		if (*tmp == _find) {
			*tmp = _replace;
			++ret;
		}
	}
	return ret;
}

uint StringBase::replace(const char* _find, const char* _replace)
{
	String<256> tmp;
	const uint findlen = strlen(_find);
	char* beg = m_buf;
	char* end = nullptr;
	uint ret = 0;
	while (end = strstr(beg, _find)) {
		if (end - beg > 0) {
			tmp.append(beg, end - beg);
		}
		tmp.append(_replace);
		beg = end + findlen;
		++ret;
		if (*beg == 0) {
			break;
		}
	}
	tmp.append(beg);
	if (ret) {
		swap(*this, tmp);
	}
	return ret;
}

uint StringBase::replacef(const char* _find, const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	uint ret = replacefv(_find, _fmt, args);
	va_end(args);
	return ret;
}

uint StringBase::replacefv(const char* _find, const char* _fmt, va_list _args)
{
	String<64> tmp;
	tmp.setfv(_fmt, _args);
	return replace(_find, (const char*)tmp);
}

uint StringBase::replacei(char _find, char _replace)
{
	_find = tolower(_find);
	char* tmp = m_buf;
	uint ret = 0;
	for (; *tmp != '\0'; ++tmp) {
		if (tolower(*tmp) == _find) {
			*tmp = _replace;
			++ret;
		}
	}
	return ret;
}

uint StringBase::replacei(const char* _find, const char* _replace)
{
	String<256> tmp;
	const uint findlen = strlen(_find);
	char* beg = m_buf;
	char* end = nullptr;
	uint ret = 0;
	while (end = _stristr(beg, _find)) {
		if (end - beg > 0) {
			tmp.append(beg, end - beg);
		}
		tmp.append(_replace);
		beg = end + findlen;
		++ret;
		if (*beg == 0) {
			break;
		}
	}
	tmp.append(beg);
	if (ret) {
		swap(*this, tmp);
	}
	return ret;
}

uint StringBase::replaceif(const char* _find, const char* _fmt, ...)
{
	va_list args;
	va_start(args, _fmt);
	uint ret = replaceifv(_find, _fmt, args);
	va_end(args);
	return ret;
}

uint StringBase::replaceifv(const char* _find, const char* _fmt, va_list _args)
{
	String<64> tmp;
	tmp.setfv(_fmt, _args);
	return replacei(_find, (const char*)tmp);
}

void StringBase::toLowerCase()
{
	char* tmp = m_buf;
	for (; *tmp != '\0'; ++tmp) {
		*tmp = (char)tolower((int)*tmp);
	}
}

void StringBase::toUpperCase()
{
	char* tmp = m_buf;
	for (; *tmp != '\0'; ++tmp) {
		*tmp = (char)toupper((int)*tmp);
	}
}

void StringBase::setLength(uint _length)
{
	if ((_length + 1) > m_capacity) {
		setCapacity(_length + 1);
	}
	m_length = _length;
	m_buf[_length] = '\0';
}
void StringBase::setCapacity(uint _capacity)
{
	FRM_ASSERT(_capacity >= m_capacity); // \todo allow reducing capacity? 
	if (m_capacity < _capacity) {
		realloc(_capacity);
	}
}

bool StringBase::operator==(const char* _rhs) const
{
	return strcmp(m_buf, _rhs) == 0;
}
bool StringBase::operator<(const char* _rhs) const
{
	return strcmp(m_buf, _rhs) < 0;
}
bool StringBase::operator>(const char* _rhs) const
{
	return strcmp(m_buf, _rhs) > 0;
}

void frm::swap(StringBase& _a_, StringBase& _b_)
{
	using std::swap;
	bool bothHeap  = !_a_.isLocal() && !_b_.isLocal();
	if (_a_.m_capacity == _b_.m_capacity) {
		if (bothHeap) {
		 // neither are local, simple swap
			swap(_a_.m_buf, _b_.m_buf);
		} else {
		 // swap the buffer contents
			for (uint i = 0; i < _a_.m_capacity; ++i) {
				swap(_a_.m_buf[i], _b_.m_buf[i]);
			}
		}
	} else {
		bool bothLocal = _a_.isLocal() && _b_.isLocal();

		if (bothHeap) {
		 // neither are local, simple swap
			swap(_a_.m_buf, _b_.m_buf);
			swap(_a_.m_capacity, _b_.m_capacity);

		} else if (bothLocal) {
		 // both are local, need to alloc the smallest one
			StringBase& smaller = _a_.m_capacity < _b_.m_capacity ? _a_ : _b_;
			StringBase& larger  = _a_.m_capacity < _b_.m_capacity ? _b_ : _a_;

			char* buf = (char*)FRM_MALLOC(larger.m_capacity * sizeof(char));
			memcpy(buf, larger.m_buf, larger.m_capacity * sizeof(char));
			memcpy(larger.m_buf, smaller.m_buf, smaller.m_capacity * sizeof(char));

			smaller.m_buf = buf;
			swap(_a_.m_capacity, _b_.m_capacity);

		} else {
		 // one is heap, alloc the other and swap (can't return to being local)
			StringBase& local = _a_.isLocal() ? _a_ : _b_;

			char* buf = (char*)FRM_MALLOC(local.m_capacity * sizeof(char));
			memcpy(buf, local.m_buf, local.m_capacity * sizeof(char));
			local.m_buf = buf;

			swap(_a_.m_buf, _b_.m_buf);
			swap(_a_.m_capacity, _b_.m_capacity);
		}
	}
	swap(_a_.m_length, _b_.m_length);
}

// PROTECTED

StringBase::StringBase()
	: m_buf(nullptr)
	, m_capacity(0)
	, m_length(0)
{
}

StringBase::StringBase(uint _localBufferSize)
	: m_buf(getLocalBuf())
	, m_capacity(_localBufferSize)
	, m_length(0)
{
	*m_buf = '\0';
}

StringBase::StringBase(StringBase&& _rhs_)
	: m_buf(getLocalBuf())
	, m_capacity(_rhs_.m_capacity)
	, m_length(_rhs_.m_length)
{
	if (_rhs_.isLocal()) {
	 // _rhs_ is local, copy contents (m_buf is guranteed to be large enough)
		memcpy(m_buf, _rhs_.m_buf, _rhs_.m_length + 1);
	} else {
	 // _rhs_ is not local, take ownership of it's heap-allocated buffer
		m_buf = _rhs_.m_buf;
	}

 // set _rhs_ to an invalid state; we can't use it's local buffer again because we cannot know the capacity
	_rhs_.m_buf      = nullptr;
	_rhs_.m_capacity = 0;
	_rhs_.m_length   = 0;
}
StringBase& StringBase::operator=(StringBase&& _rhs_)
{
	if (this == &_rhs_) {
		return *this;
	}

	if (_rhs_.isLocal()) {
	 // _rhs_ is local, copy contents (m_buf is guranteed to be large enough)
		if (!m_buf) {
			m_buf = getLocalBuf();
		}
		memcpy(m_buf, _rhs_.getLocalBuf(), _rhs_.m_length + 1);
	} else {	 
	 // _rhs_ is not local, take ownership of it's heap-allocated buffer
		if (!isLocal()) {
			FRM_FREE(m_buf);
		}
		m_buf = _rhs_.m_buf;
	}
	m_length   = _rhs_.m_length;
	m_capacity = _rhs_.m_capacity;

 // set _rhs_ to an invalid state; we can't use it's local buffer again because we cannot know the capacity
	_rhs_.m_buf      = nullptr;
	_rhs_.m_length   = 0;
	_rhs_.m_capacity = 0;

	return *this;
}

StringBase::~StringBase()
{
	if (m_buf && !isLocal()) {
		FRM_FREE(m_buf);
	}
}

// PRIVATE

void StringBase::alloc(uint _capacity)
{
	if (m_buf && !isLocal()) {
		FRM_FREE(m_buf);
	}
	m_buf = (char*)FRM_MALLOC(_capacity * sizeof(char));
	m_capacity = _capacity;
}

void StringBase::realloc(uint _capacity)
{
	if (!m_buf || isLocal()) {
		m_buf = (char*)FRM_MALLOC(_capacity * sizeof(char));
		memcpy(m_buf, getLocalBuf(), m_length + 1);
		m_capacity = _capacity;
	
	} else {
		m_buf = (char*)FRM_REALLOC(m_buf, _capacity * sizeof(char));
		m_capacity = _capacity;
	}
}
