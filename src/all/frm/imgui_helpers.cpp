#include <frm/imgui_helpers.h>

#include <frm/icon_fa.h>
#include <frm/Curve.h>
#include <frm/Input.h>

#include <apt/String.h>


using namespace frm;
using namespace apt;

/*******************************************************************************

                                 VirtualWindow

*******************************************************************************/

VirtualWindow::VirtualWindow(const vec2& _sizeV, const vec2& _originV, int _flags)
	: m_sizeV(_sizeV)
	, m_originV(_originV)
	, m_flags(_flags)
{
	if (ImGui::GetCurrentContext()) { // ImGui might not be init during the ctor e.g. if the VirutalWindow instance is declared static at namespace scope
		auto& style                = ImGui::GetStyle();
		
		m_colors[Color_Background] = ImGui::GetColorU32(ImGuiCol_WindowBg, 1.0f);
		m_colors[Color_Border]     = ImGui::GetColorU32(ImGuiCol_Border);
		m_colors[Color_Grid]       = ImGui::GetColorU32(ImGuiCol_Border, 0.1f);
		m_colors[Color_GridOrigin] = ImGui::GetColorU32(ImGuiCol_PlotLines);
	}
}

VirtualWindow::~VirtualWindow()
{
}

void VirtualWindow::begin(const vec2& _deltaSizeW, const vec2& _deltaOriginW, const vec2& _anchorW)
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	ImGui::PushID(this);

	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());

	updateRegionW();
	
	ImGui::InvisibleButton("VirtualWindow", m_sizeW); // prevent drag
	m_isActive = ImGui::IsItemHovered() && ImGui::IsWindowFocused();
	updateRegionV(_deltaSizeW, _deltaOriginW, _anchorW);
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

			ImGui::TreePop();
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

