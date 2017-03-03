#include <frm/Profiler.h>

#include <frm/gl.h>

#include <apt/log.h>
#include <apt/RingBuffer.h>
#include <apt/Time.h>

#include <imgui/imgui.h>

#include <cstring>

using namespace frm;
using namespace apt;

/******************************************************************************

                            ProfilerViewer

******************************************************************************/
struct ProfilerViewer
{
	struct Colors
	{
		ImU32 kBackground;
		ImU32 kFrame;
		float kFrameHoverAlpha;
		ImU32 kMarkerHover;
		ImU32 kMarkerColors[9];
	};
	static Colors  kColorsGpu;
	static Colors  kColorsCpu;
	static Colors* kColors;


	uint64 mbeg; // all markers draw relative to this time
	float  tbeg, tsize; // viewing region start/size in ms relative to mbeg
	vec2   wbeg, wend, wsize;
	
	ProfilerViewer()
		: tbeg(0.0f)
		, tsize(100.0f)
	{
		kColorsGpu.kBackground      = kColorsCpu.kBackground = ImColor(0xff8e8e8e);
		kColorsGpu.kFrameHoverAlpha = kColorsCpu.kFrameHoverAlpha = 0.1f;
		kColorsGpu.kMarkerHover     = kColorsCpu.kMarkerHover = ImColor(0.3f, 0.3f, 0.3f, 1.0f);
		
		kColorsGpu.kFrame           = ImColor(215,  46, 165);
		kColorsGpu.kMarkerColors[0] = ImColor( 63,  52, 183);
		kColorsGpu.kMarkerColors[1] = ImColor( 95,  54, 183);
		kColorsGpu.kMarkerColors[2] = ImColor(154,  23, 179);
		kColorsGpu.kMarkerColors[3] = ImColor(183,  22, 173);
		kColorsGpu.kMarkerColors[4] = ImColor(159, 107,  73);
		kColorsGpu.kMarkerColors[5] = ImColor(125, 184,  84);
		kColorsGpu.kMarkerColors[6] = ImColor(100, 158,  56);
		kColorsGpu.kMarkerColors[7] = ImColor( 53, 178,  21);
		kColorsGpu.kMarkerColors[8] = ImColor( 76, 156, 100);

		kColorsCpu.kFrame           = ImColor(229, 190,  47);
		kColorsCpu.kMarkerColors[0] = ImColor(183,  52,  76);
		kColorsCpu.kMarkerColors[1] = ImColor(183,  61,  54);
		kColorsCpu.kMarkerColors[2] = ImColor(179, 113,  23);
		kColorsCpu.kMarkerColors[3] = ImColor(182, 148,  22);
		kColorsCpu.kMarkerColors[4] = ImColor( 73, 159,  84);
		kColorsCpu.kMarkerColors[5] = ImColor( 84, 152, 184);
		kColorsCpu.kMarkerColors[6] = ImColor( 56, 127, 158);
		kColorsCpu.kMarkerColors[7] = ImColor( 21,  95, 178);
		kColorsCpu.kMarkerColors[8] = ImColor( 79,  76, 156);
	}

	float timeToWindowX(uint64 _time)
	{
		float ms = (float)Timestamp(_time - mbeg).asMilliseconds();
		ms = (ms - tbeg) / tsize;
		return wbeg.x + ms * wsize.x;
	}

