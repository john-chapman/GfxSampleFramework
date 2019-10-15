#pragma once

#include <frm/core/frm.h>
#include <frm/core/StaticInitializer.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Time
// Global time functions.
////////////////////////////////////////////////////////////////////////////////
class Time
{
public:
	// See TimeStamp.
	static Timestamp GetTimestamp();
	static sint64    GetSystemFrequency();
	
	// See DateTime.
	static DateTime  GetDateTime(); // UTC
	static DateTime  ToLocal(DateTime _utc);
	static DateTime  ToUTC(DateTime _local);

	// Interval since the application began.
	static Timestamp GetApplicationElapsed();

	// 
	static void      Sleep(sint64 _ms);

private:

	FRM_DECLARE_STATIC_INIT_FRIEND(Time);
	static void Init();
	static void Shutdown();
};
FRM_DECLARE_STATIC_INIT(Time);

////////////////////////////////////////////////////////////////////////////////
// Timestamp
// High resolution unsynchronized timestamp, use for interval measurments. 
////////////////////////////////////////////////////////////////////////////////
class Timestamp
{
	friend class Time;
public:
	Timestamp(sint64 _raw = 0ll): m_raw(_raw) {}

	// Raw time in system ticks.
	sint64 getRaw() const { return m_raw; }

	double asSeconds() const;
	double asMilliseconds() const; // (10^-3s)
	double asMicroseconds() const; // (10^-6s)
	
	// Return a string with an appropriate units e.g. "2.43s", "17.2ms", "400us".
	// Note that return value is a ptr to a local static buffer - for normal use this should be fine, just print the string and don't keep the ptr.
	const char*     asString() const;

	const Timestamp operator- (const Timestamp& rhs) const  { return m_raw -  rhs.m_raw; }
	const Timestamp operator+ (const Timestamp& rhs) const  { return m_raw +  rhs.m_raw; }
	Timestamp&      operator-=(const Timestamp& rhs)        { m_raw -= rhs.m_raw; return *this; }
	Timestamp&      operator+=(const Timestamp& rhs)        { m_raw += rhs.m_raw; return *this; }

	bool            operator> (const Timestamp& rhs) const  { return m_raw >  rhs.m_raw; }
	bool            operator>=(const Timestamp& rhs) const  { return m_raw >= rhs.m_raw; }
	bool            operator< (const Timestamp& rhs) const  { return m_raw <  rhs.m_raw; }
	bool            operator<=(const Timestamp& rhs) const  { return m_raw <= rhs.m_raw; }

private:
	sint64 m_raw;

};


////////////////////////////////////////////////////////////////////////////////
// DateTime
// System datetime, use for file timestamps.
////////////////////////////////////////////////////////////////////////////////
class DateTime
{
	friend class Time;
public:
	DateTime(sint64 _raw = 0ll): m_raw(_raw) {}
	
	// Init from a string. _format has the same meaning as asString(), the default is ISO 8601.
	DateTime(const char* _str, const char* _format = nullptr);

	// Raw time value in system-dependent units.
	uint64 getRaw() const { return m_raw; }

	sint32 getYear() const;
	sint32 getMonth() const;
	sint32 getDay() const;
	sint32 getHour() const;
	sint32 getMinute() const;
	sint32 getSecond() const;
	sint32 getMillisecond() const;

	// Return a formatted string. The default formatting is ISO 8601, however a format 
	// string may be supplied using the following specifiers:
	//   Specifier | Value
	//   ==========+==================================================
	//    %d       | Zero-padded day of month in [01,31]
	//    %H       | Zero-padded hour in [00,24]
	//    %m       | Zero-padded month in [01,12]
	//    %M       | Zero-padded minute in [00,59]
	//    %s       | Zero-padded millisecond in [0000,9999]
	//    %S       | Zero-padded second in [00,59]
	//    %Y       | Year
	// E.g. ISO 8601 format would be "%Y-%m-%dT%H:%M:%SZ".
	// Note that return value is a ptr to a local static buffer - for normal use this should be fine, just print the string and don't keep the ptr.
	const char*    asString(const char* _format = nullptr) const;

	bool           operator> (const DateTime& rhs) const  { return m_raw >  rhs.m_raw; }
	bool           operator>=(const DateTime& rhs) const  { return m_raw >= rhs.m_raw; }
	bool           operator< (const DateTime& rhs) const  { return m_raw <  rhs.m_raw; }
	bool           operator<=(const DateTime& rhs) const  { return m_raw <= rhs.m_raw; }

private:	
	uint64 m_raw;

};

////////////////////////////////////////////////////////////////////////////////
// AutoTimer
// Scoped timer. Measures the time between ctor and dtor, logs the interval in
// the dtor. Use FRM_AUTOTIMER_DBG to declare an AutoTimer instance for debug 
// builds only.
//
// Auto timers may be nested in which case the result is only logged when the
// outer timer is destroyed, e.g.:
//   Outer -- 20ms
//     Inner -- 10ms
//     Inner -- 10ms
////////////////////////////////////////////////////////////////////////////////
class AutoTimer
{
	int        m_stackIndex;
	Timestamp  m_start;
public:
	AutoTimer(const char* _fmt, ...);
	~AutoTimer();
};
#define FRM_AUTOTIMER(...) frm::AutoTimer FRM_UNIQUE_NAME(_frmAutoTimer_)(__VA_ARGS__)
#ifdef FRM_DEBUG
	#define FRM_AUTOTIMER_DBG(...) FRM_AUTOTIMER(__VA_ARGS__)
#else
	#define FRM_AUTOTIMER_DBG(...) do { } while (0)
#endif

} // namespace frm