void VirtualWindow::updateRegionV(const vec2& _deltaSizeW, const vec2& _deltaOriginW, const vec2& _anchorW)
{
	ImGuiIO& io = ImGui::GetIO();
	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	float aspect = m_sizeW.x / m_sizeW.y;
	if (m_isActive) {
	 // zoom
		ImGui::SetScrollX(scroll.x);
		ImGui::SetScrollY(scroll.y);
		vec2 zoom = _deltaSizeW / m_sizeW;
		zoom.x *= aspect; // maintain aspect ratio during zoom
		if (Abs(zoom.x) > 0.0f || Abs(zoom.y) > 0.0f) {
			vec2 anchorW = _anchorW;
			anchorW.x = anchorW.x == -1.0f ? io.MousePos.x : anchorW.x;
			anchorW.y = anchorW.y == -1.0f ? io.MousePos.y : anchorW.y;
			zoom *= m_sizeV; // keep zoom rate proportional to current region size = 'linear' zoom
			vec2 before = windowToVirtual(io.MousePos);
			m_sizeV = Max(m_sizeV + zoom, vec2(1e-7f));
			updateTransforms();
			vec2 after = windowToVirtual(io.MousePos);
			m_originV += m_basisV * (before - after);
		}
	 // pan
		vec2 pan = _deltaOriginW / m_sizeW;
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


/*******************************************************************************

                               GradientEditor

*******************************************************************************/

static const float kGradient_PixelsPerSegment = 4.0f;
static const float kKeyBar_Height             = 12.0f;
static const float kKey_Width                 = 8.0f;
static const float kKey_HalfWidth             = kKey_Width / 2.0f;
static const char* kColorEdit_PopupName       = "GradientEditor_ColorEditPopup";

GradientEditor* GradientEditor::s_Active;

GradientEditor::GradientEditor(int _flags)
	: m_virtualWindow(vec2(1.0f), vec2(0.5f), 0)
	, m_flags(_flags)
{
	m_colors[Color_Border]         = ImGui::GetColorU32(ImGuiCol_Button);
	m_colors[Color_BorderActive]   = ImGui::GetColorU32(ImGuiCol_ButtonActive);
	m_colors[Color_AlphaGridDark]  = IM_COL32(128, 128, 128, 255);
	m_colors[Color_AlphaGridLight] = IM_COL32(204, 204, 204, 255);

	reset();
}

void GradientEditor::setGradient(CurveGradient& _curveGradient) 
{ 
	vec2 mn = vec2(FLT_MAX);
	vec2 mx = vec2(-FLT_MAX);
	for (int i = 0; i < 4; ++i) { 
		m_curves[i] = &_curveGradient[i];
		mn = APT_MAX(mn, m_curves[i]->getValueMin());
		mx = APT_MAX(mn, m_curves[i]->getValueMax());
	}
	m_virtualWindow.setRegionV(mn, mx);
}

bool GradientEditor::drawEdit(const vec2& _sizePixels, float _t, int _flags)
{
	bool ret = false;

	#ifdef APT_DEBUG
	{ // all curves must have the same number of EPs 
		int keyCount = m_curves[0]->getBezierEndpointCount();
		APT_ASSERT(m_curves[1]->getBezierEndpointCount() == keyCount);
		APT_ASSERT(m_curves[2]->getBezierEndpointCount() == keyCount);
		//APT_ASSERT(m_curves[3]->getBezierEndpointCount() == epCount); // except alpha which is edited separately
	}
	#endif

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	
	m_virtualWindow.setSizeW(_sizePixels.x, _sizePixels.y);
	m_virtualWindow.setColor(VirtualWindow::Color_Border, s_Active == this ? getColor(Color_BorderActive) : getColor(Color_Border));
 	m_minW = ImGui::GetCursorScreenPos();
	m_maxW = m_virtualWindow.getMaxW();
	m_maxW.y += kKeyBar_Height + getFlag(Flag_Alpha) ? kKeyBar_Height : 0.0f;
	if (ImGui::IsWindowHovered()) {
		if ((io.MouseDown[0] || io.MouseClicked[1]) && ImGui::IsMouseHoveringRect(m_minW, m_maxW)) {
		 // if left mouse down or right mouse click, activate this widget and focus the window (to start dragging or open the color popup without first focusing the window)
			if (s_Active && s_Active != this) {
				s_Active->reset();
			}
			s_Active = this;
			ImGui::SetWindowFocus();
		}
	}
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
	 // reset if the window is defocused *except* when we open the color edit popup
		reset();
	}

	ImGui::PushID(this);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, vec2(0.0f)); // prevent spacing between the gradient view and the key edit bar

	// \todo
	//if (getFlag(Flag_Alpha)) {
	//	drawKeysA();
	//	ret |= editKeysA();
	//}

	vec2 zoom = vec2(0.0f);
	vec2 pan  = vec2(0.0f);	
	if (getFlag(Flag_ZoomPan)) {
		zoom = vec2(io.MouseWheel * -16.0f);
		pan = io.MouseDown[2] ? vec2(io.MouseDelta) : vec2(0.0f);
	}
	m_virtualWindow.begin(zoom, pan, io.MousePos);
		drawGradient();
		if (getFlag(Flag_Sampler)) {
			drawSampler(_t);
		}
	m_virtualWindow.end();	
	drawKeysRGB();
	ret |= editKeysRGB();

	ImGui::PopStyleVar(1);
	ImGui::PopID();

	return ret;
}

void GradientEditor::reset()
{
	m_selectedKeyRGB = -1;
	m_selectedKeyA   = -1;
	m_dragKey        = -1;
	m_dragComponent  = -1;
	m_dragOffset     = 0.0f;
	s_Active        = s_Active == this ? nullptr : s_Active;
}

