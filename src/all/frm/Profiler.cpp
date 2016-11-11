#include <frm/Profiler.h>

#include <frm/gl.h>

#include <apt/log.h>
#include <apt/RingBuffer.h>
#include <apt/Time.h>

#include <cstring>

using namespace frm;
using namespace apt;


template <typename tFrame, typename tMarker>
struct ProfilerData
{
	RingBuffer<tFrame>  m_frames;
	RingBuffer<tMarker> m_markers;
	uint                m_markerStack[Profiler::kMaxDepth];
	uint                m_markerStackTop;
	uint64              m_avgFrameDuration;

	ProfilerData(uint _frameCount, uint _maxTotalMarkersPerFrame)
		: m_frames(_frameCount)
		, m_markers(_frameCount * _maxTotalMarkersPerFrame)
	{
	 // \hack we prime the frame/marker ring buffers and fill them with zeros, basically
	 //  to avoid handling the edge case where the ring buffers are empty (which only happens
	 //  when the app launches)

		while (!m_frames.size() == m_frames.capacity()) {
			m_frames.push_back(tFrame());
		}
		memset(m_frames.data(), 0, sizeof(tFrame) * m_frames.capacity());
		while (!m_markers.size() == m_markers.capacity()) {
			m_markers.push_back(tMarker());
		}
		memset(m_markers.data(), 0, sizeof(tMarker) * m_markers.capacity());
	}

	uint     getCurrentMarkerIndex() const           { return &m_markers.back() - m_markers.data(); }
	uint     getCurrentFrameIndex() const            { return &m_frames.back() - m_frames.data(); }
	tMarker& getMarkerTop()                          { return m_markers.data()[m_markerStack[m_markerStackTop]]; }
	uint     getMarkerIndex(const tMarker& _marker)  { return &_marker - m_markers.data(); }

	tFrame& nextFrame()
	{
		APT_ASSERT_MSG(m_markerStackTop == 0, "Marker '%s' was not popped before frame end", getMarkerTop().m_name);
		
	 // get avg frame duration
		m_avgFrameDuration = 0;
		uint64 prevStart = m_frames[0].m_start; 
		uint i = 1;
		for ( ; i < m_frames.size(); ++i) {
			tFrame& frame = m_frames[i];
			if (frame.m_start == 0) { // means the frame was invalid (i.e. gpu query was unavailable)
				break;
			}
			uint64 thisStart = m_frames[i].m_start;
			m_avgFrameDuration += thisStart - prevStart;
			prevStart = thisStart;
		}
		m_avgFrameDuration /= i;

	 // advance to next frame
		uint first = m_frames.back().m_first + m_frames.back().m_count;
		m_frames.push_back(tFrame()); 
		m_frames.back().m_first = first;
		m_frames.back().m_count = 0;
		return m_frames.back(); 
	}

	tMarker& pushMarker(const char* _name)
	{ 
		APT_ASSERT(m_markerStackTop != Profiler::kMaxDepth);
		m_markers.push_back(tMarker()); 
		m_markerStack[m_markerStackTop++] = getCurrentMarkerIndex();
		m_markers.back().m_name = _name;
		m_markers.back().m_depth = (uint8)m_markerStackTop - 1;
		m_frames.back().m_count++;
		return m_markers.back(); 
	}

	tMarker& popMarker(const char* _name)
	{
		tMarker& ret = m_markers.data()[m_markerStack[--m_markerStackTop]];
		APT_ASSERT_MSG(strcmp(ret.m_name, _name) == 0, "Unmatched marker push/pop '%s'/'%s'", ret.m_name, _name);
		return ret;
	}

};
static ProfilerData<Profiler::CpuFrame, Profiler::CpuMarker> s_cpu(Profiler::kMaxFrameCount, Profiler::kMaxTotalCpuMarkersPerFrame);
static ProfilerData<Profiler::GpuFrame, Profiler::GpuMarker> s_gpu(Profiler::kMaxFrameCount, Profiler::kMaxTotalGpuMarkersPerFrame);



static uint64 s_gpuTickOffset                                                                           = 0;
static bool   s_gpuInit                                                                                 = true;
static uint   s_gpuFrameQueryRetrieved                                                                  = 0;
static GLuint s_gpuFrameStartQueries[Profiler::kMaxFrameCount]                                          = {};
static GLuint s_gpuMarkerStartQueries[Profiler::kMaxFrameCount * Profiler::kMaxTotalGpuMarkersPerFrame] = {};
static GLuint s_gpuMarkerEndQueries[Profiler::kMaxFrameCount * Profiler::kMaxTotalGpuMarkersPerFrame]   = {};


static uint64 GpuToSystemTicks(GLuint64 _gpuTime)
{
	return (uint64)_gpuTime * Time::GetSystemFrequency() / 1000000000ull; // nanoseconds -> system ticks
}
static uint64 GpuToTimestamp(GLuint64 _gpuTime)
{
	uint64 ret = GpuToSystemTicks(_gpuTime);
	return ret + s_gpuTickOffset;
}


APT_DEFINE_STATIC_INIT(Profiler);

// PUBLIC

