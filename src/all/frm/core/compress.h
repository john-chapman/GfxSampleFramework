#pragma once

#include <frm/core/frm.h>

namespace frm {

enum CompressionFlags
{
	CompressionFlags_None = 0,   // don't compress (for APIs with optional compression)
	CompressionFlags_Speed,      // faster compression, potentially larger size
	CompressionFlags_Size,       // slower compression, potentially smaller size
	CompressionFlags_Default = CompressionFlags_Speed
};

// Compress _inSizeBytes from _in to out_ (allocated by the function). The size of the resulting buffer is written to outSizeBytes_.
// out_ should subsequently be release via free().
void Compress(const void* _in, uint _inSizeBytes, void*& out_, uint& outSizeBytes_, CompressionFlags _flags = CompressionFlags_Default);

// Decompress _in to out_ (allocated by the function). The size of the resulting buffer is written to outSizeBytes_.
// out_ should subsequently be release via free().
void Decompress(const void* _in, uint _inSizeBytes, void*& out_, uint& outSizeBytes_);

} // namespace frm