void GradientEditor::drawGradient()
{
	auto& drawList = *ImGui::GetWindowDrawList();

 // alpha grid background
	if (getFlag(Flag_Alpha)) {
		const float cellSize = m_virtualWindow.getSizeW().y / 4.0f;
		const ivec2 gridSize = Max(ivec2(m_virtualWindow.getSizeW() / cellSize), ivec2(1));
		const vec2  minW     = m_virtualWindow.getMinW();
		const vec2  maxW     = m_virtualWindow.getMaxW();
		drawList.AddRectFilled(minW, maxW, getColor(Color_AlphaGridDark));
		for (int x = 0; x <= gridSize.x; ++x) {
			for (int y = 0; y <= gridSize.y; ++y) {
				if (((x + y) & 1) == 0) {
					continue;
				}
				drawList.AddRectFilled(minW + vec2(x, y) * cellSize, minW + vec2(x, y) * cellSize + vec2(cellSize), getColor(Color_AlphaGridLight));
			}
		}
	}

 // can't directly draw the piecewise curve, since each channel may have a different # segments, instead draw a fixed number of segments per pixel
	const bool alpha = getFlag(Flag_Alpha);
	float i  = m_virtualWindow.getMinW().x;
	float x0 = m_virtualWindow.windowToVirtual(i);
	vec4 color0(
		m_curves[0]->evaluate(x0),
		m_curves[1]->evaluate(x0),
		m_curves[2]->evaluate(x0),
		alpha ? m_curves[3]->evaluate(x0) : 1.0f
		);
	float n = m_virtualWindow.getMaxW().x + kGradient_PixelsPerSegment;
	for (i += kGradient_PixelsPerSegment; i <= n; i += kGradient_PixelsPerSegment) {
		float x1 = m_virtualWindow.windowToVirtual(i);
		vec4 color1(
			m_curves[0]->evaluate(x1),
			m_curves[1]->evaluate(x1),
			m_curves[2]->evaluate(x1),
			alpha ? m_curves[3]->evaluate(x1) : 1.0f
		);
		drawList.AddRectFilledMultiColor(
			vec2(i - kGradient_PixelsPerSegment, m_virtualWindow.getMinW().y),
			vec2(i, m_virtualWindow.getMaxW().y),
			ImColor(color0),
			ImColor(color1),
			ImColor(color1),
			ImColor(color0)
			);
		x0 = x1;
		color0 = color1;
	}
}

void GradientEditor::drawSampler(float _t) 
{
	vec4 color( // invert the gradient color at _t to make the line visible
		1.0f - Saturate(m_curves[0]->evaluate(_t)),
		1.0f - Saturate(m_curves[1]->evaluate(_t)),
		1.0f - Saturate(m_curves[2]->evaluate(_t)),
		0.5f
		);
	_t = m_virtualWindow.virtualToWindow(_t);
	ImGui::GetWindowDrawList()->AddLine(
		vec2(_t, m_virtualWindow.getMinW().y),
		vec2(_t, m_virtualWindow.getMaxW().y - m_virtualWindow.getSizeW().y * 0.5f),
		ImColor(color)
		);
}

