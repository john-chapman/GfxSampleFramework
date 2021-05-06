#include "Image.h"

#include <frm/core/log.h>
#include <frm/core/math.h>
#include <frm/core/memory.h>
#include <frm/core/File.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Time.h>

#include <cmath>
#include <cstring>

#ifdef FRM_COMPILER_MSVC
	#pragma warning(disable: 4244) // possible loss of data
#endif

using namespace frm;

// Copy at most _srcCount objects from _src to _dst, performing conversion from 
// tSrc to tDst. If _srcCount < _dstCount, the remaining elements of _dst are
// initialized as tDst(0).
template <typename tSrc, typename tDst>
static void ConvertCopy(const tSrc* _src, tDst* _dst, uint _srcCount, uint _dstCount)
{
	do
	{
		*_dst = DataTypeConvert<tSrc, tDst>(*_src);
		++_src;
		++_dst;
		--_dstCount;
		--_srcCount;
	} while (_dstCount != 0 && _srcCount != 0);

	while (_dstCount != 0)
	{
		*_dst = (tDst)0;
		++_dst;
		--_dstCount;
	}
}

// Copy image data (_size texels) from _src to _dst. _srcCount/_dstCount are the
// number of components per texel in _src/_dst respectively.
template <typename tSrc, typename tDst>
static void ConvertCopyImage(const void* _src, void* _dst, uint _srcCount, uint _dstCount, uint _size)
{
	const tSrc* src = (const tSrc*)_src;
	tDst* dst = (tDst*)_dst;
	for (uint i = 0, n = _size / _srcCount; i < n; ++i)
	{
		ConvertCopy(src, dst, _srcCount, _dstCount);
		src += _srcCount;
		dst += _dstCount;
	}
}

static uint GetImageSize(uint _w, uint _h, uint _d, Image::CompressionType _compression, uint _bytesPerTexel)
{
	switch (_compression)
	{
		case Image::Compression_BC1:
		case Image::Compression_BC4:
			return FRM_MAX(((_w + 3) / 4) * ((_h + 3) / 4) * 8, (uint)1) * _d;
		case Image::Compression_BC2:
		case Image::Compression_BC3:
		case Image::Compression_BC5:
		case Image::Compression_BC6:
		case Image::Compression_BC7:
			return FRM_MAX(((_w + 3) / 4) * ((_h + 3) / 4) * 16, (uint)1) * _d;
		default:
			break;
	};
	return _w * _h * _d * _bytesPerTexel;
}

/*******************************************************************************

                                   Image

*******************************************************************************/

// PUBLIC

Image* Image::Create1d(uint _width, Layout _layout, DataType _dataType, uint _mipmapCount, uint _arrayCount, CompressionType _compressionType)
{
	Image* ret = FRM_NEW(Image);
	FRM_ASSERT(ret);
	ret->init();
	ret->m_type        = _arrayCount > 1 ? Type_1dArray : Type_1d;
	ret->m_width       = _width;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_arrayCount  = _arrayCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}

Image* Image::Create2d(uint _width, uint _height, Layout _layout, DataType _dataType, uint _mipmapCount, uint _arrayCount, CompressionType _compressionType)
{
	Image* ret = FRM_NEW(Image);
	FRM_ASSERT(ret);
	ret->init();
	ret->m_type        = _arrayCount > 1 ? Type_2dArray : Type_2d;
	ret->m_width       = _width;
	ret->m_height      = _height;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_arrayCount  = _arrayCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}


Image* Image::Create3d(uint _width, uint _height, uint _depth, Layout _layout, DataType _dataType, uint _mipmapCount, uint _arrayCount, CompressionType _compressionType)
{
	Image* ret = FRM_NEW(Image);
	FRM_ASSERT(ret);
	ret->init();
	ret->m_type        = _arrayCount > 1 ? Type_3dArray : Type_3d;
	ret->m_width       = _width;
	ret->m_height      = _height;
	ret->m_depth       = _depth;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_arrayCount  = _arrayCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}

