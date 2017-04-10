#include <frm/ValueCurve.h>

#include <apt/String.h>
#include <apt/Json.h>

#include <frm/icon_fa.h>

using namespace frm;
using namespace apt;

/******************************************************************************

                               ValueBezier

******************************************************************************/

// PRIVATE

ValueBezier::ValueBezier()
	: m_wrap(Wrap_Repeat)
	, m_min(FLT_MAX)
	, m_max(-FLT_MAX)
	, m_range(FLT_MAX)
{
}

void ValueBezier::serialize(JsonSerializer& _json_)
{
	const char* kWrapModes[] = { "Clamp", "Repeat" };
	APT_STATIC_ASSERT(APT_ARRAY_COUNT(kWrapModes) == Wrap_Count);
	String<16> tmp = kWrapModes[m_wrap];
	_json_.value("Wrap", (StringBase&)tmp);
	if (_json_.getMode() == JsonSerializer::Mode_Read) {
		for (int i = 0; i < Wrap_Count; ++i) {
			if (tmp == kWrapModes[i]) {
				m_wrap = (Wrap)i;
				break;
			}
		}
	}
	if (_json_.beginArray("Endpoints")) {
		if (_json_.getMode() == JsonSerializer::Mode_Read) {
			int endpointCount = _json_.getArrayLength();
			m_endpoints.resize(endpointCount);
		}		
		for (auto& endpoint : m_endpoints) {
			if (_json_.beginArray()) {
				_json_.value(endpoint.m_in);
				_json_.value(endpoint.m_val);
				_json_.value(endpoint.m_out);
				
				_json_.endArray();
			}
		} 
		_json_.endArray();
	}
}