void GradientEditor::drawKeysRGB()
{
 // key bar is slightly expanded in x to compensate for the key width
	const vec2 cursorPos = vec2(m_virtualWindow.getMinW().x - kKey_Width, m_virtualWindow.getMaxW().y);
	ImGui::SetCursorScreenPos(cursorPos);
	ImGui::InvisibleButton("KeysRGB", vec2(m_virtualWindow.getSizeW().x + kKey_Width * 2.0f, kKeyBar_Height)); // prevent window drag
	m_keyBarRGBHovered = ImGui::IsItemHovered();

	const int keyCount = m_curves[0]->getBezierEndpointCount();
	if (keyCount == 0) {
		return;
	}

	auto& drawList = *ImGui::GetWindowDrawList();
	
 // unselected keys are small with no border
	for (int i = 0; i < keyCount; ++i) {
		if (i == m_selectedKeyRGB) {
			continue;
		}
		vec4 color(
			m_curves[0]->getBezierEndpoint(i).m_value.y,
			m_curves[1]->getBezierEndpoint(i).m_value.y,
			m_curves[2]->getBezierEndpoint(i).m_value.y,
			1.0f
			);
		float w = kKey_HalfWidth;
		float h = kKey_HalfWidth * 0.5f;
		vec2  p = vec2(m_virtualWindow.virtualToWindow(m_curves[0]->getBezierEndpoint(i).m_value.x), cursorPos.y - 1.0f); // -1 = soverlap the gradient bar
		ImVec2 keyShape[] = {
			ImVec2(p + vec2(-w, w + h)),
			ImVec2(p + vec2(-w, w)),
			ImVec2(p),
			ImVec2(p + vec2(w, w)),
			ImVec2(p + vec2(w, w + h)),
		};
		drawList.AddConvexPolyFilled(keyShape, 5, ImGui::ColorConvertFloat4ToU32(color));
		drawList.AddPolyline(keyShape, 5, getColor(Color_Border), true, 0.0f);
	}

 // selected key is large with a border
	if (m_selectedKeyRGB != Curve::kInvalidIndex) {
		vec4 color(
			m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_value.y,
			m_curves[1]->getBezierEndpoint(m_selectedKeyRGB).m_value.y,
			m_curves[2]->getBezierEndpoint(m_selectedKeyRGB).m_value.y,
			1.0f
			);
		float w  = kKey_HalfWidth;
		float h  = kKey_HalfWidth * 0.5f;
		vec2 p   = vec2(m_virtualWindow.virtualToWindow(m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_value.x), cursorPos.y - h); // -h = overlap the gradient bar
		ImVec2 keyShape[] = {
			ImVec2(p + vec2(-w, kKeyBar_Height + h)),
			ImVec2(p + vec2(-w, w)),
			ImVec2(p),
			ImVec2(p + vec2(w, w)),
			ImVec2(p + vec2(w, kKeyBar_Height + h)),
		};
		drawList.AddConvexPolyFilled(keyShape, 5, ImColor(color));
		drawList.AddPolyline(keyShape, 5, IM_COL32_WHITE, true, 1.0f);

	 // tangent in
		p.y += 5.0f;
		p.x = gradientToWindow(m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_in.x);
		w = kKey_HalfWidth * 0.5f;
		drawList.AddTriangleFilled(p, p + vec2(-w, w), p + vec2(w, w), IM_COL32_WHITE);

	 // tangent out
		p.x = gradientToWindow(m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_out.x);
		w = kKey_HalfWidth * 0.5f;
		drawList.AddTriangleFilled(p, p + vec2(-w, w), p + vec2(w, w), IM_COL32_WHITE);
	}
}

