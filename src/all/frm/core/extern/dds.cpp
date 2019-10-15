// \todo:
// - Handle pitch correctly (programming guide recommends copying a scanline at a time).
// - Need to test to ensure portability of the DDS stuff (size of BYTE, UINT, DWORD, etc).

#include <frm/core/math.h>
#include <frm/core/types.h>
#include <frm/core/File.h>

/******************************************************************************/
typedef frm::uint8   BYTE;
typedef frm::uint32  UINT;
typedef frm::uint32  DWORD;
#ifndef MAKEFOURCC
    #define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |       \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif

#ifndef __d3d10_h__
#define __d3d10_h__

typedef enum DDS_RESOURCE_DIMENSION
    {	DDS_RESOURCE_DIMENSION_UNKNOWN	= 0,
	DDS_RESOURCE_DIMENSION_BUFFER	= 1,
	DDS_RESOURCE_DIMENSION_TEXTURE1D	= 2,
	DDS_RESOURCE_DIMENSION_TEXTURE2D	= 3,
	DDS_RESOURCE_DIMENSION_TEXTURE3D	= 4
    } 	DDS_RESOURCE_DIMENSION;

static const char* DDS_RESOURCE_DIMENSION_STRINGS[] =
{
	"RESOURCE_DIMENSION_UNKNOWN",
	"RESOURCE_DIMENSION_BUFFER",
	"RESOURCE_DIMENSION_TEXTURE1D",
	"RESOURCE_DIMENSION_TEXTURE2D",
	"RESOURCE_DIMENSION_TEXTURE3D"
};

typedef
enum DDS_RESOURCE_MISC_FLAG
    {	DDS_RESOURCE_MISC_GENERATE_MIPS	= 0x1L,
	DDS_RESOURCE_MISC_SHARED	= 0x2L,
	DDS_RESOURCE_MISC_TEXTURECUBE	= 0x4L,
	DDS_RESOURCE_MISC_SHARED_KEYEDMUTEX	= 0x10L,
	DDS_RESOURCE_MISC_GDI_COMPATIBLE	= 0x20L
    } 	DDS_RESOURCE_MISC_FLAG;


#endif // #ifndef __d3d10_h__

#ifndef __dxgiformat_h__
#define __dxgiformat_h__

#define DXGI_FORMAT_DEFINED 1

typedef enum DXGI_FORMAT
{
    DXGI_FORMAT_UNKNOWN	                    = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,
    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT               	= 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,
    DXGI_FORMAT_R11G11B10_FLOAT             = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,
    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,
    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,
    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,
    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,
    DXGI_FORMAT_R1_UNORM                    = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,
    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,
    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,
    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
    DXGI_FORMAT_FORCE_UINT                  = 0xffffffff
} DXGI_FORMAT;

