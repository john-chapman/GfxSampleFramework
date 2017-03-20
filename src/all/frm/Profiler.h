#pragma once
#ifndef frm_Profiler_h
#define frm_Profiler_h

#include <frm/def.h>

#include <apt/static_initializer.h>

//#define frm_Profiler_DISABLE
#ifdef frm_Profiler_DISABLE
	#define CPU_AUTO_MARKER(_name) APT_UNUSED(_name)
	#define GPU_AUTO_MARKER(_name) APT_UNUSED(_name)
	#define AUTO_MARKER(_name)     APT_UNUSED(_name)
#else
	#define CPU_AUTO_MARKER(_name) volatile frm::Profiler::CpuAutoMarker APT_UNIQUE_NAME(_cpuAutoMarker_)(_name)
	#define GPU_AUTO_MARKER(_name) volatile frm::Profiler::GpuAutoMarker APT_UNIQUE_NAME(_gpuAutoMarker_)(_name)
	#define AUTO_MARKER(_name)     CPU_AUTO_MARKER(_name); GPU_AUTO_MARKER(_name)
#endif

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Profiler
// - Ring buffers of marker data.
// - Marker data = name, depth, start time, end time.
// - Marker depth indicates where the marker is relative to the previous marker
//   in the buffer (if this depth > prev depth, this is a child of prev).
// \todo Reduce the size of Marker for better coherency; store start/end as 
//   frame-relative times.
////////////////////////////////////////////////////////////////////////////////
class Profiler: private apt::non_copyable<Profiler>
{
public:
	static const int kMaxFrameCount              = 64; // must be at least 2 (keep 1 frame to write to while visualizing the others)
	static const int kMaxDepth                   = 255;
	static const int kMaxTotalCpuMarkersPerFrame = 32;
	static const int kMaxTotalGpuMarkersPerFrame = 32;

	struct Marker
	{
		const char*  m_name;
		uint64       m_start;
		uint64       m_end;
		uint8        m_depth;
	};
	struct CpuMarker: public Marker
	{
	};
	struct GpuMarker: public Marker
	{
		uint64 m_cpuStart; // when PushGpuMarker() was called
	};

	struct Frame
	{
		uint64 m_start;
		uint   m_first;
		uint   m_count;
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


	// Push/pop a named Gpu marker.
	static void             PushGpuMarker(const char* _name);
	static void             PopGpuMarker(const char* _name);
	static uint             GetGpuFrameIndex(const GpuFrame& _frame);
	// Access to profiler frames. 0 is the oldest frame in the history buffer.
	static const GpuFrame&  GetGpuFrame(uint _i);
	static uint             GetGpuFrameCount();
	static uint64           GetGpuAvgFrameDuration();
	// Access to marker data. Unlike access to frame data, the index accesses the internal ring buffer directly.
	static const GpuMarker& GetGpuMarker(uint _i);

	// Reset Cpu->Gpu offset (call if the graphics context changes).
	static void   ResetGpuOffset();

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

	static bool       s_pause;

	static void Init();
	static void Shutdown();

	static void ShowProfilerViewer(bool* _open_);

}; // class Profiler
APT_DECLARE_STATIC_INIT(Profiler, Profiler::Init, Profiler::Shutdown);

} // namespace frm

#endif // frm_Profiler_h
