#include <frm/Profiler.h>

#include <frm/gl.h>
#include <frm/AppSample.h>
#include <frm/Input.h>

#include <apt/log.h>
#include <apt/RingBuffer.h>
#include <apt/String.h>
#include <apt/Time.h>

#include <imgui/imgui.h>
#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

/******************************************************************************

                            ProfilerViewer

******************************************************************************/
struct ProfilerViewer
{
	enum View
	{
		View_Markers,
		View_Values,
		View_Count
	};
	int m_view;

	struct Colors
	{
		ImU32 kBackground;
		ImU32 kFrame;
		ImU32 kFrameSystem; // markers starting with '#'
		float kFrameHoverAlpha;
		ImU32 kMarkerText;
		ImU32 kMarkerTextGray;
		ImU32 kMarkerGray;
	};
	static Colors  kColorsGpu;
	static Colors  kColorsCpu;
	static Colors* kColors;

	bool            m_isMarkerHovered;
	uint64          m_hoverFrameId;
	String<64>      m_hoverName;
	ImGuiTextFilter m_filter;
	bool            m_showHidden;

	uint64 m_timeBeg; // all markers draw relative to this time
	uint64 m_timeEnd; // start of the last marker
	float  m_regionBeg, m_regionSize; // viewing region start/size in ms relative to m_timeBeg
	bool   m_regionChanged; // prevent update from scrollbar e.g. when zooming
	vec2   m_windowBeg, m_windowEnd, m_windowSize;
		
	struct Tracker
	{
		uint64      m_durations[100];
		uint64      m_avgDuration;
		uint64      m_minDuration;
		uint64      m_maxDuration;
		int         m_markerCount;
		const char* m_name;
	};
	
	ProfilerViewer()
		: m_view(View_Markers)
		, m_regionBeg(0.0f)
		, m_regionSize(100.0f)
		, m_showHidden(true)
	{
		kColorsGpu.kBackground      = kColorsCpu.kBackground = ImColor(0xff8e8e8e);
		kColorsGpu.kFrameHoverAlpha = kColorsCpu.kFrameHoverAlpha = 0.1f;
		kColorsGpu.kMarkerText      = kColorsCpu.kMarkerText = ImColor(0xffffffff);
		kColorsGpu.kMarkerTextGray  = kColorsCpu.kMarkerTextGray = ImColor(0xff4c4b4b);
		kColorsGpu.kMarkerGray      = kColorsCpu.kMarkerGray = ImColor(0xff383838);

		kColorsGpu.kFrame           = ImColor(0xffb55f29);
		kColorsGpu.kFrameSystem     = ImColor(0xff91694f);
		kColorsCpu.kFrame           = ImColor(0xff0087db);
		kColorsCpu.kFrameSystem     = ImColor(0xff428dbc);
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
		return (const char*)s_buf;
	}
	const char* idToStr(uint64 _id)
	{
		static String<sizeof("#9999999")> s_buf;
		s_buf.setf("#%07llu", _id);
		return (const char*)s_buf;
	}

	float timeToWindowX(uint64 _time)
	{
		float ms = (float)Timestamp(_time - m_timeBeg).asMilliseconds();
		ms = (ms - m_regionBeg) / m_regionSize;
		return m_windowBeg.x + ms * m_windowSize.x;
	}

	void setRegion(uint64 _beg, uint64 _end)
	{
		m_regionBeg = (float)Timestamp(_beg - m_timeBeg).asMilliseconds();
		m_regionSize = (float)Timestamp(_end - _beg).asMilliseconds();
		m_regionChanged = true;
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
		float frameBeg = timeToWindowX(_frame.m_startTime);
		float frameEnd = timeToWindowX(_frameNext.m_startTime);
		return frameBeg > m_windowEnd.x || frameEnd < m_windowBeg.x;
	}
	
