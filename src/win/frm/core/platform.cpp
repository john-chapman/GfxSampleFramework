#include <frm/core/platform.h>

#include <frm/core/win.h>
#include <frm/core/memory.h>
#include <frm/core/String.h>

#include <intrin.h> // __cpuid

#pragma comment(lib, "version")

namespace frm {

const char* GetPlatformErrorString(uint64 _err)
{
	static thread_local String<1024> ret;
	ret.setf("(%llu) ", _err);
	FRM_VERIFY(
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
			NULL, 
			(DWORD)_err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)(ret.begin() + ret.getLength()),
			(DWORD)(ret.getCapacity() - ret.getLength()),
			NULL
		) != 0
	);
	return (const char*)ret;
}

const char* GetPlatformInfoString()
{
	static thread_local String<1024> ret;

 // OS version
	ret.appendf("\tOS:     ");
	#if 0
	 // GetVersionEx is deprecated
		OSVERSIONINFOEX osinf;
		osinf.dwOSVersionInfoSize = sizeof(osinf);
		if (!GetVersionEx((LPOSVERSIONINFOA)&osinf)) {
			ret.append((const char*)GetPlatformErrorString(GetLastError()));
		} else {
			ret.appendf("Windows %u.%u.%u", osinf.dwMajorVersion, osinf.dwMinorVersion, osinf.dwBuildNumber);
		}
	#else
		void* verinf;
		VS_FIXEDFILEINFO* osinf;
		UINT osinfsz;
		DWORD sz = GetFileVersionInfoSize("kernel32.dll", NULL);
		if (sz == 0) {
			goto osver_failure;
		}
		verinf = FRM_MALLOC(sz);
		if (!GetFileVersionInfo("kernel32.dll", (DWORD)0, sz, verinf)) {
			goto osver_failure;
		}
		if (!VerQueryValue(verinf, "\\", (LPVOID*)&osinf, &osinfsz)) {
			goto osver_failure;
		}
		ret.appendf("Windows %u.%u.%u", HIWORD(osinf->dwProductVersionMS), LOWORD(osinf->dwProductVersionMS), HIWORD(osinf->dwProductVersionLS));
		goto osver_end;
	osver_failure:
		ret.appendf("Windows %u.%u.%u", HIWORD(osinf->dwProductVersionMS), LOWORD(osinf->dwProductVersionMS), HIWORD(osinf->dwProductVersionLS));
	osver_end:
		FRM_FREE(verinf);
		
	#endif
	
 // cpu vendor/brand
	int cpuinf[4] = { -1 };
	char cpustr[64];
	__cpuid(cpuinf, 0x80000002);
	memcpy(cpustr + 0,  cpuinf, sizeof(cpuinf));
	__cpuid(cpuinf, 0x80000003);
	memcpy(cpustr + 16, cpuinf, sizeof(cpuinf));
	__cpuid(cpuinf, 0x80000004);
	memcpy(cpustr + 32, cpuinf, sizeof(cpuinf));
	ret.appendf("\n\tCPU:    %s", cpustr);

 // proccessor count
	SYSTEM_INFO sysinf;
	GetSystemInfo(&sysinf);
	ret.appendf(" (%u cores)", sysinf.dwNumberOfProcessors);
	
 // global memory status
	MEMORYSTATUSEX meminf;
	meminf.dwLength = sizeof(meminf);
	ret.append("\n\tMemory: ");
	if (!GlobalMemoryStatusEx(&meminf)) {
		ret.append((const char*)GetPlatformErrorString(GetLastError()));
	} else {
		ret.appendf("%lluMb", meminf.ullTotalPhys / 1024 / 1024);
	}

	return (const char*)ret;
}

PlatformHandle PlatformForkProcess(const char* _command)
{
	STARTUPINFOA sinfo = { 0 };
	sinfo.cb           = sizeof(sinfo);
	sinfo.dwFlags      = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	sinfo.wShowWindow  = 0;
	sinfo.hStdInput    = GetStdHandle(STD_INPUT_HANDLE);
	sinfo.hStdOutput   = GetStdHandle(STD_OUTPUT_HANDLE);
	sinfo.hStdError    = GetStdHandle(STD_ERROR_HANDLE);

	PROCESS_INFORMATION pinfo = {};
	FRM_PLATFORM_VERIFY(CreateProcessA(NULL, (char*)_command, NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &pinfo));
	//FRM_PLATFORM_VERIFY(CloseHandle(sinfo.hStdInput)); // apparently it's incorrect to close handles returned by GetStdHandle? 
	//FRM_PLATFORM_VERIFY(CloseHandle(sinfo.hStdOutput));
	//FRM_PLATFORM_VERIFY(CloseHandle(sinfo.hStdError));
	FRM_PLATFORM_VERIFY(CloseHandle(pinfo.hThread));

	return (void*)pinfo.hProcess;
}

int PlatformJoinProcess(PlatformHandle _handle, int _timeoutMilliseconds)
{
	if (WaitForSingleObject((HANDLE)_handle, (DWORD)_timeoutMilliseconds) == WAIT_TIMEOUT)
	{
		return PlatformJoinProcess_Timeout;
	}

	DWORD ret;
	FRM_PLATFORM_VERIFY(GetExitCodeProcess((HANDLE)_handle, &ret));
	FRM_PLATFORM_VERIFY(CloseHandle(_handle));
	FRM_ASSERT(ret != (DWORD)PlatformJoinProcess_Timeout); // conflicts with the retval meaning 'timed out'
	
	return (int)ret;
}

} // namespace frm