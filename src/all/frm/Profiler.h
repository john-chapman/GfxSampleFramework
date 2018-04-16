#pragma once
#ifndef frm_Profiler_h
#define frm_Profiler_h

#include <frm/def.h>

//#define frm_Profiler_DISABLE
#ifndef frm_Profiler_DISABLE
	// Profile the current block. Use PROFILER_MARKER_CPU, unless the block contains gl* calls.
	#define PROFILER_MARKER_CPU(_name)              volatile frm::Profiler::CpuAutoMarker APT_UNIQUE_NAME(_cpuAutoMarker_)(_name)
	#define PROFILER_MARKER_GPU(_name)              volatile frm::Profiler::GpuAutoMarker APT_UNIQUE_NAME(_gpuAutoMarker_)(_name)
	#define PROFILER_MARKER(_name)                  PROFILER_MARKER_CPU(_name); PROFILER_MARKER_GPU(_name)

	// Track a value (call every frame). Use Profiler::kFormatTimeMs as _fmt if _value represents a time in milliseconds.
	#define PROFILER_VALUE_CPU(_name, _value, _fmt)  Profiler::CpuValue(_name, (float)_value, _fmt)

#else
	#define PROFILER_MARKER_CPU(_name)              APT_UNUSED(_name)
	#define PROFILER_MARKER_GPU(_name)              APT_UNUSED(_name)
	#define PROFILER_MARKER(_name)                  APT_UNUSED(_name)

	#define PROFILER_VALUE_CPU(_name, _value, _fmt)  APT_UNUSED(_name); APT_UNUSED(_value); APT_UNUSED(_fmt)

	
#endif

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Profiler
// \todo Reduce marker size (times relative to the frame start).
////////////////////////////////////////////////////////////////////////////////
class Profiler
{
public:
	static const char* kFormatTimeMs;   // pass as the _format arg to *Value() to indicate that a value represents time in ms, automatically chooses a suffix

	struct Marker
	{
		const char* m_name        = nullptr;
		uint64      m_issueTime   = 0;        // 0 if not a GPU marker
		uint64      m_startTime   = 0;
		uint64      m_stopTime    = 0;
		uint8       m_stackDepth  = 0;
	};

	struct Frame
	{
		uint64      m_id          = 0;
		uint64      m_startTime   = 0;
		uint32      m_markerBegin = 0;        // absolute index of first marker in the frame
		uint32      m_markerEnd   = 0;        // one past the last marker
	};

	struct Value
	{
		const char* m_name        = nullptr;
		const char* m_format      = "%.3f";
		float       m_min         = FLT_MAX;
		float       m_max         = -FLT_MAX;
		float       m_avg         = 0.0f;
	};

	class CpuAutoMarker
	{
		const char* m_name;
	public:
		CpuAutoMarker(const char* _name): m_name(_name)   { Profiler::PushCpuMarker(m_name); }
		~CpuAutoMarker()                                  { Profiler::PopCpuMarker(m_name); }
	};
	class GpuAutoMarker
	{
		const char* m_name;
	public:
		GpuAutoMarker(const char* _name): m_name(_name)   { Profiler::PushGpuMarker(m_name); }
		~GpuAutoMarker()                                  { Profiler::PopGpuMarker(m_name); }
	};

	// Finalize data for the previous frame, reset internal state for the next frame.
	static void   NextFrame();

	// Get the index for the current frame (first frame is 1).
	static uint64 GetFrameIndex() { return s_frameIndex; }
	
	// Push/pop a CPU marker. _name must point to a string literal.
	static void   PushCpuMarker(const char* _name);
	static void   PopCpuMarker(const char* _name);

	// Push/pop a GPU marker. _name must point to a string literal.
	static void   PushGpuMarker(const char* _name);
	static void   PopGpuMarker(const char* _name);

	// Track/untrack a CPU marker.
	static void   TrackCpuMarker(const char* _name);
	static void   UntrackCpuMarker(const char* _name);

	// Track/untrack a GPU marker.
	static void   TrackGpuMarker(const char* _name);
	static void   UntrackGpuMarker(const char* _name);

	// Sample a value. Note that the only difference between CPU and GPU value trackers is the way that they are displayed; in
	// general, GpuValue() is only useful for tracking GPU marker durations. Use Profiler::kFormatTimeMs as _format if the value
	// represents time in milliseconds to choose an automatic suffix (s, ms or us).
	static void   CpuValue(const char* _name, float _value, const char* _format = "%.3f");
	static void   GpuValue(const char* _name, float _value, const char* _format = "%.3f");

	static void   SetPause(bool _pause);
	static bool   GetPause() { return s_pause; }

	static void   DrawUi();
	static void   DrawPinnedValues();

private:
	static uint64 s_frameIndex;
	static bool   s_pause;
	static bool   s_setPause;

}; // class Profiler

} // namespace frm

#endif // frm_Profiler_h
