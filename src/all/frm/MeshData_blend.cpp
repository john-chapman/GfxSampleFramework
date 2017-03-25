#include "MeshData.h"

#include <apt/Time.h>

using namespace frm;
using namespace apt;

/*	Blender file format: http://www.atmind.nl/blender/mystery_ot_blend.html
	
	Blender files begin with a 12 byte header:
		- The chars 'BLENDER'
		- 1 byte pointer size flag; '_' means 32 bits, '-' means 64 bit.
		- 1 byte endianness flag; 'v' means little-endian, 'V' means big-endian.
		- 3 byte version string; '248' means 2.48

	There then follows some number of file blocks:
		- 4-byte aligned, 20 or 24 bytes size depending on the pointer size.
		- 4 byte block type code.
		- 4 byte block size (excluding header).
		- 4/8 byte address of the block when the file was written. Treat this as an ID.
		- 4 byte SDNA index.
		- 4 byte SDNA count.

	Block types:
		- ENDB marks the end of the blend file.
		- DNA1 contains the structure DNA data.

	SDNA:
		- Database of descriptors in a special file block (DNA1).
		- Hundreds of them, only interested in a few: http://www.atmind.nl/blender/blender-sdna.html.
*/
bool MeshData::ReadBlend(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{

	return false;
}
