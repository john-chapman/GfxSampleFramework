#include <catch.hpp>

#include <apt/apt.h>
#include <apt/compress.h>
#include <apt/log.h>
#include <apt/memory.h>
#include <apt/File.h>
#include <apt/Time.h>

using namespace apt;

static void CompressionTest(const char* _filePath, CompressionFlags _flags)
{
	APT_LOG("\nCompression Test '%s' (%s) *********", _filePath, (_flags == CompressionFlags_Size) ? "size" : "speed");
	
	File f;
	{	APT_AUTOTIMER("\tRead '%s'", _filePath);
		File::Read(f, _filePath);
	}

	void* c = nullptr;
	uint csz;
	{	APT_AUTOTIMER("\tCompress");
		Compress(f.getData(), f.getDataSize(), c, csz, CompressionFlags_Speed);
	}
	void* d = nullptr;
	uint dsz;
	{	APT_AUTOTIMER("\tDecompress");
		Decompress(c, csz, d, dsz);
	}
	APT_ASSERT(dsz == f.getDataSize());
	APT_ASSERT(memcmp(d, f.getData(), f.getDataSize()) == 0);
	APT_LOG("\n\tRaw size:        %.2fkb", (float)f.getDataSize() / 1024.0f);
	APT_LOG(  "\tCompressed size: %.2fkb (%f%%)", (float)csz / 1024.0f, (float)csz / (float)dsz * 100.0f);
	free(c);
	c = nullptr;
	free(d);
	d = nullptr;
}

static void CompressionTest(const char* _filePath)
{
	CompressionTest(_filePath, CompressionFlags_Size);
	CompressionTest(_filePath, CompressionFlags_Speed);
}

#if 0
TEST_CASE("performance", "[Compression]")
{
	CompressionTest("bob_lamp_update.md5anim");
}
#endif