Image* Image::CreateCubemap(uint _width, Layout _layout, DataType _dataType, uint _mipmapCount, uint _arrayCount, CompressionType _compressionType)
{
	Image* ret = FRM_NEW(Image);
	FRM_ASSERT(ret);
	ret->init();
	ret->m_type        = _arrayCount > 1 ? Type_CubemapArray : Type_Cubemap;
	ret->m_width       = _width;
	ret->m_height      = _width;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_arrayCount  = _arrayCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}

void Image::Destroy(Image*& _img_)
{
	FRM_ASSERT(_img_);
	if (_img_)
	{
		FRM_DELETE(_img_);
		_img_ = 0;
	}
}

bool Image::Read(Image& img_, const File& _file, FileFormat _format)
{
	if (_format == FileFormat_Invalid)
	{
		_format = GuessFormat(_file.getPath());
		if (_format == FileFormat_Invalid)
		{
			FRM_LOG_ERR("Image: Unsupported file format '%s'", _file.getPath());
			return false;
		}
	}

	img_.init(); // \todo don't affect the image before loading succeeded
	switch (_format)
	{
		case FileFormat_Dds: return ReadDds(img_, _file.getData(), _file.getDataSize());
		case FileFormat_Png: return ReadPng(img_, _file.getData(), _file.getDataSize());
		case FileFormat_Exr: return ReadExr(img_, _file.getData(), _file.getDataSize());
		case FileFormat_Bmp:
		case FileFormat_Gif:
		case FileFormat_Hdr:
		case FileFormat_Jpg:
		case FileFormat_Psd:
		case FileFormat_Tga: return ReadDefault(img_, _file.getData(), _file.getDataSize());

		default: FRM_ASSERT(false); return false; // should never happen as we check the ext above
	};

	return true;
}

bool Image::Read(Image& img_, const char* _path, FileFormat _format)
{
	FRM_AUTOTIMER("Image::Read(%s)", _path);
	File f;
	f.setPath(_path);
	if (!File::Read(f, _path))
	{
		return false;
	}
	return Read(img_, f, _format);
}


bool Image::Write(const Image& _img, File& file_, FileFormat _format)
{
	bool ret = false;

	if (_format == FileFormat_Invalid) 
	{
		_format = GuessFormat(file_.getPath());
		if (_format == FileFormat_Invalid)
		{
			FRM_LOG_ERR("Image: Unsupported file format '%s'", file_.getPath());
			goto Image_Write_end;
		}
	}

	if (!_img.validateFileFormat(_format))
	{
		FRM_ASSERT(false);
		goto Image_Write_end;
	}

	switch (_format)
	{
		case FileFormat_Bmp: ret = WriteBmp(file_, _img); break;
		case FileFormat_Dds: ret = WriteDds(file_, _img); break;
		case FileFormat_Png: ret = WritePng(file_, _img); break;
		case FileFormat_Tga: ret = WriteTga(file_, _img); break;
		case FileFormat_Hdr: ret = WriteHdr(file_, _img); break;
		case FileFormat_Exr: ret = WriteExr(file_, _img); break;
		default: FRM_LOG_ERR("Image: File format does not supported writing '%s'", file_.getPath()); goto Image_Write_end;
	};

	ret = true;

Image_Write_end:
	FRM_ASSERT(ret);
	return ret;
}

bool Image::Write(const Image& _img, const char* _path, FileFormat _format)
{
	FRM_AUTOTIMER("Image::Write(%s)", _path);
	File f;
	f.setPath(_path);
	if (!Write(_img, f, _format))
	{
		return false;
	}
	return File::Write(f, _path);
}

uint Image::GetMaxMipmapCount(uint _width, uint _height, uint _depth)
{
	uint log2Width  = (uint)floor(log2((double)_width));
	uint log2Height = (uint)floor(log2((double)_height));
	uint log2Depth  = (uint)floor(log2((double)_depth));
	return FRM_MAX(log2Width, FRM_MAX(log2Height, log2Depth)) + 1; // +1 for level 0
}

char* Image::getRawImage(uint _array, uint _mip) const
{
	FRM_ASSERT(m_data);
	uint offset = _array * m_arrayLayerSize + m_mipOffsets[_mip];
	return m_data + offset;
}