/*vec2 ValueBezier::sample(float _t)
{
 // handle degenerate curve (empty list or <2 points)
	if (m_endpoints.empty()) {
		return vec2(0.0f, 0.0f);
	} else if (m_endpoints.size() < 2) {
		return m_endpoints[0].m_val;
	}

 // find endpoints
	_t = wrap(_t);
	int i0 = findSegmentStart(_t);
	int i1 = i0 + 1;
	i1 = m_wrap == Wrap_Clamp ? APT_MIN(i1, (int)m_endpoints.size()) : i1 % ((int)m_endpoints.size() - 1);

 // normalize _t over segment range
	float range = m_endpoints[i1].m_val.x - m_endpoints[i0].m_val.x;
	_t = (_t - m_endpoints[i0].m_val.x) / (range > 0.0f ? range : 1.0f);

	return Sample(m_endpoints[i0], m_endpoints[i1], _t);
}

vec2 ValueBezier::Sample(const Endpoint& _p0, const Endpoint& _p1, float _t)
{
	vec2 p0 = _p0.m_val;
	vec2 p1 = _p0.m_out;
	vec2 p2 = _p1.m_in;
	vec2 p3 = _p1.m_val;

 // constrain control point on segment (prevent loops)
	p1 = Constrain(p1, p0, p0.x, p3.x);
	p2 = Constrain(p2, p3, p0.x, p3.x);
	
 // evaluate curve
	float u = 1.0f - _t;
	float t2 = _t * _t;
	float u2 = u * u;
	float t3 = t2 * _t;
	float u3 = u2 * u;
	vec2 ret = u3 * p0;
	ret += 3.0f * u2 * _t * p1;
	ret += 3.0f * u * t2 * p2;
	ret += t3 * p3;

	return ret;
}
*/
int ValueBezier::insert(const vec2& _pos, float _tangentScale)
{
 // find insertion point
	int ret = (int)m_endpoints.size();
	if (!m_endpoints.empty() && _pos.x < m_endpoints[ret - 1].m_val.x) {
	 // can't insert at end, do binary search
		ret = findSegmentStart(_pos.x);
		ret += (_pos.x >= m_endpoints[ret].m_val.x) ? 1 : 0; // handle case where _pos.x should be inserted at 0, normally we +1 to ret
	}

 // estimate CPs
 // \todo better tangent estimation?
	vec2 tangent = vec2(_tangentScale, 0.0f);
	//if (ret > 0 && ret < (int)m_endpoints.size() - 1) {
	// // inserting between 2 existing points
	//	float beg = m_endpoints[ret - 1].m_val.x;
	//	float end = m_endpoints[ret].m_val.x;
	//	float off = (end - beg) * 0.05f;
	//	tangent = (sample(_pos.x + off) - sample(_pos.x - off)) * 0.5f;
	//}

 // insert new point
	Endpoint p;
	p.m_val = _pos;
	p.m_in  = p.m_val - tangent;
	p.m_out = p.m_val + tangent;
	m_endpoints.insert(m_endpoints.begin() + ret, p);

	if (m_wrap == Wrap_Repeat) {
		if (ret == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (ret == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}


	updateExtents();
	
	return ret;
}

int ValueBezier::move(int _endpoint, int _component, const vec2& _pos)
{
	APT_ASSERT(_endpoint < (int)m_endpoints.size());
	APT_ASSERT(_component < Component_Count);

	Endpoint& ep = m_endpoints[_endpoint];

	int ret = _endpoint;
	if (_component == Component_Val) {
	 // move CPs
		vec2 delta = _pos - ep[Component_Val];
		ep[Component_In] += delta;
		ep[Component_Out] += delta;

	 // swap EP with neighbor
		ep.m_val = _pos;
		if (delta.x > 0.0f && _endpoint < (int)m_endpoints.size() - 1) {
			int i = _endpoint + 1;
			if (_pos.x > m_endpoints[i].m_val.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		} else if (_endpoint > 0) {
			int i = _endpoint - 1;
			if (_pos.x < m_endpoints[i].m_val.x) {
				eastl::swap(ep, m_endpoints[i]);
				ret = i;
			}
		}


	} else {
	 // prevent EP crossing CP in x
		ep[_component] = _pos;
		if (_component == Component_In) {
			ep[_component].x = min(ep[_component].x, ep[Component_Val].x);
		} else {
			ep[_component].x = max(ep[_component].x, ep[Component_Val].x);
		}

	 // CP pairs are locked so we must update the other
		Component other = _component == Component_In ? Component_Out : Component_In;
		int i = other == Component_In ? 0 : 1;
		vec2 v = ep[Component_Val] - ep[_component];
		ep[other] = ep[Component_Val] + v;

	}
	
	updateExtents();

	if (m_wrap == Wrap_Repeat) {
		if (_endpoint == (int)m_endpoints.size() - 1) {
			copyValueAndTangent(m_endpoints.back(), m_endpoints.front());
		} else if (_endpoint == 0) {
			copyValueAndTangent(m_endpoints.front(), m_endpoints.back());
		}
	}

	return ret;
}

void ValueBezier::erase(int _i)
{
	APT_ASSERT(_i < (int)m_endpoints.size());
	m_endpoints.erase(m_endpoints.begin() + _i);
	updateExtents();
}

vec2 ValueBezier::Constrain(const vec2& _cp, const vec2& _ep, float _x0, float _x1)
{
 // \todo must be a simple linear algebra solution
	vec2 ret = _cp;
	if (ret.x < _x0) {
		//vec2 a = vec2(_x0 - _cp.x, 0.0f);
		//vec2 b = _ep - _cp;
		//ret = _cp + (dot(a, b) / dot(b, b)) * b;
		
		vec2 v, n;
		float vlen, t;
		v = _cp - _ep;
		vlen = length(v);
		v = v / vlen;
		n = vec2(1.0f, 0.0f);
		t = dot(n, n * _x0 - _ep.x) / dot(n, v);
		vlen = min(vlen, t > 0.0f ? t : vlen);
		ret = _ep + v * vlen;

	} else if (ret.x > _x1) {
		vec2 v, n;
		float vlen, t;
		v = _cp - _ep;
		vlen = length(v);
		v = v / vlen;
		n = vec2(1.0f, 0.0f);
		t = dot(n, n * _x1 - _ep.x) / dot(n, v);
		vlen = min(vlen, t > 0.0f ? t : vlen);
		ret = _ep + v * vlen;

	}
	return ret;
}

void ValueBezier::updateExtents()
{
	m_min = vec2(FLT_MAX);
	m_max = vec2(-FLT_MAX);
	for (auto& ep : m_endpoints) {
		for (int i = 0; i < Component_Count; ++i) {
			m_min = min(m_min, ep[i]);
			m_max = max(m_max, ep[i]);
		}
	}
	m_range = m_max - m_min;
}

float ValueBezier::wrap(float _t) const
{
	float ret = _t;
	switch (m_wrap) {
		case Wrap_Repeat:
			ret -= m_min.x;
			ret = m_min.x + ret - m_range.x * floorf(ret / m_range.x);
			break;
		case Wrap_Clamp:
		default:
			ret = clamp(ret, m_min.x, m_max.x);
			break;
	};
	APT_ASSERT(ret >= m_min.x && ret <= m_max.x);
	return ret;
}

int ValueBezier::findSegmentStart(float _t) const
{
	int lo = 0, hi = (int)m_endpoints.size() - 1;
	while (hi - lo > 1) {
		u32 md = (hi + lo) / 2;
		if (_t > m_endpoints[md].m_val.x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_endpoints[hi].m_val.x ? hi : lo;
}

void ValueBezier::copyValueAndTangent(const Endpoint& _src, Endpoint& dst_)
{
	dst_.m_val.y = _src.m_val.y;
	dst_.m_in = dst_.m_val + (_src.m_in - _src.m_val);
	dst_.m_out = dst_.m_val + (_src.m_out - _src.m_val);
}

/******************************************************************************

                                ValueCurve

******************************************************************************/

// PUBLIC

float ValueCurve::sample(float _t) const
{
	if (m_endpoints.size() < 2) {
		return m_endpoints.front().y;
	}

	_t = wrap(_t);
	int i = findSegmentStart(_t);
	
	float range = m_endpoints[i + 1].x - m_endpoints[i].x;
	_t = (_t - m_endpoints[i].x) / (range > 0.0f ? range : 1.0f);;

	return mix(m_endpoints[i].y, m_endpoints[i + 1].y, _t);
}

// PRIVATE

const float ValueCurve::kDefaultMaxError = 0.002f;


float ValueCurve::wrap(float _t) const
{
	float ret = _t;
	switch (m_wrap) {
		case Wrap_Repeat:
			//ret -= m_min.x;
			//ret = m_min.x + ret - m_range.x * floorf(ret / m_range.x);
			ret =  m_min.x + fract((ret - m_min.x) / m_range.x) * m_range.x; 
			break;
		case Wrap_Clamp:
		default:
			ret = clamp(ret, m_min.x, m_max.x);
			break;
	};
	APT_ASSERT(ret >= m_min.x && ret <= m_max.x);
	return ret;
}

int ValueCurve::findSegmentStart(float _t) const
{
	int lo = 0, hi = (int)m_endpoints.size() - 1;
	while (hi - lo > 1) {
		u32 md = (hi + lo) / 2;
		if (_t > m_endpoints[md].x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_endpoints[hi].x ? hi : lo;
}

void ValueCurve::fromBezier(const ValueBezier& _bezier, float _maxError)
{
	m_wrap = (Wrap)_bezier.m_wrap;
	m_endpoints.clear();
	if (m_bezier.m_endpoints.empty()) {
		m_min = m_max = m_range = vec2(0.0f);
		return;
	}

	if (m_bezier.m_endpoints.size() == 1) {
		m_endpoints.push_back(m_bezier.m_endpoints[0].m_val);
		m_min = m_max = m_range = vec2(m_endpoints.back());
		return;
	}

	m_min = vec2(FLT_MAX);
	m_max = vec2(-FLT_MAX);
	auto p0 = _bezier.m_endpoints.begin();
	auto p1 = _bezier.m_endpoints.begin() + 1;
	for (; p1 != _bezier.m_endpoints.end(); ++p0, ++p1) {
		subdivide(*p0, *p1, _maxError);
	}
	m_range = m_max - m_min;
}

void ValueCurve::subdivide(const ValueBezier::Endpoint& _p0, const ValueBezier::Endpoint& _p1, float _maxError, int _limit)
{
	if (_limit == 1) {
		m_endpoints.push_back(_p0.m_val);
		m_endpoints.push_back(_p1.m_val);
		m_min = min(m_min, min(_p0.m_val, _p1.m_val));
		m_max = max(m_max, max(_p0.m_val, _p1.m_val));
		return;
	}
	
	vec2 p0 = _p0.m_val;
	vec2 p1 = _p0.m_out;
	vec2 p2 = _p1.m_in;
	vec2 p3 = _p1.m_val;

 // constrain control point on segment (prevent loops)
	p1 = ValueBezier::Constrain(p1, p0, p0.x, p3.x);
	p2 = ValueBezier::Constrain(p2, p3, p0.x, p3.x);

 // http://antigrain.com/research/adaptive_bezier/ suggests a better error metric: use the height of CPs above the line p1.m_val - p0.m_val
	vec2 q0 = mix(p0, p1, 0.5f);
	vec2 q1 = mix(p1, p2, 0.5f);
	vec2 q2 = mix(p2, p3, 0.5f);
	vec2 r0 = mix(q0, q1, 0.5f);
	vec2 r1 = mix(q1, q2, 0.5f);
	vec2 s  = mix(r0, r1, 0.5f);
	float err = length(p1 - r0) + length(q1 - s) + length(p2 - r1);
	if (err > _maxError) {
		ValueBezier::Endpoint pa, pb;
		pa.m_val = p0;
		pa.m_out = q0;
		pb.m_in  = r0;
		pb.m_val = s;
		subdivide(pa, pb, _maxError, _limit - 1);

		pa.m_val = s;
		pa.m_out = r1;
		pb.m_in  = q2;
		pb.m_val = p3;
		subdivide(pa, pb, _maxError, _limit - 1);
		
	} else {
		subdivide(_p0, _p1, _maxError, 1); // push p0,p1

	}
}

/******************************************************************************

                             ValueCurveEditor

******************************************************************************/
#ifdef ValueCurve_ENABLE_EDIT

#include <frm/Input.h>
#include <apt/log.h>
#include <imgui/imgui.h>

// PUBLIC

static const ImU32 kColorBorder          = ImColor(0x5f888888);
static const ImU32 kColorBackground      = ImColor(0x2f222222);
static const ImU32 kColorCurveBackground = ImColor(0x2f555555);
static const ImU32 kColorGridLine        = ImColor(0x2f666666);
static const ImU32 kColorGridLabel       = ImColor(0x5fffffff);
static const ImU32 kColorZeroAxis        = ImColor(0x5f4fe7ff);
static const ImU32 kColorValuePoint      = ImColor(0xffffffff);
static const ImU32 kColorControlPoint    = ImColor(0xffaaaaaa);
static const ImU32 kColorSampler         = ImColor(0xff00ff00);
static const float kAlphaCurveRepeat     = 0.3f;
static const float kSizeValuePoint       = 3.0f;
static const float kSizeControlPoint     = 2.0f;
static const float kSizeSelectPoint      = 6.0f;
static const float kSizeSampler          = 4.0f;

ValueCurveEditor::ValueCurveEditor()
{
	m_regionBeg = vec2(-0.1f);
	m_regionSize = vec2(2.0f);
	m_regionEnd = m_regionBeg + m_regionSize;
	m_selectedEndpoint = m_dragEndpoint = m_dragComponent = -1;
	m_gridSpacing = 64.0f;
	m_showSampler = true;
	m_showGrid = true;
	m_showZeroAxis = true;
	m_showRuler = true;
}

void ValueCurveEditor::draw(float _t)
{
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	ValueBezier& bezier = m_curve.m_bezier;
	bool reqUpdate = false;
	
	vec2 mousePos = vec2(io.MousePos);
	bool windowActive = ImGui::IsWindowFocused() && isInside(mousePos, m_windowBeg, m_windowEnd);
	
	bool regionBegChanged = false; // don't set from scrollbar when dragging the region view
	if (windowActive) {
		if (io.KeyCtrl) {
		 // zoom Y (value)
			float wy = ImGui::GetWindowContentRegionMax().y;
			float zoom = (io.MouseWheel * m_regionSize.y * 0.1f);
			float before = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionSize.y = APT_MAX(m_regionSize.y - zoom, 0.1f);
			float after = (io.MousePos.y - ImGui::GetWindowPos().y) / wy * m_regionSize.y;
			m_regionBeg.y += (before - after);
			regionBegChanged = fabs(before - after) > FLT_EPSILON;

		} else {
		 // zoom X (time)
			float wx = ImGui::GetWindowContentRegionMax().x;
			float zoom = (io.MouseWheel * m_regionSize.x * 0.1f);
			float before = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionSize.x = APT_MAX(m_regionSize.x - zoom, 0.1f);
			float after = (io.MousePos.x - ImGui::GetWindowPos().x) / wx * m_regionSize.x;
			m_regionBeg.x += (before - after);
			regionBegChanged = fabs(before - after) > FLT_EPSILON;

		}

	 // pan
		if (io.MouseDown[2]) {
			vec2 delta = vec2(io.MouseDelta) / vec2(ImGui::GetContentRegionMax()) * m_regionSize;
			delta.y = -delta.y;
			m_regionBeg -= delta;
			regionBegChanged = true;
		}
		m_regionEnd = m_regionBeg + m_regionSize;
	}

	m_windowBeg    = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowContentRegionMin());
	m_windowBeg.y += ImGui::GetItemsLineHeightWithSpacing();
	m_windowEnd    = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetContentRegionMax());
	m_windowEnd   -= vec2(ImGui::GetItemsLineHeightWithSpacing());
	m_windowSize   = m_windowEnd - m_windowBeg;
	if (m_windowSize.x < 1.0f || m_windowSize.y < 1.0f) {
		return;
	}
	ImGui::SetCursorPos(m_windowBeg - vec2(ImGui::GetWindowPos()));
	ImGui::InvisibleButton("##PreventDrag", m_windowSize);
	
	ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
	ImGui::AlignFirstTextHeightToWidgets();

if (ImGui::Button("Save")) {
	Json json;
	JsonSerializer jsonSerializer(&json, JsonSerializer::Mode_Write);
	bezier.serialize(jsonSerializer);
	Json::Write(json, "BezierTest.json");
}
ImGui::SameLine();
if (ImGui::Button("Load")) {
	Json json("BezierTest.json");
	JsonSerializer jsonSerializer(&json, JsonSerializer::Mode_Read);
	bezier.serialize(jsonSerializer);
}
ImGui::SameLine();

	float iconButtonSize = ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
	ImGui::Text(ICON_FA_QUESTION_CIRCLE);
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
			ImGui::Text("Mouse wheel:             Zoom H");
			ImGui::Text("Ctrl + mouse wheel:      Zoom V");
			ImGui::Text("Middle Mouse + drag:     Pan");
			ImGui::Text("Mouse left:              Select/move");
			ImGui::Text("Mouse left double click: Insert endpoint");
			ImGui::Text("DEL:                     Delete endpoint");
		ImGui::EndTooltip();
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_COG, ImVec2(iconButtonSize, 0.0f))) {
		ImGui::OpenPopup("ValueCurveEditorOptions");
	}

	if (ImGui::BeginPopup("ValueCurveEditorOptions")) {
		ImGui::Checkbox("Show Grid", &m_showGrid);
		ImGui::SameLine();
		ImGui::SliderFloat("Spacing", &m_gridSpacing, 8.0f, 64.0f);

		ImGui::Checkbox("Show Zero Axis", &m_showZeroAxis);
		ImGui::Checkbox("Show Ruler", &m_showRuler);
		ImGui::Checkbox("Show Sampler", &m_showSampler);

		ImGui::EndPopup();
	}
	if (!bezier.m_endpoints.empty()) {
		ImGui::SameLine();
		ImGui::Text("Fit");
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROWS_H, ImVec2(iconButtonSize, 0.0f))) {
			fit(0);
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROWS_V, ImVec2(iconButtonSize, 0.0f))) {
			fit(1);
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROWS, ImVec2(iconButtonSize, 0.0f))) {
			fit(0);
			fit(1);
		}
		
		//ImGui::SameLine();
		//ImGui::Text("Min (%.3f,%.3f)  Max(%.3f,%.3f)", bezier.m_min.x, bezier.m_min.y, bezier.m_max.x, bezier.m_max.y);
	}
	ImGui::SameLine();
	ImGui::PushItemWidth(ImGui::GetFontSize() * sizeof(" Repeat "));
	if (ImGui::Combo("Wrap", (int*)&bezier.m_wrap, "Clamp\0Repeat\0")) {
		m_curve.m_wrap = (ValueCurve::Wrap)bezier.m_wrap;
	}

	if (m_selectedEndpoint != -1) {
		ImGui::SameLine();
		vec2 val = bezier.m_endpoints[m_selectedEndpoint].m_val;
		bool moved = ImGui::DragFloat("t", &val.x, m_regionSize.x * 0.01f);
		ImGui::SameLine();
		moved |= ImGui::DragFloat("v", &val.y, m_regionSize.y * 0.01f);
		if (moved) {
			m_selectedEndpoint = bezier.move(m_selectedEndpoint, ValueBezier::Component_Val, val);
			reqUpdate = true;
		}
	}

	ImGui::PopItemWidth();

 // scroll bars
	/*bool refocusMainWindow = ImGui::IsWindowFocused();
	vec2 regionSizePx = vec2(0.0f);
	if (bezier.m_endpoints.size() > 1) {
		regionSizePx = (bezier.m_max - m_regionBeg) / m_regionSize * m_windowSize;
	} 
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImColor(IM_COL32_BLACK_TRANS));
	if (regionSizePx.x > m_windowSize.x) {
		ImGui::SetNextWindowContentSize(ImVec2(regionSizePx.x, 0.0f));
		ImGui::SetCursorPosX(m_windowBeg.x - ImGui::GetWindowPos().x);
		ImGui::SetCursorPosY(m_windowEnd.y - ImGui::GetWindowPos().y);
		ImGui::BeginChild("hscroll", ImVec2(m_windowSize.x, ImGui::GetStyle().ScrollbarSize), true, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			if (regionBegChanged) { 
				ImGui::SetScrollX(m_regionBeg.x / bezier.m_range.x * regionSizePx.x);
			} else {
				m_regionBeg.x = ImGui::GetScrollX() / regionSizePx.x * bezier.m_range.x;
			}
		ImGui::EndChild();
	}
	if (regionSizePx.y > m_windowSize.y) {
		ImGui::SetNextWindowContentSize(ImVec2(0.0f, regionSizePx.y));
		ImGui::SetCursorPosX(m_windowEnd.x);
		ImGui::SetCursorPosY(m_windowBeg.y);
		ImGui::BeginChild("vscroll", ImVec2(ImGui::GetStyle().ScrollbarSize, m_windowSize.y), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			if (regionBegChanged) {
				ImGui::SetScrollY(m_regionBeg.y / bezier.m_range.y * regionSizePx.y);
			} else {
				m_regionBeg.y = ImGui::GetScrollY() / regionSizePx.y * bezier.m_range.y;
			}
		ImGui::EndChild();
	}
	ImGui::PopStyleColor();
	if (refocusMainWindow) {
		ImGui::SetWindowFocus();
	}*/

	drawBackground();
	if (m_showGrid) {
		drawGrid();
	}
	if (m_showZeroAxis) {
		drawZeroAxis();
	}
	if (m_showSampler && !m_curve.m_endpoints.empty()) {
		drawSampler(_t);
	}

 // manipulate
 // \todo move selection logic inside the draw loop?
 // \todo clicking on a point automatically selects it for dragging - this makes the point 'move' on selection :/
	if (windowActive || m_dragEndpoint != -1) {
	 // point selection
		if (!bezier.m_endpoints.empty() && windowActive && io.MouseDown[0] && m_dragEndpoint == -1) {
		 // \todo cull at window Y boundary
			for (int i = 0, n = (int)bezier.m_endpoints.size(); i < n; ++i) {
				ValueBezier::Endpoint& ep = bezier.m_endpoints[i];
				bool done = false;
				for (int j = 0; j < 3; ++j) {
					vec2 pos = curveToWindow(ep[j]);
					if (pos.x > m_windowEnd.x) {
						done = true;
						break;
					}
					if (pos.x < m_windowBeg.x) {
						continue;
					}
					if (length(mousePos - pos) < kSizeSelectPoint) {
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
		if (m_dragEndpoint != -1) {
		 // left click + drag: move selected point
			if (io.MouseDown[0] && io.MouseDownDuration[0]) {
			 // point is being dragged
				vec2 newPos = windowToCurve(mousePos);				
				m_selectedEndpoint = m_dragEndpoint =  bezier.move(m_dragEndpoint, m_dragComponent, newPos);

				//if (m_dragComponent == ValueBezier::Component_Val) {
				//	ImGui::BeginTooltip();
				//		ImGui::Text("%1.3f, %1.3f", bezier.m_endpoints[m_selectedEndpoint].m_val.x, bezier.m_endpoints[m_selectedEndpoint].m_val.y);
				//	ImGui::EndTooltip();
				//}

			} else {
			 // mouse just released
				m_dragEndpoint = m_dragComponent = -1;
			}
			reqUpdate = true;

		} else if (io.MouseDoubleClicked[0]) {
		 // double click: insert a point
			int i = bezier.insert(windowToCurve(io.MousePos), m_regionSize.x * 0.05f);
			reqUpdate = true;

		} else if (io.MouseClicked[0]) {
		 // click off a point: deselect
			m_selectedEndpoint = m_dragEndpoint = m_dragComponent = -1;
		}

		if (m_selectedEndpoint != -1) {
			if (ImGui::IsKeyPressed(Keyboard::Key_Delete)) {
				bezier.erase(m_selectedEndpoint);
				m_selectedEndpoint = -1;
				reqUpdate = true;
			}			
		}
	
	}
	
 // draw
	ImU32 curveColor = IM_COL32_MAGENTA;
	ImGui::PushClipRect(m_windowBeg, m_windowEnd, false);
	if (!m_curve.m_endpoints.empty()) {
	 // \todo cull at window Y boundary

	 // repeat curve
		switch (m_curve.m_wrap) {
			case ValueCurve::Wrap_Clamp: {
				vec2 p = curveToWindow(m_curve.m_endpoints.front());
				drawList.AddLine(vec2(m_windowBeg.x, p.y), p, IM_COLOR_ALPHA(curveColor, kAlphaCurveRepeat), 1.0f);
				p = curveToWindow(m_curve.m_endpoints.back());
				drawList.AddLine(vec2(m_windowEnd.x, p.y), p, IM_COLOR_ALPHA(curveColor, kAlphaCurveRepeat), 1.0f);
				break;
			}
			case ValueCurve::Wrap_Repeat: {
				if (m_curve.m_endpoints.size() < 2) {
					break;
				}
				int i = m_curve.findSegmentStart(m_curve.wrap(m_regionBeg.x));
				vec2 p0 = curveToWindow(m_curve.m_endpoints[i]);
				float windowScale = m_windowSize.x / m_regionSize.x;
				float offset = p0.x - m_windowBeg.x;
				offset += (m_curve.wrap(m_regionBeg.x) - m_curve.m_endpoints[i].x) * windowScale;
				float offsetStep = m_curve.m_range.x * windowScale;
				p0.x -= offset;
				for (;;) {
					++i;
					if (p0.x > m_windowEnd.x) {
						break;
					}
					if (i >= (int)m_curve.m_endpoints.size()) {
						i = 0;
						offset -= offsetStep;
					}
					vec2 p1 = curveToWindow(m_curve.m_endpoints[i]);
					p1.x -= offset;
					drawList.AddLine(p0, p1, IM_COLOR_ALPHA(curveColor, kAlphaCurveRepeat), 1.0f);
					p0 = p1;
				}
				break;
			}
			default:
				break;
		};

	 // main curve
		vec2 p0 = curveToWindow(m_curve.m_endpoints[0]);
		for (int i = 1, n = (int)m_curve.m_endpoints.size(); i < n; ++i) {
			if (p0.x > m_windowEnd.x) {
				break;
			}
			vec2 p1 = curveToWindow(m_curve.m_endpoints[i]);
			if (p0.x < m_windowBeg.x && p1.x < m_windowBeg.x) {
				p0 = p1;
				continue;
			}
			drawList.AddLine(p0, p1, curveColor, 2.0f);
			p0 = p1;
		}
	}
	if (!bezier.m_endpoints.empty()) {
	 // \todo cull at window Y boundary
	 	for (int i = 0, n = (int)bezier.m_endpoints.size(); i < n; ++i) {
			ValueBezier::Endpoint& ep = bezier.m_endpoints[i];
			vec2 pos = curveToWindow(ep.m_val);
			if (pos.x > m_windowEnd.x) {
				break;
			}
			if (pos.x < m_windowBeg.x) {
				continue;
			}
			ImU32 col = i == m_selectedEndpoint ? kColorValuePoint : curveColor;
			drawList.AddCircleFilled(pos, kSizeValuePoint, col, 8);
		}

		for (int i = 0, n = (int)bezier.m_endpoints.size(); i < n; ++i) {
			ValueBezier::Endpoint& ep = bezier.m_endpoints[i];
			vec2 posIn = curveToWindow(ep.m_in);
			vec2 posOut = curveToWindow(ep.m_out);
			if (posIn.x > m_windowEnd.x && posOut.x > m_windowEnd.x) {
				break;
			}
			if (posIn.x < m_windowBeg.x && posOut.x < m_windowBeg.x) {
				continue;
			}
			ImU32 col = i == m_selectedEndpoint ? kColorControlPoint : IM_COLOR_ALPHA(curveColor, 0.5f);
			drawList.AddCircleFilled(posIn, kSizeControlPoint, col, 8);
			drawList.AddCircleFilled(posOut, kSizeControlPoint, col, 8);
			drawList.AddLine(posIn, posOut, col, 1.0f);

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

	if (m_showRuler) {
		drawRuler();
	}

	// border
	drawList.AddRect(m_windowBeg, m_windowEnd, kColorBorder, 0.0f, -1, 2.0f);

	ImGui::PopClipRect();

	if (reqUpdate) {
		m_curve.fromBezier(bezier);
	}
	
}

// PRIVATE

vec2 ValueCurveEditor::curveToRegion(const vec2& _pos)
{
	vec2 ret = (_pos - m_regionBeg) / m_regionSize;
	ret.y = 1.0f - ret.y;
	return ret;
}
vec2 ValueCurveEditor::curveToWindow(const vec2& _pos)
{
	vec2 ret = curveToRegion(_pos);
	return m_windowBeg + ret * m_windowSize;
}
vec2 ValueCurveEditor::regionToCurve(const vec2& _pos)
{
	vec2 pos = _pos;
	pos.y = 1.0f - _pos.y;
	return m_regionBeg + pos *  m_regionSize;
}
vec2 ValueCurveEditor::windowToCurve(const vec2& _pos)
{
	return regionToCurve((_pos - m_windowBeg) / m_windowSize);
}
bool ValueCurveEditor::isInside(const vec2& _point, const vec2& _min, const vec2& _max)
{
	return _point.x > _min.x && _point.x < _max.x && _point.y > _min.y && _point.y < _max.y;
}
void ValueCurveEditor::fit(int _dim)
{
	m_regionBeg[_dim] = m_curve.m_bezier.m_min[_dim] - 0.1f;
	m_regionSize[_dim] = (m_curve.m_bezier.m_max[_dim] - m_regionBeg[_dim]) + 0.1f;
}

void ValueCurveEditor::drawBackground()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	

 // main background
	drawList.AddRectFilled(m_windowBeg, m_windowEnd, kColorBackground);

 // curve region bacground
	if (m_curve.m_endpoints.size() > 1) {
		float curveMinX = curveToWindow(m_curve.m_min).x;
		float curveMaxX = curveToWindow(m_curve.m_max).x;
		drawList.AddRectFilled(vec2(curveMinX, m_windowBeg.y), vec2(curveMaxX, m_windowEnd.y), kColorCurveBackground);
	}
}

void ValueCurveEditor::drawGrid()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
 
 // vertical 
	float spacing = 0.01f;
	while ((spacing / m_regionSize.x * m_windowSize.x) < m_gridSpacing) {
		spacing *= 2.0f;
	}
	for (float i = floorf(m_regionBeg.x / spacing) * spacing; i < m_regionEnd.x; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(i, 0.0f)));
		if (line.x > m_windowBeg.x && line.x < m_windowEnd.x) {
			drawList.AddLine(vec2(line.x, m_windowBeg.y), vec2(line.x, m_windowEnd.y), kColorGridLine);
		}
	}
 // horizontal
	spacing = 0.01f;
	while ((spacing / m_regionSize.y * m_windowSize.y) < m_gridSpacing) {
		spacing *= 2.0f;
	}
	for (float i = floorf(m_regionBeg.y / spacing) * spacing; i < m_regionEnd.y; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(0.0f, i)));
		if (line.y > m_windowBeg.y && line.y < m_windowEnd.y) {
			drawList.AddLine(vec2(m_windowBeg.x, line.y), vec2(m_windowEnd.x, line.y), kColorGridLine);
		}
	}
}