	void draw(bool* _open_)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y / 4), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Profiler", _open_, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Options")) {
				ImGui::EndMenu();
			}
			if (ImGui::SmallButton(Profiler::s_pause ? "Resume" : "Pause")) {
				Profiler::s_pause = !Profiler::s_pause;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset GPU Offset")) {
				Profiler::ResetGpuOffset();
			}

			ImGui::SameLine();
			ImGui::PushItemWidth(128.0f);
			ImGui::SliderFloat("tbeg", &tbeg, 0.0f, 1000.0f);
			ImGui::SameLine();
			ImGui::SliderFloat("tsize", &tsize, 0.0f, 1000.0f);
			ImGui::PopItemWidth();
			ImGui::EndMenuBar();
		}
		
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		
		mbeg = APT_MIN(Profiler::GetCpuFrame(0).m_start, Profiler::GetGpuFrame(0).m_start);
		wbeg  = vec2(ImGui::GetContentRegionMax()) + vec2(ImGui::GetWindowContentRegionMin());
		wsize = ImGui::GetContentRegionMax();
		wend  = vec2(ImGui::GetWindowPos()) + wsize;
		//ImGui::PushClipRect(wbeg, wend, false);

		// GPU ---
		kColors   = &kColorsGpu;
		wbeg      = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowContentRegionMin());
		wsize     = vec2(ImGui::GetContentRegionMax()) - (wbeg - vec2(ImGui::GetWindowPos()));
		wsize.y   = wsize.y * 0.5f;
		wend      = wbeg + wsize;
		
		for (uint i = 0, n = Profiler::GetGpuFrameCount(); i < n; ++i) {
			const Profiler::GpuFrame& frame = Profiler::GetGpuFrame(i);

			float frameX = timeToWindowX(frame.m_start);
			drawList.AddLine(vec2(frameX, wbeg.y), vec2(frameX, wend.y), kColors->kFrame);
		}
		drawList.AddRect(wbeg, wend, kColors->kBackground);

		// CPU ---
		kColors   = &kColorsCpu;
		wbeg.y    = wend.y;
		wend.y    = wbeg.y + wsize.y;
		for (uint i = 0, n = Profiler::GetGpuFrameCount(); i < n; ++i) {
			const Profiler::CpuFrame& frame = Profiler::GetCpuFrame(i);

			float frameX = timeToWindowX(frame.m_start);
			drawList.AddLine(vec2(frameX, wbeg.y), vec2(frameX, wend.y), kColors->kFrame);
		}
		drawList.AddRect(wbeg, wend, kColors->kBackground);

		ImGui::End();
	}
};
ProfilerViewer::Colors  ProfilerViewer::kColorsGpu;
ProfilerViewer::Colors  ProfilerViewer::kColorsCpu;
ProfilerViewer::Colors* ProfilerViewer::kColors;

static ProfilerViewer g_profilerViewer;

void Profiler::ShowProfilerViewer(bool* _open_)
{
	g_profilerViewer.draw(_open_);
}


/******************************************************************************

                               Profiler

******************************************************************************/
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
	 // \hack prime the frame/marker ring buffers and fill them with zeros, basically
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



static uint64 g_gpuTickOffset                                                                           = 0;
static bool   g_gpuInit                                                                                 = true;
static uint   g_gpuFrameQueryRetrieved                                                                  = 0;
static GLuint g_gpuFrameStartQueries[Profiler::kMaxFrameCount]                                          = {};
static GLuint g_gpuMarkerStartQueries[Profiler::kMaxFrameCount * Profiler::kMaxTotalGpuMarkersPerFrame] = {};
static GLuint g_gpuMarkerEndQueries[Profiler::kMaxFrameCount * Profiler::kMaxTotalGpuMarkersPerFrame]   = {};


static uint64 GpuToSystemTicks(GLuint64 _gpuTime)
{
	return (uint64)_gpuTime * Time::GetSystemFrequency() / 1000000000ull; // nanoseconds -> system ticks
}
static uint64 GpuToTimestamp(GLuint64 _gpuTime)
{
	uint64 ret = GpuToSystemTicks(_gpuTime);
	return ret + g_gpuTickOffset;
}


APT_DEFINE_STATIC_INIT(Profiler);

// PUBLIC