uint Image::getRawImageSize(uint _mip) const
{
	FRM_ASSERT(m_data);
	FRM_ASSERT(_mip < m_mipmapCount);
	if (!m_data || _mip >= m_mipmapCount)
	{
		return 0;
	}

	return m_mipSizes[_mip];
}


void Image::setRawImage(uint _array, uint _mip, const void* _src, Layout _layout, DataType _dataType, CompressionType _compressionType)
{
	FRM_ASSERT(_src);

	FRM_ASSERT_MSG(_compressionType == m_compression, "Compression types must match");

 // layout/type match, memcpy data
	if (_layout == m_layout && _dataType == m_dataType && _compressionType == m_compression)
	{
		memcpy(const_cast<char*>(getRawImage(_array, _mip)), _src, getRawImageSize(_mip));
		return;
	}

 // layout/type don't match, must convert
	uint srcCount = GetComponentCount(_layout);
	uint dstCount = GetComponentCount(m_layout);
	void* dst = const_cast<char*>(getRawImage(_array, _mip));

	#define CONVERT_FROM(type) \
		switch (m_dataType) \
		{ \
			case DataType_Uint8N:   ConvertCopyImage<internal::DataType_EnumToType<type>::Type, uint8N >(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType_Uint16N:  ConvertCopyImage<internal::DataType_EnumToType<type>::Type, uint16N>(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType_Uint32N:  ConvertCopyImage<internal::DataType_EnumToType<type>::Type, uint32N>(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType_Float32:  ConvertCopyImage<internal::DataType_EnumToType<type>::Type, float32>(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			default: FRM_ASSERT(false); \
		}
	switch (_dataType)
	{
		case DataType_Uint8N:  CONVERT_FROM(DataType_Uint8N); break;
		case DataType_Uint16N: CONVERT_FROM(DataType_Uint16N); break;
		case DataType_Uint32N: CONVERT_FROM(DataType_Uint32N); break;
		case DataType_Float32: CONVERT_FROM(DataType_Float32); break;
		default:               FRM_ASSERT_MSG(false, "Unsupported DataType: %s", DataTypeString(_dataType));
	};
	#undef CONVERT_FROM
}

// PRIVATE

Image::~Image()
{
	free(m_data);
}

void Image::init()
{
	m_width       = m_height = m_depth = 1;
	m_arrayCount  = 1;
	m_mipmapCount = 1;
	m_type        = Type_Invalid;
	m_compression = Compression_None;
	m_layout      = Layout_Invalid;
	m_dataType    = DataType_Invalid;
	
	char* m_data = nullptr;
	memset(m_mipOffsets, 0, sizeof(uint) * kMaxMipmapCount);
	memset(m_mipSizes,   0, sizeof(uint) * kMaxMipmapCount);
	m_arrayLayerSize = 0;
	m_bytesPerTexel = 0.0f;
}

void Image::alloc()
{
	free(m_data);

	if (m_compression == Compression_None)
	{
		m_bytesPerTexel = (float)(DataTypeSizeBytes(m_dataType) * GetComponentCount(m_layout));
	}
	else
	{
		switch (m_compression)
		{
			case Compression_BC1:
			case Compression_BC4:
				m_bytesPerTexel = 0.5f;
				break;
			case Compression_BC2:
			case Compression_BC3:
			case Compression_BC5:
			case Compression_BC6:
			case Compression_BC7:
				m_bytesPerTexel = 1.0f;
				break;
			default:
				FRM_ASSERT(false); // unknown compression type?
		};
	}
	uint w = m_width, h = m_height, d = m_depth;
	m_arrayLayerSize = 0;
	uint i = 0, lim = FRM_MIN(m_mipmapCount, GetMaxMipmapCount(w, h, d));
	do
	{
		m_mipOffsets[i] = m_arrayLayerSize;
		m_mipSizes[i] = GetImageSize(w, h, d, m_compression, (uint)m_bytesPerTexel);
		m_arrayLayerSize += m_mipSizes[i];
		w = FRM_MAX(w >> 1, (uint)1);
		h = FRM_MAX(h >> 1, (uint)1);
		d = FRM_MAX(d >> 1, (uint)1);
		++i;
		FRM_ASSERT(i < kMaxMipmapCount);
	} while (i < lim);

	uint imageCount = isCubemap() ? m_arrayCount * 6 : m_arrayCount;
	m_data = (char*)FRM_MALLOC(m_arrayLayerSize * imageCount);
	FRM_ASSERT(m_data);
}

bool Image::validateFileFormat(FileFormat _format) const
{
	#define Image_ERR_IF(_cond, _msg, ...) \
		if (_cond) \
		{ \
			FRM_LOG_ERR("Image: " _msg); \
			return false; \
		}

	switch (_format)
	{
		case FileFormat_Bmp:
			Image_ERR_IF(m_compression != Compression_None,   "BMP compression not supported");
			Image_ERR_IF(DataTypeIsFloat(m_dataType),         "BMP float data types not supported");
			Image_ERR_IF(DataTypeIsSigned(m_dataType),        "BMP signed data types not supported");
			Image_ERR_IF(!DataTypeIsNormalized(m_dataType),   "BMP only normalized data types are supported");
			Image_ERR_IF(!IsDataTypeBpc(m_dataType, 8),       "BMP only 8 bit data types are supported");
			break;
		case FileFormat_Dds:
			Image_ERR_IF(m_type == Type_3dArray,              "DDS 3d arrays not supported");
			break;
		case FileFormat_Exr:
			Image_ERR_IF(m_compression != Compression_None,   "EXR compression not supported");
			Image_ERR_IF(!DataTypeIsFloat(m_dataType),        "EXR only float data types are supported");
			break;
		case FileFormat_Hdr:
			Image_ERR_IF(m_compression != Compression_None,   "HDR compression not supported");
			Image_ERR_IF(!DataTypeIsFloat(m_dataType),        "HDR only float data types are supported");
			Image_ERR_IF(!IsDataTypeBpc(m_dataType, 32),      "HDR only 32 bit data types are supported");
			break;
		case FileFormat_Png:
			Image_ERR_IF(m_compression != Compression_None,   "PNG compression not supported");
			Image_ERR_IF(DataTypeIsFloat(m_dataType),         "PNG float data types not supported");
			Image_ERR_IF(DataTypeIsSigned(m_dataType),        "PNG signed data types not supported");
			Image_ERR_IF(!DataTypeIsNormalized(m_dataType),   "PNG only normalized data types are supported");
			Image_ERR_IF(IsDataTypeBpc(m_dataType, 32),       "PNG only 8 and 16 bit data types are supported");
			break;
		case FileFormat_Tga:
			Image_ERR_IF(m_compression != Compression_None,   "TGA compression not supported");
			Image_ERR_IF(DataTypeIsFloat(m_dataType),         "TGA float data types not supported");
			Image_ERR_IF(DataTypeIsSigned(m_dataType),        "TGA signed data types not supported");
			Image_ERR_IF(!DataTypeIsNormalized(m_dataType),   "TGA only normalized data types are supported");
			Image_ERR_IF(IsDataTypeBpc(m_dataType, 32),       "TGA only 8 and 16 bit data types are supported");
			break;
		default:
			return false;
	};

	return true;

	#undef Image_ERR_IF
}

uint Image::GetComponentCount(Layout _layout)
{
	switch (_layout)
	{
		case Layout_R:         return 1;
		case Layout_RG:        return 2;
		case Layout_RGB:       return 3;
		case Layout_RGBA:      return 4;
		case Layout_Invalid:
		default:               return 0;
	};
}

Image::Layout Image::GuessLayout(uint _cmpCount)
{
	switch (_cmpCount)
	{
		case 1:  return Layout_R;
		case 2:  return Layout_RG;
		case 3:  return Layout_RGB;
		case 4:  return Layout_RGBA;
		default: return Layout_Invalid;
	};
}

Image::FileFormat Image::GuessFormat(const char* _path)
{
	if (FileSystem::CompareExtension("bmp", _path))
	{
		return FileFormat_Bmp;
	}
	else if (FileSystem::CompareExtension("dds", _path))
	{
		return FileFormat_Dds;
	}
	else if (FileSystem::CompareExtension("exr", _path))
	{
		return FileFormat_Exr;
	}
	else if (FileSystem::CompareExtension("hdr", _path))
	{
		return FileFormat_Hdr;
	}
	else if (FileSystem::CompareExtension("png", _path))
	{
		return FileFormat_Png;
	}	
	else if (FileSystem::CompareExtension("tga", _path))
	{
		return FileFormat_Tga;
	}
	else if (FileSystem::CompareExtension("jpg", _path) || FileSystem::CompareExtension("jpeg", _path))
	{
		return FileFormat_Jpg;
	}
	else if (FileSystem::CompareExtension("gif", _path))
	{
		return FileFormat_Gif;
	}	
	else if (FileSystem::CompareExtension("psd", _path)) 
	{
		return FileFormat_Psd;
	}

	return FileFormat_Invalid;
}

bool Image::IsDataTypeBpc(DataType _type, int _bpc)
{
	return _bpc == DataTypeSizeBytes(_type) * 8;
}

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_HDR
#define STBI_ONLY_JPEG
#define STBI_ONLY_PSD
#define STBI_ONLY_TGA
#define STBI_FAILURE_USERMSG
#define STBI_ASSERT(x)        FRM_ASSERT(x)
#define STBI_MALLOC(size)     FRM_MALLOC(size)
#define STBI_REALLOC(p, size) FRM_REALLOC(p, size)
#define STBI_FREE(p)          FRM_FREE(p)
#include <stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x)        FRM_ASSERT(x)
#define STBIW_MALLOC(size)     FRM_MALLOC(size)
#define STBIW_REALLOC(p, size) FRM_REALLOC(p, size)
#define STBIW_FREE(p)          FRM_FREE(p)
#include <cstdio>
#include <stb_image_write.h>
static void StbiWriteFile(void* file_, void* _data, int _size)
{
	if (_size == 0)
	{
		return;
	}
	FRM_ASSERT(file_);
	File* f = (File*)file_;
	f->appendData((const char*)_data, _size);
}

//#include <miniz.h> // \todo use miniz instead of the zlib version compiled into lodepng
#include <lodepng.h>
void* lodepng_malloc(size_t size)                   { return FRM_MALLOC(size); }
void* lodepng_realloc(void* ptr, size_t new_size)   { return FRM_REALLOC(ptr, new_size); }
void  lodepng_free(void* ptr)                       { FRM_FREE(ptr); }
static void SwapByteOrder(char* _d_, unsigned _dsize)
{
    for (unsigned i = 0; i < _dsize; i += 2)
	{
        unsigned j = i + 1;
        unsigned char tmp = _d_[j];
        _d_[j] = _d_[i];
        _d_[i] = tmp;
    }
}

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#include <miniz/miniz.h>
#include <tinyexr.h>

bool Image::ReadDefault(Image& img_, const char* _data, uint _dataSize)
{
	int w, h, cmp;
	stbi_uc* d;
	bool isHdr = stbi_is_hdr_from_memory((stbi_uc*)_data, (int)_dataSize) != 0;
	if (isHdr)
	{
		d = (stbi_uc*)stbi_loadf_from_memory((stbi_uc*)_data, (int)_dataSize, &w, &h, &cmp, 0);
	}
	else
	{
		d = stbi_load_from_memory((stbi_uc*)_data, (int)_dataSize, &w, &h, &cmp, 0);
	}
	if (!d)
	{
		FRM_LOG_ERR("stbi_load%s_from_memory() failed '%s'", isHdr ? "f" : "", stbi_failure_reason());
		return false;
	}
	img_.m_width       = w;
	img_.m_height      = h;
	img_.m_depth       = img_.m_arrayCount = img_.m_mipmapCount = 1;
	img_.m_type        = Type_2d;
	img_.m_layout      = GuessLayout((uint)cmp);
	img_.m_dataType    = isHdr ? DataType_Float32 : DataType_Uint8N;
	img_.m_compression = Compression_None;
	img_.alloc();
	memcpy(img_.m_data, d, w * h * cmp * (isHdr ? sizeof(float) : sizeof(stbi_uc))); // \todo avoid this, let image own the ptr
	stbi_image_free(d);
	return true;
}
bool Image::ReadPng(Image& img_, const char* _data, uint _dataSize)
{
	LodePNGDecoderSettings dec;
    lodepng_decoder_settings_init(&dec);
	dec.color_convert = 0;
    LodePNGState state;
    lodepng_state_init(&state);
    state.decoder = dec;
	DataType dataType;	
	bool ret = true;
	unsigned x, y, cmp;
	unsigned char* d;
	unsigned err = lodepng_inspect(&x, &y, &state, (unsigned char*)_data, _dataSize);
	if (err)
	{
		goto Image_ReadPng_end;
	}
	err = lodepng_decode(&d, &x, &y, &state, (unsigned char*)_data, _dataSize);
	if (err)
	{
		goto Image_ReadPng_end;
	}

	switch (state.info_raw.colortype)
	{
		case LCT_GREY:          cmp = 1; break;
		case LCT_GREY_ALPHA:    cmp = 2; break;
		case LCT_RGB:           cmp = 3; break;
		case LCT_RGBA:          cmp = 4; break;
		case LCT_PALETTE:
		default:                FRM_ASSERT_MSG(false, "Unsupported PNG format");
		                        ret = false;
	};
	switch (state.info_raw.bitdepth)
	{
		case 8:                 dataType = DataType_Uint8N;  break;
		case 16:                dataType = DataType_Uint16N;
		                        SwapByteOrder((char*)d, x * y * cmp * 2); break; // \todo swizzle bytes during copy to img_
		default:                FRM_ASSERT_MSG(false, "Unsupported bit depth (%d)", state.info_raw.bitdepth);
		                        ret = false;
	};

	img_.m_width       = x;
	img_.m_height      = y;
	img_.m_depth       = img_.m_arrayCount = img_.m_mipmapCount = 1;
	img_.m_type        = Type_2d;
	img_.m_layout      = GuessLayout((uint)cmp);
	img_.m_dataType    = dataType;
	img_.m_compression = Compression_None;
	img_.alloc();
	memcpy(img_.m_data, d, x * y * cmp * DataTypeSizeBytes(dataType)); // \todo, avoid this?

Image_ReadPng_end:
	FRM_FREE(d);
	lodepng_state_cleanup(&state);
	if (err) {
		FRM_LOG_ERR("lodepng error: '%s'", lodepng_error_text(err));
		return false;
	}
	return ret;
}
bool Image::WritePng(File& file_, const Image& _img)
{
	LodePNGColorType colorType;
	unsigned bitdepth;
	bool ret = true;

	char* buf = 0;

	switch (_img.m_layout)
	{
		case Layout_R:     colorType = LCT_GREY;       break;
		case Layout_RG:    colorType = LCT_GREY_ALPHA; break;
		case Layout_RGB:   colorType = LCT_RGB;        break;
		case Layout_RGBA:  colorType = LCT_RGBA;       break;
		default:           FRM_ASSERT_MSG(false, "Invalid image layout");
		                   ret = false;
	};

	switch (_img.m_dataType)
	{
		case DataType_Uint8N:  bitdepth = 8;   break;
		case DataType_Uint16N: bitdepth = 16;
		                       // \hack \todo Can lodepng be modified to swizzle the bytes automatically on x86?
	                            buf = (char*)FRM_MALLOC(_img.getRawImageSize());
	                            memcpy(buf, _img.getRawImage(), _img.getRawImageSize());
		                        SwapByteOrder(buf, (unsigned)_img.getRawImageSize()); break;
		default:                FRM_ASSERT_MSG(false, "Unsupported data type");
		                        ret = false;
	};

	LodePNGEncoderSettings enc;
	lodepng_encoder_settings_init(&enc);
	enc.auto_convert = 0;
	LodePNGState state;
	lodepng_state_init(&state);
	state.info_raw.colortype = colorType;
	state.info_raw.bitdepth = bitdepth;
	state.info_png.color.colortype = colorType;
	state.info_png.color.bitdepth = bitdepth;
	state.encoder = enc;

	unsigned char* d;
	uint dsize;
	unsigned err = lodepng_encode(&d, &dsize, (unsigned char*)(buf ? buf : _img.getRawImage()), (unsigned)_img.getWidth(), (unsigned)_img.getHeight(), &state);
	if (err) {
		goto Image_WritePng_end;
	}
	file_.setData((const char*)d, dsize);

Image_WritePng_end:
	lodepng_state_cleanup(&state);
	FRM_FREE(buf);
	FRM_FREE(d);
	if (err) {
		FRM_LOG_ERR("lodepng error: '%s'", lodepng_error_text(err));
		return false;
	}
	return ret;
}

bool Image::ReadExr(Image& img_, const char* _data, uint _dataSize)
{
	bool ret = true;
	const char* err = nullptr;
	char* data = nullptr;
	uint32 dataTypeSizeBytes = 1;
	
	EXRVersion version = {};
	EXRHeader header = {};
	InitEXRHeader(&header);
	EXRImage exr = {};
	InitEXRImage(&exr);
	
	if (ParseEXRVersionFromMemory(&version, (const unsigned char*)_data, _dataSize) != TINYEXR_SUCCESS)
	{
		ret = false;
		goto Image_ReadExr_end;
	}
	if (ParseEXRHeaderFromMemory(&header, &version, (const unsigned char*)_data, _dataSize, &err) != TINYEXR_SUCCESS)
	{
		ret = false;
		goto Image_ReadExr_end;
	}
	if (header.multipart)
	{
		err = "Multipart EXR not supported";
		ret = false;
		goto Image_ReadExr_end;
	}
	if (header.tiled)
	{
		err = "Tiled EXR not supported";
		ret = false;
		goto Image_ReadExr_end;
	}
	for (int i = 0; i < header.num_channels; ++i)
	{
		//header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
		header.requested_pixel_types[i] = header.channels[i].pixel_type;
		FRM_ASSERT(header.channels[i].pixel_type == header.channels[0].pixel_type); // require homogneous pixel type across all channels
	}
	if (LoadEXRImageFromMemory(&exr, &header, (const unsigned char*)_data, _dataSize, &err) != TINYEXR_SUCCESS)
	{
		ret = false;
		goto Image_ReadExr_end;
	}

	switch (exr.num_channels)
	{
		case 1: img_.m_layout = Layout_R;    break;
		case 2: img_.m_layout = Layout_RG;   break;
		case 3: img_.m_layout = Layout_RGB;  break;
		case 4: img_.m_layout = Layout_RGBA; break;
		default: err = "Unsupported # channels"; ret = false; goto Image_ReadExr_end; 
	};
	switch (header.channels[0].pixel_type) {
		case TINYEXR_PIXELTYPE_FLOAT: img_.m_dataType = DataType_Float32; break;
		case TINYEXR_PIXELTYPE_HALF:  img_.m_dataType = DataType_Float16; break;
		case TINYEXR_PIXELTYPE_UINT:  img_.m_dataType = DataType_Uint32;  break;
		default: err = "Unsupported pixel type"; ret = false; goto Image_ReadExr_end;
	};
	dataTypeSizeBytes = (uint32)DataTypeSizeBytes(img_.m_dataType);

	img_.m_width       = exr.width;
	img_.m_height      = exr.height;
	img_.m_depth       = img_.m_arrayCount = img_.m_mipmapCount = 1;
	img_.m_type        = Type_2d; 
	img_.m_compression = Compression_None;
	img_.alloc();

	data = (char*)img_.m_data;
	for (uint i = 0, n = img_.m_width * img_.m_height; i < n; ++i)
	{
	 // \hack read the channels in reverse order (convert ABGR -> RGBA)
	 // \todo inspect the channel names directly, assume ABGR
		for (int j = exr.num_channels - 1; j >= 0; --j, data += dataTypeSizeBytes)
		{
			memcpy(data, exr.images[j] + (i * dataTypeSizeBytes), dataTypeSizeBytes);
		}
	}

Image_ReadExr_end:
	FreeEXRHeader(&header);
	FreeEXRImage(&exr);
	if (err) {
		FRM_LOG_ERR("ReadExr: '%s'", err);
		return false;
	}
	return ret;
}

bool Image::WriteExr(File& file_, const Image& _img)
{
	const char* err = nullptr;

	EXRHeader header;
	InitEXRHeader(&header);
	EXRImage exr;
	InitEXRImage(&exr);

	switch (_img.m_layout)
	{
		case Layout_R:    exr.num_channels = 1; break;
		case Layout_RG:   exr.num_channels = 2; break;
		case Layout_RGBA: exr.num_channels = 4; break;
		case Layout_RGB:  
		default:          exr.num_channels = 3; break;
	};
	char* channels[4] = {};
	const size_t bytesPerTexelPerChannel = _img.getBytesPerTexel() / exr.num_channels;
	for (int i = 0; i < exr.num_channels; ++i)
	{
		int k = exr.num_channels - i - 1;
		channels[i] = (char*)FRM_MALLOC(bytesPerTexelPerChannel * _img.m_width * _img.m_height);
		for (uint j = 0, n = _img.m_width * _img.m_height; j < n; ++j)
		{
			//channels[i][j] = ((float*)_img.m_data)[j * exr.num_channels + k];
			memcpy(channels[i] + j * bytesPerTexelPerChannel, _img.m_data + (j * exr.num_channels + k) * bytesPerTexelPerChannel, bytesPerTexelPerChannel);
		}
	}
	exr.images = (unsigned char**)channels;
	exr.width = (int)_img.m_width;
	exr.height = (int)_img.m_height;

	header.num_channels = exr.num_channels;
	header.channels = (EXRChannelInfo*)FRM_MALLOC(sizeof(EXRChannelInfo) * header.num_channels);
	header.pixel_types = (int*)FRM_MALLOC(sizeof(int) * header.num_channels);
	header.requested_pixel_types = header.pixel_types;
	const char* channelNames[] = { "A", "B", "G", "R" };
	const char** name = channelNames + (4 - header.num_channels);
	int pixelType = TINYEXR_PIXELTYPE_FLOAT;
	switch (_img.getImageDataType()) {
		case DataType_Float32: pixelType = TINYEXR_PIXELTYPE_FLOAT; break;
		case DataType_Float16: pixelType = TINYEXR_PIXELTYPE_HALF; break;
		case DataType_Uint32:  pixelType = TINYEXR_PIXELTYPE_UINT; break;
		default: err = "Unsupported pixel type"; return false;
	};
	for (int i = 0; i < header.num_channels; ++i)
	{
		strcpy(header.channels[i].name, name[i]);
		header.pixel_types[i] = pixelType;
	}

	unsigned char* data;
	size_t dataSize = SaveEXRImageToMemory(&exr, &header, &data, &err);
	if (!err)
	{
		file_.setData((char*)data, dataSize);
	}
	FRM_FREE(data);
	FRM_FREE(header.channels);
	FRM_FREE(header.pixel_types);
	for (int i = 0; i < exr.num_channels; ++i)
	{
		FRM_FREE(channels[i]);
	}
	if (err)
	{
		FRM_LOG_ERR("ReadExr: '%s'", err);
		return false;
	}
	return true;
}

bool Image::WriteBmp(File& file_, const Image& _img)
{
	file_.reserveData(_img.getRawImageSize());
	if (!stbi_write_bmp_to_func(StbiWriteFile, &file_, (int)_img.m_width, (int)_img.m_height, (int)GetComponentCount(_img.m_layout), _img.m_data))
	{
		FRM_LOG_ERR("stbi_write_bmp_to_func() failed");
		return false;
	}
	return true;
}
bool Image::WriteTga(File& file_, const Image& _img)
{
	stbi_write_tga_with_rle = 0; // encoding speed can be extremely slow with RLE enabled
	file_.reserveData(_img.getRawImageSize());
	if (!stbi_write_tga_to_func(StbiWriteFile, &file_, (int)_img.m_width, (int)_img.m_height, (int)GetComponentCount(_img.m_layout), _img.m_data))
	{
		FRM_LOG_ERR("stbi_write_tga_to_func() failed");
		return false;
	}
	return true;
}
bool Image::WriteHdr(File& file_, const Image& _img)
{
	file_.reserveData(_img.getRawImageSize());
	if (!stbi_write_hdr_to_func(StbiWriteFile, &file_, (int)_img.m_width, (int)_img.m_height, (int)GetComponentCount(_img.m_layout), (float*)_img.m_data))
	{
		FRM_LOG_ERR("stbi_write_hdr_to_func() failed");
		return false;
	}
	return true;
}
