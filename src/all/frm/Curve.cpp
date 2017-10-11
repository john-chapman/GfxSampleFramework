#include <frm/Curve.h>

#include <frm/Input.h>

#include <apt/String.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

#define Curve_DEBUG 1

// PUBLIC

Curve::Curve()
	: m_regionBeg(0.0f)
	, m_regionEnd(1.0f)
	, m_regionSize(1.0f)
	, m_selectedEndpoint(kInvalidIndex)
	, m_dragEndpoint(kInvalidIndex)
	, m_dragComponent(-1)
	, m_dragOffset(0.0f)
	, m_editEndpoint(false)
	, m_isDragging(false)
	, m_editFlags(EditFlags_Default)
{
}

Curve::Index Curve::insert(const Endpoint& _endpoint)
{
	Index ret = (Index)m_endpoints.size();
	if (!m_endpoints.empty() && _endpoint.m_value.x < m_endpoints[ret - 1].m_value.x) {
	 // can't insert at end, do binary search
		ret = findSegmentStart(_endpoint.m_value.x);
		ret += (_endpoint.m_value.x >= m_endpoints[ret].m_value.x) ? 1 : 0; // handle case where _pos.x should be inserted at 0, normally we +1 to ret
	}
	m_endpoints.insert(m_endpoints.begin() + ret, _endpoint);

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (ret == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (ret == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}

	updateExtentsAndConstrain(ret);

	return ret;
}

Curve::Index Curve::move(Index _endpoint, Component _component, const vec2& _value)
{
	Endpoint& ep = m_endpoints[_endpoint];

	Index ret = _endpoint;
	if (_component == Component_Value) {
	 // move CPs
		vec2 delta = _value - ep[Component_Value];
		ep[Component_In] += delta;
		ep[Component_Out] += delta;

	 // swap with neighbor
		ep.m_value = _value;
		if (delta.x > 0.0f && _endpoint < (Index)m_endpoints.size() - 1) {
			int i = _endpoint + 1;
			if (_value.x > m_endpoints[i].m_value.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		} else if (_endpoint > 0) {
			int i = _endpoint - 1;
			if (_value.x < m_endpoints[i].m_value.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		}

	} else {
	 // prevent crossing VP in x
		ep[_component] = _value;
		if (_component == Component_In) {
			ep[_component].x = min(ep[_component].x, ep[Component_Value].x);
		} else {
			ep[_component].x = max(ep[_component].x, ep[Component_Value].x);
		}

	 // CPs are locked so we must update the other
	 // \todo unlocked CPs?
		Component other = _component == Component_In ? Component_Out : Component_In;
		int i = other == Component_In ? 0 : 1;
		vec2 v = ep[Component_Value] - ep[_component];
		ep[other] = ep[Component_Value] + v;

	}

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (ret == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (ret == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}

	updateExtentsAndConstrain(ret);

	return ret;
}

void Curve::erase(Index _endpoint)
{
	APT_ASSERT(_endpoint < (Index)m_endpoints.size());
	m_endpoints.erase(m_endpoints.begin() + _endpoint);
	updateExtentsAndConstrain(APT_MIN(_endpoint, APT_MAX((Index)m_endpoints.size() - 1, 0)));
}

float Curve::wrap(float _t) const
{
	float ret = _t;
	switch (m_wrap) {
		case Wrap_Repeat:
			ret = ret - m_valueMin.x * floor(ret / (m_valueMax.x - m_valueMin.x));
			break;
		case Wrap_Clamp:
		default:
			ret = APT_CLAMP(ret, m_valueMin.x, m_valueMax.x);
			break;
	};
	APT_ASSERT(ret >= m_valueMin.x && ret <= m_valueMax.x);
	return ret;
}

static const ImU32 kColorBorder          = ImColor(0xdba0a0a0);
static const ImU32 kColorBackground      = ImColor(0x55191919);
static const ImU32 kColorRuler           = ImColor(0x66050505);
static const ImU32 kColorRulerLabel      = ImColor(0xff555555);
static const ImU32 kColorCurveBackground = ImColor(0x11a0a0a0);
static const ImU32 kColorGridLine        = ImColor(0x11a0a0a0);
static const ImU32 kColorGridLabel       = ImColor(0xdba9a9a9);
static const ImU32 kColorZeroAxis        = ImColor(0x22d6d6d6);
static const ImU32 kColorValuePoint      = ImColor(0xffffffff);
static const ImU32 kColorControlPoint    = ImColor(0xffaaaaaa);
static const ImU32 kColorSampler         = ImColor(0xdb00ff00);
static const float kAlphaCurveWrap       = 0.3f;
static const float kSizeValuePoint       = 3.0f;
static const float kSizeControlPoint     = 2.0f;
static const float kSizeSelectPoint      = 6.0f;
static const float kSizeRuler            = 17.0f;
static const float kSizeSampler          = 3.0f;


bool Curve::edit(const vec2& _sizePixels, float _t, EditFlags _flags)
{
	bool ret = false;
	m_editFlags = _flags;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

 // set the 'window' size to either fill the available space or use the specified size
	m_windowBeg = (vec2)ImGui::GetCursorPos() + (vec2)ImGui::GetWindowPos();
	m_windowEnd = (vec2)ImGui::GetContentRegionMax() + (vec2)ImGui::GetWindowPos();
	if (_sizePixels.x >= 0.0f) {
		m_windowEnd.x = m_windowBeg.x + _sizePixels.x;
	}
	if (_sizePixels.y >= 0.0f) {
		m_windowEnd.y = m_windowBeg.y + _sizePixels.y;
	}
	m_windowBeg = floor(m_windowBeg);
	m_windowEnd = floor(m_windowEnd);
	m_windowSize = m_windowEnd - m_windowBeg;
	ImGui::InvisibleButton("##PreventDrag", m_windowSize);

 // focus window on middle-click if inside the curve editor
	vec2 mousePos = vec2(io.MousePos);
	bool mouseInWindow = isInside(mousePos, m_windowBeg, m_windowEnd);
	bool windowActive = ImGui::IsWindowFocused();
	if (!windowActive && mouseInWindow && io.MouseDown[2]) {
		ImGui::SetWindowFocus();
	}

 // manage zoom/pan
	if (m_isDragging || (windowActive && mouseInWindow)) {
		if (io.KeyCtrl) {
		 // zoom Y (value)
			float wy = ImGui::GetWindowContentRegionMax().y;
			float zoom = (io.MouseWheel * m_regionSize.y * 0.1f);
			float before = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionSize.y = max(m_regionSize.y - zoom, 0.01f);
			float after = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionBeg.y += (before - after);

		} else {
		 // zoom X (time)
			float wx = ImGui::GetWindowContentRegionMax().x;
			float zoom = (io.MouseWheel * m_regionSize.x * 0.1f);
			float before = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionSize.x = max(m_regionSize.x - zoom, 0.01f);
			float after = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionBeg.x += (before - after);

		}
	 // pan
		if (io.MouseDown[2]) {
			vec2 delta = vec2(io.MouseDelta) / m_windowSize * m_regionSize;
			delta.y = -delta.y;
			m_regionBeg -= delta;
			m_isDragging = true;
			ImGui::CaptureMouseFromApp();
		} else {
			m_isDragging = false;
		}
		m_regionEnd = m_regionBeg + m_regionSize;
	}

	if (editCurve()) {
		ret = true;
		updateCache();
	}

	drawBackground();
	if (checkEditFlag(EditFlags_ShowGrid)) {
		drawGrid();
	}
	ImGui::PushClipRect(m_windowBeg + vec2(1.0f), m_windowEnd - vec2(1.0f), true);
		drawCurve();
		if (checkEditFlag(EditFlags_ShowRuler)) {
			drawRuler();
		}
	ImGui::PopClipRect();

	return ret;
}

// PRIVATE

Curve::Index Curve::findSegmentStart(float _t) const
{
	Index lo = 0, hi = (Index)m_endpoints.size() - 1;
	while (hi - lo > 1) {
		u32 md = (hi + lo) / 2;
		if (_t > m_endpoints[md].m_value.x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_endpoints[hi].m_value.x ? hi : lo;
}

void Curve::updateExtentsAndConstrain(Index _endpoint)
{
	m_valueMin = m_endpointMin = vec2(FLT_MAX);
	m_valueMax = m_endpointMax = vec2(-FLT_MAX);
	for (auto& ep : m_endpoints) {
		m_valueMin = min(m_valueMin, ep.m_value);
		m_valueMax = min(m_valueMax, ep.m_value);
		for (int i = 0; i < Component_Count; ++i) {
			m_endpointMin = min(m_endpointMin, ep[i]);
			m_endpointMax = max(m_endpointMax, ep[i]);
		}
	}

	if (m_wrap == Wrap_Repeat) {
	 // synchronize first/last endpoints
		if (_endpoint == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (_endpoint == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}
}

void Curve::copyValueAndTangent(const Endpoint& _src, Endpoint& dst_)
{
	dst_.m_value.y = _src.m_value.y;
	dst_.m_in  = dst_.m_value + (_src.m_in  - _src.m_value);
	dst_.m_out = dst_.m_value + (_src.m_out - _src.m_value);
}

bool Curve::isInside(const vec2& _point, const vec2& _min, const vec2& _max)
{
	return _point.x > _min.x && _point.x < _max.x && _point.y > _min.y && _point.y < _max.y;
}

vec2 Curve::curveToRegion(const vec2& _pos)
{
	vec2 ret = (_pos - m_regionBeg) / m_regionSize;
	ret.y = 1.0f - ret.y;
	return ret;
}
vec2 Curve::curveToWindow(const vec2& _pos)
{
	vec2 ret = curveToRegion(_pos);
	return m_windowBeg + ret * m_windowSize;
}

vec2 Curve::regionToCurve(const vec2& _pos)
{
	vec2 pos = _pos;
	pos.y = 1.0f - _pos.y;
	return m_regionBeg + pos *  m_regionSize;
}

vec2 Curve::windowToCurve(const vec2& _pos)
{
	return regionToCurve((_pos - m_windowBeg) / m_windowSize);
}

bool Curve::editCurve()
{
	if (!(m_editEndpoint || ImGui::IsWindowFocused() || m_dragEndpoint != kInvalidIndex)) {
		return false;
	}
	
	bool ret = false;

	ImGuiIO& io = ImGui::GetIO();
	vec2 mousePos = io.MousePos;
	
 // point selection
	if (!m_endpoints.empty() && !m_editEndpoint && (io.MouseDown[0] || io.MouseDown[1]) && m_dragEndpoint == kInvalidIndex) {
		for (int i = 0, n = (int)m_endpoints.size(); i < n; ++i) {
			Endpoint& ep = m_endpoints[i];
			bool done = false;
			for (int j = 0; j < 3; ++j) {
				vec2 p = curveToWindow(ep[j]);
				if (!isInside(p, m_windowBeg, m_windowEnd)) {
					if (p.x > m_windowEnd.x) { // can end search if beyond window X
						done = true;
						break;
					}
					continue;
				}
				if (length(mousePos - p) < kSizeSelectPoint) {
					m_dragOffset = p - mousePos;
					m_selectedEndpoint = m_dragEndpoint = i;
					m_dragComponent = j;
				}
			}
			if (done) {
				break;
			}
		}
	}

 // manipulate
	if (m_dragEndpoint != kInvalidIndex) {
	 // left click + drag: move selected point
		if (io.MouseDown[0] && io.MouseDownDuration[0]) {
		 // point is being dragged
			vec2 newPos = windowToCurve(mousePos + m_dragOffset);				
			m_selectedEndpoint = m_dragEndpoint = move(m_dragEndpoint, (Component)m_dragComponent, newPos);
			ImGui::CaptureMouseFromApp();

			if (m_dragComponent == Component_Value && io.MouseDownDuration[0] > 0.1f) {
				ImGui::BeginTooltip();
					ImGui::Text("%1.3f, %1.3f", m_endpoints[m_selectedEndpoint].m_value.x, m_endpoints[m_selectedEndpoint].m_value.y);
				ImGui::EndTooltip();
			}

		} else {
		 // mouse just released
			m_dragEndpoint = m_dragComponent = -1;
		}
		ret = true;

	} else if (io.MouseDoubleClicked[0]) {
	 // double click: insert a point
		Endpoint ep;
		ep.m_value = windowToCurve(io.MousePos);
		ep.m_in    = ep.m_value + vec2(-0.01f, 0.0f);
		ep.m_out   = ep.m_value + vec2( 0.01f, 0.0f);
		m_selectedEndpoint = insert(ep);
		ret = true;

	} else if (io.MouseClicked[0] && !m_editEndpoint) {
	 // click off a point: deselect
		m_selectedEndpoint = m_dragEndpoint = m_dragComponent = -1;
	}

	if (m_selectedEndpoint != kInvalidIndex) {
		if (ImGui::IsKeyPressed(Keyboard::Key_Delete)) {
			erase(m_selectedEndpoint);
			m_selectedEndpoint = kInvalidIndex;
			ret = true;
		}

		ImGui::PushID(&m_endpoints[m_selectedEndpoint]);
			if (!m_editEndpoint && io.MouseClicked[1]) {
				m_editEndpoint = true;
				m_dragOffset = io.MousePos; // store the mouse pos for window positioning
			}
			if (m_editEndpoint) {
				ImGui::SetNextWindowPos(m_dragOffset);
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
				ImGui::Begin("EndpointEdit", nullptr,
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoSavedSettings
					);
					vec2 p = m_endpoints[m_selectedEndpoint].m_value;
					ret |= ImGui::DragFloat("X", &p.x, m_regionSize.x * 0.01f);
					ImGui::SameLine();
					ret |= ImGui::DragFloat("Y", &p.y, m_regionSize.y * 0.01f);
					m_selectedEndpoint = move(m_selectedEndpoint, Component_Value, p);
					
					if (!ImGui::IsWindowFocused()) {
						m_editEndpoint = false;
					}
				ImGui::End();
				ImGui::PopStyleColor();
			}
		ImGui::PopID();
	}

	return ret;
}

void Curve::drawBackground()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	

	drawList.AddRectFilled(m_windowBeg, m_windowEnd, kColorBackground);
	drawList.AddRect(m_windowBeg, m_windowEnd, kColorBorder);

 // curve region bacground
	if (checkEditFlag(EditFlags_ShowHighlight)) {
	/*if (m_curve.m_endpoints.size() > 1) {
		vec2 curveMin = curveToWindow(m_curve.m_min);
		vec2 curveMax = curveToWindow(m_curve.m_max);
		drawList.AddRectFilled(vec2(curveMin.x, m_windowBeg.y), vec2(curveMax.x, m_windowEnd.y), kColorCurveBackground);
		drawList.AddRectFilled(vec2(m_windowBeg.x, curveMin.y), vec2(m_windowEnd.x, curveMax.y), kColorCurveBackground);
	}*/
	}
}

void Curve::drawGrid()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
 
	const float kSpacing = 16.0f;
	const float kBase = 10.0f;

 // vertical 
	float spacing = 0.01f;
	while ((spacing / m_regionSize.x * m_windowSize.x) < kSpacing) {
		spacing *= kBase;
	}
	for (float i = floor(m_regionBeg.x / spacing) * spacing; i < m_regionEnd.x; i += spacing) {
		vec2 line = curveToWindow(vec2(i, 0.0f));
		if (line.x > m_windowBeg.x && line.x < m_windowEnd.x) {
			line = floor(line);
			drawList.AddLine(vec2(line.x, m_windowBeg.y), vec2(line.x, m_windowEnd.y), kColorGridLine);
		}
	}
 // horizontal
	spacing = 0.01f;
	while ((spacing / m_regionSize.y * m_windowSize.y) < kSpacing) {
		spacing *= kBase;
	}
	for (float i = floorf(m_regionBeg.y / spacing) * spacing; i < m_regionEnd.y; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(0.0f, i)));
		if (line.y > m_windowBeg.y && line.y < m_windowEnd.y) {
			drawList.AddLine(vec2(m_windowBeg.x, line.y), vec2(m_windowEnd.x, line.y), kColorGridLine);
		}
	}

 // zero axis
	vec2 zero = floor(curveToWindow(vec2(0.0f)));
	if (zero.x > m_windowBeg.x && zero.x < m_windowEnd.x) {
		drawList.AddLine(vec2(zero.x, m_windowBeg.y), vec2(zero.x, m_windowEnd.y), kColorZeroAxis);
	}
	if (zero.y > m_windowBeg.y && zero.y < m_windowEnd.y) {
		drawList.AddLine(vec2(m_windowBeg.x, zero.y), vec2(m_windowEnd.x, zero.y), kColorZeroAxis);
	}
}

void Curve::drawCurve()
{
	if (m_cache.empty()) {
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();

ImU32 m_curveColor = IM_COL32_MAGENTA;

 // curve
	vec2 p0 = curveToWindow(m_cache[0]);
	for (int i = 1, n = (int)m_cache.size(); i < n; ++i) {
		if (p0.x > m_windowEnd.x) {
			break;
		}
		vec2 p1 = curveToWindow(m_cache[i]);
		if (p0.x < m_windowBeg.x && p1.x < m_windowBeg.x) {
			p0 = p1;
			continue;
		}
		drawList->AddLine(p0, p1, m_curveColor, 2.0f);
		#if Curve_DEBUG
			drawList->AddCircleFilled(p0, 1.5f, IM_COLOR_ALPHA(kColorGridLabel, 0.2f), 6);
		#endif
		p0 = p1;
	}

 // endpoints
	for (int i = 0, n = (int)m_endpoints.size(); i < n; ++i) {
		Endpoint& ep = m_endpoints[i];
		vec2 p = curveToWindow(ep.m_value);
		if (!isInside(p, m_windowBeg, m_windowEnd)) {
			if (p.x > m_windowEnd.x) { // can end search if beyond window X
				break;
			}
			continue;
		}
		ImU32 col = i == m_selectedEndpoint ? kColorValuePoint : m_curveColor;
		drawList->AddCircleFilled(p, kSizeValuePoint, col, 8);
	}

	for (int i = 0, n = (int)m_endpoints.size(); i < n; ++i) {
		Endpoint& ep = m_endpoints[i];
		vec2 pin = curveToWindow(ep.m_in);
		vec2 pout = curveToWindow(ep.m_out);
		if (pin.x > m_windowEnd.x && pout.x > m_windowEnd.x) {
			break;
		}
		if (pout.x < m_windowBeg.x) {			
			continue;
		}
		ImU32 col = i == m_selectedEndpoint ? kColorControlPoint : IM_COLOR_ALPHA(m_curveColor, 0.5f);
		drawList->AddCircleFilled(pin, kSizeControlPoint, col, 8);
		drawList->AddCircleFilled(pout, kSizeControlPoint, col, 8);
		drawList->AddLine(pin, pout, col, 1.0f);

	 // visualize CP constraint
		//if (i > 0) {
		//	posIn = ValueBezier::Constrain(ep.m_in, ep.m_val, bezier.m_endpoints[i - 1].m_val.x, ep.m_val.x);
		//	posIn = curveToWindow(posIn);
		//	drawList.AddCircleFilled(posIn, kSizeControlPoint, IM_COL32_YELLOW, 8);
		//}
		//if (i < n - 1) {
		//	posOut = ValueBezier::Constrain(ep.m_out, ep.m_val, ep.m_val.x, bezier.m_endpoints[i + 1].m_val.x);
		//	posOut = curveToWindow(posOut);
		//	drawList.AddCircleFilled(posOut, kSizeControlPoint, IM_COL32_CYAN, 8);
		//}
	}
}

void Curve::drawSampler(float _t)
{
}

void Curve::drawRuler()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
 
	const float kSpacing = 32.0f;
	const float kBase = 10.0f;
	String<sizeof("999.999")> label;

 // vertical 
	drawList.AddRectFilled(vec2(m_windowBeg.x + kSizeRuler, m_windowBeg.y), vec2(m_windowEnd.x, m_windowBeg.y + kSizeRuler), kColorRuler);
	float spacing = 0.01f;
	while ((spacing / m_regionSize.x * m_windowSize.x) < kSpacing) {
		spacing *= kBase;
	}
	for (float i = floorf(m_regionBeg.x / spacing) * spacing; i < m_regionEnd.x; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(i, 0.0f)));
		if (line.x > m_windowBeg.x && line.x < m_windowEnd.x) {
			label.setf((spacing < 0.1f) ? "%.2f" : (spacing < 1.0f) ?  "%0.1f" : "%1.1f", i);
			drawList.AddText(vec2(line.x + 2.0f, m_windowBeg.y + 1.0f), kColorRulerLabel, (const char*)label);
			drawList.AddLine(vec2(line.x, m_windowBeg.y), vec2(line.x, m_windowBeg.y + kSizeRuler - 1.0f), kColorRulerLabel);
		}
	}
 // horizontal
 // \todo vertical text here
	drawList.AddRectFilled(m_windowBeg, vec2(m_windowBeg.x + kSizeRuler, m_windowEnd.y), kColorRuler);
	spacing = 0.01f;
	while ((spacing / m_regionSize.y * m_windowSize.y) < kSpacing) {
		spacing *= kBase;
	}
	for (float i = floorf(m_regionBeg.y / spacing) * spacing; i < m_regionEnd.y; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(0.0f, i)));
		if (line.y > m_windowBeg.y && line.y < m_windowEnd.y) {
			label.setf((spacing < 0.1f) ? "%.2f" : (spacing < 1.0f) ?  "%0.1f" : "%1.1f", i);
			drawList.AddText(vec2(m_windowBeg.x + 2.0f, line.y), kColorRulerLabel, (const char*)label);
			drawList.AddLine(vec2(m_windowBeg.x, line.y), vec2(m_windowBeg.x + kSizeRuler - 1.0f, line.y), kColorRulerLabel);
		}
	}
}


