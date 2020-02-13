#pragma once

// Global options

//#define FRM_ENABLE_ASSERT              1   // Enable asserts. If FRM_DEBUG this is enabled by default.
//#define FRM_ENABLE_STRICT_ASSERT       1   // Enable 'strict' asserts.
//#define FRM_LOG_CALLBACK_ONLY          1   // By default, log messages are written to stdout/stderr prior to the log callback dispatch. Disable this behavior.

#if defined(FRM_DEBUG)
	#ifndef FRM_ENABLE_ASSERT
		#define FRM_ENABLE_ASSERT 1
	#endif
#endif

// Compiler
#if defined(__GNUC__)
	#define FRM_COMPILER_GNU 1
#elif defined(_MSC_VER)
	#define FRM_COMPILER_MSVC 1
#else
	#error frm: Compiler not defined
#endif

// Platform 
#if defined(_WIN32) || defined(_WIN64)
	#define FRM_PLATFORM_WIN 1
#else
	#error frm: Platform not defined
#endif

// Architecture
#if defined(_M_X64) || defined(__x86_64)
	#define FRM_DCACHE_LINE_SIZE 64
#else
	#error frm: Architecture not defined
#endif

// Modules
#ifndef FRM_MODULE_CORE
	#define FRM_MODULE_CORE 0
#endif
#ifndef FRM_MODULE_AUDIO
	#define FRM_MODULE_AUDIO 0
#endif
#ifndef FRM_MODULE_PHYSICS
	#define FRM_MODULE_PHYSICS 0
#endif
#ifndef FRM_MODULE_VR
	#define FRM_MODULE_VR 0
#endif
#if !FRM_MODULE_CORE
	#error frm: FRM_MODULE_CORE was not enabled
#endif

// Control Z range in NDC, either FRM_NDC_Z_NEG_ONE_TO_ONE (OpenGL default) or FRM_NDC_Z_ZERO_TO_ONE (Direct3D default).
// This modifies how the projection matrix is constructed, see Camera.cpp.
#if !defined(FRM_NDC_Z_ZERO_TO_ONE) && !defined(FRM_NDC_Z_NEG_ONE_TO_ONE)
	#define FRM_NDC_Z_ZERO_TO_ONE 1
#endif