	void drawFrameBounds(const Profiler::Frame& _frame, const Profiler::Frame& _frameNext)
	{
		float frameBeg = timeToWindowX(_frame.m_startTime);
		float frameEnd = timeToWindowX(_frameNext.m_startTime);
		frameBeg = floorf(APT_MAX(frameBeg, m_windowBeg.x));
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		if (ImGui::IsWindowFocused() && (m_hoverFrameId == _frame.m_id || isMouseInside(vec2(frameBeg, m_windowBeg.y), vec2(frameEnd, m_windowEnd.y)))) {
			drawList.AddRectFilled(vec2(frameBeg, m_windowBeg.y), vec2(frameEnd, m_windowEnd.y), IM_COLOR_ALPHA(kColors->kFrame, kColors->kFrameHoverAlpha));
			drawList.AddText(vec2(frameBeg + 4.0f, m_windowBeg.y + 2.0f), kColors->kFrame, timeToStr(_frameNext.m_startTime - _frame.m_startTime));
			m_hoverFrameId = _frame.m_id;
		}
		float fontSize = ImGui::GetFontSize();
		if ((frameEnd - frameBeg) > fontSize * 7.0f) {
			drawList.AddText(vec2(frameBeg + 4.0f, m_windowEnd.y - fontSize - 2.0f), kColors->kMarkerTextGray, idToStr(_frame.m_id));
		}
		drawList.AddLine(vec2(frameBeg, m_windowBeg.y), vec2(frameBeg, m_windowEnd.y), kColors->kFrame);
	}

	// Return true if the marker is hovered.
	bool drawFrameMarker(const Profiler::Marker& _marker, float _frameEndX)
	{
		float markerHeight = ImGui::GetItemsLineHeightWithSpacing();
		vec2 markerBeg = vec2(timeToWindowX(_marker.m_startTime), m_windowBeg.y + markerHeight * (float)_marker.m_markerDepth);
		vec2 markerEnd = vec2(timeToWindowX(_marker.m_endTime) - 1.0f, markerBeg.y + markerHeight);
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
		ImGui::SetCursorPosX(floorf(markerBeg.x - wpos.x));
		ImGui::SetCursorPosY(floorf(markerBeg.y - wpos.y));

		ImU32 buttonColor = kColors->kMarkerGray;
		ImU32 textColor = kColors->kMarkerTextGray;
		
	 // if the marker is hovered and no filter is set, highlight the marker
		bool hoverMatch = true;
		if (!m_filter.IsActive() && !m_hoverName.isEmpty()) {
			hoverMatch = m_hoverName == _marker.m_name;
		}
		if (hoverMatch && m_filter.PassFilter(_marker.m_name)) {
			textColor = kColors->kMarkerText;
			buttonColor = kColors->kFrame;
			if (_marker.m_name[0] == '#') {
				buttonColor = kColors->kFrameSystem;
			}
		}
		ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);
		ImGui::PushStyleColor(ImGuiCol_Text, textColor); 

		ImGui::Button(_marker.m_name, ImVec2(floorf(markerWidth), floorf(markerHeight) - 1.0f));
		
		ImGui::PopStyleColor(4);

		ImGui::PushID(&_marker);
		if (ImGui::BeginPopup("marker context")) {
			if (_marker.m_isCpuMarker) {
				if (Profiler::IsCpuMarkerTracked(_marker.m_name)) {
					if (ImGui::MenuItem("Untrack")) {
						Profiler::UntrackCpuMarker(_marker.m_name);
					}
				} else if (ImGui::MenuItem("Track")) {
					Profiler::TrackCpuMarker(_marker.m_name);
				}
			} else {
				if (Profiler::IsGpuMarkerTracked(_marker.m_name)) {
					if (ImGui::MenuItem("Untrack")) {
						Profiler::UntrackGpuMarker(_marker.m_name);
					}
				} else if (ImGui::MenuItem("Track")) {
					Profiler::TrackGpuMarker(_marker.m_name);
				}
			}
			ImGui::EndPopup();
			ImGui::PopID();
			return false; // prevent tooltip
		}
		ImGui::PopID();

