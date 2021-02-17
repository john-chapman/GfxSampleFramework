#include <frm/core/compress.h>

#include <miniz/miniz.h>

using namespace frm;

void frm::Compress(const void* _in, uint _inSizeBytes, void*& out_, uint& outSizeBytes_, CompressionFlags _flags)
{
	FRM_ASSERT(_in);
	FRM_ASSERT(_inSizeBytes);
	FRM_ASSERT(!out_);
	FRM_ASSERT(_flags != CompressionFlags_None); // the calling code should skip calling Compress in this case

	int tdeflFlags = TDEFL_WRITE_ZLIB_HEADER;
	if (_flags & CompressionFlags_Speed)
	{
		tdeflFlags |= TDEFL_GREEDY_PARSING_FLAG;
	}
	out_ = tdefl_compress_mem_to_heap(_in, _inSizeBytes, &outSizeBytes_, tdeflFlags);
	FRM_ASSERT(out_);
}

void frm::Decompress(const void* _in, uint _inSizeBytes, void*& out_, uint& outSizeBytes_)
{
	FRM_ASSERT(_in);
	FRM_ASSERT(_inSizeBytes);
	FRM_ASSERT(!out_);

	int tinflFlags = TINFL_FLAG_PARSE_ZLIB_HEADER;
	out_ = tinfl_decompress_mem_to_heap(_in, _inSizeBytes, &outSizeBytes_, tinflFlags);
	FRM_ASSERT(out_);
}
