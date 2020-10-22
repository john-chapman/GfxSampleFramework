#pragma once

namespace frm {

constexpr size_t Base64GetEncodedSizeBytes(size_t _sizeBytes) 
{
	size_t n = _sizeBytes;
	return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

size_t Base64GetDecodedSizeBytes(char* _b64, size_t _b64SizeBytes);

void Base64Encode(const char* _in, size_t _inSizeBytes, char* out_, size_t outSizeBytes_);
void Base64Decode(const char* _in, size_t _inSizeBytes, char* out_, size_t outSizeBytes_);

} // namespace frm
