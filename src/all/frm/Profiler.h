#pragma once
#ifndef frm_Profiler_h
#define frm_Profiler_h

#include <frm/def.h>

#include <apt/static_initializer.h>

//#define frm_Profiler_DISABLE
#ifdef frm_Profiler_DISABLE
	#define PROFILER_MARKER_CPU(_name)         APT_UNUSED(_name)
	#define PROFILER_MARKER_GPU(_name)         APT_UNUSED(_name)
	#define PROFILER_MARKER(_name)             APT_UNUSED(_name)

	#define PROFILER_VALUE_CPU(_name, _value)  APT_UNUSED(_name)
	#define PROFILER_VALUE(_name, _value)      APT_UNUSED(_name)

#else
	#define PROFILER_MARKER_CPU(_name)         volatile frm::Profiler::CpuAutoMarker APT_UNIQUE_NAME(_cpuAutoMarker_)(_name)
	#define PROFILER_MARKER_GPU(_name)         volatile frm::Profiler::GpuAutoMarker APT_UNIQUE_NAME(_gpuAutoMarker_)(_name)
	#define PROFILER_MARKER(_name)             PROFILER_MARKER_CPU(_name); PROFILER_MARKER_GPU(_name)

	#define PROFILER_VALUE_CPU(_name, _value)  Profiler::CpuValue(_name, (float)_value)
	#define PROFILER_VALUE(_name, _value)      PROFILER_VALUE_CPU(_name, _value)

#endif

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Profiler
// - Ring buffers of marker data.
// - Marker data = name, depth, start time, end time.
// - Marker depth indicates where the marker is relative to the previous marker
//   in the buffer (if this depth > prev depth, this is a child of prev).
// \todo Unify Cpu/Gpu markers (reduce duplicate code, clean interface).
// \todo Reduce the size of Marker for better coherency; store start/end as 
//   frame-relative times.
////////////////////////////////////////////////////////////////////////////////
class Profiler: private apt::non_copyable<Profiler>
{
public:
	static const int kMaxFrameCount              = 32; // must be at least 2 (keep 1 frame to write to while visualizing the others)
	static const int kMaxDepth                   = 255;
	static const int kMaxTotalCpuMarkersPerFrame = 32;
	static const int kMaxTotalGpuMarkersPerFrame = 32;

	struct Marker
	{
		const char*  m_name;
		uint64       m_startTime;
		uint64       m_endTime;
		uint8        m_markerDepth;
		bool         m_isCpuMarker; // \todo \hack unify Cpu/Gpu markers
	};
	struct CpuMarker: public Marker
	{
	};
	struct GpuMarker: public Marker
	{
		uint64 m_cpuStart; // when PushGpuMarker() was called
	};

	struct Value
	{
		const char*             m_name;
		float                   m_min;
		float                   m_max;
		float                   m_avg;
		float                   m_accum;
		uint64                  m_count;
		apt::RingBuffer<float>* m_history;
	};


	struct Frame
	{
		uint64 m_id;
		uint64 m_startTime;
		uint   m_firstMarker;
		uint   m_markerCount;
	};
	struct CpuFrame: public Frame
	{
	};
	struct GpuFrame: public Frame
	{
	};
	
	static void NextFrame();

	// Push/pop a named Cpu marker.
	static void             PushCpuMarker(const char* _name);
	static void             PopCpuMarker(const char* _name);
	// Access to profiler frames. 0 is the oldest frame in the history buffer.
	static const CpuFrame&  GetCpuFrame(uint _i);
	static uint             GetCpuFrameCount();
	static const uint64     GetCpuAvgFrameDuration();
	static uint             GetCpuFrameIndex(const CpuFrame& _frame);
	// Access to marker data. Unlike access to frame data, the index accesses the internal ring buffer directly.
	static const CpuMarker& GetCpuMarker(uint _i);
	// Track a named marker duration.
	static void             TrackCpuMarker(const char* _name);
	static void             UntrackCpuMarker(const char* _name);
	static bool             IsCpuMarkerTracked(const char* _name);
	// Sample a value.
	static void             CpuValue(const char* _name, float _value, uint _historySize = 128);
	static uint             GetCpuValueCount();
	static const Value&     GetCpuValue(uint _i);

	// Push/pop a named Gpu marker.
	static void             PushGpuMarker(const char* _name);
	static void             PopGpuMarker(const char* _name);
	// Access to profiler frames. 0 is the oldest frame in the history buffer.
	static const GpuFrame&  GetGpuFrame(uint _i);
	static uint             GetGpuFrameCount();
	static uint64           GetGpuAvgFrameDuration();
	static uint             GetGpuFrameIndex(const GpuFrame& _frame);
	// Access to marker data. Unlike access to frame data, the index accesses the internal ring buffer directly.
	static const GpuMarker& GetGpuMarker(uint _i);
	// Track a named marker duration.
	static void             TrackGpuMarker(const char* _name);
	static void             UntrackGpuMarker(const char* _name);
	static bool             IsGpuMarkerTracked(const char* _name);
	// Sample a value.
	static void             GpuValue(const char* _name, float _value, uint _historySize = 128);
	static uint             GetGpuValueCount();
	static const Value&     GetGpuValue(uint _i);

	// Reset Cpu->Gpu offset (call if the graphics context changes).
	static void ResetGpuOffset();

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

	static bool s_pause;

	static void Init();
	static void Shutdown();

	static void ShowProfilerViewer(bool* _open_);

}; // class Profiler
APT_DECLARE_STATIC_INIT(Profiler, Profiler::Init, Profiler::Shutdown);

} // namespace frm

#endif // frm_Profiler_h