void ValueCurveEditor::drawRuler()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
 
	const float kSpacing = 80.0f;
	String<sizeof("999.999")> label;

 // vertical 
	drawList.AddRectFilled(m_windowBeg, vec2(m_windowEnd.x, m_windowBeg.y + ImGui::GetFontSize() + 1.0f), kColorCurveBackground);
	float spacing = 0.01f;
	while ((spacing / m_regionSize.x * m_windowSize.x) < kSpacing) {
		spacing *= 2.0f;
	}
	for (float i = floorf(m_regionBeg.x / spacing) * spacing; i < m_regionEnd.x; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(i, 0.0f)));
		if (line.x > m_windowBeg.x && line.x < m_windowEnd.x) {
			label.setf((spacing < 1.0f) ? "%.2f" : (spacing < 10.0f) ?  "%1.1f" : "%1.0f", i);
			drawList.AddText(vec2(line.x + 2.0f, m_windowBeg.y + 1.0f), kColorGridLabel, label);
		}
	}
 // horizontal
 // \todo vertical text here
	drawList.AddRectFilled(m_windowBeg, vec2(m_windowBeg.x + ImGui::GetFontSize() + 1.0f, m_windowEnd.y), kColorCurveBackground);
	spacing = 0.01f;
	while ((spacing / m_regionSize.y * m_windowSize.y) < kSpacing) {
		spacing *= 2.0f;
	}
	for (float i = floorf(m_regionBeg.y / spacing) * spacing; i < m_regionEnd.y; i += spacing) {
		vec2 line = floor(curveToWindow(vec2(0.0f, i)));
		if (line.y > m_windowBeg.y && line.y < m_windowEnd.y) {
			label.setf((spacing < 1.0f) ? "%.2f" : (spacing < 10.0f) ?  "%1.1f" : "%1.0f", i);
			drawList.AddText(vec2(m_windowBeg.x + 2.0f, line.y), kColorGridLabel, label);
		}
	}
}