bool GradientEditor::editKeysRGB()
{
	if (s_Active != this) {
		return false;
	}
		
	bool ret = false;
	ImGui::PushID(this);

	auto& io = ImGui::GetIO();
	const vec2 mousePos = io.MousePos;

	const int  keyCount = m_curves[0]->getBezierEndpointCount();
	
	if (m_dragKey == Curve::kInvalidIndex) {
	 // select key
		if (m_keyBarRGBHovered && io.MouseClicked[0] || io.MouseClicked[1]) {
		 // left or right click to select/start dragging
			for (int i = 0; i < keyCount; ++i) {
				float x = gradientToWindow(m_curves[0]->getBezierEndpoint(i).m_value.x);
				if (mousePos.x > x - kKey_HalfWidth && mousePos.x < x + kKey_HalfWidth) {
					m_selectedKeyRGB = m_dragKey = i;
					m_dragComponent  = Curve::Component_Value;
					m_dragOffset     = x - mousePos.x;
					break;
				}
			}
		}
	} else {
	 // drag key
		if (io.MouseDown[0]) {
		 // key is being dragged
			ret = true;
			m_selectedKeyRGB = m_dragKey = moveSelectedKeyRGB(windowToGradient(mousePos.x + m_dragOffset), m_dragKey, m_dragComponent);
			ImGui::CaptureMouseFromApp();

		} else {
		 // mouse left was just released, stop dragging
			m_dragKey = Curve::kInvalidIndex;
		}
	}

	if (m_keyBarRGBHovered && io.MouseDoubleClicked[0]) {
	 // double left click to insert a key
		float x = windowToGradient(mousePos.x);
		for (int i = 0; i < 3; ++i) {
			m_selectedKeyRGB = m_curves[i]->insert(x, m_curves[i]->evaluate(x));
		}
	}

	if (ImGui::IsWindowFocused() && m_selectedKeyRGB != Curve::kInvalidIndex) {
		const Curve::Endpoint& key = m_curves[0]->getBezierEndpoint(m_selectedKeyRGB);
		float keyValW = gradientToWindow(key.m_value.x);
		float keyInW  = gradientToWindow(key.m_in.x);
		float keyOutW = gradientToWindow(key.m_out.x);

		const vec2 keyMin = vec2(keyValW - kKey_HalfWidth, m_virtualWindow.getMaxW().y);
		const vec2 keyMax = keyMin + vec2(kKey_Width, kKeyBar_Height);

		if (m_keyBarRGBHovered) {
			if (io.MouseClicked[1]) {
			 // right-click to edit the selected key			
				if (mousePos.x > keyValW - kKey_HalfWidth && mousePos.x < keyValW + kKey_HalfWidth) {
					ImGui::OpenPopup(kColorEdit_PopupName);
				}
			} else if (io.MouseClicked[0]) {
			 // left-click to select tangents
				if (mousePos.x > keyInW - kKey_HalfWidth && mousePos.x < keyInW + kKey_HalfWidth) {
					m_dragKey = m_selectedKeyRGB;
					m_dragComponent = Curve::Component_In;
				} else if (mousePos.x > keyOutW - kKey_HalfWidth && mousePos.x < keyOutW + kKey_HalfWidth) {
					m_dragKey = m_selectedKeyRGB;
					m_dragComponent = Curve::Component_Out;
				}
			}
		}

	 // keboard input
		if (!io.WantCaptureKeyboard && s_Active == this) { // !io.WantCaptureKeybaord = ignore key presses when interacting with adjacent widgets (e.g. float edit)
			if (ImGui::IsKeyPressed(Keyboard::Key_Delete)) {
			 // delete to remove a key
				for (int i = 0; i < 3; ++i) {
					m_curves[i]->erase(m_selectedKeyRGB);
					ret = true;
				}
				m_selectedKeyRGB = m_dragKey = Curve::kInvalidIndex;
				ImGui::CaptureKeyboardFromApp();
			}

			if (ImGui::IsKeyDown(Keyboard::Key_LCtrl)) {
			 // lctrl + left/right arrows to 'nudge' selected key by 1 pixel
				if (ImGui::IsKeyPressed(Keyboard::Key_Right)) {
					m_selectedKeyRGB = moveSelectedKeyRGB(key.m_value.x + m_virtualWindow.getSizeV().x / m_virtualWindow.getSizeW().x, m_selectedKeyRGB, Curve::Component_Value);
					ImGui::CaptureKeyboardFromApp();
				} else if (ImGui::IsKeyPressed(Keyboard::Key_Left)) {
					m_selectedKeyRGB = moveSelectedKeyRGB(key.m_value.x - m_virtualWindow.getSizeV().x / m_virtualWindow.getSizeW().x, m_selectedKeyRGB, Curve::Component_Value);
					ImGui::CaptureKeyboardFromApp();
				}
			} else {
			 // left/right arrows to navigate key selection
				if (ImGui::IsKeyPressed(Keyboard::Key_Right)) {
					m_selectedKeyRGB = (m_selectedKeyRGB + 1) % keyCount;
					ImGui::CaptureKeyboardFromApp();
				} else if (ImGui::IsKeyPressed(Keyboard::Key_Left)) {
					if (--m_selectedKeyRGB < 0) {
						m_selectedKeyRGB = keyCount - 1;
					}
					ImGui::CaptureKeyboardFromApp();
				}
			}
		}
	}
	
 // key popup
	if (ImGui::BeginPopup(kColorEdit_PopupName)) {
		APT_ASSERT(m_selectedKeyRGB != Curve::kInvalidIndex);
		vec3 color(
			m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_value.y,
			m_curves[1]->getBezierEndpoint(m_selectedKeyRGB).m_value.y,
			m_curves[2]->getBezierEndpoint(m_selectedKeyRGB).m_value.y
			);
		float x = m_curves[0]->getBezierEndpoint(m_selectedKeyRGB).m_value.x;
		if (ImGui::DragFloat("###", &x, m_virtualWindow.getSizeV().x * 0.01f)) {
			int newKeyIndex = m_curves[0]->moveX(m_selectedKeyRGB, Curve::Component_Value, x);
			APT_VERIFY(m_curves[1]->moveX(m_selectedKeyRGB, Curve::Component_Value, x) == newKeyIndex);
			APT_VERIFY(m_curves[2]->moveX(m_selectedKeyRGB, Curve::Component_Value, x) == newKeyIndex);
			m_selectedKeyRGB = newKeyIndex;
			ret = true;
		}

		if (ImGui::ColorPicker3("##GradientEditorColorPicker", &color.x,
			ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSidePreview | 
			ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_HEX | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR
			)) {
			m_curves[0]->moveY(m_selectedKeyRGB, Curve::Component_Value, color.x);
			m_curves[1]->moveY(m_selectedKeyRGB, Curve::Component_Value, color.y);
			m_curves[2]->moveY(m_selectedKeyRGB, Curve::Component_Value, color.z);
			ret = true;
		}
		ImGui::EndPopup();
	}
	
	ImGui::PopID();
	return ret;
}

