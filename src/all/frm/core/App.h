#pragma once

#include <frm/core/frm.h>
#include <frm/core/Time.h>

#include <EASTL/fixed_vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// App
// Base class for framework apps. 
////////////////////////////////////////////////////////////////////////////////
class App: private frm::non_copyable<App>
{
public:

	virtual bool init(const frm::ArgList& _args);
	virtual void shutdown();

	// Return true if the application should continue (i.e. if no quit message was received).
	virtual bool update();

	// Call immediately after blocking i/o or slow operations.
	void  resetTime() { m_deltaTime = 0.0; m_prevUpdate = frm::Time::GetTimestamp(); }

	frm::Timestamp getCurrentTime() const       { return m_prevUpdate; }
	double         getDeltaTime() const         { return m_deltaTime; }
	double         getTimeScale() const         { return m_timeScale; }
	void           setTimeScale(double _scale)  { m_timeScale = _scale; }
	
	enum Event_
	{
		Event_OnInit,
		Event_OnShutdown,
		Event_OnUpdate,

		Event_Count
	};
	typedef int Event;
	typedef void (Callback)(void* _arg);

	// Register/unregister a callback.
	void  registerCallback(Event _event, Callback* _callback, void* _arg_);
	void  unregisterCallback(Event _event, Callback* _callback, void* _arg_);

protected:

	double m_timeScale, m_deltaTime;
	frm::Timestamp m_prevUpdate;

	App();
	virtual ~App();

private:

	struct CallbackListEntry
	{
		Callback* func;
		void*     arg;

		bool operator==(const CallbackListEntry& _rhs) { return _rhs.func == func && _rhs.arg == arg; }
		void operator()()                              { func(arg); }
	};

	using CallbackList = eastl::fixed_vector<CallbackListEntry, 1>;
	CallbackList m_callbacks[Event_Count];

	void dispatchCallbacks(Event _event);

}; // class App

} // namespace frm