void ValueCurveEditor::drawZeroAxis()
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	
	vec2 zero = floor(curveToWindow(vec2(0.0f)));
	if (zero.x > m_windowBeg.x && zero.x < m_windowEnd.x) {
		drawList.AddLine(vec2(zero.x, m_windowBeg.y), vec2(zero.x, m_windowEnd.y), kColorZeroAxis);
	}
	if (zero.y > m_windowBeg.y && zero.y < m_windowEnd.y) {
		drawList.AddLine(vec2(m_windowBeg.x, zero.y), vec2(m_windowEnd.x, zero.y), kColorZeroAxis);
	}
}

void ValueCurveEditor::drawSampler(float _t)
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	float y = m_curve.sample(_t);
	vec2 p = curveToWindow(vec2(_t, y));
	p.x = floorf(p.x);
	drawList.AddLine(vec2(p.x, m_windowBeg.y), vec2(p.x, m_windowEnd.y), kColorSampler);
	drawList.AddCircleFilled(p, kSizeSampler, kColorSampler, 8);
	//String<sizeof("999.999")> label;
	//label.setf("%.3f", m_curve.wrap(_t));
	//drawList.AddText(vec2(p.x + 2.0f, m_windowBeg.y + ImGui::GetFontSize() + 2.0f), kColorSampler, label);
}

#endif // ValueCurve_ENABLE_EDIT