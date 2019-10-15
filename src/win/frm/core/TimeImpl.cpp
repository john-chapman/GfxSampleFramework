#include <frm/core/Time.h>

#include <frm/core/memory.h>
#include <frm/core/platform.h>
#include <frm/core/win.h>
#include <frm/core/String.h>

using namespace frm;

static SYSTEMTIME ToSystemTime(sint64 _raw)
{
	LARGE_INTEGER li;
	li.QuadPart = _raw;
	FILETIME ft;
	ft.dwLowDateTime  = li.LowPart;
	ft.dwHighDateTime = li.HighPart;
	SYSTEMTIME st;
	FileTimeToSystemTime(&ft, &st);	
	return st;
}

static DateTime FromSystemTime(SYSTEMTIME _st)
{
	FILETIME ft;
	SystemTimeToFileTime(&_st, &ft);
	LARGE_INTEGER li;
	li.LowPart  = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	return DateTime(li.QuadPart);
}

/*******************************************************************************
	
                                 Time

*******************************************************************************/

static storage<sint64, 1>    s_sysFreq;
static storage<Timestamp, 1> s_appInit;

Timestamp Time::GetTimestamp() 
{
	LARGE_INTEGER t;
	FRM_PLATFORM_VERIFY(QueryPerformanceCounter(&t));
	return Timestamp(t.QuadPart);
}

sint64 Time::GetSystemFrequency() 
{
	return *s_sysFreq;
}

DateTime Time::GetDateTime() 
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);//GetSystemTimePreciseAsFileTime(&ft); // \todo 'Precise' version Windows 8+ only
	// MS docs recommend copying into a LARGE_INTEGER first
	LARGE_INTEGER li;
	li.LowPart  = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	return DateTime(li.QuadPart);
}

DateTime Time::ToLocal(DateTime _utc)
{
	SYSTEMTIME utc = ToSystemTime(_utc.getRaw());
	SYSTEMTIME ret;
	SystemTimeToTzSpecificLocalTime(NULL, &utc, &ret);
	return FromSystemTime(ret);
}

DateTime Time::ToUTC(DateTime _local)
{
	SYSTEMTIME local = ToSystemTime(_local.getRaw());
	SYSTEMTIME ret;
	TzSpecificLocalTimeToSystemTime(NULL, &local, &ret);
	return FromSystemTime(ret);
}

Timestamp Time::GetApplicationElapsed()
{
	return GetTimestamp() - *s_appInit;
}

void Time::Sleep(sint64 _ms)
{
	::Sleep((DWORD)_ms);
}

void Time::Init()
{
	LARGE_INTEGER f;
	FRM_PLATFORM_VERIFY(QueryPerformanceFrequency(&f));
	*s_sysFreq = f.QuadPart;
	*s_appInit = GetTimestamp();
}

void Time::Shutdown()
{
}

/*******************************************************************************
	
                                 Timestamp

*******************************************************************************/

double Timestamp::asSeconds() const
{
	return asMicroseconds() / 1000000.0;
}

double Timestamp::asMilliseconds() const
{
	return asMicroseconds() / 1000.0;
}

double Timestamp::asMicroseconds() const
{
	return (double)((m_raw * 1000000ll) / Time::GetSystemFrequency());
}

/*******************************************************************************
	
                                   DateTime

*******************************************************************************/

sint32 DateTime::getYear() const         { return (sint32)ToSystemTime(m_raw).wYear; }
sint32 DateTime::getMonth() const        { return (sint32)ToSystemTime(m_raw).wMonth; }
sint32 DateTime::getDay() const          { return (sint32)ToSystemTime(m_raw).wDay; }
sint32 DateTime::getHour() const         { return (sint32)ToSystemTime(m_raw).wHour; }
sint32 DateTime::getMinute() const       { return (sint32)ToSystemTime(m_raw).wMinute; }
sint32 DateTime::getSecond() const       { return (sint32)ToSystemTime(m_raw).wSecond; }
sint32 DateTime::getMillisecond() const  { return (sint32)ToSystemTime(m_raw).wMilliseconds; }

frm::DateTime::DateTime(const char* _str, const char* _format)
{
	_format = _format ? _format : "%Y-%m-%dT%H:%M:%SZ"; // default ISO 8601

	SYSTEMTIME st = {};
	while (*_format) {
		if (*_format == '%') {
			char* str;
			switch (*(++_format)) {
				case 'Y':
					st.wYear = (WORD)strtol(_str, &str, 0);
					break;
				case 'm':
					st.wMonth = (WORD)strtol(_str, &str, 0);
					break;
				case 'd':
					st.wDay = (WORD)strtol(_str, &str, 0);
					break;
				case 'H':
					st.wHour = (WORD)strtol(_str, &str, 0);
					break;
				case 'M':
					st.wMinute = (WORD)strtol(_str, &str, 0);
					break;
				case 'S':
					st.wSecond = (WORD)strtol(_str, &str, 0);
					break;
				case 's':
					st.wMilliseconds = (WORD)strtol(_str, &str, 0);
					break;
				default:
					break;
			};
			++_format;
			_str = str;

		} else {
			FRM_ASSERT(*_str == *_format); // mismatch
			++_format;
			++_str;
		}
	}
	*this = FromSystemTime(st);
}


const char* frm::DateTime::asString(const char* _format) const
{
	static String<128> s_buf;
	SYSTEMTIME st = ToSystemTime(m_raw);
	if (!_format) { // default ISO 8601 format
		s_buf.setf("%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	} else {
		s_buf.clear();
		for (int i = 0; _format[i] != 0; ++i) {
			if (_format[i] == '%') {
				switch (_format[++i]) {
					case 'Y': s_buf.appendf("%.4d", st.wYear);         break;
					case 'm': s_buf.appendf("%.2d", st.wMonth);        break;
					case 'd': s_buf.appendf("%.2d", st.wDay);          break;
					case 'H': s_buf.appendf("%.2d", st.wHour);         break;
					case 'M': s_buf.appendf("%.2d", st.wMinute);       break;
					case 'S': s_buf.appendf("%.2d", st.wSecond);       break;
					case 's': s_buf.appendf("%.2d", st.wMilliseconds); break;
					default:
						if (_format[i] != 0) {
							s_buf.append(&_format[i], 1);
						}
				};
			} else {
				s_buf.append(&_format[i], 1);
			}
		}
	}
	return (const char*)s_buf;
}

