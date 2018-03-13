#include <frm/Curve.h>

#include <frm/icon_fa.h>
#include <frm/interpolation.h>
#include <frm/Input.h>

#include <apt/Serializer.h>
#include <apt/String.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

#define Curve_DEBUG 0

/*******************************************************************************

                                    Curve

*******************************************************************************/

static const char* WrapStr[Curve::Wrap_Count] =
{
	"Clamp",  //Wrap_Clamp,
	"Repeat"  //Wrap_Repeat
};

// PUBLIC

Curve::Curve()
	: m_constrainMin(-FLT_MAX)
	, m_constrainMax(FLT_MAX)
	, m_wrap(Wrap_Clamp)
	, m_maxError(1e-3f)
{
}

bool frm::Serialize(apt::Serializer& _serializer_, Curve& _curve_)
{
	bool ret = true;

 // metadata
	String<32> tmp = WrapStr[_curve_.m_wrap];
	ret &= Serialize(_serializer_, tmp, "Wrap");
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		for (int i = 0; i < Curve::Wrap_Count; ++i) {
			if (tmp == WrapStr[i]) {
				_curve_.m_wrap = (Curve::Wrap)i;
				break;
			}
		}
	}
	ret &= Serialize(_serializer_, _curve_.m_constrainMin, "ConstrainMin");
	ret &= Serialize(_serializer_, _curve_.m_constrainMax, "ConstrainMax");


 // endpoints
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		_curve_.m_bezier.clear();
	}
	uint endpointCount = _curve_.m_bezier.size();
	if (_serializer_.beginArray(endpointCount, "Endpoints")) {
		if (_serializer_.getMode() == Serializer::Mode_Read) {
			_curve_.m_bezier.resize(endpointCount);
		}
		for (uint i = 0; i < endpointCount; ++i) {
			Curve::Endpoint& ep = _curve_.m_bezier[i];
			_serializer_.beginArray();			
			ret &= Serialize(_serializer_, ep.m_in);
			ret &= Serialize(_serializer_, ep.m_value);
			ret &= Serialize(_serializer_, ep.m_out);
			_serializer_.endArray();
		}
		_serializer_.endArray();
	} else {
		ret = false;
	}

	if (_serializer_.getMode() == Serializer::Mode_Read) {
		_curve_.updateExtentsAndConstrain(-1);
		_curve_.updatePiecewise();
	}

	return ret;
}

int Curve::insert(const Endpoint& _endpoint)
{
	int ret = Curve::findInsertIndex(_endpoint.m_value.x);
	m_bezier.insert(m_bezier.begin() + ret, _endpoint);
	updateExtentsAndConstrain(ret);
	updatePiecewise();
	return ret;
}

int Curve::insert(float _valueX, float _valueY)
{
	int ret = findInsertIndex(_valueX);
	Endpoint ep;
	ep.m_value = vec2(_valueX, _valueY);

 // tangent estimation
	if (ret > 0 && ret < (int)m_bezier.size()) {
		vec2 prev = m_bezier[ret - 1].m_value;
		vec2 next = m_bezier[ret    ].m_value;
		
		#if 0
		// use the tangent at _valueX
			float xd = (next.x - prev.x) * 0.001f;
			ep.m_in.x  = _valueX - xd;
			ep.m_out.x = _valueX + xd;
			ep.m_in.y  = evaluate(ep.m_in.x);
			ep.m_out.y = evaluate(ep.m_out.x);
		
			vec2 tangent = normalize(ep.m_out - ep.m_in) * length(next - prev) * 0.1f;
			ep.m_in  = ep.m_value - tangent;
			ep.m_out = ep.m_value + tangent;
		#else
		// horizontal with the CPs at 50% along the segment
			float xd   = Min(_valueX - prev.x, next.x - _valueX) * 0.5f;
			ep.m_in.x  = _valueX - xd;
			ep.m_out.x = _valueX + xd;
			ep.m_in.y = ep.m_out.y = _valueY;
		#endif

	} else {
		ep.m_in  = vec2(_valueX - 0.05f, _valueY);
		ep.m_out = vec2(_valueX + 0.05f, _valueY);
	}

	m_bezier.insert(m_bezier.begin() + ret, ep);
	updateExtentsAndConstrain(ret);
	updatePiecewise();
	return ret;
}

