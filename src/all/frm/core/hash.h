#pragma once

#include <frm/core/frm.h>

namespace frm { namespace internal {

constexpr uint32 kFnv1aBase32 = 0x811C9DC5u;
constexpr uint64 kFnv1aBase64 = 0xCBF29CE484222325ull;

uint16 Hash16(const uint8* _buf, uint _bufSize);
uint16 Hash16(const uint8* _buf, uint _bufSize, uint16 _base);
uint32 Hash32(const uint8* _buf, uint _bufSize, uint32 _base = kFnv1aBase32);
uint64 Hash64(const uint8* _buf, uint _bufSize, uint64 _base = kFnv1aBase64);

uint16 HashString16(const char* _str);
uint16 HashString16(const char* _str, uint16 _base);
uint32 HashString32(const char* _str, uint32 _base = kFnv1aBase32);
uint64 HashString64(const char* _str, uint64 _base = kFnv1aBase64);

} } // namespace frm::internal


namespace frm {

// Hash _bufSize bytes from _buf. _base is used to initialize the result.
// tType = uint16, uint32, uint64
template <typename tType>
tType Hash(const void* _buf, uint _bufSize, tType _base);
	template <> inline uint16 Hash<uint16>(const void* _buf, uint _bufSize, uint16 _base) { return internal::Hash16((const uint8*)_buf, _bufSize, _base); }
	template <> inline uint32 Hash<uint32>(const void* _buf, uint _bufSize, uint32 _base) { return internal::Hash32((const uint8*)_buf, _bufSize, _base); }
	template <> inline uint64 Hash<uint64>(const void* _buf, uint _bufSize, uint64 _base) { return internal::Hash64((const uint8*)_buf, _bufSize, _base); }
// Hash _bufSize bytes from _buf.
// tType = uint16, uint32, uint64
template <typename tType>
tType Hash(const void* _buf, uint _bufSize);
	template <> inline uint16 Hash<uint16>(const void* _buf, uint _bufSize) { return internal::Hash16((const uint8*)_buf, _bufSize); }
	template <> inline uint32 Hash<uint32>(const void* _buf, uint _bufSize) { return internal::Hash32((const uint8*)_buf, _bufSize); }
	template <> inline uint64 Hash<uint64>(const void* _buf, uint _bufSize) { return internal::Hash64((const uint8*)_buf, _bufSize); }

// Hash a null-terminted string. _base is used to initialize the result.
// tType = uint16, uint32, uint64
template <typename tType>
tType HashString(const char* _str, tType _base);
	template <> inline uint16 HashString<uint16>(const char* _str, uint16 _base) { return internal::HashString16(_str, _base); }
	template <> inline uint32 HashString<uint32>(const char* _str, uint32 _base) { return internal::HashString32(_str, _base); }
	template <> inline uint64 HashString<uint64>(const char* _str, uint64 _base) { return internal::HashString64(_str, _base); }
// Hash a null-terminated string.
// tType = uint16, uint32, uint64
template <typename tType>
tType HashString(const char* _str);
	template <> inline uint16 HashString<uint16>(const char* _str) { return internal::HashString16(_str); }
	template <> inline uint32 HashString<uint32>(const char* _str) { return internal::HashString32(_str); }
	template <> inline uint64 HashString<uint64>(const char* _str) { return internal::HashString64(_str); }

} // namespace frm
