#include <frm/ui/ProfilerViewer.h>

#include <frm/Input.h>
#include <frm/Profiler.h>

#include <apt/String.h>
#include <apt/Time.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace ui;
using namespace apt;

struct Colors
{
	ImU32 kBackground;
	ImU32 kFrame;
	float kFrameHoverAlpha;
	ImU32 kMarkerHover;
	ImU32 kMarkerColors[9];
};
static Colors kGpuColors;
static Colors kCpuColors;
static Colors* kColors = &kGpuColors;

static const char* TimestampStr(Timestamp& _t)
{
	static String<16> s_buf; // we're single threaded, it's fine

	float x = (float)_t.asSeconds();
	if (x >= 1.0f) {
		s_buf.setf("%1.3fs", x);
	} else {
		x = (float)_t.asMilliseconds();
		if (x >= 0.1f) {
			s_buf.setf("%1.2fms", x);
		} else {
			x = (float)_t.asMicroseconds();
			s_buf.setf("%1.0fus", x);
		}
	}

	return (const char*)s_buf;
}


// PUBLIC

ProfilerViewer::ProfilerViewer()
	: m_timeStart(0.0)
	, m_timeRange(66.66f)
	, m_gridDensity(128.0f)
{
	kGpuColors.kBackground = kCpuColors.kBackground = ImColor(0.1f, 0.1f, 0.1f, 1.0f);
	kGpuColors.kFrameHoverAlpha = kCpuColors.kFrameHoverAlpha = 0.1f;
	kGpuColors.kMarkerHover = kCpuColors.kMarkerHover = ImColor(0.3f, 0.3f, 0.3f, 1.0f);
	
	kGpuColors.kFrame           = ImColor(215,  46, 165);
	kGpuColors.kMarkerColors[0] = ImColor( 63,  52, 183);
	kGpuColors.kMarkerColors[1] = ImColor( 95,  54, 183);
	kGpuColors.kMarkerColors[2] = ImColor(154,  23, 179);
	kGpuColors.kMarkerColors[3] = ImColor(183,  22, 173);
	kGpuColors.kMarkerColors[4] = ImColor(159, 107,  73);
	kGpuColors.kMarkerColors[5] = ImColor(125, 184,  84);
	kGpuColors.kMarkerColors[6] = ImColor(100, 158,  56);
	kGpuColors.kMarkerColors[7] = ImColor( 53, 178,  21);
	kGpuColors.kMarkerColors[8] = ImColor( 76, 156, 100);

	kCpuColors.kFrame           = ImColor(229, 190,  47);
	kCpuColors.kMarkerColors[0] = ImColor(183,  52,  76);
	kCpuColors.kMarkerColors[1] = ImColor(183,  61,  54);
	kCpuColors.kMarkerColors[2] = ImColor(179, 113,  23);
	kCpuColors.kMarkerColors[3] = ImColor(182, 148,  22);
	kCpuColors.kMarkerColors[4] = ImColor( 73, 159,  84);
	kCpuColors.kMarkerColors[5] = ImColor( 84, 152, 184);
	kCpuColors.kMarkerColors[6] = ImColor( 56, 127, 158);
	kCpuColors.kMarkerColors[7] = ImColor( 21,  95, 178);
	kCpuColors.kMarkerColors[8] = ImColor( 79,  76, 156);
}

