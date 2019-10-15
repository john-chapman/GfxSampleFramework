#pragma once

#include <frm/core/frm.h>

#if !(FRM_PLATFORM_WIN)
	#error frm: FRM_PLATFORM_WIN was not defined, probably the build system was configured incorrectly
#endif

// ASSERT/VERIFY with platform-specific error string (use to wrap OS calls).
#define FRM_PLATFORM_ASSERT(_err) FRM_ASSERT_MSG(_err, frm::GetPlatformErrorString((uint64)::GetLastError()))
#define FRM_PLATFORM_VERIFY(_err) FRM_VERIFY_MSG(_err, frm::GetPlatformErrorString((uint64)::GetLastError()))

namespace frm {

typedef void* PlatformHandle;

// Format a system error code as a string.
const char* GetPlatformErrorString(uint64 _err);

// Return a string containing OS, CPU and system memory info.
const char* GetPlatformInfoString(); 


constexpr int PlatformJoinProcess_Timeout  = -1;
constexpr int PlatformJoinProcess_Infinite = -1;

// Issue a system command (fork the current process). Return a handle to the process used when calling PlatformJoinProcess().
PlatformHandle PlatformForkProcess(const char* _command);

// Join a previously forked process, block until the specified _timeoutMilliseconds. Return the process retval (or PlatformJoinProcess_Timeout).
int PlatformJoinProcess(PlatformHandle _handle, int _timeoutMilliseconds = PlatformJoinProcess_Infinite);

} // namespace frm
