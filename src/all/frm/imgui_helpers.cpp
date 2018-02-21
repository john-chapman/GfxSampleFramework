#include <frm/imgui_helpers.h>

#include <apt/String.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                 VirtualWindow

*******************************************************************************/
/*vec2 VirtualWindow::windowToVirtual(const vec2& _posW)
{
	vec2 ret = (_posW - m_minW) / m_sizeW;
	ret = m_minV + ret * m_sizeV;
	return ret * m_rscaleV;
}

vec2 VirtualWindow::virtualToWindow(const vec2& _posV)
{
	vec2 ret = (_posV * m_scaleV - m_minV) / m_sizeV;
	ret = m_minW + ret * m_sizeW;
	return Floor(ret);
}*/

void VirtualWindow::init(int _flags/* = Flags_Default*/)
{
	m_flags = _flags;

	auto& style       = ImGui::GetStyle();
	m_colorBackground = IM_COLOR_ALPHA((ImU32)ImColor(style.Colors[ImGuiCol_WindowBg]), 1.0f);
	m_colorBorder     = ImColor(style.Colors[ImGuiCol_Border]);
	m_colorGrid       = IM_COLOR_ALPHA((ImU32)ImColor(style.Colors[ImGuiCol_Border]), 0.8f);
}

void VirtualWindow::begin(const vec2& _zoom, const vec2& _pan, int _flags/* = Flags_Default*/)
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	ImGui::PushID(this);

	if (_flags != Flags_Default) {
		m_flags = _flags;
	}

 // window beg, end, size
	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_minW = Floor((vec2)ImGui::GetCursorPos() - scroll + (vec2)ImGui::GetWindowPos());
	m_maxW = (vec2)ImGui::GetContentRegionAvail() - scroll + (vec2)ImGui::GetWindowPos();
	m_sizeW = Max(m_maxW - m_minW, vec2(16.0f));
	if (m_size.x > 0.0f) {
		m_sizeW.x = m_size.x;
	}
	if (m_size.y > 0.0f) {
		m_sizeW.y = m_size.y;
	}
	if (checkFlag(Flags_Square)) {
		m_sizeW.x = m_sizeW.y = Min(m_sizeW.x, m_sizeW.y);
	}
	m_maxW = Floor(m_minW + m_sizeW);
	float aspect = m_sizeW.x / m_sizeW.y;

 // prevent drag
	ImGui::InvisibleButton("##prevent drag", m_sizeW);
	m_isActive = ImGui::IsItemHovered() && ImGui::IsWindowFocused();

 // virtual beg, end, size (= zoom/pan)
	if (m_isActive) {
	 // zoom
		ImGui::SetScrollX(scroll.x);
		ImGui::SetScrollY(scroll.y);
		vec2 zoom = _zoom / m_sizeW;
		zoom.x *= aspect; // maintain aspect ratio during zoom
		if (Abs(zoom.x) > 0.0f || Abs(zoom.y) > 0.0f) {
			zoom *= m_sizeV; // keep zoom rate proportional to current region size = 'linear' zoom
			vec2 before = windowToVirtual(io.MousePos);
			m_sizeV = Max(m_sizeV + zoom, vec2(1e-7f));
			updateTransforms();
			vec2 after = windowToVirtual(io.MousePos);
			m_originV += m_basisV * (before - after);
		}
	 // pan
		vec2 pan = _pan / m_sizeW;
		if (Abs(pan.x) > 0.0f || Abs(pan.y) > 0.0f) {
			m_originV -= pan * m_sizeV;
			ImGui::CaptureMouseFromApp(); // \todo?
		}
	}
	m_maxV = m_originV + m_sizeV;
	m_minV = m_originV - m_sizeV;
	updateTransforms();

	drawList.AddRectFilled(m_minW, m_maxW, m_colorBackground);
	ImGui::PushClipRect(m_minW, m_maxW, true);
 // grid X
	if (checkFlag(Flags_GridX)) {
		float spacingV = m_minGridSpacingV.x;
		float spacingW = (spacingV / m_sizeV.x) * m_sizeW.x;
		while (spacingW < m_minGridSpacingW.x) {
			spacingV *= m_gridSpacingBase.x;
			spacingW *= m_gridSpacingBase.x;
		}
		float i = Round(m_minV.x / spacingV) * spacingV;
		for (; i <= m_maxV.x; i += spacingV) {
			drawList.AddLine(
				virtualToWindow(vec2(i, m_minV.y)),
				virtualToWindow(vec2(i, m_maxV.y)),
				m_colorGrid
				);
		}
	}
 // grid Y
	if (checkFlag(Flags_GridY)) {
		float spacingV = m_minGridSpacingV.y;
		float spacingW = (spacingV / m_sizeV.y) * m_sizeW.y;
		while (spacingW < m_minGridSpacingW.y) {
			spacingV *= m_gridSpacingBase.y;
			spacingW *= m_gridSpacingBase.y;
		}
		float i = Round(m_minV.y / spacingV) * spacingV;
		for (; i <= m_maxV.y; i += spacingW) {
			drawList.AddLine(
				virtualToWindow(vec2(m_minV.x, i)),
				virtualToWindow(vec2(m_maxV.x, i)),
				m_colorGrid
				);
		}
	}

	#if 0
	 // debug text
		String<128> dbg(
			"V Min:  %+0.5f, %+0.5f\n"
			"V Max:  %+0.5f, %+0.5f\n"
			"V Size: %+0.5f, %+0.5f\n",
			m_minV.x,  m_minV.y,
			m_maxV.x,  m_maxV.y,
			m_sizeV.x, m_sizeV.y
			);
		drawList.AddText(m_minW + vec2(2.0f), IM_COL32_YELLOW, (const char*)dbg);

	 // orientation grid (X red, Y green)
		const int gridCount = 12;
		const float gridSize = 1.0f;
		const float gridHalf = gridSize * 0.5f;
		for (int i = 0; i < gridCount; ++i) {
			float alpha = (float)i / (float)(gridCount - 1);
			float x = (alpha - 0.5f) * gridSize;
			drawList.AddLine(
				virtualToWindow(vec2(x, -gridHalf)),
				virtualToWindow(vec2(x,  gridHalf)),
				IM_COLOR_ALPHA(IM_COL32_RED, alpha)
				);
			drawList.AddLine(
				virtualToWindow(vec2(-gridHalf, x)),
				virtualToWindow(vec2( gridHalf, x)),
				IM_COLOR_ALPHA(IM_COL32_GREEN, alpha)
				);
		}
	#endif
}

