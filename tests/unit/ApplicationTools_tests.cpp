#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include <apt/log.h>
#include <apt/math.h>
#include <apt/platform.h>
#include <apt/Time.h>

using namespace apt;

TEST_CASE("adhoc")
{
	#ifdef APT_PLATFORM_WIN
	 // force the current working directoy to the exe location
		TCHAR buf[MAX_PATH] = {};
		DWORD buflen;
		APT_PLATFORM_VERIFY(buflen = GetModuleFileName(0, buf, MAX_PATH));
		char* pathend = strrchr(buf, (int)'\\');
		*(++pathend) = '\0';
		APT_PLATFORM_VERIFY(SetCurrentDirectory(buf));
		APT_LOG("Set current directory: '%s'", buf);
	#endif
}