void ProfilerViewer::draw(bool* _open_)
{
	ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y / 3), ImGuiSetCond_FirstUseEver);
	ImGui::Begin("Profiler", _open_);
		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		Keyboard* keyb = Input::GetKeyboard();

		ImGui::Checkbox("Pause (P)", &Profiler::s_pause);
		ImGui::SameLine();
		if (ImGui::Button("Fit")) {
			m_timeStart = 0.0f;
			m_timeRange = (float)Timestamp(Profiler::GetCpuMarker(Profiler::GetCpuFrame(Profiler::GetCpuFrameCount() - 1).m_first).m_end - Profiler::GetCpuMarker(Profiler::GetCpuFrame(0).m_first).m_start).asMilliseconds();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Gpu Offset")) {
			Profiler::ResetGpuOffset();
		}
		ImGui::Separator();

		vec2 woff = ImGui::GetWindowPos();
		m_wsize   = vec2(ImGui::GetContentRegionAvail());
		m_wpos    = woff + vec2(ImGui::GetCursorPos());
		m_wpos.y -= ImGui::GetScrollY();
		m_wend    = m_wpos + m_wsize;

		if (/*ImGui::IsWindowFocused() && */ImGui::IsWindowHovered()) {
			if (keyb->wasPressed(Keyboard::kP)) {
				Profiler::s_pause = !Profiler::s_pause;
			}

			float zoom   = (io.MouseWheel * m_timeRange * 0.1f);
			float before = windowXToMilliseconds(io.MousePos.x - m_wpos.x);
			m_timeRange  = APT_CLAMP(m_timeRange - zoom, 0.1f, 1000.0f);
			float after  = windowXToMilliseconds(io.MousePos.x - m_wpos.x);
			m_timeStart += (before - after);
			if (io.MouseDown[2]) {
				m_timeStart -= io.MouseDelta.x / m_wsize.x * m_timeRange;
			}
		}		

		ImGui::PushClipRect(m_wpos, vec2(m_wend.x, m_wend.y + 8.0f), true);
		 // all markers are drawn relative to the first cpu frame
			m_firstFrameStart = Profiler::GetCpuFrame(0).m_start;

		 // gpu
			m_wsize.y *= 0.5f;
			m_wsize.y -= 1.0f;
			m_wend.y   = m_wpos.y + m_wsize.y;
			
			kColors = &kGpuColors;
			drawList->AddRectFilled(m_wpos, m_wend, kColors->kBackground);

			m_markerStart = ImGui::GetCursorPos();
			m_markerStart.y += ImGui::GetItemsLineHeightWithSpacing();
			for (uint i = 0, n = Profiler::GetGpuFrameCount(); i < n; ++i) {
				const Profiler::GpuFrame& frame = Profiler::GetGpuFrame(i);

				if (i > 0) {
					if (!drawFrameBounds(frame, Profiler::GetGpuFrame(i - 1))) {
						continue;
					}
				}

				m_markerColor = 0;
				for (uint j = frame.m_first, m = frame.m_first + frame.m_count; j < m; ++j) {
					const Profiler::GpuMarker& marker = Profiler::GetGpuMarker(j);
					if (drawFrameMarker(marker)) {
					 // if a gpu marker is hovered, show its CPU start time
						float mstart = markerToWindowX(marker.m_cpuStart);
						float mend = markerToWindowX(marker.m_start);
						float my = m_wsize.y - 6.0f;
						drawList->AddLine(m_wpos + vec2(mend, 0.0f), m_wpos + vec2(mend, my), kGpuColors.kMarkerColors[m_markerColor], 2.0f);
						drawList->AddLine(m_wpos + vec2(mstart, my), m_wpos + vec2(mend, my), kGpuColors.kMarkerColors[m_markerColor], 2.0f);
						drawList->AddCircleFilled(m_wpos + vec2(mstart, my), 2.0f, kGpuColors.kMarkerColors[m_markerColor]);

						ImGui::BeginTooltip();
							ImGui::TextColored(ImColor(kColors->kMarkerColors[m_markerColor]), marker.m_name);
							const char* durStr = TimestampStr(Timestamp(marker.m_end - marker.m_start));
							ImGui::Text("Duration: %s", durStr);
							durStr = TimestampStr(Timestamp(marker.m_start - marker.m_cpuStart));
							ImGui::Text("Latency: %s", durStr);
						ImGui::EndTooltip();
					}
				}
			}
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(m_wpos.x + 4.0f, m_wpos.y + m_wsize.y - ImGui::GetFontSize() - 4.0f), IM_COL32_WHITE, "GPU");

		// cpu
			m_wpos.y += m_wsize.y + 1.0f;
			m_wend.y  = m_wpos.y + m_wsize.y;

			kColors = &kCpuColors;
			drawList->AddRectFilled(m_wpos, m_wend, kColors->kBackground);

			m_markerStart.y = m_wpos.y + ImGui::GetItemsLineHeightWithSpacing() - ImGui::GetWindowPos().y;
			for (uint i = 0, n = Profiler::GetCpuFrameCount(); i < n; ++i) {
				const Profiler::CpuFrame& frame = Profiler::GetCpuFrame(i);

				if (i > 0) {
					if (!drawFrameBounds(frame, Profiler::GetCpuFrame(i - 1))) {
						continue;
					}
				}

				m_markerColor = 0;
				for (uint j = frame.m_first, m = frame.m_first + frame.m_count; j < m; ++j) {
					const Profiler::CpuMarker& marker = Profiler::GetCpuMarker(j);
					if (drawFrameMarker(marker)) {
						ImGui::BeginTooltip();
							ImGui::TextColored(ImColor(kColors->kMarkerColors[m_markerColor]), marker.m_name);
							const char* durStr = TimestampStr(Timestamp(marker.m_end - marker.m_start));
							ImGui::Text("Duration: %s", durStr);
						ImGui::EndTooltip();
					}
				}
			}
			drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(m_wpos.x + 4.0f, m_wpos.y + m_wsize.y - ImGui::GetFontSize() - 4.0f), IM_COL32_WHITE, "CPU");


