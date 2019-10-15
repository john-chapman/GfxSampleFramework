#pragma once

#include <frm/core/frm.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// StringHash
// Fast, non-cryptographic hash generated from a character string.
////////////////////////////////////////////////////////////////////////////////
class StringHash
{
public:
	typedef uint64 HashType;
	static const StringHash kInvalidHash;

	// Default ctor, hash is invalid.
	StringHash(): m_hash(0)  {}

	// Initialize from a null-terminated string.
	StringHash(const char* _str);

	// Initialize from _len characters of _str.
	StringHash(const char* _str, uint _len);

	// May be kInvalidHash in the case of an uninitialized StringHash.
	HashType getHash() const { return m_hash; }

	operator HashType() const                      { return m_hash; }
	bool operator==(const StringHash& _rhs) const  { return m_hash == _rhs.m_hash; }
	bool operator!=(const StringHash& _rhs) const  { return m_hash != _rhs.m_hash; }
	bool operator> (const StringHash& _rhs) const  { return m_hash >  _rhs.m_hash; }
	bool operator>=(const StringHash& _rhs) const  { return m_hash >= _rhs.m_hash; }
	bool operator< (const StringHash& _rhs) const  { return m_hash <  _rhs.m_hash; }
	bool operator<=(const StringHash& _rhs) const  { return m_hash <= _rhs.m_hash; }

private:
	HashType m_hash;
};

inline bool operator==(StringHash::HashType _lhs, const StringHash& _rhs) { return _lhs == _rhs.getHash(); }
inline bool operator!=(StringHash::HashType _lhs, const StringHash& _rhs) { return _lhs != _rhs.getHash(); }

} // namespace frm
