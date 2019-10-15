#pragma once

#include <frm/core/frm.h>
#include <frm/core/Time.h>

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
	

protected:

	double m_timeScale, m_deltaTime;
	frm::Timestamp m_prevUpdate;

	App();
	virtual ~App();

}; // class App

} // namespace frm
