#include <frm/imgui_helpers.h>

#include <apt/String.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                 VirtualWindow

*******************************************************************************/

VirtualWindow::VirtualWindow()
{
	if (ImGui::GetCurrentContext()) { // ImGui might not be init during the ctor e.g. if the VirutalWindow instance is declared static at namespace scope
		auto& style                = ImGui::GetStyle();
		m_colors[Color_Background] = IM_COLOR_ALPHA((ImU32)ImColor(style.Colors[ImGuiCol_WindowBg]), 1.0f);
		m_colors[Color_Border]     = ImColor(style.Colors[ImGuiCol_Border]);
		m_colors[Color_Grid]       = IM_COLOR_ALPHA((ImU32)ImColor(style.Colors[ImGuiCol_Border]), 0.1f);
		m_colors[Color_GridOrigin] = ImColor(style.Colors[ImGuiCol_PlotLines]);
	}
}

VirtualWindow::~VirtualWindow()
{
}

void VirtualWindow::begin(const vec2& _zoom, const vec2& _pan)
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	ImGui::PushID(this);

	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());

	updateRegionW();
	
	ImGui::InvisibleButton("##prevent drag", m_sizeW); // prevent drag
	m_isActive = ImGui::IsItemHovered() && ImGui::IsWindowFocused();
	updateRegionV(_zoom, _pan);
	updateTransforms();

	drawList.AddRectFilled(m_minW, m_maxW, m_colors[Color_Background]);

	#if 0
	 // debug, draw the V region
		mat2 basisV = Transpose(m_basisV);
		drawList.AddQuadFilled(
			virtualToWindow(vec2(m_minV.x, m_minV.y)),
			virtualToWindow(vec2(m_minV.x, m_maxV.y)),
			virtualToWindow(vec2(m_maxV.x, m_maxV.y)),
			virtualToWindow(vec2(m_maxV.x, m_minV.y)),
			IM_COLOR_ALPHA(IM_COL32_YELLOW, 0.25f)
			);
		drawList.AddLine(
			virtualToWindow(vec2(m_originV.x, m_minV.y)),
			virtualToWindow(vec2(m_originV.x, m_maxV.y)),
			IM_COL32_YELLOW
			);
		drawList.AddLine(
			virtualToWindow(vec2(m_minV.x, m_originV.y)),
			virtualToWindow(vec2(m_maxV.x, m_originV.y)),
			IM_COL32_YELLOW
			);

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

	  // debug text
		String<128> dbg(
			"Origin V: %+0.5f, %+0.5f\n"
			"Min V:    %+0.5f, %+0.5f\n"
			"Max V:    %+0.5f, %+0.5f\n"
			"Size V:   %+0.5f, %+0.5f\n",
			m_originV.x, m_originV.y,
			m_minV.x,    m_minV.y,
			m_maxV.x,    m_maxV.y,
			m_sizeV.x,   m_sizeV.y
			);
		drawList.AddText(m_minW + vec2(2.0f), IM_COL32_YELLOW, (const char*)dbg);
	#endif

	ImGui::PushClipRect(m_minW, m_maxW, true);
	drawGrid();
}

void VirtualWindow::end()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	ImGui::PopClipRect();
	drawList.AddRect(m_minW, m_maxW, m_colors[Color_Border]);
	ImGui::PopID();
}

void VirtualWindow::edit()
{
	ImGui::PushID(this);
		if (ImGui::TreeNode("Flags")) {
			bool square = getFlag(Flag_Square);
			if (ImGui::Checkbox("Square", &square)) setFlag(Flag_Square, square);
			bool gridX = getFlag(Flag_GridX);
			if (ImGui::Checkbox("Grid X", &gridX)) setFlag(Flag_GridX, gridX);
			ImGui::SameLine();
			bool gridY = getFlag(Flag_GridY);
			if (ImGui::Checkbox("Grid Y", &gridY)) setFlag(Flag_GridY, gridY);
			ImGui::SameLine();
			bool gridOrigin = getFlag(Flag_GridOrigin);
			if (ImGui::Checkbox("Grid Origin", &gridOrigin)) setFlag(Flag_GridOrigin, gridOrigin);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Colors")) {
			editColor(Color_Background, "Background");
			editColor(Color_Border,     "Border");
			editColor(Color_Grid,       "Grid");
			editColor(Color_GridOrigin, "Grid Origin");

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Sizes")) {
			if (getFlag(Flag_Square)) {
				ImGui::DragFloat("Size", &m_requestedSizeW.x, 1.0f, -1.0f);
				if (getFlag(Flag_GridX) || getFlag(Flag_GridY)) {
					ImGui::DragFloat("Grid Spacing V", &m_minGridSpacingV.x, 0.1f, 0.1f);
					ImGui::DragFloat("Grid Spacing W", &m_minGridSpacingW.x, 1.0f, 1.0f);
					ImGui::DragFloat("Grid Base",      &m_gridSpacingBase.x, 1.0f, 1.0f);
				}
				m_requestedSizeW.y  = m_requestedSizeW.x;
				m_minGridSpacingV.y = m_minGridSpacingV.x;
				m_minGridSpacingW.y = m_minGridSpacingW.x;
				m_gridSpacingBase.y = m_gridSpacingBase.x;

			} else {		
				ImGui::DragFloat2("Size", &m_requestedSizeW.x, 1.0f, -1.0f);
				if (getFlag(Flag_GridX) || getFlag(Flag_GridY)) {
					ImGui::DragFloat2("Grid Spacing V", &m_minGridSpacingV.x, 0.1f, 0.1f);
					ImGui::DragFloat2("Grid Spacing W", &m_minGridSpacingW.x, 1.0f, 1.0f);
					ImGui::DragFloat2("Grid Base",      &m_gridSpacingBase.x, 1.0f, 1.0f);
				}

			}
		}
	ImGui::PopID();

	m_minGridSpacingV = Max(m_minGridSpacingV, vec2(0.1f));
	m_minGridSpacingW = Max(m_minGridSpacingW, vec2(1.0f));
	m_gridSpacingBase = Max(m_gridSpacingBase, vec2(1.0f));
}