void Profiler::NextFrame()
{
	if_unlikely (s_gpuInit) {
		glAssert(glGenQueries(kMaxFrameCount, s_gpuFrameStartQueries));
		glAssert(glGenQueries(kMaxFrameCount * kMaxTotalGpuMarkersPerFrame, s_gpuMarkerStartQueries));
		glAssert(glGenQueries(kMaxFrameCount * kMaxTotalGpuMarkersPerFrame, s_gpuMarkerEndQueries));

		GLint64 gpuTime;
		glAssert(glGetInteger64v(GL_TIMESTAMP, &gpuTime));
		uint64 gpuTicks = GpuToSystemTicks(gpuTime);
		uint64 cpuTicks = Time::GetTimestamp().getRaw();
		s_gpuTickOffset = cpuTicks - gpuTicks; // \todo is it possible that gpuTicks > cpuTicks?
		s_gpuInit = false;
	}

	if (s_pause) {
		return;
	}

 // CPU: advance frame, get start time/first marker index
	s_cpu.nextFrame().m_start = (uint64)Time::GetTimestamp().getRaw();

 // GPU: retrieve all queries **up to** the last available frame (i.e. when we implicitly know they are available)
	GLint frameAvailable = GL_FALSE;
	uint gpuFrameQueryAvail	= s_gpuFrameQueryRetrieved;
	while (s_gpuFrameQueryRetrieved != s_gpu.getCurrentFrameIndex()) {
		glAssert(glGetQueryObjectiv(s_gpuFrameStartQueries[s_gpuFrameQueryRetrieved], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
		if (frameAvailable == GL_FALSE) {
			break;
		}
		s_gpuFrameQueryRetrieved = (s_gpuFrameQueryRetrieved + 1) % kMaxFrameCount;
	}

	for (; gpuFrameQueryAvail != s_gpuFrameQueryRetrieved; gpuFrameQueryAvail = (gpuFrameQueryAvail + 1) % kMaxFrameCount) {
		GpuFrame& frame = s_gpu.m_frames.data()[gpuFrameQueryAvail];
		GLuint64 gpuTime;
		glAssert(glGetQueryObjectui64v(s_gpuFrameStartQueries[gpuFrameQueryAvail], GL_QUERY_RESULT, &gpuTime));
		frame.m_start = GpuToTimestamp(gpuTime);

		for (uint i = frame.m_first, n = frame.m_first + frame.m_count; i < n; ++i) {
			uint j = i % s_gpu.m_markers.capacity();
			GpuMarker& marker = s_gpu.m_markers.data()[j];

	//glAssert(glGetQueryObjectiv(s_gpuMarkerStartQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(s_gpuMarkerStartQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_start = GpuToTimestamp(gpuTime);
			
	//glAssert(glGetQueryObjectiv(s_gpuMarkerEndQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(s_gpuMarkerEndQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_end = GpuToTimestamp(gpuTime);
		}
	}

	s_gpu.nextFrame().m_start = 0;
	glAssert(glQueryCounter(s_gpuFrameStartQueries[s_gpu.getCurrentFrameIndex()], GL_TIMESTAMP));
}

void Profiler::PushCpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	s_cpu.pushMarker(_name).m_start = (uint64)Time::GetTimestamp().getRaw();
}

void Profiler::PopCpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	s_cpu.popMarker(_name).m_end = (uint64)Time::GetTimestamp().getRaw();
}

const Profiler::CpuFrame& Profiler::GetCpuFrame(uint _i)
{
	return s_cpu.m_frames[_i];
}

uint Profiler::GetCpuFrameCount()
{
	return s_cpu.m_frames.size();
}

const uint64 Profiler::GetCpuAvgFrameDuration()
{
	return s_cpu.m_avgFrameDuration;
}

//uint Profiler::GetCpuFrameIndex(const CpuFrame& _frame)
//{
//	return (uint)(&_frame - s_cpu.m_frames.data());
//}

const Profiler::CpuMarker& Profiler::GetCpuMarker(uint _i) 
{
	return s_cpu.m_markers.data()[_i % s_cpu.m_markers.capacity()];
}

void Profiler::PushGpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	s_gpu.pushMarker(_name).m_cpuStart = Time::GetTimestamp().getRaw();
	glAssert(glQueryCounter(s_gpuMarkerStartQueries[s_gpu.getCurrentMarkerIndex()], GL_TIMESTAMP));
}

void Profiler::PopGpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	GpuMarker& marker = s_gpu.popMarker(_name);
	glAssert(glQueryCounter(s_gpuMarkerEndQueries[s_gpu.getMarkerIndex(marker)], GL_TIMESTAMP));
}

const Profiler::GpuFrame& Profiler::GetGpuFrame(uint _i)
{
	return s_gpu.m_frames[_i];
}

uint Profiler::GetGpuFrameCount()
{
	return s_gpu.m_frames.size();
}

const uint64 Profiler::GetGpuAvgFrameDuration()
{
	return s_gpu.m_avgFrameDuration;
}

//uint Profiler::GetGpuFrameIndex(const GpuFrame& _frame)
//{
//	return (uint)(&_frame - s_gpu.m_frames.data());
//}

const Profiler::GpuMarker& Profiler::GetGpuMarker(uint _i) 
{
	return s_gpu.m_markers.data()[_i % s_gpu.m_markers.capacity()];
}


// PROTECTED

void Profiler::Init()
{
	s_pause      = false;
}

void Profiler::Shutdown()
{
 // \todo static initialization means we can't delete the queries
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuFrameStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuMarkerStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuMarkerEndQueries)); 
}

// PRIVATE

bool                 Profiler::s_pause;
