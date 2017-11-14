#include <frm/Curve.h>

#include <frm/icon_fa.h>
#include <frm/interpolation.h>
#include <frm/Input.h>

#include <apt/String.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

#define Curve_DEBUG 0

/*******************************************************************************

                                    Curve

*******************************************************************************/

// PUBLIC

Curve::Curve()
	: m_constrainMin(-FLT_MAX)
	, m_constrainMax(FLT_MAX)
	, m_wrap(Wrap_Clamp)
{
}

int Curve::insert(const Endpoint& _endpoint)
{
	int ret = (int)m_endpoints.size();
	if (!m_endpoints.empty() && _endpoint.m_value.x < m_endpoints[ret - 1].m_value.x) {
	 // can't insert at end, do binary search
		ret = findSegmentStartIndex(_endpoint.m_value.x);
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

int Curve::move(int _endpoint, Component _component, const vec2& _value)
{
	Endpoint& ep = m_endpoints[_endpoint];

	int ret = _endpoint;
	if (_component == Component_Value) {
	 // move CPs
		vec2 delta = _value - ep[Component_Value];
		ep[Component_In] += delta;
		ep[Component_Out] += delta;

	 // swap with neighbor
		ep.m_value = _value;
		if (delta.x > 0.0f && _endpoint < (int)m_endpoints.size() - 1) {
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
			ep[_component].x = APT_MIN(ep[_component].x, ep[Component_Value].x);
		} else {
			ep[_component].x = APT_MAX(ep[_component].x, ep[Component_Value].x);
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

void Curve::erase(int _endpoint)
{
	APT_ASSERT(_endpoint < (int)m_endpoints.size());
	m_endpoints.erase(m_endpoints.begin() + _endpoint);
	updateExtentsAndConstrain(APT_MIN(_endpoint, APT_MAX((int)m_endpoints.size() - 1, 0)));
}

float Curve::wrap(float _t) const
{
	float ret = _t;
	switch (m_wrap) {
		case Wrap_Repeat:
			ret -= m_valueMin.x;
			ret = m_valueMin.x + ret - (m_valueMax.x - m_valueMin.x) * floorf(ret / (m_valueMax.x - m_valueMin.x));
			break;
		case Wrap_Clamp:
		default:
			ret = APT_CLAMP(ret, m_valueMin.x, m_valueMax.x);
			break;
	};
	APT_ASSERT(ret >= m_valueMin.x && ret <= m_valueMax.x);
	return ret;
}

void Curve::setValueConstraint(const vec2& _min, const vec2& _max)
{
	m_constrainMin = _min; 
	m_constrainMax = _max;
}

// PRIVATE

int Curve::findSegmentStartIndex(float _t) const
{
	int lo = 0, hi = (int)m_endpoints.size() - 1;
	while (hi - lo > 1) {
		uint32 md = (hi + lo) / 2;
		if (_t > m_endpoints[md].m_value.x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_endpoints[hi].m_value.x ? hi : lo;
}

void Curve::updateExtentsAndConstrain(int _endpoint)
{
	m_valueMin = m_endpointMin = vec2(FLT_MAX);
	m_valueMax = m_endpointMax = vec2(-FLT_MAX);
	for (auto& ep : m_endpoints) {
	 // constrain value points inside constraint region
		vec2 inDelta = ep.m_in - ep.m_value;
		vec2 outDelta = ep.m_out - ep.m_value;
		ep.m_value = min(max(ep.m_value, m_constrainMin), m_constrainMax);
		ep.m_in = ep.m_value + inDelta;
		ep.m_out = ep.m_value + outDelta;
	 // constrain control points
		// \todo		

		m_valueMin = min(m_valueMin, ep.m_value);
		m_valueMax = max(m_valueMax, ep.m_value);
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

void Curve::constrainCp(vec2& _cp_, const vec2& _vp, float _x0, float _x1)
{
 // \todo simple linear algebra solution? This is a ray-plane intersection
	vec2 ret = _cp_;
	if (ret.x < _x0) {
		vec2 v, n;
		float vlen, t;
		v = _cp_ - _vp;
		vlen = length(v);
		v = v / vlen;
		n = vec2(1.0f, 0.0f);
		t = dot(n, n * _x0 - _vp.x) / dot(n, v);
		vlen = APT_MIN(vlen, t > 0.0f ? t : vlen);
		ret = _vp + v * vlen;

	} else if (ret.x > _x1) {
		vec2 v, n;
		float vlen, t;
		v = _cp_ - _vp;
		vlen = length(v);
		v = v / vlen;
		n = vec2(1.0f, 0.0f);
		t = dot(n, n * _x1 - _vp.x) / dot(n, v);
		vlen = APT_MIN(vlen, t > 0.0f ? t : vlen);
		ret = _vp + v * vlen;

	}
	_cp_ = ret;
}

/*******************************************************************************

                                 CurveEditor

*******************************************************************************/

static const ImU32 kColorBorder          = ImColor(0xdba0a0a0);
static const ImU32 kColorBackground      = ImColor(0x55191919);
static const ImU32 kColorRuler           = ImColor(0x66050505);
static const ImU32 kColorRulerLabel      = ImColor(0xff555555);
static const ImU32 kColorCurveHighlight  = ImColor(0x06a0a0aa);
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

// PUBLIC

CurveEditor::CurveEditor()
	: m_regionBeg(0.0f)
	, m_regionEnd(1.0f)
	, m_regionSize(1.0f)
	, m_selectedEndpoint(Curve::kInvalidIndex)
	, m_dragEndpoint(Curve::kInvalidIndex)
	, m_dragComponent(-1)
	, m_dragOffset(0.0f)
	, m_dragRuler(false)
	, m_editEndpoint(false)
	, m_showAllCurves(true)
	, m_isDragging(false)
	, m_editFlags(Flags_Default)
	, m_selectedCurve(-1)
{
}

void CurveEditor::addCurve(Curve* _curve_, const ImColor& _color)
{
	int curveIndex = (int)m_curves.size();
	m_curves.push_back(_curve_);
	m_curveColors.push_back((ImU32)_color);
	m_drawCaches.push_back(DrawCache());
	updateCache(curveIndex);
	if (m_selectedCurve == -1) {
		m_selectedCurve = curveIndex;
		if (!m_curves[m_selectedCurve]->m_endpoints.empty()) {
			fit(0);
			fit(1);
		}
	}
}

void CurveEditor::selectCurve(const Curve* _curve)
{
	for (int i = 0; i < m_curves.size(); ++i) {
		if (m_curves[i] == _curve) {
			m_selectedCurve = i;
		}
	}
}

bool CurveEditor::drawEdit(const vec2& _sizePixels, float _t, Flags _flags)
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

 // zoom/pan
	if (m_isDragging || (windowActive && mouseInWindow)) {
		if (io.KeyCtrl) {
		 // zoom Y (value)
			float wy = ImGui::GetWindowContentRegionMax().y;
			float zoom = (io.MouseWheel * m_regionSize.y * 0.1f);
			float before = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionSize.y = APT_MAX(m_regionSize.y - zoom, 0.01f);
			float after = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionBeg.y += (before - after);

		} else {
		 // zoom X (time)
			float wx = ImGui::GetWindowContentRegionMax().x;
			float zoom = (io.MouseWheel * m_regionSize.x * 0.1f);
			float before = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionSize.x = APT_MAX(m_regionSize.x - zoom, 0.01f);
			float after = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionBeg.x += (before - after);

		}
		
		vec2 zoom = vec2(0.0f);
		if (io.KeyCtrl) {
		 // zoom Y (value)
			zoom.y += (io.MouseWheel * m_regionSize.y * 0.1f);
		} else {
		 // zoom X (time)
			zoom.x += (io.MouseWheel * m_regionSize.x * 0.1f);
		}
		if (checkEditFlag(Flags_ShowRuler)) {
		 // zoom X/Y via ruler drag
			if (!m_isDragging && io.MouseDown[2] && isInside(mousePos, m_windowBeg, vec2(m_windowEnd.x, m_windowBeg.y + kSizeRuler))) {
				m_dragRuler.x = true;
			}
			if (!m_isDragging && io.MouseDown[2] && isInside(mousePos, m_windowBeg, vec2(m_windowBeg.x + kSizeRuler, m_windowEnd.y))) {
				m_dragRuler.y = true;
			}
			if (m_dragRuler.x) {
				m_dragRuler.x = io.MouseDown[2];
				zoom.x += io.MouseDelta.x * m_regionSize.x * 0.03f;
			}
			if (m_dragRuler.y) {
				m_dragRuler.y = io.MouseDown[2];
				zoom.y += io.MouseDelta.y * m_regionSize.y * 0.03f;
			}
		}

		vec2 before = (vec2(io.MousePos) - vec2(ImGui::GetWindowPos())) / m_windowSize * m_regionSize;
		m_regionSize.x = APT_MAX(m_regionSize.x - zoom.x, 0.1f);
		m_regionSize.y = APT_MAX(m_regionSize.y - zoom.y, 0.1f);
		vec2 after = (vec2(io.MousePos) - vec2(ImGui::GetWindowPos())) / m_windowSize * m_regionSize;
		m_regionBeg += (before - after);

	 // pan
		if (!any(m_dragRuler) && io.MouseDown[2]) {
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
		updateCache(m_selectedCurve);
	}

	drawBackground();
	if (checkEditFlag(Flags_ShowGrid)) {
		drawGrid();
	}
	ImGui::PushClipRect(m_windowBeg + vec2(1.0f), m_windowEnd - vec2(1.0f), true);
		if (m_showAllCurves) {
			for (int i = 0; i < (int)m_curves.size(); ++i) {
				if (i != m_selectedCurve) {
					drawCurve(i);
				}
			}
		}
		if (m_selectedCurve != -1) {
			drawCurve(m_selectedCurve);
		}

		if (checkEditFlag(Flags_ShowRuler)) {
			drawRuler();
		}
	ImGui::PopClipRect();

	if (!m_editEndpoint && mouseInWindow && windowActive && io.MouseClicked[1]) {
		ImGui::OpenPopup("CurveEditorPopup");
	}
	if (ImGui::BeginPopup("CurveEditorPopup")) {
		if (m_selectedCurve != -1) {
			Curve& curve = *m_curves[m_selectedCurve];
			if (ImGui::BeginMenu("Wrap")) {
				if (ImGui::MenuItem("Clamp", nullptr, curve.m_wrap == Curve::Wrap_Clamp)) {
					curve.m_wrap = Curve::Wrap_Clamp;
				}
				if (ImGui::MenuItem("Repeat", nullptr, curve.m_wrap == Curve::Wrap_Repeat)) {
					curve.m_wrap = Curve::Wrap_Repeat;
				}
				ImGui::EndMenu();
			}
			ImGui::Spacing();
		}
		if (ImGui::MenuItem("Fit")) {
			fit(0);
			fit(1);
		}
		if (m_curves.size() > 1) {
			if (ImGui::MenuItem("Show All", "", m_showAllCurves)) {
				m_showAllCurves = !m_showAllCurves;
			}
		}

		ImGui::EndPopup();
	}

	return ret;
}

// PRIVATE

bool CurveEditor::isInside(const vec2& _point, const vec2& _min, const vec2& _max)
{
	return _point.x > _min.x && _point.x < _max.x && _point.y > _min.y && _point.y < _max.y;
}

bool CurveEditor::isInside(const vec2& _point, const vec2& _origin, float _radius)
{
	return distance2(_point, _origin) < (_radius * _radius);
}

vec2 CurveEditor::curveToRegion(const vec2& _pos)
{
	vec2 ret = (_pos - m_regionBeg) / m_regionSize;
	ret.y = 1.0f - ret.y;
	return ret;
}
vec2 CurveEditor::curveToWindow(const vec2& _pos)
{
	vec2 ret = curveToRegion(_pos);
	return m_windowBeg + ret * m_windowSize;
}

vec2 CurveEditor::regionToCurve(const vec2& _pos)
{
	vec2 pos = _pos;
	pos.y = 1.0f - _pos.y;
	return m_regionBeg + pos *  m_regionSize;
}

vec2 CurveEditor::windowToCurve(const vec2& _pos)
{
	return regionToCurve((_pos - m_windowBeg) / m_windowSize);
}

void CurveEditor::fit(int _dim)
{
	Curve& curve = *m_curves[m_selectedCurve];
	float pad = (curve.m_endpointMax[_dim] - curve.m_endpointMin[_dim]) * 0.1f;
	m_regionBeg[_dim]  = curve.m_endpointMin[_dim] - pad;
	m_regionSize[_dim] = (curve.m_endpointMax[_dim] - m_regionBeg[_dim]) + pad * 2.0f;
}

bool CurveEditor::editCurve()
{
	if (!(m_editEndpoint || ImGui::IsWindowFocused() || m_dragEndpoint != Curve::kInvalidIndex)) {
		return false;
	}
	
	bool ret = false;

	Curve& curve = *m_curves[m_selectedCurve];
	ImGuiIO& io = ImGui::GetIO();
	vec2 mousePos = io.MousePos;
	
 // point selection
	if (!curve.m_endpoints.empty() && !m_editEndpoint && (io.MouseDown[0] || io.MouseDown[1]) && m_dragEndpoint == Curve::kInvalidIndex) {
		for (int i = 0, n = (int)curve.m_endpoints.size(); i < n; ++i) {
			Curve::Endpoint& ep = curve.m_endpoints[i];
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
				if (isInside(mousePos, p, kSizeSelectPoint)) {
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
	if (m_dragEndpoint != Curve::kInvalidIndex) {
	 // left click + drag: move selected point
		if (io.MouseDown[0] && io.MouseDownDuration[0]) {
		 // point is being dragged
			vec2 newPos = windowToCurve(mousePos + m_dragOffset);				

			if (m_dragComponent == Curve::Component_Value) {
			 // dragging component, display X Y value
				if (io.MouseDownDuration[0] > 0.1f) {
					ImGui::BeginTooltip();
						ImGui::Text("X %1.3f, Y %1.3f", curve.m_endpoints[m_selectedEndpoint].m_value.x, curve.m_endpoints[m_selectedEndpoint].m_value.y);
					ImGui::EndTooltip();
				}
			} else {
			 // dragging endpoint, constrain to X/Y axis if ctrl pressed
				if (io.KeyCtrl) {
					vec2 delta = normalize(mousePos - curveToWindow(curve.m_endpoints[m_selectedEndpoint].m_value));
					if (abs(delta.y) > 0.5f) {
						newPos.x = curve.m_endpoints[m_selectedEndpoint].m_value.x;
					} else {
						newPos.y = curve.m_endpoints[m_selectedEndpoint].m_value.y;
					}
				}
			}

			m_selectedEndpoint = m_dragEndpoint = curve.move(m_dragEndpoint, (Curve::Component)m_dragComponent, newPos);
			ImGui::CaptureMouseFromApp();

		} else {
		 // mouse just released
			m_dragEndpoint = m_dragComponent = -1;
		}
		ret = true;

	} else if (io.MouseDoubleClicked[0]) {
	 // double click: insert a point
	 // \todo better tangent estimation?
		float tangentScale = m_regionSize.x * 0.05f;
		Curve::Endpoint ep;
		ep.m_value = windowToCurve(io.MousePos);
		ep.m_in    = ep.m_value + vec2(-tangentScale, 0.0f);
		ep.m_out   = ep.m_value + vec2( tangentScale, 0.0f);
		m_selectedEndpoint = curve.insert(ep);
		ret = true;

	} else if (io.MouseClicked[0] && !m_editEndpoint) {
	 // click off a point: deselect
		m_selectedEndpoint = m_dragEndpoint = m_dragComponent = -1;
	}

	if (m_selectedEndpoint != Curve::kInvalidIndex) {
		bool deleteEndpoint = false;

		if (ImGui::IsKeyPressed(Keyboard::Key_Delete)) {
			deleteEndpoint = true;

		} else {
			ImGui::PushID(&curve.m_endpoints[m_selectedEndpoint]);
				if (!m_editEndpoint && io.MouseClicked[1] && isInside(mousePos, curveToWindow(curve.m_endpoints[m_selectedEndpoint].m_value), kSizeSelectPoint)) {
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
						vec2 p = curve.m_endpoints[m_selectedEndpoint].m_value;
						ImGui::PushItemWidth(128.0f);
						ret |= ImGui::DragFloat("X", &p.x, m_regionSize.x * 0.01f);
						ImGui::SameLine();
						ret |= ImGui::DragFloat("Y", &p.y, m_regionSize.y * 0.01f);
						m_selectedEndpoint = curve.move(m_selectedEndpoint, Curve::Component_Value, p);
						ImGui::PopItemWidth();
						
						if (ImGui::Button("Delete")) {
							deleteEndpoint = true;
							m_editEndpoint = false;
						}

						if (!ImGui::IsWindowFocused()) {
							m_editEndpoint = false;
						}
					ImGui::End();
					ImGui::PopStyleColor();
				}
			ImGui::PopID();
		}

		if (deleteEndpoint) {
			curve.erase(m_selectedEndpoint);
			m_selectedEndpoint = Curve::kInvalidIndex;
			ret = true;
		}
	}

	return ret;
}

void CurveEditor::drawBackground()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
	Curve& curve = *m_curves[m_selectedCurve];

	drawList.AddRectFilled(m_windowBeg, m_windowEnd, kColorBackground);
	drawList.AddRect(m_windowBeg, m_windowEnd, kColorBorder);
}

void CurveEditor::drawGrid()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
	Curve& curve = *m_curves[m_selectedCurve];
 
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

 // zero axis/constraint region
	vec2 zero = floor(curveToWindow(vec2(0.0f)));
	if (zero.x > m_windowBeg.x && zero.x < m_windowEnd.x) {
		drawList.AddLine(vec2(zero.x, m_windowBeg.y), vec2(zero.x, m_windowEnd.y), kColorZeroAxis);
	}
	if (zero.y > m_windowBeg.y && zero.y < m_windowEnd.y) {
		drawList.AddLine(vec2(m_windowBeg.x, zero.y), vec2(m_windowEnd.x, zero.y), kColorZeroAxis);
	}
	drawList.AddRect(floor(curveToWindow(curve.m_constrainMin)), floor(curveToWindow(curve.m_constrainMax)), kColorZeroAxis);
}

void CurveEditor::drawCurve(int _curveIndex)
{
	Curve& curve = *m_curves[_curveIndex];
	DrawCache& cache = m_drawCaches[_curveIndex];
	ImU32 curveColor =  IM_COLOR_ALPHA(m_curveColors[_curveIndex], _curveIndex == m_selectedCurve ? 1.0f : kAlphaCurveWrap);
	bool isSelected = _curveIndex == m_selectedCurve;

	if (cache.empty()) {
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();

 // curve region highlight
	if (isSelected && checkEditFlag(Flags_ShowHighlight)) {
		if (curve.m_endpoints.size() > 1) {
			vec2 curveMin = curveToWindow(curve.m_valueMin);
			vec2 curveMax = curveToWindow(curve.m_valueMax);
			drawList->AddRectFilled(vec2(curveMin.x, m_windowBeg.y), vec2(curveMax.x, m_windowEnd.y), kColorCurveHighlight);
			drawList->AddRectFilled(vec2(m_windowBeg.x, curveMin.y), vec2(m_windowEnd.x, curveMax.y), kColorCurveHighlight);
			drawList->AddRect(curveMin, curveMax, kColorCurveHighlight);
		}
	}

 // wrap/unselected curve
	switch (curve.m_wrap) {
		case Curve::Wrap_Clamp: {
			vec2 p = curveToWindow(cache.front());
			drawList->AddLine(vec2(m_windowBeg.x, p.y), p, curveColor, 1.0f);
			p = curveToWindow(cache.back());
			drawList->AddLine(vec2(m_windowEnd.x, p.y), p, curveColor, 1.0f);
			break;
		}
		case Curve::Wrap_Repeat: {
			if (cache.size() < 2) {
				vec2 p = curveToWindow(cache.front());
				drawList->AddLine(vec2(m_windowBeg.x, p.y), vec2(m_windowEnd.x, p.y), curveColor, 1.0f);
				break;
			}
			int i = curve.findSegmentStartIndex(curve.wrap(m_regionBeg.x));
			vec2 p0 = curveToWindow(cache[i]);
			float windowScale = m_windowSize.x / m_regionSize.x;
			float offset = p0.x - m_windowBeg.x;
			offset += (curve.wrap(m_regionBeg.x) - cache[i].x) * windowScale;
			float offsetStep = (curve.m_valueMax.x - curve.m_valueMin.x) * windowScale;
			p0.x -= offset;
			for (;;) {
				++i;
				if (p0.x > m_windowEnd.x) {
					break;
				}
				if (i >= (int)cache.size()) {
					i = 0;
					offset -= offsetStep;
				}
				vec2 p1 = curveToWindow(cache[i]);
				p1.x -= offset;
				drawList->AddLine(p0, p1, curveColor, 1.0f);
				p0 = p1;
			}
			break;
		}
		default:
			break;
	};

 // curve
	vec2 p0 = curveToWindow(cache[0]);
	for (int i = 1, n = (int)cache.size(); i < n; ++i) {
		if (p0.x > m_windowEnd.x) {
			break;
		}
		vec2 p1 = curveToWindow(cache[i]);
		if (p0.x < m_windowBeg.x && p1.x < m_windowBeg.x) {
			p0 = p1;
			continue;
		}
		drawList->AddLine(p0, p1, curveColor, isSelected ? 2.0f : 1.0f);
		#if Curve_DEBUG
			drawList->AddCircleFilled(p0, 1.5f, IM_COLOR_ALPHA(kColorGridLabel, 0.2f), 6);
		#endif
		p0 = p1;
	}

	if (!isSelected) {
		return;
	}

 // endpoints
	for (int i = 0, n = (int)curve.m_endpoints.size(); i < n; ++i) {
		Curve::Endpoint& ep = curve.m_endpoints[i];
		vec2 p = curveToWindow(ep.m_value);
		if (!isInside(p, m_windowBeg, m_windowEnd)) {
			if (p.x > m_windowEnd.x) { // can end search if beyond window X
				break;
			}
			continue;
		}
		ImU32 col = i == m_selectedEndpoint ? kColorValuePoint : curveColor;
		drawList->AddCircleFilled(p, kSizeValuePoint, col, 8);
	}

	for (int i = 0, n = (int)curve.m_endpoints.size(); i < n; ++i) {
		Curve::Endpoint& ep = curve.m_endpoints[i];
		vec2 pin = curveToWindow(ep.m_in);
		vec2 pout = curveToWindow(ep.m_out);
		if (pin.x > m_windowEnd.x && pout.x > m_windowEnd.x) {
			break;
		}
		if (pout.x < m_windowBeg.x) {			
			continue;
		}
		ImU32 col = i == m_selectedEndpoint ? kColorControlPoint : curveColor;
		drawList->AddCircleFilled(pin, kSizeControlPoint, col, 8);
		drawList->AddCircleFilled(pout, kSizeControlPoint, col, 8);
		drawList->AddLine(pin, pout, col, 1.0f);

		#if Curve_DEBUG
		 // visualize CP constraint
			if (i > 0) {
				pin = ep.m_in;
				constrainCp(pin, ep.m_value, m_endpoints[i - 1].m_value.x, ep.m_value.x);
				pin = curveToWindow(pin);
				drawList->AddCircleFilled(pin, kSizeControlPoint, IM_COL32_YELLOW, 8);
			}
			if (i < n - 1) {
				pout = ep.m_out;
				constrainCp(pout, ep.m_value, ep.m_value.x, m_endpoints[i + 1].m_value.x);
				pout = curveToWindow(pout);
				drawList->AddCircleFilled(pout, kSizeControlPoint, IM_COL32_CYAN, 8);
			}
		#endif
	}
}

void CurveEditor::drawSampler(float _t)
{
}

void CurveEditor::drawRuler()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
 	Curve& curve = *m_curves[m_selectedCurve];

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

void CurveEditor::updateCache(int _curveIndex)
{
	Curve& curve = *m_curves[_curveIndex];
	DrawCache& cache = m_drawCaches[_curveIndex];

	cache.clear();
	if (curve.m_endpoints.empty()) {
		return;
	}
	if (curve.m_endpoints.size() == 1) {
		cache.push_back(curve.m_endpoints[0].m_value);
		return;
	}

 // \todo only cache the visible subrange of the curve
	auto p0 = curve.m_endpoints.begin();
	auto p1 = curve.m_endpoints.begin() + 1;
	for (; p1 != curve.m_endpoints.end(); ++p0, ++p1) {
		subdivide(_curveIndex, *p0, *p1);
	}
}

void CurveEditor::subdivide(int _curveIndex, const Curve::Endpoint& _p0, const Curve::Endpoint& _p1, float _maxError, int _limit)
{
	Curve& curve = *m_curves[_curveIndex];
	DrawCache& cache = m_drawCaches[_curveIndex];

	if (_limit == 1) {
		cache.push_back(_p0.m_value);
		cache.push_back(_p1.m_value);
		return;
	}
	
	vec2 p0 = _p0.m_value;
	vec2 p1 = _p0.m_out;
	vec2 p2 = _p1.m_in;
	vec2 p3 = _p1.m_value;

 // constrain control point on segment (prevent loops)
	curve.constrainCp(p1, p0, p0.x, p3.x);
	curve.constrainCp(p2, p3, p0.x, p3.x);

 // http://antigrain.com/research/adaptive_bezier/ suggests a better error metric: use the height of CPs above the line p1.m_val - p0.m_val
	vec2 q0 = lerp(p0, p1, 0.5f);
	vec2 q1 = lerp(p1, p2, 0.5f);
	vec2 q2 = lerp(p2, p3, 0.5f);
	vec2 r0 = lerp(q0, q1, 0.5f);
	vec2 r1 = lerp(q1, q2, 0.5f);
	vec2 s  = lerp(r0, r1, 0.5f);
	float err = length(p1 - r0) + length(q1 - s) + length(p2 - r1);
	if (err > _maxError) {
		Curve::Endpoint pa, pb;
		pa.m_value = p0;
		pa.m_out   = q0;
		pb.m_in    = r0;
		pb.m_value = s;
		subdivide(_curveIndex, pa, pb, _maxError, _limit - 1);

		pa.m_value = s;
		pa.m_out   = r1;
		pb.m_in    = q2;
		pb.m_value = p3;
		subdivide(_curveIndex, pa, pb, _maxError, _limit - 1);
		
	} else {
		subdivide(_curveIndex, _p0, _p1, _maxError, 1); // push p0,p1

	}
}
