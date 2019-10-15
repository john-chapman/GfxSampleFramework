#include "Time.h"

#include <frm/core/log.h>
#include <frm/core/String.h>

#include <EASTL/vector.h>

using namespace frm;

FRM_DEFINE_STATIC_INIT(Time, Time::Init, Time::Shutdown);

/*******************************************************************************

                                  Timestamp

*******************************************************************************/
const char* Timestamp::asString() const
{
	static String<64> s_buf;
	s_buf.clear();
	float x = (float)asSeconds();
	if (x >= 1.0f) {
		s_buf.setf("%1.3fs", x);
	} else {
		x = (float)asMilliseconds();
		if (x >= 0.1f) {
			s_buf.setf("%1.2fms", x);
		} else {
			x = (float)asMicroseconds();
			s_buf.setf("%1.0fus", x);
		}
	}
	return (const char*)s_buf;
}

/*******************************************************************************

                                  AutoTimer

*******************************************************************************/

namespace {
	struct AutoTimer_StackEntry
	{
		String<64> m_msg;
		int        m_level;
		Timestamp  m_interval;
	};
	thread_local eastl::vector<AutoTimer_StackEntry> s_AutoTimerStack;
	thread_local int s_AutoTimerStackTop;
}

AutoTimer::AutoTimer(const char* _fmt, ...)
{
	m_stackIndex = (int)s_AutoTimerStack.size();
	s_AutoTimerStack.push_back();
	s_AutoTimerStack.back().m_level = s_AutoTimerStackTop++;

	va_list args;
	va_start(args, _fmt);
	s_AutoTimerStack.back().m_msg.setfv(_fmt, args);
	va_end(args);

	m_start = Time::GetTimestamp(); 
}

AutoTimer::~AutoTimer() 
{
	s_AutoTimerStack[m_stackIndex].m_interval = Time::GetTimestamp() - m_start;
	if (--s_AutoTimerStackTop == 0) {
		for (auto& stackEntry : s_AutoTimerStack) {
			FRM_LOG("%*s%s -- %s", stackEntry.m_level * 2, "", (const char*)stackEntry.m_msg, stackEntry.m_interval.asString());
		}
		s_AutoTimerStack.clear();
	}
}