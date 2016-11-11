#pragma once
#ifndef frm_ui_ProfilerViewer_h
#define frm_ui_ProfilerViewer_h

#include <frm/def.h>
#include <frm/math.h>
#include <frm/Profiler.h>

namespace frm { namespace ui {

////////////////////////////////////////////////////////////////////////////////
/// \class ProfilerViewer
/// \todo Select a marker and draw a plot of its length over the history buffer
///    (plus some metadata, = min/max/avg).
/// \todo Horizontal scroll bar (useful if no mouse wheel)
////////////////////////////////////////////////////////////////////////////////
class ProfilerViewer
{
public:
	ProfilerViewer();

	void draw(bool* _open_);

private:
	float m_timeStart;    //< Time at the left edge of the window (miliseconds).
	float m_timeRange;    //< Time range covered by the window (miliseconds).
	float m_gridDensity;  //< Grid density (pixel distance between grid lines).

	// draw constants
	uint    m_markerColor;
	vec2    m_markerStart;
	uint64  m_firstFrameStart;
	vec2    m_wpos;
	vec2    m_wsize;
	vec2    m_wend;

	bool drawFrameBounds(const Profiler::Frame& _frame, const Profiler::Frame& _prevFrame);
	bool drawFrameMarker(const Profiler::Marker& _marker);

	/// Convert from time -> window-relative coordinates
	float millisecondsToWindowX(float _ms) const;
	float markerToWindowX(uint64 _time) const;

	/// Convert from window-relative coordinates -> time
	float windowXToMilliseconds(float _x) const;

}; // class ProfilerViewer

} } // namespace frm::ui

#endif // frm_ui_ProfilerViewer_h
