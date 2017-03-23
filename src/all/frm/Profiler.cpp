#include <frm/Profiler.h>

#include <frm/gl.h>
#include <frm/Input.h>

#include <apt/log.h>
#include <apt/RingBuffer.h>
#include <apt/String.h>
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
		ImU32 kMarkerText;
		ImU32 kMarkerTextGray;
		ImU32 kMarkerGray;
	};
	static Colors  kColorsGpu;
	static Colors  kColorsCpu;
	static Colors* kColors;

	bool            m_isMarkerHovered;
	String<64>      m_hoverName;
	ImGuiTextFilter m_markerFilter;

	uint64 m_timeBeg; // all markers draw relative to this time
	uint64 m_timeEnd; // start of the last marker
	float  m_regionBeg, m_regionSize; // viewing region start/size in ms relative to m_timeBeg
	vec2   m_windowBeg, m_windowEnd, m_windowSize;
	
	ProfilerViewer()
		: m_regionBeg(0.0f)
		, m_regionSize(100.0f)
	{
		kColorsGpu.kBackground      = kColorsCpu.kBackground = ImColor(0xff8e8e8e);
		kColorsGpu.kFrameHoverAlpha = kColorsCpu.kFrameHoverAlpha = 0.1f;
		kColorsGpu.kMarkerText      = kColorsCpu.kMarkerText = ImColor(0xffffffff);
		kColorsGpu.kMarkerTextGray  = kColorsCpu.kMarkerTextGray = ImColor(0xff4c4b4b);
		kColorsGpu.kMarkerGray      = kColorsCpu.kMarkerGray = ImColor(0xff383838);

		kColorsGpu.kFrame           = ImColor(0xffb55f29);
		kColorsCpu.kFrame           = ImColor(0xff0087db);
	}

	const char* timeToStr(uint64 _time)
	{
		static String<sizeof("999.999ms\0")> s_buf; // safe because we're single threaded
		Timestamp t(_time);
		float x = (float)t.asSeconds();
		if (x >= 1.0f) {
			s_buf.setf("%1.3fs", x);
		} else {
			x = (float)t.asMilliseconds();
			if (x >= 0.1f) {
				s_buf.setf("%1.2fms", x);
			} else {
				x = (float)t.asMicroseconds();
				s_buf.setf("%1.0fus", x);
			}
		}
		return s_buf;
	}

	float timeToWindowX(uint64 _time)
	{
		float ms = (float)Timestamp(_time - m_timeBeg).asMilliseconds();
		ms = (ms - m_regionBeg) / m_regionSize;
		return m_windowBeg.x + ms * m_windowSize.x;
	}

	bool isMouseInside(const vec2& _rectMin, const vec2& _rectMax)
	{
		const vec2 mpos = ImGui::GetIO().MousePos;
		return
			(mpos.x > _rectMin.x && mpos.x < _rectMax.x) &&
			(mpos.y > _rectMin.y && mpos.y < _rectMax.y);
	}

	bool cullFrame(const Profiler::Frame& _frame, const Profiler::Frame& _frameNext)
	{
		float frameBeg = timeToWindowX(_frame.m_start);
		float frameEnd = timeToWindowX(_frameNext.m_start);
		return frameBeg > m_windowEnd.x || frameEnd < m_windowBeg.x;
	}
	
	void drawFrameBounds(const Profiler::Frame& _frame, const Profiler::Frame& _frameNext)
	{
		float frameBeg = timeToWindowX(_frame.m_start);
		float frameEnd = timeToWindowX(_frameNext.m_start);
		frameBeg = APT_MAX(frameBeg, m_windowBeg.x);
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		if (isMouseInside(vec2(frameBeg, m_windowBeg.y), vec2(frameEnd, m_windowEnd.y))) {
			drawList.AddRectFilled(vec2(frameBeg, m_windowBeg.y), vec2(frameEnd, m_windowEnd.y), IM_COLOR_ALPHA(kColors->kFrame, kColors->kFrameHoverAlpha));
			drawList.AddText(vec2(frameBeg + 4.0f, m_windowBeg.y + 2.0f), kColors->kFrame, timeToStr(_frameNext.m_start - _frame.m_start));
		}
		drawList.AddLine(vec2(frameBeg, m_windowBeg.y), vec2(frameBeg, m_windowEnd.y), kColors->kFrame);
	}

	// Return true if the marker is hovered.
	bool drawFrameMarker(const Profiler::Marker& _marker, float _frameEndX)
	{
		float markerHeight = ImGui::GetItemsLineHeightWithSpacing();
		vec2 markerBeg = vec2(timeToWindowX(_marker.m_start), m_windowBeg.y + markerHeight * (float)_marker.m_depth);
		vec2 markerEnd = vec2(timeToWindowX(_marker.m_end) - 1.0f, markerBeg.y + markerHeight);
		if (markerBeg.x > m_windowEnd.x || markerEnd.x < m_windowBeg.x) {
			return false;
		}
		
		markerBeg.x = APT_MAX(markerBeg.x, m_windowBeg.x);
		markerEnd.x = APT_MIN(APT_MIN(markerEnd.x, m_windowEnd.x), _frameEndX);

		float markerWidth = markerEnd.x - markerBeg.x;
		if (markerWidth < 2.0f) {
		 // \todo push culled markers into a list, display on the tooltip
			return false;
		}
		
		vec2 wpos = ImGui::GetWindowPos();
		ImGui::SetCursorPosX(markerBeg.x - wpos.x);
		ImGui::SetCursorPosY(markerBeg.y - wpos.y);

		ImU32 buttonColor = kColors->kMarkerGray;
		ImU32 textColor = kColors->kMarkerTextGray;
		
	 // if the marker is hovered and no filter is set, highlight the marker
		bool hoverMatch = true;
		if (!m_markerFilter.IsActive() && !m_hoverName.isEmpty()) {
			hoverMatch = m_hoverName == _marker.m_name;
		}
		if (hoverMatch && m_markerFilter.PassFilter(_marker.m_name)) {
			buttonColor =  kColors->kFrame;
			textColor = kColors->kMarkerText;
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImColor(buttonColor));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(buttonColor));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(buttonColor));
		ImGui::PushStyleColor(ImGuiCol_Text, ImColor(textColor)); 

		ImGui::Button(_marker.m_name, ImVec2(markerWidth, markerHeight - 1.0f));
		
		ImGui::PopStyleColor(4);

		if (isMouseInside(markerBeg, markerEnd)) {
			m_hoverName.set(_marker.m_name);
			m_isMarkerHovered = true;
			return true;
		}
		return false;
	}

	void draw(bool* _open_)
	{
		String<sizeof("999.999ms\0")> str;

		m_isMarkerHovered = false;

		m_timeBeg = APT_MIN(Profiler::GetCpuFrame(0).m_start, Profiler::GetGpuFrame(0).m_start);
		m_timeEnd = APT_MAX(Profiler::GetCpuFrame(Profiler::GetCpuFrameCount() - 1).m_start, Profiler::GetGpuFrame(Profiler::GetGpuFrameCount() - 1).m_start);
		float timeRange = (float)Timestamp(m_timeEnd - m_timeBeg).asMilliseconds();

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y / 4), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Profiler", _open_, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Options")) {
				if (ImGui::MenuItem("Reset GPU Offset")) {
					Profiler::ResetGpuOffset();
				}
				ImGui::EndMenu();
			}
			if (ImGui::SmallButton(Profiler::s_pause ? "Resume" : "Pause")) {
				Profiler::s_pause = !Profiler::s_pause;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Fit")) {
				float spacing = timeRange * 0.01f;
				m_regionSize = timeRange + spacing * 2.0f;
				m_regionBeg = - spacing;
			}
			ImGui::SameLine();
			m_markerFilter.Draw("Filter", 160.0f);

			ImGui::EndMenuBar();
		}
		
		bool regionBegChanged = false; // don't set from scrollbar when dragging the region view
		if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered()) {
			float wx = ImGui::GetWindowContentRegionMax().x;
		 // zoom
			float zoom = (io.MouseWheel * m_regionSize * 0.1f);
			float before = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize;
			m_regionSize = APT_MAX(m_regionSize - zoom, 0.1f);
			float after = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize;
			m_regionBeg += (before - after);
			regionBegChanged = fabs(before - after) > FLT_EPSILON;

		 // pan
			if (io.MouseDown[2]) {
				regionBegChanged = true;
				m_regionBeg -= io.MouseDelta.x / wx * m_regionSize;
			}

		 // shortcuts
			if (Input::GetKeyboard()->wasPressed(Keyboard::Key_P)) {
				Profiler::s_pause = !Profiler::s_pause;
			}
		}

		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		// GPU ---
		kColors          = &kColorsGpu;
		m_windowBeg      = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowContentRegionMin());
		float infoX      = m_windowBeg.x; // where to draw the CPU/GPU global info
		m_windowBeg.x   += ImGui::GetFontSize() * 4.0f;
		m_windowSize     = vec2(ImGui::GetContentRegionMax()) - (m_windowBeg - vec2(ImGui::GetWindowPos()));
		m_windowSize    -= ImGui::GetItemsLineHeightWithSpacing();
		m_windowSize.y   = m_windowSize.y * 0.5f;
		m_windowEnd      = m_windowBeg + m_windowSize;
		
		str.setf("GPU\n%s", timeToStr(Profiler::GetGpuAvgFrameDuration()));
		drawList.AddText(vec2(infoX, m_windowBeg.y), kColors->kBackground, str);
		
		ImGui::PushClipRect(m_windowBeg, m_windowEnd, false);
		
		// \todo this skips drawing the newest frame's data, add an extra step to draw those markers?
		for (uint i = 0, n = Profiler::GetGpuFrameCount() - 1; i < n; ++i) {
			const Profiler::GpuFrame& frame = Profiler::GetGpuFrame(i);
			const Profiler::GpuFrame& frameNext = Profiler::GetGpuFrame(i + 1);
			if (cullFrame(frame, frameNext)) {
				continue;
			}
			
		 // markers
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			m_windowBeg.y += ImGui::GetFontSize() + 2.0f; // space for the frame time
			float fend = timeToWindowX(frameNext.m_start);
			for (uint j = frame.m_first, m = frame.m_first + frame.m_count; j < m; ++j) {
				const Profiler::GpuMarker& marker = Profiler::GetGpuMarker(j);
				if (drawFrameMarker(marker, fend)) {
					vec2 lbeg = vec2(timeToWindowX(marker.m_start), m_windowBeg.y + ImGui::GetItemsLineHeightWithSpacing() * (float)marker.m_depth);
					lbeg.y += ImGui::GetItemsLineHeightWithSpacing() * 0.5f;
					vec2 lend = vec2(timeToWindowX(marker.m_cpuStart), m_windowBeg.y + m_windowSize.y);
					drawList.AddLine(lbeg, lend, kColors->kFrame, 2.0f);
					ImGui::BeginTooltip();
						ImGui::TextColored(ImColor(kColors->kFrame), marker.m_name);
						ImGui::Text("Duration: %s", timeToStr(marker.m_end - marker.m_start));
						ImGui::Text("Latency:  %s", timeToStr(marker.m_start - marker.m_cpuStart));
					ImGui::EndTooltip();
				}
			}
			m_windowBeg.y -= ImGui::GetFontSize() + 2.0f;
			ImGui::PopStyleVar();

			drawFrameBounds(frame, frameNext);
		}
		ImGui::PopClipRect();
		drawList.AddRect(m_windowBeg, m_windowEnd, kColors->kBackground);

		// CPU ---
		kColors       = &kColorsCpu;
		m_windowBeg.y = m_windowEnd.y + 1.0f;
		m_windowEnd.y = m_windowBeg.y + m_windowSize.y + 1.0f;

		str.setf("CPU\n%s", timeToStr(Profiler::GetCpuAvgFrameDuration()));
		drawList.AddText(vec2(infoX, m_windowBeg.y), kColors->kBackground, str);

		ImGui::PushClipRect(m_windowBeg, m_windowEnd, false);
		for (uint i = 0, n = Profiler::GetGpuFrameCount() - 1; i < n; ++i) {
			const Profiler::CpuFrame& frame = Profiler::GetCpuFrame(i);
			const Profiler::CpuFrame& frameNext = Profiler::GetCpuFrame(i + 1);
			if (cullFrame(frame, frameNext)) {
				continue;
			}

		 // markers
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			m_windowBeg.y += ImGui::GetFontSize() + 2.0f; // space for the frame time
			float fend = timeToWindowX(frameNext.m_start);
			for (uint j = frame.m_first, m = frame.m_first + frame.m_count; j < m; ++j) {
				const Profiler::CpuMarker& marker = Profiler::GetCpuMarker(j);
				if (drawFrameMarker(marker, fend)) {
					ImGui::BeginTooltip();
						ImGui::TextColored(ImColor(kColors->kFrame), marker.m_name);
						ImGui::Text("Duration: %s", timeToStr(marker.m_end - marker.m_start));
					ImGui::EndTooltip();
				}
			}
			m_windowBeg.y -= ImGui::GetFontSize() + 2.0f;
			ImGui::PopStyleVar();

			drawFrameBounds(frame, frameNext);
		}
		ImGui::PopClipRect();
		drawList.AddRect(m_windowBeg, m_windowEnd, kColors->kBackground);

		float regionSizePx = timeRange / m_regionSize * m_windowSize.x;
		ImGui::SetNextWindowContentSize(ImVec2(regionSizePx, 0.0f));
		ImGui::SetCursorPosX(m_windowBeg.x - ImGui::GetWindowPos().x);
		ImGui::SetCursorPosY(m_windowEnd.y - ImGui::GetWindowPos().y);
		ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(IM_COL32_BLACK_TRANS));
		bool refocusMainWindow = ImGui::IsWindowFocused();
		ImGui::BeginChild("hscroll", ImVec2(m_windowSize.x, ImGui::GetStyle().ScrollbarSize), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (regionBegChanged) {
				ImGui::SetScrollX(m_regionBeg / timeRange * regionSizePx);
			} else {
				m_regionBeg = ImGui::GetScrollX() / regionSizePx * timeRange;
			}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (refocusMainWindow) {
			ImGui::SetWindowFocus();
		}

		ImGui::End();

		if (!m_isMarkerHovered) {
			m_hoverName.clear();
		}
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



static uint64 g_gpuTickOffset; // convert gpu time -> cpu time; note that this value can be arbitrarily large as the clocks aren't necessarily relative to the same moment
static uint   g_gpuFrameQueryRetrieved;
static bool   g_gpuInit = true;
static GLuint g_gpuFrameStartQueries[Profiler::kMaxFrameCount] = {};
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
		g_gpuInit = false;

		ResetGpuOffset();
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

uint64 Profiler::GetGpuAvgFrameDuration()
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
	GLint64 gpuTime;
	glAssert(glGetInteger64v(GL_TIMESTAMP, &gpuTime));
	uint64 cpuTicks = Time::GetTimestamp().getRaw();
	uint64 gpuTicks = GpuToSystemTicks(gpuTime);
	APT_ASSERT(gpuTicks < cpuTicks);
	g_gpuTickOffset = cpuTicks - gpuTicks; // \todo is it possible that gpuTicks > cpuTicks?
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