void Profiler::NextFrame()
{
	if_unlikely (g_gpuInit) {
		glAssert(glGenQueries(kMaxFrameCount, g_gpuFrameStartQueries));
		glAssert(glGenQueries(kMaxFrameCount * kMaxTotalGpuMarkersPerFrame, g_gpuMarkerStartQueries));
		glAssert(glGenQueries(kMaxFrameCount * kMaxTotalGpuMarkersPerFrame, g_gpuMarkerEndQueries));

		GLint64 gpuTime;
		glAssert(glGetInteger64v(GL_TIMESTAMP, &gpuTime));
		uint64 cpuTicks = Time::GetTimestamp().getRaw();
		uint64 gpuTicks = GpuToSystemTicks(gpuTime);
		APT_ASSERT(gpuTicks < cpuTicks);
		g_gpuTickOffset = cpuTicks - gpuTicks; // \todo is it possible that gpuTicks > cpuTicks?
		g_gpuInit = false;
	}

	if (s_pause) {
		return;
	}

 // CPU: advance frame, get start time/first marker index
	s_cpu.nextFrame().m_start = (uint64)Time::GetTimestamp().getRaw();

 // GPU: retrieve all queries **up to** the last available frame (i.e. when we implicitly know they are available)
	GLint frameAvailable = GL_FALSE;
	uint gpuFrameQueryAvail	= g_gpuFrameQueryRetrieved;
	while (g_gpuFrameQueryRetrieved != s_gpu.getCurrentFrameIndex()) {
		glAssert(glGetQueryObjectiv(g_gpuFrameStartQueries[g_gpuFrameQueryRetrieved], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
		if (frameAvailable == GL_FALSE) {
			break;
		}
		g_gpuFrameQueryRetrieved = (g_gpuFrameQueryRetrieved + 1) % kMaxFrameCount;
	}

	for (; gpuFrameQueryAvail != g_gpuFrameQueryRetrieved; gpuFrameQueryAvail = (gpuFrameQueryAvail + 1) % kMaxFrameCount) {
		GpuFrame& frame = s_gpu.m_frames.data()[gpuFrameQueryAvail];
		GLuint64 gpuTime;
		glAssert(glGetQueryObjectui64v(g_gpuFrameStartQueries[gpuFrameQueryAvail], GL_QUERY_RESULT, &gpuTime));
		frame.m_start = GpuToTimestamp(gpuTime);

		for (uint i = frame.m_first, n = frame.m_first + frame.m_count; i < n; ++i) {
			uint j = i % s_gpu.m_markers.capacity();
			GpuMarker& marker = s_gpu.m_markers.data()[j];

	//glAssert(glGetQueryObjectiv(g_gpuMarkerStartQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(g_gpuMarkerStartQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_start = GpuToTimestamp(gpuTime);
			
	//glAssert(glGetQueryObjectiv(g_gpuMarkerEndQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(g_gpuMarkerEndQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_end = GpuToTimestamp(gpuTime);
		}
	}

	s_gpu.nextFrame().m_start = 0;
	glAssert(glQueryCounter(g_gpuFrameStartQueries[s_gpu.getCurrentFrameIndex()], GL_TIMESTAMP));
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

uint Profiler::GetCpuFrameIndex(const CpuFrame& _frame)
{
	return (uint)(&_frame - s_cpu.m_frames.data());
}

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
	glAssert(glQueryCounter(g_gpuMarkerStartQueries[s_gpu.getCurrentMarkerIndex()], GL_TIMESTAMP));
}

void Profiler::PopGpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	GpuMarker& marker = s_gpu.popMarker(_name);
	glAssert(glQueryCounter(g_gpuMarkerEndQueries[s_gpu.getMarkerIndex(marker)], GL_TIMESTAMP));
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

uint Profiler::GetGpuFrameIndex(const GpuFrame& _frame)
{
	return (uint)(&_frame - s_gpu.m_frames.data());
}

const Profiler::GpuMarker& Profiler::GetGpuMarker(uint _i) 
{
	return s_gpu.m_markers.data()[_i % s_gpu.m_markers.capacity()];
}

void Profiler::ResetGpuOffset()
{
	g_gpuInit = false;
}

// PROTECTED

void Profiler::Init()
{
	s_pause      = false;
}

void Profiler::Shutdown()
{
 // \todo static initialization means we can't delete the queries
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, g_gpuFrameStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, g_gpuMarkerStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, g_gpuMarkerEndQueries)); 
}

// PRIVATE

bool Profiler::s_pause;
