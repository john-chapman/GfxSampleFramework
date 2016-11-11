#pragma once
#ifndef frm_App_h
#define frm_App_h

#include <frm/def.h>

#include <apt/Time.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
/// \class App
/// Base class for framework apps. 
////////////////////////////////////////////////////////////////////////////////
class App: private apt::non_copyable<App>
{
public:
	virtual bool   init(const apt::ArgList& _args);
	virtual void   shutdown();

	/// \return true if the application should continue (i.e. if no quit message was received).
	virtual bool   update();

	/// Call immediately after blocking i/o or slow operations.
	void           resetTime()                  { m_deltaTime = 0.0; m_prevUpdate = apt::Time::GetTimestamp(); }

	apt::Timestamp getCurrentTime() const       { return m_prevUpdate; }
	double         getDeltaTime() const         { return m_deltaTime; }
	double         getTimeScale() const         { return m_timeScale; }
	void           setTimeScale(double _scale)  { m_timeScale = _scale; }
	

protected:
	double m_timeScale, m_deltaTime;
	apt::Timestamp m_prevUpdate;

	App();
	virtual ~App();

}; // class App

} // namespace frm

#endif // frm_App_h