int Curve::move(int _endpoint, Component _component, const vec2& _value)
{
	Endpoint& ep = m_bezier[_endpoint];

	int ret = _endpoint;
	if (_component == Component_Value) {
	 // move CPs
		vec2 delta = _value - ep[Component_Value];
		ep[Component_In] += delta;
		ep[Component_Out] += delta;

	 // swap with neighbor
		ep.m_value = _value;
		if (delta.x > 0.0f && _endpoint < (int)m_bezier.size() - 1) {
			int i = _endpoint + 1;
			if (_value.x > m_bezier[i].m_value.x) {
				eastl::swap(ep, m_bezier[i]);
				ret = i;
			}
		} else if (_endpoint > 0) {
			int i = _endpoint - 1;
			if (_value.x < m_bezier[i].m_value.x) {
				eastl::swap(ep, m_bezier[i]);
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

	updateExtentsAndConstrain(ret);
	updatePiecewise();

	return ret;
}

int Curve::moveX(int _endpointIndex, Component _component, float _value)
{
	return move(_endpointIndex, _component, vec2(_value, m_bezier[_endpointIndex][_component].y));
}
void Curve::moveY(int _endpointIndex, Component _component, float _value)
{
	move(_endpointIndex, _component, vec2(m_bezier[_endpointIndex][_component].x, _value));
}

void Curve::erase(int _endpoint)
{
	APT_ASSERT(_endpoint < (int)m_bezier.size());
	m_bezier.erase(m_bezier.begin() + _endpoint);
	updateExtentsAndConstrain(APT_MIN(_endpoint, APT_MAX((int)m_bezier.size() - 1, 0)));
	updatePiecewise();
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

float Curve::evaluate(float _t) const
{
	if (m_piecewise.empty()) {
		return 0.0f;
	}
	if (m_piecewise.size() < 2) {
		return m_piecewise.front().y;
	}
	_t = wrap(_t);
	int i = findPiecewiseSegmentStartIndex(_t);
	float range = m_piecewise[i + 1].x - m_piecewise[i].x;
	_t = (_t - m_piecewise[i].x) / (range > 0.0f ? range : 1.0f);;
	return lerp(m_piecewise[i].y, m_piecewise[i + 1].y, _t);
}

// PRIVATE

int Curve::findInsertIndex(float _t)
{
	int ret = (int)m_bezier.size();
	if (!m_bezier.empty() && _t < m_bezier[ret - 1].m_value.x) {
	 // can't insert at end, do binary search
		ret = findBezierSegmentStartIndex(_t);
		ret += (_t >= m_bezier[ret].m_value.x) ? 1 : 0; // handle case where _pos.x should be inserted at 0, normally we +1 to ret
	}
	return ret;
}

int Curve::findBezierSegmentStartIndex(float _t) const
{
	int lo = 0, hi = (int)m_bezier.size() - 1;
	while (hi - lo > 1) {
		uint32 md = (hi + lo) / 2;
		if (_t > m_bezier[md].m_value.x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_bezier[hi].m_value.x ? hi : lo;
}

int Curve::findPiecewiseSegmentStartIndex(float _t) const
{
	int lo = 0, hi = (int)m_piecewise.size() - 1;
	while (hi - lo > 1) {
		uint32 md = (hi + lo) / 2;
		if (_t > m_piecewise[md].x) {
			lo = md;
		} else {
			hi = md;
		}
	}
	return _t > m_piecewise[hi].x ? hi : lo;
}

void Curve::updateExtentsAndConstrain(int _endpoint)
{
	m_valueMin = m_endpointMin = vec2(FLT_MAX);
	m_valueMax = m_endpointMax = vec2(-FLT_MAX);
	for (auto& ep : m_bezier) {
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
		if (_endpoint == (int)m_bezier.size() - 1) {
			copyValueAndTangent(m_bezier.back(), m_bezier.front());
		} else if (_endpoint == 0) {
			copyValueAndTangent(m_bezier.front(), m_bezier.back());
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

void Curve::updatePiecewise()
{
	m_piecewise.clear();
	if (m_bezier.empty()) {
		return;
	}
	if (m_bezier.size() == 1) {
		m_piecewise.push_back(m_bezier[0].m_value);
		return;
	}

	auto p0 = m_bezier.begin();
	auto p1 = m_bezier.begin() + 1;
	for (; p1 != m_bezier.end(); ++p0, ++p1) {
		subdivide(*p0, *p1);
	}
}
void Curve::subdivide(const Endpoint& _p0, const Endpoint& _p1, int _limit)
{
	if (_limit == 1) {
		m_piecewise.push_back(_p0.m_value);
		m_piecewise.push_back(_p1.m_value);
		return;
	}
	
	vec2 p0 = _p0.m_value;
	vec2 p1 = _p0.m_out;
	vec2 p2 = _p1.m_in;
	vec2 p3 = _p1.m_value;

 // constrain control point on segment (prevent loops)
	constrainCp(p1, p0, p0.x, p3.x);
	constrainCp(p2, p3, p0.x, p3.x);

 // http://antigrain.com/research/adaptive_bezier/ suggests a better error metric: use the height of CPs above the line p1.m_val - p0.m_val
	vec2 q0 = lerp(p0, p1, 0.5f);
	vec2 q1 = lerp(p1, p2, 0.5f);
	vec2 q2 = lerp(p2, p3, 0.5f);
	vec2 r0 = lerp(q0, q1, 0.5f);
	vec2 r1 = lerp(q1, q2, 0.5f);
	vec2 s  = lerp(r0, r1, 0.5f);
	float err = length(p1 - r0) + length(q1 - s) + length(p2 - r1);
	if (err > m_maxError) {
		Curve::Endpoint pa, pb;
		pa.m_value = p0;
		pa.m_out   = q0;
		pb.m_in    = r0;
		pb.m_value = s;
		subdivide(pa, pb, _limit - 1);

		pa.m_value = s;
		pa.m_out   = r1;
		pb.m_in    = q2;
		pb.m_value = p3;
		subdivide(pa, pb, _limit - 1);
		
	} else {
		subdivide(_p0, _p1, 1); // push p0,p1

	}
}

/*******************************************************************************

                               CurveGradient

*******************************************************************************/
CurveGradient::CurveGradient()
{
	for (auto& curve : m_curves) {
		curve.setWrap(Curve::Wrap_Clamp);
		curve.setMaxError(1e-3f); // larger error = use a smaller number of piecewise segments
		curve.insert(0.0f, 1.0f);
	}
}

vec4 CurveGradient::evaluate(float _t) const
{
	return vec4(
		m_curves[0].evaluate(_t),
		m_curves[1].evaluate(_t),
		m_curves[2].evaluate(_t),
		m_curves[3].evaluate(_t)
		);
}

bool frm::Serialize(apt::Serializer& _serializer_, CurveGradient& _curveGradient_)
{
	const char* kCurveNames[] = { "Red", "Green", "Blue", "Alpha" };
	bool ret = true;
	for (int i = 0; i < 4; ++i) {
		if (_serializer_.beginObject(kCurveNames[i])) {
			ret &= Serialize(_serializer_, _curveGradient_.m_curves[i]);
			_serializer_.endObject();
		}
	}
	return ret;
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
static const ImU32 kColorSampler         = ImColor(0x9900ffff);
static const float kAlphaCurveWrap       = 0.3f;
static const float kSizeValuePoint       = 3.0f;
static const float kSizeControlPoint     = 2.0f;
static const float kSizeSelectPoint      = 6.0f;
static const float kSizeRuler            = 17.0f;
static const float kSizeSampler          = 3.0f;

// PUBLIC

CurveEditor::CurveEditor()
{
	reset();
}

void CurveEditor::addCurve(Curve* _curve_, const ImColor& _color)
{
	int curveIndex = (int)m_curves.size();
	m_curves.push_back(_curve_);
	m_curveColors.push_back((ImU32)_color);
	if (m_selectedCurve == -1) {
		m_selectedCurve = curveIndex;
		if (!m_curves[m_selectedCurve]->m_bezier.empty()) {
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

void CurveEditor::reset()
{
	m_regionBeg        = vec2(0.0f);
	m_regionEnd        = vec2(1.0f);
	m_regionSize       = vec2(1.0f);
	m_selectedEndpoint = Curve::kInvalidIndex;
	m_dragEndpoint     = Curve::kInvalidIndex;
	m_dragComponent    = -1;
	m_dragOffset       = vec2(0.0f);
	m_dragRuler        = bvec2(false);
	m_editEndpoint     = false;
	m_showAllCurves    = true;
	m_isDragging       = false;
	m_editFlags        = Flags_Default;
	m_selectedCurve    = -1;
}

bool CurveEditor::drawEdit(const vec2& _sizePixels, float _t, int _flags)
{
	bool ret = false;
	m_editFlags = _flags;

	ImGuiIO& io = ImGui::GetIO();
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	
 // set the 'window' size to either fill the available space or use the specified size
	vec2 scroll = vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_windowBeg = (vec2)ImGui::GetCursorPos() - scroll + (vec2)ImGui::GetWindowPos();
	m_windowEnd = (vec2)ImGui::GetContentRegionMax() - scroll + (vec2)ImGui::GetWindowPos();
	if (_sizePixels.x >= 0.0f) {
		m_windowEnd.x = m_windowBeg.x + _sizePixels.x;
	}
	if (_sizePixels.y >= 0.0f) {
		m_windowEnd.y = m_windowBeg.y + _sizePixels.y;
	}
	m_windowBeg  = Floor(m_windowBeg);
	m_windowEnd  = Floor(m_windowEnd);
	m_windowSize = Max(m_windowEnd - m_windowBeg, vec2(64.0f));
	m_windowEnd  = m_windowBeg + m_windowSize;
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
	 // prevent mouse wheel scrolling on zoom
		ImGui::SetScrollX(scroll.x);
		ImGui::SetScrollY(scroll.y);

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

	m_isDragging |= m_dragEndpoint != Curve::kInvalidIndex;
	if (m_isDragging || (windowActive && mouseInWindow)) {
		ret |= editCurve();
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
			if (checkEditFlag(Flags_ShowSampler)) {
				drawSampler(_t);
			}
		}

		if (checkEditFlag(Flags_ShowRuler)) {
			drawRuler();
		}
	ImGui::PopClipRect();

	if (!m_editEndpoint && mouseInWindow && windowActive && io.MouseClicked[1]) {
		ImGui::OpenPopup("CurveEditorPopup");
	}
	if (ImGui::BeginPopup("CurveEditorPopup")) {
		if (ImGui::MenuItem("Fit")) {
			fit(0);
			fit(1);
		}
		if (m_curves.size() > 1) {
			if (ImGui::MenuItem("Show All", "", m_showAllCurves)) {
				m_showAllCurves = !m_showAllCurves;
			}
		}

		if (m_selectedCurve != -1) {
			ImGui::Separator();

			Curve& curve = *m_curves[m_selectedCurve];
			if (ImGui::BeginMenu("Wrap")) {
				Curve::Wrap newWrapMode = curve.m_wrap;
				for (int i = 0; i < Curve::Wrap_Count; ++i) {
					if (ImGui::MenuItem(WrapStr[i], nullptr, newWrapMode == (Curve::Wrap)i)) {
						newWrapMode = (Curve::Wrap)i;
					}
				}
				if (newWrapMode != curve.m_wrap) {
					if (newWrapMode == Curve::Wrap_Repeat) {
					 // constrain curve to remain continuous 
						float delta = curve.m_bezier.front().m_value.y - curve.m_bezier.back().m_value.y;
						curve.m_bezier.back().m_in.y += delta;
						curve.m_bezier.back().m_value.y += delta;
						curve.m_bezier.back().m_out.y += delta;
						curve.updateExtentsAndConstrain((int)curve.m_bezier.size() - 1);
						curve.updatePiecewise();
					}
					curve.m_wrap = newWrapMode;
					ret = true;
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Max Error")) {
				if (ImGui::DragFloat("##Max Error Drag", &curve.m_maxError, 1e-4f, 1e-6f, 1.0f, "%.4f")) {
					curve.m_maxError = APT_CLAMP(curve.m_maxError, 1e-6f, 1.0f);
					curve.updatePiecewise();
				}
				ImGui::EndMenu();
			}
		}		

		ImGui::EndPopup();
	}

	return ret;
}

// PRIVATE

bool CurveEditor::isInside(const vec2& _point, const vec2& _min, const vec2& _max) const
{
	return _point.x > _min.x && _point.x < _max.x && _point.y > _min.y && _point.y < _max.y;
}

bool CurveEditor::isInside(const vec2& _point, const vec2& _origin, float _radius) const
{
	return distance2(_point, _origin) < (_radius * _radius);
}

vec2 CurveEditor::curveToRegion(const vec2& _pos) const 
{
	vec2 ret = (_pos - m_regionBeg) / m_regionSize;
	ret.y = 1.0f - ret.y;
	return ret;
}
vec2 CurveEditor::curveToWindow(const vec2& _pos) const
{
	vec2 ret = curveToRegion(_pos);
	return m_windowBeg + ret * m_windowSize;
}

vec2 CurveEditor::regionToCurve(const vec2& _pos) const
{
	vec2 pos = _pos;
	pos.y = 1.0f - _pos.y;
	return m_regionBeg + pos *  m_regionSize;
}

vec2 CurveEditor::windowToCurve(const vec2& _pos) const
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
	if (!curve.m_bezier.empty() && !m_editEndpoint && (io.MouseDown[0] || io.MouseDown[1]) && m_dragEndpoint == Curve::kInvalidIndex) {
		for (int i = 0, n = (int)curve.m_bezier.size(); i < n; ++i) {
			Curve::Endpoint& ep = curve.m_bezier[i];
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
				if (isInside(mousePos, p, kSizeSelectPoint) && !ImGui::IsMouseDragging()) {
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
						ImGui::Text("X %1.3f, Y %1.3f", curve.m_bezier[m_selectedEndpoint].m_value.x, curve.m_bezier[m_selectedEndpoint].m_value.y);
					ImGui::EndTooltip();
				}
			} else {
			 // dragging endpoint, constrain to X/Y axis if ctrl pressed
				if (io.KeyCtrl) {
					vec2 delta = normalize(mousePos - curveToWindow(curve.m_bezier[m_selectedEndpoint].m_value));
					if (abs(delta.y) > 0.5f) {
						newPos.x = curve.m_bezier[m_selectedEndpoint].m_value.x;
					} else {
						newPos.y = curve.m_bezier[m_selectedEndpoint].m_value.y;
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
		vec2 value = windowToCurve(io.MousePos);
		m_selectedEndpoint = curve.insert(value.x, value.y);
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
			ImGui::PushID(&curve.m_bezier[m_selectedEndpoint]);
				if (!m_editEndpoint && io.MouseClicked[1] && isInside(mousePos, curveToWindow(curve.m_bezier[m_selectedEndpoint].m_value), kSizeSelectPoint)) {
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
						vec2 p = curve.m_bezier[m_selectedEndpoint].m_value;
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
}

void CurveEditor::drawCurve(int _curveIndex)
{
	Curve& curve = *m_curves[_curveIndex];
	eastl::vector<vec2>& cache = curve.m_piecewise;
	ImU32 curveColor =  IM_COLOR_ALPHA(m_curveColors[_curveIndex], _curveIndex == m_selectedCurve ? 1.0f : kAlphaCurveWrap);
	bool isSelected = _curveIndex == m_selectedCurve;

	if (cache.empty()) {
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRect(floor(curveToWindow(curve.m_constrainMin)), floor(curveToWindow(curve.m_constrainMax)), kColorZeroAxis);

 // curve region highlight
	if (isSelected && checkEditFlag(Flags_ShowHighlight)) {
		if (curve.m_bezier.size() > 1) {
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
			int i = curve.findBezierSegmentStartIndex(curve.wrap(m_regionBeg.x));
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
	for (int i = 0, n = (int)curve.m_bezier.size(); i < n; ++i) {
		Curve::Endpoint& ep = curve.m_bezier[i];
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

	for (int i = 0, n = (int)curve.m_bezier.size(); i < n; ++i) {
		Curve::Endpoint& ep = curve.m_bezier[i];
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
				curve.constrainCp(pin, ep.m_value, curve.m_bezier[i - 1].m_value.x, ep.m_value.x);
				pin = curveToWindow(pin);
				drawList->AddCircleFilled(pin, kSizeControlPoint, IM_COL32_YELLOW, 8);
			}
			if (i < n - 1) {
				pout = ep.m_out;
				curve.constrainCp(pout, ep.m_value, ep.m_value.x, curve.m_bezier[i + 1].m_value.x);
				pout = curveToWindow(pout);
				drawList->AddCircleFilled(pout, kSizeControlPoint, IM_COL32_CYAN, 8);
			}
		#endif
	}
}

void CurveEditor::drawSampler(float _t)
{
	ImDrawList& drawList = *ImGui::GetWindowDrawList();	
	Curve& curve = *m_curves[m_selectedCurve];
	
	float x = floor(curveToWindow(vec2(_t, 0.0f)).x);
	if (x > m_windowBeg.x && x < m_windowEnd.x) {
		drawList.AddLine(vec2(x, m_windowBeg.y), vec2(x, m_windowEnd.y), kColorSampler);
		if (!curve.m_piecewise.empty()) {
			float y = floor(curveToWindow(vec2(0.0f, curve.evaluate(_t))).y);
			drawList.AddRect(vec2(x, y) - vec2(2.0f), vec2(x, y) + vec2(3.0f), kColorSampler);
			#if Curve_DEBUG
				String<sizeof("999")> label;
				label.setf("%d", curve.findPiecewiseSegmentStartIndex(_t));
				drawList.AddText(vec2(x, y) + vec2(3.0f, -3.0f), kColorRulerLabel, (const char*)label);
			#endif
		}
	}

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