		if (ImGui::IsWindowFocused() && isMouseInside(markerBeg, markerEnd)) {
			m_hoverName.set(_marker.m_name);
			m_isMarkerHovered = true;

			ImGuiIO& io = ImGui::GetIO();

		 // double-click to zoom on a marker
			if (io.MouseDoubleClicked[0]) {
				setRegion(_marker.m_startTime, _marker.m_endTime);
			}

		 // right-click = menu
			if (Profiler::s_pause && io.MouseClicked[1]) {
				ImGui::PushID(&_marker);
					ImGui::OpenPopup("marker context");
				ImGui::PopID();
			}
			
			return true;
		}
		return false;
	}

	void drawMarkers()
	{
		String<sizeof("999.999ms\0")> str;

		m_isMarkerHovered = false;

		m_timeBeg = APT_MIN(Profiler::GetCpuFrame(0).m_startTime, Profiler::GetGpuFrame(0).m_startTime);
		m_timeEnd = APT_MAX(Profiler::GetCpuFrame(Profiler::GetCpuFrameCount() - 1).m_startTime, Profiler::GetGpuFrame(Profiler::GetGpuFrameCount() - 1).m_startTime);
		float timeRange = (float)Timestamp(m_timeEnd - m_timeBeg).asMilliseconds();

		ImGuiIO& io = ImGui::GetIO();

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Options")) {
				if (ImGui::MenuItem("Reset GPU Offset")) {
					Profiler::ResetGpuOffset();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
		
		if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered()) {
			float wx = ImGui::GetWindowContentRegionMax().x;
		 // zoom
			float zoom = (io.MouseWheel * m_regionSize * 0.1f);
			float before = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize;
			m_regionSize = APT_MAX(m_regionSize - zoom, 0.1f);
			float after = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize;
			m_regionBeg += (before - after);
			m_regionChanged = fabs(before - after) > FLT_EPSILON;

		 // pan
			if (io.MouseDown[2]) {
				m_regionChanged = true;
				m_regionBeg -= io.MouseDelta.x / wx * m_regionSize;
			}
		} else {
			m_hoverFrameId = 0;
		}

		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		// GPU ---
		kColors          = &kColorsGpu;
		m_windowBeg      = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowContentRegionMin());
		m_windowBeg.y   += ImGui::GetItemsLineHeightWithSpacing();
		float infoX      = m_windowBeg.x; // where to draw the CPU/GPU global info
		m_windowBeg.x   += ImGui::GetFontSize() * 4.0f;
		m_windowSize     = vec2(ImGui::GetContentRegionMax()) - (m_windowBeg - vec2(ImGui::GetWindowPos()));
		m_windowSize    -= ImGui::GetItemsLineHeightWithSpacing();
		m_windowSize.y   = m_windowSize.y * 0.5f;
		m_windowEnd      = m_windowBeg + m_windowSize;
		
		ImGui::SetCursorPosX(m_windowBeg.x - ImGui::GetWindowPos().x);
		if (ImGui::SmallButton("Fit")) {
			float spacing = timeRange * 0.01f;
			m_regionSize = timeRange + spacing * 2.0f;
			m_regionBeg = - spacing;
		}

		str.setf("GPU\n%s", timeToStr(Profiler::GetGpuAvgFrameDuration()));
		drawList.AddText(vec2(infoX, m_windowBeg.y), kColors->kBackground, (const char*)str);
		
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
			float fend = timeToWindowX(frameNext.m_startTime);
			for (uint j = frame.m_firstMarker, m = frame.m_firstMarker + frame.m_markerCount; j < m; ++j) {
				const Profiler::GpuMarker& marker = Profiler::GetGpuMarker(j);
				if (!m_showHidden && marker.m_name[0] == '#') {
					continue;
				}
				if (drawFrameMarker(marker, fend)) {
					vec2 lbeg = vec2(timeToWindowX(marker.m_startTime), m_windowBeg.y + ImGui::GetItemsLineHeightWithSpacing() * (float)marker.m_markerDepth);
					lbeg.y += ImGui::GetItemsLineHeightWithSpacing() * 0.5f;
					vec2 lend = vec2(timeToWindowX(marker.m_cpuStart), m_windowBeg.y + m_windowSize.y);
					drawList.AddLine(lbeg, lend, kColors->kFrame, 2.0f);
					ImGui::BeginTooltip();
						ImGui::TextColored(ImColor(kColors->kFrame), marker.m_name);
						ImGui::Text("Duration: %s", timeToStr(marker.m_endTime - marker.m_startTime));
						ImGui::Text("Latency:  %s", timeToStr(marker.m_startTime - marker.m_cpuStart));
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
		drawList.AddText(vec2(infoX, m_windowBeg.y), kColors->kBackground, (const char*)str);

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
			float fend = timeToWindowX(frameNext.m_startTime);
			for (uint j = frame.m_firstMarker, m = frame.m_firstMarker + frame.m_markerCount; j < m; ++j) {
				const Profiler::CpuMarker& marker = Profiler::GetCpuMarker(j);
				if (!m_showHidden && marker.m_name[0] == '#') {
					continue;
				}
				if (drawFrameMarker(marker, fend)) {
					ImGui::BeginTooltip();
						ImGui::TextColored(ImColor(kColors->kFrame), marker.m_name);
						ImGui::Text("Duration: %s", timeToStr(marker.m_endTime - marker.m_startTime));
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
		ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, IM_COL32_BLACK_TRANS);
		bool refocusMainWindow = ImGui::IsWindowFocused();
		ImGui::BeginChild("hscroll", ImVec2(m_windowSize.x, ImGui::GetStyle().ScrollbarSize), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (m_regionChanged) {
				ImGui::SetScrollX(m_regionBeg / timeRange * regionSizePx);
				m_regionChanged = false;
			} else {
				m_regionBeg = ImGui::GetScrollX() / regionSizePx * timeRange;
			}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (refocusMainWindow) {
			ImGui::SetWindowFocus();
		}

		if (!m_isMarkerHovered) {
			m_hoverName.clear();
		}
	}

	void drawValues()
	{
		vec2 graphSize = vec2(150.0f, 64.0f);
		vec2 windowSize = ImGui::GetWindowSize();

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 0.0f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

	 // CPU values
		ImGui::PushStyleColor(ImGuiCol_PlotLines, kColorsCpu.kFrame);
		ImGui::PushStyleColor(ImGuiCol_Text, kColorsCpu.kMarkerText);
		for (uint i = 0, n = Profiler::GetCpuValueCount(); i < n; ++i) {
			const Profiler::Value& val = Profiler::GetCpuValue(i);
			if (!m_showHidden && val.m_name[0] == '#') {
				continue;
			}
			if (!m_filter.PassFilter(val.m_name)) {
				continue;
			}
			int off = (int)(&val.m_history->front() - val.m_history->data());
			ImGui::PlotLines("", val.m_history->data(), (int)val.m_history->size(), off, val.m_name, FLT_MAX, FLT_MAX, graphSize);
			
			ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::PopStyleColor(2);

	 // GPU values
		ImGui::PushStyleColor(ImGuiCol_PlotLines, kColorsGpu.kFrame);
		ImGui::PushStyleColor(ImGuiCol_Text, kColorsGpu.kMarkerText);
		for (uint i = 0, n = Profiler::GetGpuValueCount(); i < n; ++i) {
			const Profiler::Value& val = Profiler::GetGpuValue(i);
			if (!m_showHidden && val.m_name[0] == '#') {
				continue;
			}
			if (!m_filter.PassFilter(val.m_name)) {
				continue;
			}
			int off = (int)(&val.m_history->front() - val.m_history->data());
			ImGui::PlotLines("", val.m_history->data(), (int)val.m_history->size(), off, val.m_name, FLT_MAX, FLT_MAX, graphSize);
			
			ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::PopStyleColor(2);

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
	}

	void draw(bool *_isOpen_)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y / 4), ImGuiCond_FirstUseEver);
		ImGui::Begin("Profiler", _isOpen_, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar()) {
				if (ImGui::BeginMenu("View")) {
					if (ImGui::MenuItem("Markers")) {
						m_view = View_Markers;
					}
					if (ImGui::MenuItem("Values")) {
						m_view = View_Values;
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
		
			switch (m_view) {
				case View_Values:
					drawValues();
					break;
				case View_Markers:
				default:
					drawMarkers();
					break;
			};

			if (ImGui::BeginMenuBar()) {			
				m_filter.Draw("Filter", 160.0f);
				ImGui::SameLine();
				ImGui::Checkbox("Show Hidden", &m_showHidden);
				ImGui::SameLine();
				if (ImGui::SmallButton(Profiler::s_pause ? "Resume" : "Pause")) {
					Profiler::s_pause = !Profiler::s_pause;
				}
				ImGui::EndMenuBar();
			}
		ImGui::End();

	 // shortcuts
		Keyboard* keyb = Input::GetKeyboard();
		if (keyb->isDown(Keyboard::Key_LCtrl) && keyb->isDown(Keyboard::Key_LShift) && keyb->wasPressed(Keyboard::Key_P)) {
			Profiler::s_pause = !Profiler::s_pause;
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
		uint64 prevStart = m_frames[0].m_startTime; 
		uint i = 1;
		for ( ; i < m_frames.size(); ++i) {
			tFrame& frame = m_frames[i];
			if (frame.m_startTime == 0) { // means the frame was invalid (i.e. gpu query was unavailable)
				break;
			}
			uint64 thisStart = m_frames[i].m_startTime;
			m_avgFrameDuration += thisStart - prevStart;
			prevStart = thisStart;
		}
		m_avgFrameDuration /= i;

	 // advance to next frame
		uint first = m_frames.back().m_firstMarker + m_frames.back().m_markerCount;
		m_frames.push_back(tFrame());
		m_frames.back().m_id = AppSample::GetCurrent()->getFrameIndex();
		m_frames.back().m_firstMarker = first;
		m_frames.back().m_markerCount = 0;
		return m_frames.back(); 
	}

	tMarker& pushMarker(const char* _name)
	{ 
		APT_ASSERT(m_markerStackTop != Profiler::kMaxDepth);
		m_markers.push_back(tMarker()); 
		m_markerStack[m_markerStackTop++] = getCurrentMarkerIndex();
		m_markers.back().m_name = _name;
		m_markers.back().m_markerDepth = (uint8)m_markerStackTop - 1;
		m_markers.back().m_isCpuMarker = false;
		m_frames.back().m_markerCount++;
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

static eastl::vector<const char*> s_cpuTrackedMarkers;
static eastl::vector<const char*> s_gpuTrackedMarkers;

static eastl::vector<Profiler::Value> s_cpuValues;
static eastl::vector<Profiler::Value> s_gpuValues;

static uint64 s_gpuTickOffset; // convert gpu time -> cpu time; note that this value can be arbitrarily large as the clocks aren't necessarily relative to the same moment
static uint   s_gpuFrameQueryRetrieved;
static bool   s_gpuInit = true;
static GLuint s_gpuFrameStartQueries[Profiler::kMaxFrameCount] = {};
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
		s_gpuInit = false;

		ResetGpuOffset();
	}

	if (s_pause) {
		return;
	}

 // track markers
	for (auto& trackedName : s_cpuTrackedMarkers) {
		uint i = s_cpu.m_frames.back().m_firstMarker;
		uint n = i + s_cpu.m_frames.back().m_markerCount;
		for (; i < n; ++i) {
			uint j = i % s_cpu.m_markers.capacity();
			CpuMarker& marker = s_cpu.m_markers.data()[j];
			if (strcmp(trackedName, marker.m_name) == 0) {
				CpuValue(trackedName, (float)Timestamp(marker.m_endTime - marker.m_startTime).asMilliseconds());
				break;
			}
		}
	}
	
 // reset value counters
	for (auto& cpuValue : s_cpuValues) {
		cpuValue.m_avg = cpuValue.m_accum / (float)cpuValue.m_count;
		cpuValue.m_history->push_back(cpuValue.m_avg);
		cpuValue.m_count = 0;
		cpuValue.m_accum = 0.0f;
	}
	for (auto& gpuValue : s_gpuValues) {
		gpuValue.m_avg = gpuValue.m_accum / (float)gpuValue.m_count;
		gpuValue.m_history->push_back(gpuValue.m_avg);
		gpuValue.m_count = 0;
		gpuValue.m_accum = 0.0f;
	}

 // CPU: advance frame, get start time/first marker index
	s_cpu.nextFrame().m_startTime = (uint64)Time::GetTimestamp().getRaw();

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
		frame.m_startTime = GpuToTimestamp(gpuTime);

		for (uint i = frame.m_firstMarker, n = frame.m_firstMarker + frame.m_markerCount; i < n; ++i) {
			uint j = i % s_gpu.m_markers.capacity();
			GpuMarker& marker = s_gpu.m_markers.data()[j];

	//glAssert(glGetQueryObjectiv(s_gpuMarkerStartQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(s_gpuMarkerStartQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_startTime = GpuToTimestamp(gpuTime);
			
	//glAssert(glGetQueryObjectiv(s_gpuMarkerEndQueries[j], GL_QUERY_RESULT_AVAILABLE, &frameAvailable));
	//APT_ASSERT(frameAvailable == GL_TRUE);
			glAssert(glGetQueryObjectui64v(s_gpuMarkerEndQueries[j], GL_QUERY_RESULT, &gpuTime));
			marker.m_endTime = GpuToTimestamp(gpuTime);
		}

	 // track markers
		for (auto& trackedName : s_gpuTrackedMarkers) {
			uint i = frame.m_firstMarker;
			uint n = i + frame.m_markerCount;
			for (; i < n; ++i) {
				uint j = i % s_gpu.m_markers.capacity();
				GpuMarker& marker = s_gpu.m_markers.data()[j];
				if (strcmp(trackedName, marker.m_name) == 0) {
					GpuValue(trackedName, (float)Timestamp(marker.m_endTime - marker.m_startTime).asMilliseconds());
					break;
				}
			}
		}
	}

	s_gpu.nextFrame().m_startTime = 0;
	glAssert(glQueryCounter(s_gpuFrameStartQueries[s_gpu.getCurrentFrameIndex()], GL_TIMESTAMP));
}

void Profiler::PushCpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	CpuMarker& marker = s_cpu.pushMarker(_name);
	marker.m_startTime = (uint64)Time::GetTimestamp().getRaw();
	marker.m_isCpuMarker = true;
}

void Profiler::PopCpuMarker(const char* _name)
{
	if (s_pause) {
		return;
	}
	s_cpu.popMarker(_name).m_endTime = (uint64)Time::GetTimestamp().getRaw();
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

void Profiler::TrackCpuMarker(const char* _name)
{
	for (auto& trackedName : s_cpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			return;
		}
	}
	s_cpuTrackedMarkers.push_back(_name);
}

void Profiler::UntrackCpuMarker(const char* _name)
{
	for (auto& trackedName : s_cpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			eastl::swap(trackedName, s_cpuTrackedMarkers.back());
			s_cpuTrackedMarkers.pop_back();
			return;
		}
	}
}

bool Profiler::IsCpuMarkerTracked(const char* _name)
{
	for (auto& trackedName : s_cpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			return true;
		}
	}
	return false;
}

void Profiler::CpuValue(const char* _name, float _value, uint _historySize)
{
	for (auto& v : s_cpuValues) {
		if (strcmp(_name, v.m_name) == 0) {
			++v.m_count;
			v.m_accum += _value;
			v.m_max = APT_MAX(v.m_max, _value);
			v.m_min = APT_MIN(v.m_min, _value);
			return;
		}
	}
	s_cpuValues.push_back();
	s_cpuValues.back().m_name  = _name;
	s_cpuValues.back().m_count = 1;
	s_cpuValues.back().m_accum = _value;
	s_cpuValues.back().m_min   = _value;
	s_cpuValues.back().m_max   = _value;
	s_cpuValues.back().m_history = new RingBuffer<float>(_historySize);
}

uint Profiler::GetCpuValueCount()
{
	return s_cpuValues.size();
}

const Profiler::Value& Profiler::GetCpuValue(uint _i)
{
	return s_cpuValues[_i];
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

void Profiler::TrackGpuMarker(const char* _name)
{
	for (auto& trackedName : s_gpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			return;
		}
	}
	s_gpuTrackedMarkers.push_back(_name);
}

void Profiler::UntrackGpuMarker(const char* _name)
{
	for (auto& trackedName : s_gpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			eastl::swap(trackedName, s_gpuTrackedMarkers.back());
			s_gpuTrackedMarkers.pop_back();
			return;
		}
	}
}