void Curve::updateCache()
{
	m_cache.clear();
	if (m_endpoints.empty()) {
		return;
	}
	if (m_endpoints.size() == 1) {
		m_cache.push_back(m_endpoints[0].m_value);
		return;
	}

 // \todo only cache the visible subrange of the curve
	auto p0 = m_endpoints.begin();
	auto p1 = m_endpoints.begin() + 1;
	for (; p1 != m_endpoints.end(); ++p0, ++p1) {
		subdivide(*p0, *p1);
	}
}

void Curve::subdivide(const Endpoint& _p0, const Endpoint& _p1, float _maxError, int _limit)
{
	if (_limit == 1) {
		m_cache.push_back(_p0.m_value);
		m_cache.push_back(_p1.m_value);
		return;
	}
	
	vec2 p0 = _p0.m_value;
	vec2 p1 = _p0.m_out;
	vec2 p2 = _p1.m_in;
	vec2 p3 = _p1.m_value;

 // constrain control points on segment (prevent loops)
	//p1 = ValueBezier::Constrain(p1, p0, p0.x, p3.x);
	//p2 = ValueBezier::Constrain(p2, p3, p0.x, p3.x);

 // http://antigrain.com/research/adaptive_bezier/ suggests a better error metric: use the height of CPs above the line p1.m_val - p0.m_val
	vec2 q0 = mix(p0, p1, 0.5f);
	vec2 q1 = mix(p1, p2, 0.5f);
	vec2 q2 = mix(p2, p3, 0.5f);
	vec2 r0 = mix(q0, q1, 0.5f);
	vec2 r1 = mix(q1, q2, 0.5f);
	vec2 s  = mix(r0, r1, 0.5f);
	float err = length(p1 - r0) + length(q1 - s) + length(p2 - r1);
	if (err > _maxError) {
		Endpoint pa, pb;
		pa.m_value = p0;
		pa.m_out   = q0;
		pb.m_in    = r0;
		pb.m_value = s;
		subdivide(pa, pb, _maxError, _limit - 1);

		pa.m_value = s;
		pa.m_out   = r1;
		pb.m_in    = q2;
		pb.m_value = p3;
		subdivide(pa, pb, _maxError, _limit - 1);
		
	} else {
		subdivide(_p0, _p1, _maxError, 1); // push p0,p1

	}
}