void VirtualWindow::end()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	ImGui::PopClipRect();
	drawList.AddRect(m_minW, m_maxW, m_colorBorder);
	ImGui::PopID();
}

void VirtualWindow::edit()
{
	ImGui::PushID(this);
		bool square = checkFlag(Flags_Square);
		if (ImGui::Checkbox("Square", &square)) setFlag(Flags_Square, square);
		bool gridX = checkFlag(Flags_GridX);
		if (ImGui::Checkbox("Grid X", &gridX)) setFlag(Flags_GridX, gridX);
		bool gridY = checkFlag(Flags_GridY);
		if (ImGui::Checkbox("Grid Y", &gridY)) setFlag(Flags_GridY, gridY);

		if (square) {
			ImGui::DragFloat("Size", &m_size.x, 1.0f, -1.0f);
			if (gridX || gridY) {
				ImGui::DragFloat("Grid Spacing V", &m_minGridSpacingV.x, 0.1f, 0.1f);
				ImGui::DragFloat("Grid Spacing W", &m_minGridSpacingW.x, 1.0f, 1.0f);
				ImGui::DragFloat("Grid Base",      &m_gridSpacingBase.x, 1.0f, 1.0f);
			}
			m_size.y = m_size.x;
			m_minGridSpacingV.y = m_minGridSpacingV.x;
			m_minGridSpacingW.y = m_minGridSpacingW.x;
			m_gridSpacingBase.y = m_gridSpacingBase.x;

		} else {		
			ImGui::DragFloat2("Size", &m_size.x, 1.0f, -1.0f);
			if (gridX || gridY) {
				ImGui::DragFloat2("Grid Spacing V", &m_minGridSpacingV.x, 0.1f, 0.1f);
				ImGui::DragFloat2("Grid Spacing W", &m_minGridSpacingW.x, 1.0f, 1.0f);
				ImGui::DragFloat2("Grid Base",      &m_gridSpacingBase.x, 1.0f, 1.0f);
			}

		}
	ImGui::PopID();

	m_minGridSpacingV = Max(m_minGridSpacingV, vec2(0.1f));
	m_minGridSpacingW = Max(m_minGridSpacingW, vec2(1.0f));
	m_gridSpacingBase = Max(m_gridSpacingBase, vec2(1.0f));
}

#include <apt/log.h>

void VirtualWindow::updateTransforms()
{
	mat3 view, proj, ndc, window;
	
 // virtual -> window
	{
	#if 0
		view = mat3(
			m_basisV[0].x,    m_basisV[1].x,    -m_originV.x,
			m_basisV[0].y,    m_basisV[1].y,    -m_originV.y,
			0.0f,             0.0f,             1.0f
			);
		proj = mat3(
			2.0f / m_sizeV.x, 0.0f,             0.0f,
			0.0f,             2.0f / m_sizeV.y, 0.0f,
			0.0f,             0.0f,             1.0f
			);
		ndc = mat3(
			0.5f,             0.0f,             0.5f,
			0.0f,             0.5f,             0.5f,
			0.0f,             0.0f,             1.0f
			);
		window = mat3(
			m_sizeW.x,        0.0f,             m_minW.x,
			0.0f,             m_sizeW.y,        m_minW.y,
			0.0f,             0.0f,             1.0f
			);
		m_virtualToWindow = window * ndc * proj * view;
	#else
		float a = m_sizeW.x / m_sizeV.x;
		float b = m_sizeW.y / m_sizeV.y;
		float c = m_sizeW.x * 0.5f + m_minW.x;
		float d = m_sizeW.y * 0.5f + m_minW.y;
		m_virtualToWindow = mat3(
			a * m_basisV[0].x, a * m_basisV[1].x, -a * m_originV.x + c,
			b * m_basisV[0].y, b * m_basisV[1].y, -b * m_originV.y + d,
			0.0f,              0.0f,              1.0f
			);
	#endif
	}

 // window -> virtual
	m_windowToVirtual = Inverse(m_virtualToWindow); // \todo construct directly
}