bool Profiler::IsGpuMarkerTracked(const char* _name)
{
	for (auto& trackedName : s_gpuTrackedMarkers) {
		if (strcmp(trackedName, _name) == 0) {
			return true;
		}
	}
	return false;
}

void Profiler::GpuValue(const char* _name, float _value, uint _historySize)
{
	for (auto& v : s_gpuValues) {
		if (strcmp(_name, v.m_name) == 0) {
			++v.m_count;
			v.m_accum += _value;
			v.m_max = APT_MAX(v.m_max, _value);
			v.m_min = APT_MIN(v.m_min, _value);
			return;
		}
	}
	s_gpuValues.push_back();
	s_gpuValues.back().m_name  = _name;
	s_gpuValues.back().m_count = 1;
	s_gpuValues.back().m_accum = _value;
	s_gpuValues.back().m_min   = _value;
	s_gpuValues.back().m_max   = _value;
	s_gpuValues.back().m_history = new RingBuffer<float>(_historySize);
}

uint Profiler::GetGpuValueCount()
{
	return s_gpuValues.size();
}

const Profiler::Value& Profiler::GetGpuValue(uint _i)
{
	return s_gpuValues[_i];
}

void Profiler::ResetGpuOffset()
{
	GLint64 gpuTime;
	glAssert(glGetInteger64v(GL_TIMESTAMP, &gpuTime));
	uint64 cpuTicks = Time::GetTimestamp().getRaw();
	uint64 gpuTicks = GpuToSystemTicks(gpuTime);
	APT_ASSERT(gpuTicks < cpuTicks);
	s_gpuTickOffset = cpuTicks - gpuTicks; // \todo is it possible that gpuTicks > cpuTicks?
}

// PROTECTED

void Profiler::Init()
{
	s_pause = false;
}

void Profiler::Shutdown()
{
 // \todo static initialization means we can't delete the queries!
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuFrameStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuMarkerStartQueries)); 
	//glAssert(glDeleteQueries(kMaxTotalGpuMarkersPerFrame, s_gpuMarkerEndQueries)); 
}

// PRIVATE

bool Profiler::s_pause;
