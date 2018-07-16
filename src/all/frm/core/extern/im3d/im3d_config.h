#pragma once
#ifndef im3d_config_h
#define im3d_config_h

#include <frm/core/def.h>
#include <frm/core/math.h>
#include <apt/memory.h>

// User-defined assertion handler (default is cassert assert()).
#define IM3D_ASSERT(e) APT_ASSERT(e)

// User-defined malloc/free. Define both or neither (default is cstdlib malloc()/free()).
#define IM3D_MALLOC(size) APT_MALLOC(size)
#define IM3D_FREE(ptr)    APT_FREE(ptr) 

// Use row-major internal matrix layout. 
//#define IM3D_MATRIX_ROW_MAJOR 1

// Force vertex data alignment (default is 4 bytes).
//#define IM3D_VERTEX_ALIGNMENT 4

// Enable internal culling for primitives (everything drawn between Begin*()/End()). The application must set a culling frustum via AppData.
//#define IM3D_CULL_PRIMITIVES 1

// Enable internal culling for gizmos. The application must set a culling frustum via AppData.
//#define IM3D_CULL_GIZMOS 1

// Conversion to/from application math types.
#define IM3D_VEC2_APP \
	Vec2(const frm::vec2& _v)          { x = _v.x; y = _v.y;     } \
	operator frm::vec2() const         { return frm::vec2(x, y); }
#define IM3D_VEC3_APP \
	Vec3(const frm::vec3& _v)          { x = _v.x; y = _v.y; z = _v.z; } \
	operator frm::vec3() const         { return frm::vec3(x, y, z);    }
#define IM3D_VEC4_APP \
	Vec4(const frm::vec4& _v)          { x = _v.x; y = _v.y; z = _v.z; w = _v.w; } \
	operator frm::vec4() const         { return frm::vec4(x, y, z, w);           }
#define IM3D_MAT3_APP \
	Mat3(const frm::mat3& _m)          { for (int i = 0; i < 9; ++i) m[i] = *(&(_m[0][0]) + i); } \
	operator frm::mat3() const         { frm::mat3 ret; for (int i = 0; i < 9; ++i) *(&(ret[0][0]) + i) = m[i]; return ret; }
#define IM3D_MAT4_APP \
	Mat4(const frm::mat4& _m)          { for (int i = 0; i < 16; ++i) m[i] = *(&(_m[0][0]) + i); } \
	operator frm::mat4() const         { frm::mat4 ret; for (int i = 0; i < 16; ++i) *(&(ret[0][0]) + i) = m[i]; return ret; }

	
#endif // im3d_config_h
