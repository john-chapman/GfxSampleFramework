#include "StringHash.h"

#include <frm/core/hash.h>

using namespace frm;

const StringHash StringHash::kInvalidHash;

StringHash::StringHash(const char* _str)
	: m_hash(0)
{
	m_hash = HashString<HashType>(_str);
}

StringHash::StringHash(const char* _str, uint _len)
	: m_hash(0)
{
	m_hash = Hash<HashType>(_str, _len);
}