typedef enum DXGI_DATA_TYPE
{
	DATA_TYPE_TYPELESS =0,
	DATA_TYPE_FLOAT,
	DATA_TYPE_UNORM,
	DATA_TYPE_SNORM,
	DATA_TYPE_UINT,
	DATA_TYPE_SINT
} DXGI_DATA_TYPE;
struct DxgiInfo
{
	const char*     name;
	bool            compressed;
	bool            srgb;
	char            components;
	char            bitsPerChannel;
	DXGI_DATA_TYPE  dataType;
};
static DxgiInfo DXGI_INFO[] =
{
	{	"UNKNOWN",					false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R32G32B32A32_TYPELESS",	false,	false,	4,	32,	DATA_TYPE_TYPELESS	},
	{	"R32G32B32A32_FLOAT",		false,	false,	4,	32,	DATA_TYPE_FLOAT		},
	{	"R32G32B32A32_UINT",		false,	false,	4,	32,	DATA_TYPE_UINT		},
	{	"R32G32B32A32_SINT",		false,	false,	4,	32,	DATA_TYPE_SINT		},
	{	"R32G32B32_TYPELESS",		false,	false,	3,	32,	DATA_TYPE_TYPELESS	},
	{	"R32G32B32_FLOAT",			false,	false,	3,	32,	DATA_TYPE_FLOAT		},
	{	"R32G32B32_UINT",			false,	false,	3,	32,	DATA_TYPE_UINT		},
	{	"R32G32B32_SINT",			false,	false,	3,	32,	DATA_TYPE_SINT		},
	{	"R16G16B16A16_TYPELESS",	false,	false,	4,	16,	DATA_TYPE_TYPELESS	},
	{	"R16G16B16A16_FLOAT",		false,	false,	4,	16,	DATA_TYPE_FLOAT		},
	{	"R16G16B16A16_UNORM",		false,	false,	4,	16,	DATA_TYPE_UNORM		},
	{	"R16G16B16A16_UINT",		false,	false,	4,	16,	DATA_TYPE_UINT		},
	{	"R16G16B16A16_SNORM",		false,	false,	4,	16,	DATA_TYPE_SNORM		},
	{	"R16G16B16A16_SINT",		false,	false,	4,	16,	DATA_TYPE_SINT		},
	{	"R32G32_TYPELESS",			false,	false,	2,	32,	DATA_TYPE_TYPELESS	},
	{	"R32G32_FLOAT",				false,	false,	2,	32,	DATA_TYPE_FLOAT		},
	{	"R32G32_UINT",				false,	false,	2,	32,	DATA_TYPE_UINT		},
	{	"R32G32_SINT",				false,	false,	2,	32,	DATA_TYPE_SINT		},
	{	"R32G8X24_TYPELESS",		false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"D32_FLOAT_S8X24_UINT",		false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R32_FLOAT_X8X24_TYPELESS",	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"X32_TYPELESS_G8X24_UINT",	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R10G10B10A2_TYPELESS",		false,	false,	4,	0,	DATA_TYPE_TYPELESS	},
	{	"R10G10B10A2_UNORM",		false,	false,	4,	0,	DATA_TYPE_UNORM		},
	{	"R10G10B10A2_UINT",			false,	false,	4,	0,	DATA_TYPE_UINT		},
	{	"R11G11B10_FLOAT",			false,	false,	0,	0,	DATA_TYPE_FLOAT		},
	{	"R8G8B8A8_TYPELESS",		false,	false,	4,	8,	DATA_TYPE_TYPELESS	},
	{	"R8G8B8A8_UNORM",			false,	false,	4,	8,	DATA_TYPE_UNORM		},
	{	"R8G8B8A8_UNORM_SRGB",		false,	true,	4,	8,	DATA_TYPE_UNORM		},
	{	"R8G8B8A8_UINT",			false,	false,	4,	8,	DATA_TYPE_UINT		},
	{	"R8G8B8A8_SNORM",			false,	false,	4,	8,	DATA_TYPE_SNORM		},
	{	"R8G8B8A8_SINT",			false,	false,	4,	8,	DATA_TYPE_SINT		},
	{	"R16G16_TYPELESS",			false,	false,	2,	16,	DATA_TYPE_TYPELESS	},
	{	"R16G16_FLOAT",				false,	false,	2,	16,	DATA_TYPE_FLOAT		},
	{	"R16G16_UNORM",				false,	false,	2,	16,	DATA_TYPE_UNORM		},
	{	"R16G16_UINT",				false,	false,	2,	16,	DATA_TYPE_UINT		},
	{	"R16G16_SNORM",				false,	false,	2,	16,	DATA_TYPE_SNORM		},
	{	"R16G16_SINT",				false,	false,	2,	16,	DATA_TYPE_SINT		},
	{	"R32_TYPELESS",				false,	false,	1,	32,	DATA_TYPE_TYPELESS	},
	{	"D32_FLOAT",				false,	false,	1,	32,	DATA_TYPE_FLOAT		},
	{	"R32_FLOAT",				false,	false,	1,	32,	DATA_TYPE_FLOAT		},
	{	"R32_UINT",					false,	false,	1,	32,	DATA_TYPE_UINT		},
	{	"R32_SINT",					false,	false,	1,	32,	DATA_TYPE_SINT		},
	{	"R24G8_TYPELESS",			false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"D24_UNORM_S8_UINT",		false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R24_UNORM_X8_TYPELESS",	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"X24_TYPELESS_G8_UINT",		false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R8G8_TYPELESS",			false,	false,	2,	8,	DATA_TYPE_TYPELESS	},
	{	"R8G8_UNORM",				false,	false,	2,	8,	DATA_TYPE_UNORM		},
	{	"R8G8_UINT",               	false,	false,	2,	8,	DATA_TYPE_UINT		},
	{	"R8G8_SNORM",              	false,	false,	2,	8,	DATA_TYPE_SNORM		},
	{	"R8G8_SINT",               	false,	false,	2,	8,	DATA_TYPE_SINT		},
	{	"R16_TYPELESS",            	false,	false,	1,	16,	DATA_TYPE_TYPELESS	},
	{	"R16_FLOAT",               	false,	false,	1,	16,	DATA_TYPE_FLOAT		},
	{	"D16_UNORM",               	false,	false,	1,	16,	DATA_TYPE_UNORM		},
	{	"R16_UNORM",               	false,	false,	1,	16,	DATA_TYPE_UNORM		},
	{	"R16_UINT",                	false,	false,	1,	16,	DATA_TYPE_UINT		},
	{	"R16_SNORM",               	false,	false,	1,	16,	DATA_TYPE_SNORM		},
	{	"R16_SINT",                	false,	false,	1,	16,	DATA_TYPE_SINT		},
	{	"R8_TYPELESS",             	false,	false,	1,	8,	DATA_TYPE_TYPELESS	},
	{	"R8_UNORM",                	false,	false,	1,	8,	DATA_TYPE_UNORM		},
	{	"R8_UINT",                 	false,	false,	1,	8,	DATA_TYPE_UINT		},
	{	"R8_SNORM",                	false,	false,	1,	8,	DATA_TYPE_SNORM		},
	{	"R8_SINT",                 	false,	false,	1,	8,	DATA_TYPE_SINT		},
	{	"A8_UNORM",                	false,	false,	1,	8,	DATA_TYPE_UNORM		},
	{	"R1_UNORM",                	false,	false,	1,	1,	DATA_TYPE_UNORM		},
	{	"R9G9B9E5_SHAREDEXP",		false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"R8G8_B8G8_UNORM",      	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"G8R8_G8B8_UNORM",      	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"BC1_TYPELESS",         	true,	false,	3,	0,	DATA_TYPE_TYPELESS	}, // 1-bit alpha, not discerned here
	{	"BC1_UNORM",            	true,	false,	3,	0,	DATA_TYPE_UNORM		}, // 1-bit alpha, not discerned here
	{	"BC1_UNORM_SRGB",       	true,	true,	3,	0,	DATA_TYPE_UNORM		}, // 1-bit alpha, not discerned here
	{	"BC2_TYPELESS",         	true,	false,	4,	0,	DATA_TYPE_TYPELESS	}, // 4-bit alpha (don't use this)
	{	"BC2_UNORM",            	true,	false,	4,	0,	DATA_TYPE_UNORM		}, // 4-bit alpha (don't use this)
	{	"BC2_UNORM_SRGB",       	true,	true,	4,	0,	DATA_TYPE_TYPELESS	}, // 4-bit alpha (don't use this)
	{	"BC3_TYPELESS",         	true,	false,	4,	0,	DATA_TYPE_TYPELESS	}, // 8-bit alpha
	{	"BC3_UNORM",            	true,	false,	4,	0,	DATA_TYPE_UNORM		}, // 8-bit alpha
	{	"BC3_UNORM_SRGB",       	true,	true,	4,	0,	DATA_TYPE_UNORM		}, // 8-bit alpha
	{	"BC4_TYPELESS",         	true,	false,	1,	0,	DATA_TYPE_TYPELESS	}, // 1 grayscale channel
	{	"BC4_UNORM",            	true,	false,	1,	0,	DATA_TYPE_UNORM		}, // 1 grayscale channel
	{	"BC4_SNORM",            	true,	false,	1,	0,	DATA_TYPE_SNORM		}, // 1 grayscale channel
	{	"BC5_TYPELESS",         	true,	false,	2,	0,	DATA_TYPE_TYPELESS	}, // 2 grayscale channels
	{	"BC5_UNORM",            	true,	false,	2,	0,	DATA_TYPE_UNORM		}, // 2 grayscale channels
	{	"BC5_SNORM",            	true,	false,	2,	0,	DATA_TYPE_SNORM		}, // 2 grayscale channels
	{	"B5G6R5_UNORM",         	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"B5G5R5A1_UNORM",       	false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"B8G8R8A8_UNORM",       	false,	false,	4,	8,	DATA_TYPE_UNORM		},
	{	"B8G8R8X8_UNORM",       	false,	false,	4,	8,	DATA_TYPE_UNORM		},
	{	"R10G10B10_XR_BIAS_A2_UNORM",false,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"B8G8R8A8_TYPELESS",        false,	false,	4,	8,	DATA_TYPE_TYPELESS	},
	{	"B8G8R8A8_UNORM_SRGB",      false,	true,	4,	8,	DATA_TYPE_UNORM		},
	{	"B8G8R8X8_TYPELESS",        false,	false,	4,	8,	DATA_TYPE_TYPELESS	},
	{	"B8G8R8X8_UNORM_SRGB",      false,	true,	4,	8,	DATA_TYPE_UNORM		},
	{	"BC6H_TYPELESS",            true,	false,	3,	0,	DATA_TYPE_FLOAT		},
	{	"BC6H_UF16",                true,	false,	3,	0,	DATA_TYPE_FLOAT		},
	{	"BC6H_SF16",                true,	false,	3,	0,	DATA_TYPE_FLOAT		},
	{	"BC7_TYPELESS",             true,	false,	0,	0,	DATA_TYPE_TYPELESS	},
	{	"BC7_UNORM",                true,	false,	0,	0,	DATA_TYPE_UNORM		},
	{	"BC7_UNORM_SRGB",           true,	true,	0,	0,	DATA_TYPE_UNORM		}
};

#endif // __dxgiformat_h__

#ifndef _DDS_H_
#define _DDS_H_

//#include <dxgiformat.h>
//#include <d3d10.h>

#pragma pack(push,1)

#define DDS_MAGIC 0x20534444 // "DDS "

struct DDS_PIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwABitMask;
};