int GradientEditor::moveSelectedKeyRGB(float _newX, int _keyIndex, int _keyComponent)
{
	_newX = APT_CLAMP(_newX, m_virtualWindow.getMinV().x, m_virtualWindow.getMaxV().x);
	int ret = m_curves[0]->moveX(_keyIndex, (Curve::Component)_keyComponent, _newX);
	for (int i = 1; i < 3; ++i) {
		APT_VERIFY(m_curves[i]->moveX(_keyIndex, (Curve::Component)_keyComponent, _newX) == ret);
	}
	return ret;
}

void GradientEditor::edit()
{
	ImGui::PushID(this);
	if (ImGui::TreeNode("Flags")) {
		bool zoomPan = getFlag(Flag_ZoomPan);
		if (ImGui::Checkbox("Zoom/Pan", &zoomPan)) setFlag(Flag_ZoomPan, zoomPan);
		bool alpha = getFlag(Flag_Alpha);
		if (ImGui::Checkbox("Alpha", &alpha)) setFlag(Flag_Alpha, alpha);

		ImGui::TreePop();
	}
	ImGui::PopID();
}


/*******************************************************************************

                               GradientEditor

*******************************************************************************/
namespace GradientEdit
{
struct GradientEdit_State
{
	int      m_selectedKeyRGB   = -1;
	int      m_dragKey          = -1;
	int      m_dragComponent    = -1;
	float    m_dragOffset       = 0.0f;
};

// Globally we only interact with 1 gradient at a time, hence static state. Alternative would be to allocate via ImGuiStorage.
static GradientEdit_State s_activeState;
static int                s_activeID     = -1;

}


bool frm::GradientEdit(const char* _label, CurveGradient& _gradient_, const ImVec2& _sizePixels, GradientEditFlags _flags)
{
	ImGuiID id = ImGui::GetID(_label);
	
	bool ret = false;

	ImGui::PushID(id);
	
	ImGui::PopID();

	return ret;
}