// PRIVATE

void VirtualWindow::updateRegionW()
{
	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_minW  = Floor((vec2)ImGui::GetCursorPos() - scroll + (vec2)ImGui::GetWindowPos());
	m_maxW  = (vec2)ImGui::GetContentRegionAvail() - scroll + (vec2)ImGui::GetWindowPos();
	m_sizeW = Max(m_maxW - m_minW, vec2(16.0f));
	if (m_requestedSizeW.x > 0.0f) {
		m_sizeW.x = m_requestedSizeW.x;
	}
	if (m_requestedSizeW.y > 0.0f) {
		m_sizeW.y = m_requestedSizeW.y;
	}
	if (getFlag(Flag_Square)) {
		m_sizeW.x = m_sizeW.y = Min(m_sizeW.x, m_sizeW.y);
	}
	m_maxW = Floor(m_minW + m_sizeW);
}

void VirtualWindow::updateRegionV(const vec2& _zoom, const vec2& _pan)
{
	ImGuiIO& io = ImGui::GetIO();
	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	float aspect = m_sizeW.x / m_sizeW.y;
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
			ImGui::CaptureMouseFromApp();
		}	
	}

 // minV,maxV computed from the positions of the window corners in V
	vec2 a = windowToVirtual(vec2(m_minW.x, m_minW.y));
	vec2 b = windowToVirtual(vec2(m_minW.x, m_maxW.y));
	vec2 c = windowToVirtual(vec2(m_maxW.x, m_maxW.y));
	vec2 d = windowToVirtual(vec2(m_maxW.x, m_minW.y));
	m_maxV = Max(a, Max(b, Max(c, d)));
	m_minV = Min(a, Min(b, Min(c, d)));
}

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

void VirtualWindow::drawGrid()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	if (getFlag(Flag_GridX)) {
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
				m_colors[Color_Grid]
				);
		}
	}

	if (getFlag(Flag_GridY)) {
		float spacingV = m_minGridSpacingV.y;
		float spacingW = (spacingV / m_sizeV.y) * m_sizeW.y;
		while (spacingW < m_minGridSpacingW.y) {
			spacingV *= m_gridSpacingBase.y;
			spacingW *= m_gridSpacingBase.y;
		}
		float i = Round(m_minV.y / spacingV) * spacingV;
		for (; i <= m_maxV.y; i += spacingV) {
			drawList.AddLine(
				virtualToWindow(vec2(m_minV.x, i)),
				virtualToWindow(vec2(m_maxV.x, i)),
				m_colors[Color_Grid]
				);
		}
	}

	if (getFlag(Flag_GridOrigin)) {
		drawList.AddLine(
			virtualToWindow(vec2(0.0f, m_minV.y)),
			virtualToWindow(vec2(0.0f, m_maxV.y)),
			m_colors[Color_GridOrigin]
			);
		drawList.AddLine(
			virtualToWindow(vec2(m_minV.x, 0.0f)),
			virtualToWindow(vec2(m_maxV.x, 0.0f)),
			m_colors[Color_GridOrigin]
			);
	}
}

void VirtualWindow::editColor(Color _enum, const char* _name)
{
	ImVec4 col4 = (ImVec4)ImColor(m_colors[_enum]);
	ImGui::ColorEdit4(_name, &col4.x);
	m_colors[_enum] = (ImU32)ImColor(col4);
}