#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
#define DDS_RGB         0x00000040  // DDPF_RGB
#define DDS_RGBA        0x00000041  // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE   0x00020000  // DDPF_LUMINANCE
#define DDS_ALPHA       0x00000002  // DDPF_ALPHA

const DDS_PIXELFORMAT DDSPF_DXT1 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','1'), 0, 0, 0, 0, 0 };

const DDS_PIXELFORMAT DDSPF_DXT2 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','2'), 0, 0, 0, 0, 0 };

const DDS_PIXELFORMAT DDSPF_DXT3 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','3'), 0, 0, 0, 0, 0 };

const DDS_PIXELFORMAT DDSPF_DXT4 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','4'), 0, 0, 0, 0, 0 };

const DDS_PIXELFORMAT DDSPF_DXT5 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','T','5'), 0, 0, 0, 0, 0 };

const DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

const DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 };

const DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 };

const DDS_PIXELFORMAT DDSPF_R8G8B8 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

const DDS_PIXELFORMAT DDSPF_R5G6B5 =
    { sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 };

// This indicates the DDS_HEADER_DXT10 extension is present (the format is in dxgiFormat)
const DDS_PIXELFORMAT DDSPF_DX10 =
    { sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D','X','1','0'), 0, 0, 0, 0, 0 };

#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE     0x00080000  // DDSD_LINEARSIZE

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000 // DDSCAPS_TEXTURE
#define DDS_SURFACE_FLAGS_MIPMAP  0x00400008 // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000208 // DDSCAPS_COMPLEX | DDSCAPS2_CUBEMAP

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_FLAGS_VOLUME 0x00200000 // DDSCAPS2_VOLUME

typedef struct
{
	DWORD           dwSize;
	DWORD           dwFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	DWORD           dwPitchOrLinearSize;
	DWORD           dwDepth;
	DWORD           dwMipMapCount;
	DWORD           dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	DWORD           dwCaps;
	DWORD           dwCaps2;
	DWORD           dwCaps3;
	DWORD           dwCaps4;
	DWORD           dwReserved2;
} DDS_HEADER;

typedef struct
{
  DXGI_FORMAT              dxgiFormat;
  DDS_RESOURCE_DIMENSION resourceDimension;
  UINT                     miscFlag;
  UINT                     arraySize;
  UINT                     miscFlags2;
} DDS_HEADER_DXT10;

#pragma pack(pop)

#endif // _DDS_H

/*******************************************************************************/

#include <frm/core/frm.h>
#include <frm/core/log.h>
#include <frm/core/Image.h>

#include <algorithm>

using namespace frm;

bool Image::ReadDds(Image& img_, const char* _data, uint _dataSize)
{
	FRM_ASSERT(_data);
	FRM_ASSERT(_dataSize > (sizeof(DWORD) + sizeof(DDS_HEADER)));

 // validate DDS_MAGIC, get header ptrs
	if (*(DWORD*)_data != DDS_MAGIC) {
		FRM_LOG_ERR("Invalid DDS");
		return false;
	}
	const DDS_HEADER* ddsh = (DDS_HEADER*)(_data + sizeof(DWORD));
	const DDS_HEADER_DXT10* dxt10h = nullptr;
	if ((ddsh->ddspf.dwFlags & DDS_FOURCC) && (MAKEFOURCC('D','X','1','0') == ddsh->ddspf.dwFourCC)) {
		dxt10h = (DDS_HEADER_DXT10*)(_data + sizeof(DWORD) + sizeof(DDS_HEADER));
	}

 // extract image metadata
	// dimensions (min == 1)
	img_.m_width       = FRM_MAX(ddsh->dwWidth,       (DWORD)1);
	img_.m_height      = FRM_MAX(ddsh->dwHeight,      (DWORD)1);
	img_.m_depth       = FRM_MAX(ddsh->dwDepth,       (DWORD)1);
	img_.m_mipmapCount = FRM_MAX(ddsh->dwMipMapCount, (DWORD)1);
	img_.m_arrayCount  = 1;
	if (dxt10h != 0) {
		img_.m_arrayCount = FRM_MAX(dxt10h->arraySize, (UINT)1);
	}
	// image type
	if (dxt10h != 0) {
		switch (dxt10h->resourceDimension) {
			case DDS_RESOURCE_DIMENSION_TEXTURE1D: img_.m_type = Image::Type_1d; break;
			case DDS_RESOURCE_DIMENSION_TEXTURE2D: img_.m_type = Image::Type_2d; break;
			case DDS_RESOURCE_DIMENSION_TEXTURE3D: img_.m_type = Image::Type_3d; break;
			case DDS_RESOURCE_DIMENSION_BUFFER:
			case DDS_RESOURCE_DIMENSION_UNKNOWN:   
			default:                               FRM_LOG_ERR("DDS: Unknown image type"); return false;
		};
		if (dxt10h->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) {
			img_.m_type = Image::Type_Cubemap;
		}
	} else {
		img_.m_type = Image::Type_1d;
		if (img_.m_height > 1) {
			img_.m_type = Image::Type_2d;
		}
		if (img_.m_depth > 1) {
			img_.m_type = Image::Type_3d;
		}
		if (ddsh->dwCaps2 & DDS_SURFACE_FLAGS_CUBEMAP) {
			img_.m_type = Image::Type_Cubemap;
		}
	}
	if (img_.m_arrayCount > 1) {
		switch (img_.m_type) {
			case Image::Type_1d:      img_.m_type = Image::Type_1dArray; break;
			case Image::Type_2d:      img_.m_type = Image::Type_2dArray; break;
			case Image::Type_3d:      img_.m_type = Image::Type_3dArray; break;
			case Image::Type_Cubemap: img_.m_type = Image::Type_CubemapArray; break;
			default:                  FRM_LOG_ERR("DDS: Unknown image type"); return false;
		};
	}
	// layout, data type, compression format
	if (dxt10h != 0) {
		switch (DXGI_INFO[dxt10h->dxgiFormat].dataType) {
			case DATA_TYPE_FLOAT:
				img_.m_dataType = DataType_Float32;
				break;
			case DATA_TYPE_SINT:
				switch (DXGI_INFO[dxt10h->dxgiFormat].bitsPerChannel) {
					case 32:  img_.m_dataType = DataType_Sint32; break;
					case 16:  img_.m_dataType = DataType_Sint16; break;
					case 8:   
					default:  img_.m_dataType = DataType_Sint8; break;
				};
				break;
			case DATA_TYPE_SNORM:
				switch (DXGI_INFO[dxt10h->dxgiFormat].bitsPerChannel) {
					case 32:  img_.m_dataType = DataType_Sint32N; break;
					case 16:  img_.m_dataType = DataType_Sint16N; break;
					case 8:   
					default:  img_.m_dataType = DataType_Sint8N; break;
				};
				break;
			
			case DATA_TYPE_UINT:
				switch (DXGI_INFO[dxt10h->dxgiFormat].bitsPerChannel) {
					case 32:  img_.m_dataType = DataType_Uint32; break;
					case 16:  img_.m_dataType = DataType_Uint16; break;
					case 8:   
					default:  img_.m_dataType = DataType_Uint8; break;
				};
				break;
			case DATA_TYPE_UNORM:
			case DATA_TYPE_TYPELESS: // assume uint*N if typeless
			default:
				switch (DXGI_INFO[dxt10h->dxgiFormat].bitsPerChannel) {
					case 32:  img_.m_dataType = DataType_Uint32N; break;
					case 16:  img_.m_dataType = DataType_Uint16N; break;
					case 8:   
					default:  img_.m_dataType = DataType_Uint8N; break;
				};
				break;
		}

		switch (dxt10h->dxgiFormat) {
		 // uncompressed rgba formats
			case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			case DXGI_FORMAT_R32G32B32A32_FLOAT:
			case DXGI_FORMAT_R32G32B32A32_UINT:
			case DXGI_FORMAT_R32G32B32A32_SINT:
			case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			case DXGI_FORMAT_R16G16B16A16_FLOAT:
			case DXGI_FORMAT_R16G16B16A16_UNORM:
			case DXGI_FORMAT_R16G16B16A16_UINT:
			case DXGI_FORMAT_R16G16B16A16_SNORM:
			case DXGI_FORMAT_R16G16B16A16_SINT:
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:    img_.m_layout = Image::Layout_RGBA; break;

		 // uncompressed rgb formats
			case DXGI_FORMAT_R32G32B32_TYPELESS:
			case DXGI_FORMAT_R32G32B32_FLOAT:
			case DXGI_FORMAT_R32G32B32_UINT:
			case DXGI_FORMAT_R32G32B32_SINT:   img_.m_layout = Image::Layout_RGB; break;
			
		 // uncompressed rg formats
			case DXGI_FORMAT_R32G32_TYPELESS:
			case DXGI_FORMAT_R32G32_FLOAT:
			case DXGI_FORMAT_R32G32_UINT:
			case DXGI_FORMAT_R32G32_SINT:            
			case DXGI_FORMAT_R16G16_TYPELESS:
			case DXGI_FORMAT_R16G16_FLOAT:
			case DXGI_FORMAT_R16G16_UNORM:
			case DXGI_FORMAT_R16G16_UINT:
			case DXGI_FORMAT_R16G16_SNORM:
			case DXGI_FORMAT_R16G16_SINT:            
			case DXGI_FORMAT_R8G8_TYPELESS:
			case DXGI_FORMAT_R8G8_UNORM:
			case DXGI_FORMAT_R8G8_UINT:
			case DXGI_FORMAT_R8G8_SNORM:
			case DXGI_FORMAT_R8G8_SINT:        img_.m_layout = Image::Layout_RG; break;

		 // uncompressed r formats
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_UINT:
			case DXGI_FORMAT_R32_SINT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			case DXGI_FORMAT_R16_SNORM:
			case DXGI_FORMAT_R16_SINT:
			case DXGI_FORMAT_R8_TYPELESS:
			case DXGI_FORMAT_R8_UNORM:
			case DXGI_FORMAT_R8_UINT:
			case DXGI_FORMAT_R8_SNORM:
			case DXGI_FORMAT_R8_SINT:
			case DXGI_FORMAT_A8_UNORM:
			case DXGI_FORMAT_R1_UNORM:         img_.m_layout = Image::Layout_R; break;

		 // compressed formats
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:   img_.m_layout = Image::Layout_RGB; img_.m_compression = Image::Compression_BC1; break; // layout could also be rgba
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:   img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC2; break;
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:   img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC3; break;
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:        img_.m_layout = Image::Layout_R; img_.m_compression = Image::Compression_BC4; break;
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:        img_.m_layout = Image::Layout_RG; img_.m_compression = Image::Compression_BC5; break;
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:        img_.m_layout = Image::Layout_RGB; img_.m_compression = Image::Compression_BC6; break;
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:   img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC7; break; // layout could also be rgb

		 // unsupported formats
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			case DXGI_FORMAT_R10G10B10A2_UNORM:
			case DXGI_FORMAT_R10G10B10A2_UINT:
			case DXGI_FORMAT_R11G11B10_FLOAT:			
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			case DXGI_FORMAT_R8G8_B8G8_UNORM:
			case DXGI_FORMAT_G8R8_G8B8_UNORM:
			case DXGI_FORMAT_B5G6R5_UNORM:
			case DXGI_FORMAT_B5G5R5A1_UNORM:
			case DXGI_FORMAT_B8G8R8A8_UNORM:
			case DXGI_FORMAT_B8G8R8X8_UNORM:
			case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
			case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:			
			default: FRM_LOG_ERR("DDS: Unsupported format (dxt10h->dxgiFormat = %d)", dxt10h->dxgiFormat); return false;
		};
		if (img_.m_compression != Image::Compression_None) {
			img_.m_dataType = DataType_Invalid;
		}
	} else {
		if (ddsh->ddspf.dwFlags & DDS_FOURCC) {
		 // special format
			switch (ddsh->ddspf.dwFourCC) {
			 // compressed
				case MAKEFOURCC('D','X','T','1'): img_.m_layout = Image::Layout_RGB;  img_.m_compression = Image::Compression_BC1;  img_.m_dataType = DataType_Invalid; break; // layout could also be rgba
				case MAKEFOURCC('D','X','T','2'): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC2;  img_.m_dataType = DataType_Invalid; break;
				case MAKEFOURCC('D','X','T','3'): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC2;  img_.m_dataType = DataType_Invalid; break;
				case MAKEFOURCC('D','X','T','4'): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC3;  img_.m_dataType = DataType_Invalid; break;
				case MAKEFOURCC('D','X','T','5'): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_BC3;  img_.m_dataType = DataType_Invalid; break;
				case MAKEFOURCC('B','C','4','S'):
				case MAKEFOURCC('B','C','4','U'): img_.m_layout = Image::Layout_R;    img_.m_compression = Image::Compression_BC4;  img_.m_dataType = DataType_Invalid; break;
				case MAKEFOURCC('B','C','5','S'):
				case MAKEFOURCC('B','C','5','U'): img_.m_layout = Image::Layout_RG;   img_.m_compression = Image::Compression_BC5;  img_.m_dataType = DataType_Invalid; break;
				
			 // Nvidia DDS exporter for Photoshop doesn't write the DXT10 header and instead uses some esoteric fourcc codes for special formats >:'(
				case MAKEFOURCC('o',  0,  0,  0): img_.m_layout = Image::Layout_R;    img_.m_compression = Image::Compression_None; img_.m_dataType = DataType_Float16; break;
				case MAKEFOURCC('r',  0,  0,  0): img_.m_layout = Image::Layout_R;    img_.m_compression = Image::Compression_None; img_.m_dataType = DataType_Float32; break;
				case MAKEFOURCC('q',  0,  0,  0): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_None; img_.m_dataType = DataType_Float16; break;
				case MAKEFOURCC('t',  0,  0,  0): img_.m_layout = Image::Layout_RGBA; img_.m_compression = Image::Compression_None; img_.m_dataType = DataType_Float32; break;
				
				default: FRM_LOG_ERR("DDS: Unsupported format (ddsh->ddspf.dwFourCC = %d)", ddsh->ddspf.dwFourCC); return false;
			};
			
		} else {
		 // standard format
			switch (ddsh->ddspf.dwRGBBitCount) {
				case 8:   img_.m_layout = Image::Layout_R; break;
				case 16:  img_.m_layout = Image::Layout_RG; break;
				case 24:  img_.m_layout = Image::Layout_RGB; break;
				case 32:  
				default:  img_.m_layout = Image::Layout_RGBA;
			};
			img_.m_dataType = DataType_Uint8N;
		}
	}
	img_.alloc();
	size_t count = img_.isCubemap() ? img_.m_arrayCount * 6 : img_.m_arrayCount;
	unsigned doff = sizeof(DWORD) + sizeof(DDS_HEADER) + (dxt10h ? sizeof(DDS_HEADER_DXT10) : 0);
	if (img_.m_depth > 1 && img_.m_mipmapCount > 1) {
	 // 3d textures are stored mip-wise (all slices for mip0 followed by all slices for mip1, etc).
		const char* src = _data + doff;
		char* dst = img_.m_data;
		for (unsigned i = 0; i < img_.m_mipmapCount; ++i) {
			const auto mipSize = img_.m_mipSizes[i];
			memcpy(dst, src, mipSize);
			dst += mipSize;
			src += mipSize;
		}
	} else {
	 // non-3d textures are stored slice-wise (all mips for slice0 followed by all mips for slice1, etc).
		memcpy(img_.m_data, _data + doff, img_.m_arrayLayerSize * count);
	}

	return true;
}

bool Image::WriteDds(File& file_, const Image& _img)
{
	bool ret = false;
	char* dst = nullptr;
	char* src = nullptr;
	DDS_HEADER* ddsh = nullptr;
	DDS_HEADER_DXT10* dxt10h = nullptr;

 // allocate scratch buffer
	size_t count = _img.isCubemap() ? _img.m_arrayCount * 6 : _img.m_arrayCount;
	size_t buflen = _img.m_arrayLayerSize * count;
	buflen += sizeof(DDS_MAGIC) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
	file_.setData(nullptr, buflen);
	char* buf = file_.getData();
	if (!buf) {
		FRM_LOG_ERR("Failed to allocate %d bytes writing %s", buflen, file_.getPath());
		goto WriteDds_End;
	}

 // write headers
	*((DWORD*)buf)      = DDS_MAGIC;
	ddsh                = (DDS_HEADER*)(buf + sizeof(DWORD));
	ddsh->dwSize        = 124; FRM_ASSERT(ddsh->dwSize == sizeof(DDS_HEADER));
	ddsh->dwFlags       = DDS_HEADER_FLAGS_TEXTURE | (_img.m_mipmapCount > 1 ? DDS_HEADER_FLAGS_MIPMAP : 0) | (_img.m_depth > 1 ? DDS_HEADER_FLAGS_VOLUME : 0);
	ddsh->dwWidth       = (DWORD)_img.m_width;
	ddsh->dwHeight      = (DWORD)_img.m_height;
	ddsh->dwDepth       = (DWORD)(_img.m_depth > 1 ? _img.m_depth : 0);
	ddsh->dwMipMapCount = (DWORD)(_img.m_mipmapCount > 1 ? _img.m_mipmapCount : 0);
	ddsh->ddspf         = DDSPF_DX10;
	ddsh->dwCaps        = DDS_SURFACE_FLAGS_TEXTURE | (_img.m_mipmapCount > 1 ? DDS_SURFACE_FLAGS_MIPMAP : 0);
	ddsh->dwCaps2       = (_img.isCubemap() ? (DDS_SURFACE_FLAGS_CUBEMAP | DDS_CUBEMAP_ALLFACES) : 0) | (_img.m_depth > 1 ? DDS_FLAGS_VOLUME : 0);

	dxt10h = (DDS_HEADER_DXT10*)(buf + sizeof(DWORD) + sizeof(DDS_HEADER));
	if (_img.isCompressed()) {
		switch (_img.m_compression) {
			case Image::Compression_BC1:     dxt10h->dxgiFormat = DXGI_FORMAT_BC1_TYPELESS; break;
			case Image::Compression_BC2:     dxt10h->dxgiFormat = DXGI_FORMAT_BC2_TYPELESS; break;
			case Image::Compression_BC3:     dxt10h->dxgiFormat = DXGI_FORMAT_BC3_TYPELESS; break;
			case Image::Compression_BC4:     dxt10h->dxgiFormat = DXGI_FORMAT_BC4_TYPELESS; break;
			case Image::Compression_BC5:     dxt10h->dxgiFormat = DXGI_FORMAT_BC5_TYPELESS; break;
			case Image::Compression_BC6:     dxt10h->dxgiFormat = DXGI_FORMAT_BC6H_TYPELESS; break;
			case Image::Compression_BC7:     dxt10h->dxgiFormat = DXGI_FORMAT_BC7_TYPELESS; break;
			default:
				FRM_ASSERT(false);
				goto WriteDds_End;
		};
	} else {
		switch (_img.m_dataType) {
			case DataType_Float32:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R32_FLOAT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R32G32_FLOAT; break;
					case Image::Layout_RGB:  dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32_FLOAT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
				};
				break;
			case DataType_Uint32:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R32_UINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R32G32_UINT; break;
					case Image::Layout_RGB:  dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32_UINT; break;
					case Image::Layout_RGBA: 
					default:                   dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32A32_UINT; break;
				};
				break;
			case DataType_Sint32:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R32_SINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R32G32_SINT; break;
					case Image::Layout_RGB:  dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32_SINT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R32G32B32A32_SINT; break;
				};
				break;
			case DataType_Uint16:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R16_UINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R16G16_UINT; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16_UINT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16A16_UINT; break;
				};
				break;
			case DataType_Uint16N:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R16_UNORM; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R16G16_UNORM; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16_UNORM; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16A16_UNORM; break;
				};
				break;
			case DataType_Sint16:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R16_SINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R16G16_SINT; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16_SINT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16A16_SINT; break;
				};
				break;
			case DataType_Sint16N:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R16_SNORM; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R16G16_SNORM; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16_SNORM; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R16G16B16A16_SNORM; break;
				};
				break;
			case DataType_Uint8:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R8_UINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R8G8_UINT; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8_UINT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;
				};
				break;
			case DataType_Uint8N:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R8_UNORM; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R8G8_UNORM; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8_UNORM; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
				};
				break;
			case DataType_Sint8:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R8_SINT; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R8G8_SINT; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8_SINT; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8A8_SINT; break;
				};
				break;
			case DataType_Sint8N:
				switch (_img.m_layout) {
					case Image::Layout_R:    dxt10h->dxgiFormat = DXGI_FORMAT_R8_SNORM; break;
					case Image::Layout_RG:   dxt10h->dxgiFormat = DXGI_FORMAT_R8G8_SNORM; break;
					case Image::Layout_RGB:  FRM_ASSERT(false);//dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8_SNORM; break;
					case Image::Layout_RGBA: 
					default:                 dxt10h->dxgiFormat = DXGI_FORMAT_R8G8B8A8_SNORM; break;
				};
				break;
			default:
				FRM_ASSERT(false);
				goto WriteDds_End;
		};
	}
	switch (_img.m_type) {
		case Image::Type_1d: 
		case Image::Type_1dArray:      dxt10h->resourceDimension = DDS_RESOURCE_DIMENSION_TEXTURE1D; break;
		case Image::Type_2d: 
		case Image::Type_2dArray: 
		case Image::Type_Cubemap:
		case Image::Type_CubemapArray: dxt10h->resourceDimension = DDS_RESOURCE_DIMENSION_TEXTURE2D; break;
		case Image::Type_3d:           dxt10h->resourceDimension = DDS_RESOURCE_DIMENSION_TEXTURE3D; break;
		default:                       dxt10h->resourceDimension = DDS_RESOURCE_DIMENSION_UNKNOWN; break;
	};
	dxt10h->miscFlag   = (_img.isCubemap() ? DDS_RESOURCE_MISC_TEXTURECUBE : 0);
	dxt10h->arraySize  = (UINT)_img.m_arrayCount;
	dxt10h->miscFlags2 = 0;
	
 // write data
	dst = buf + sizeof(DWORD) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
	if (_img.m_depth > 1 && _img.m_mipmapCount > 1) {
	 // 3d textures are stored mip-wise (all slices for mip0 followed by all slices for mip1, etc).
		src = _img.m_data;
		for (unsigned i = 0; i < _img.m_mipmapCount; ++i) {
			const auto mipSize = _img.m_mipSizes[i];
			memcpy(dst, src, mipSize);
			dst += mipSize;
			src += mipSize;
		}
	} else {
	 // non-3d textures are stored slice-wise (all mips for slice0 followed by all mips for slice1, etc).
		memcpy(dst, _img.m_data, _img.m_arrayLayerSize * count);
	}

WriteDds_End:
	return ret;
}