/*
		 // grid lines
			float gridStep = 133.33f;
				while ((gridStep / m_timeRange * m_wsize.x) > m_gridDensity) {
				gridStep /= 2.0f;
			}
			for (float i = floorf(m_timeStart / gridStep) * gridStep; i < (m_timeStart + m_timeRange); i += gridStep) {
				float x = m_wpos.x + millisecondsToWindowX(i + gridStep * 0.5f);
				drawList->AddLine(ImVec2(x, m_wpos.y), ImVec2(x, m_wend.y), kColor_ProfilerGridMin);

				if (i != 0.0f) {
					x = m_wpos.x + millisecondsToWindowX(i);
					drawList->AddLine(ImVec2(x, m_wpos.y), ImVec2(x, m_wend.y), kColor_ProfilerGridMaj);
					
					String<8> gridLabel;
					if (fabs(i) > 1000.0f) {
						gridLabel.setf("%1.2fs", i / 1000.0f);
					} else if (fabs(i) < 1.0f) {
						gridLabel.setf("%1.2fus", i * 1000.0f);
					} else {
						gridLabel.setf("%1.2fms", i);
					}
					drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x + 2.0f, m_wpos.y + 1.0f), kColor_ProfilerGridMaj, (const char*)gridLabel);
				}
			}
*/			

		ImGui::PopClipRect();
	ImGui::End();
}

bool ProfilerViewer::drawFrameBounds(const Profiler::Frame& _frame, const Profiler::Frame& _prevFrame)
{
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	vec2 mpos = vec2(ImGui::GetMousePos());

 // frame region (+ cull)
	float fstart = m_wpos.x + markerToWindowX(_prevFrame.m_start);
	if (fstart > m_wend.x) {
		return false;
	}
	float fend = m_wpos.x + markerToWindowX(_frame.m_start);
	
 // frame start boundary
	drawList->AddLine(ImVec2(fstart, m_wpos.y), ImVec2(fstart, m_wend.y), kColors->kFrame, 2.0f);
	
 // frame focus (if hovered)
	if ((mpos.y > m_wpos.y && mpos.y < m_wend.y) && (mpos.x > fstart && mpos.x < fend)) {
		drawList->AddRectFilled(ImVec2(fstart, m_wpos.y), ImVec2(fend, m_wend.y), ImColorAlpha(kColors->kFrame, kColors->kFrameHoverAlpha), 0);
		const char* durStr = TimestampStr(Timestamp(_frame.m_start - _prevFrame.m_start));
		drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(APT_MAX(fstart, m_wpos.x) + 4.0f, m_wpos.y), kColors->kFrame, durStr);
		if (ImGui::GetIO().MouseDoubleClicked[0]) {
			m_timeStart = (float)Timestamp(_prevFrame.m_start - m_firstFrameStart).asMilliseconds();
			m_timeRange = (float)Timestamp(_frame.m_start - _prevFrame.m_start).asMilliseconds();
		}
	}

 // reset marker color per frame
	m_markerColor = 0;

	return fend >= 0.0f;
}

bool ProfilerViewer::drawFrameMarker(const Profiler::Marker& _marker)
{
	bool ret = false; // return true only if hovered
	static uint8 s_markerDepth = 0;

	if (_marker.m_depth == 0) {
		m_markerColor = (m_markerColor + 1) % APT_ARRAY_COUNT(kColors->kMarkerColors);
	}
	s_markerDepth = _marker.m_depth;

 // marker size (+ cull small markers)
	float mstart = markerToWindowX(_marker.m_start);
	float mend   = markerToWindowX(_marker.m_end) - 1.0f; // shrink by 1 pixel to prevent adjacent markers running together
	float msize  = mend - mstart;
	if (msize < 2.0f) {
		return false;
	}
			
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 1.0f));
	float mheight = ImGui::GetItemsLineHeightWithSpacing();

	ImGui::SetCursorPosX(m_markerStart.x + mstart);
	ImGui::SetCursorPosY(m_markerStart.y + (float)s_markerDepth * mheight);

	ImGui::PushStyleColor(ImGuiCol_Button, ImColor(kColors->kMarkerColors[m_markerColor]));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(kColors->kMarkerHover));
	
	ImGui::Button(_marker.m_name, ImVec2(msize, 0.0f));	
	if (ImGui::IsItemHovered()) {
		ret = true;
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	return ret;
}

float ProfilerViewer::millisecondsToWindowX(float _ms) const
{
	float ret = (_ms - m_timeStart) / m_timeRange;
	return ret * m_wsize.x;
}

float ProfilerViewer::markerToWindowX(uint64 _time) const
{
	return millisecondsToWindowX((float)Timestamp(_time - m_firstFrameStart).asMilliseconds());
}

float ProfilerViewer::windowXToMilliseconds(float _x) const
{
	return (_x / m_wsize.x) * m_timeRange + m_timeStart;
}
