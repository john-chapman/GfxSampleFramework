#include "Base64.h"

#include <frm/core/frm.h>

static const char kBase64Alphabet[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/"
	;

static inline void Base64A3ToA4(const unsigned char* _a3, unsigned char* a4_) 
{
	a4_[0] = (_a3[0] & 0xfc) >> 2;
	a4_[1] = ((_a3[0] & 0x03) << 4) + ((_a3[1] & 0xf0) >> 4);
	a4_[2] = ((_a3[1] & 0x0f) << 2) + ((_a3[2] & 0xc0) >> 6);
	a4_[3] = (_a3[2] & 0x3f);
}

static inline void Base64A4ToA3(const unsigned char* _a4, unsigned char* a3_)
{
	a3_[0] = (_a4[0] << 2) + ((_a4[1] & 0x30) >> 4);
	a3_[1] = ((_a4[1] & 0xf) << 4) + ((_a4[2] & 0x3c) >> 2);
	a3_[2] = ((_a4[2] & 0x3) << 6) + _a4[3];
}

static inline unsigned char Base64Index(char _c)
{
	if (_c >= 'A' && _c <= 'Z') return _c - 'A';
	if (_c >= 'a' && _c <= 'z') return _c - 71;
	if (_c >= '0' && _c <= '9') return _c + 4;
	if (_c == '+')              return 62;
	if (_c == '/')              return 63;
	return -1;
}

namespace frm {

size_t Base64GetDecodedSizeBytes(char* _b64, size_t _b64SizeBytes) 
{
	size_t padCount = 0;
	for (size_t i = _b64SizeBytes - 1; _b64[i] == '='; i--)
	{
		padCount++;
	}
	return ((6 * _b64SizeBytes) / 8) - padCount;
}


void Base64Encode(const char* _in, size_t _inSizeBytes, char* out_, size_t outSizeBytes_)
{
	size_t i = 0;
	size_t j = 0;
	size_t k = 0;
	unsigned char a3[3];
	unsigned char a4[4];
	while (_inSizeBytes--)
	{
		a3[i++] = *(_in++);
		if (i == 3)
		{
			Base64A3ToA4(a3, a4);
			for (i = 0; i < 4; i++)
			{
				out_[k++] = kBase64Alphabet[a4[i]];
			}
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
		{
			a3[j] = '\0';
		}

		Base64A3ToA4(a3, a4);

		for (j = 0; j < i + 1; j++)
		{
			out_[k++] = kBase64Alphabet[a4[j]];
		}

		while ((i++ < 3))
		{
			out_[k++] = '=';
		}
	}

	out_[k] = '\0';

	FRM_ASSERT(outSizeBytes_ == k); // overflow
}

void Base64Decode(const char* _in, size_t _inSizeBytes, char* out_, size_t outSizeBytes_)
{
	size_t i = 0;
	size_t j = 0;
	size_t k = 0;
	unsigned char a3[3];
	unsigned char a4[4];

	while (_inSizeBytes--)
	{
		if (*_in == '=')
		{
			break;
		}

		a4[i++] = *(_in++);

		if (i == 4)
		{
			for (i = 0; i < 4; i++)
			{
				a4[i] = Base64Index(a4[i]);
			}

			Base64A4ToA3(a4, a3);

			for (i = 0; i < 3; i++)
			{
				out_[k++] = a3[i];
			}

			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 4; j++)
		{
			a4[j] = '\0';
		}

		for (j = 0; j < 4; j++)
		{
			a4[j] = Base64Index(a4[j]);
		}

		Base64A4ToA3(a4, a3);

		for (j = 0; j < i - 1; j++)
		{
			out_[k++] = a3[j];
		}
	}

	FRM_ASSERT(outSizeBytes_ == k); // overflow
}

} // namespace frm
