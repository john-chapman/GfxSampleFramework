#pragma once
#ifndef frm_Curve_h
#define frm_Curve_h

#include <frm/def.h>
#include <frm/math.h>

#include <EASTL/vector.h>
#include <imgui/imgui.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// Curve
// 2D curve. This is designed for edit/storage; runtime evaluation should use 
// a piecewise linear approximation which is generally cheaper to evaluate.
// 
// The curve representation is flat list of 'endpoints' (EP); each EP contains 
// 3 components: the 'value point' (VP) through which the curve will pass, plus 
// 2 'control points' (CP) which describe the in/out tangent of the curve at
// the VP.
//
// When sampling, CPs are constrained to lie within their containing segment. This 
// is necessary to ensure a 1:1 maping between the curve input and output (loops
// are prohibited).
//
// \todo Allow unlocked CPs (e.g. to create cusps).
////////////////////////////////////////////////////////////////////////////////
class Curve
{
	friend class CurveEditor;
public:
	static const int kInvalidIndex = -1;

	enum Wrap
	{
		Wrap_Clamp,
		Wrap_Repeat,

		Wrap_Count
	};

	enum Component
	{
		Component_In,
		Component_Value,
		Component_Out,

		Component_Count
	};

	struct Endpoint
	{
		vec2 m_in;
		vec2 m_value;
		vec2 m_out;

		vec2& operator[](Component _component) { return (&m_in)[_component]; }
		vec2& operator[](int _component)       { return (&m_in)[_component]; }
	};

	Curve();
	~Curve() {}

	// Insert a new endpoint with the given value, return its index.
	int insert(const Endpoint& _endpoint);

	// Move the specified component on endpoint by setting its value, return the new index.
	int move(int _endpointIndex, Component _component, const vec2& _value);

	// Erase the specified endpoint.
	void  erase(int _endpointIndex);

	// Apply the wrap mode to _t.
	float wrap(float _t) const;

	Endpoint&       operator[](int _i)       { APT_ASSERT(_i < (int)m_endpoints.size()); return m_endpoints[_i]; }
	const Endpoint& operator[](int _i) const { APT_ASSERT(_i < (int)m_endpoints.size()); return m_endpoints[_i]; }

	void setValueConstraint(const vec2& _min, const vec2& _max);

	//bool edit(const vec2& _sizePixels = vec2(-1.0f), float _t = 0.0f, EditFlags _flags = EditFlags_Default);

private:
	eastl::vector<Endpoint> m_endpoints;
	vec2 m_endpointMin, m_endpointMax;     // endpoint bounding box, including CPs
	vec2 m_valueMin, m_valueMax;           // endpoint bounding box, excluding CPs
	Wrap m_wrap;
	vec2 m_constrainMin, m_constrainMax;   // limit endpoint values

	int  findSegmentStartIndex(float _t) const;
	void updateExtentsAndConstrain(int _modified); // applies additional constraints, e.g. synchronize endpoints if Wrap_Repeat
	void copyValueAndTangent(const Endpoint& _src, Endpoint& dst_);
	void constrainCp(vec2& _cp_, const vec2& _vp, float _x0, float _x1); // move _cp_ towards _vp such that _x0 <= _cp_.x <= _x1

}; // class Curve

////////////////////////////////////////////////////////////////////////////////
// CurveEditor
// Simultaneously edit one or more curves.
////////////////////////////////////////////////////////////////////////////////
class CurveEditor
{
public:
	enum Flags
	{
		Flags_None          = 0,
		Flags_ShowGrid      = 1 << 0,  // Show the background grid.
		Flags_ShowRuler     = 1 << 1,  // Show the edge ruler.
		Flags_ShowHighlight = 1 << 2,  // Show curve bounding box highlight.
		Flags_NoPan         = 1 << 3,  // Disable pan (middle click).
		Flags_NoZoom        = 1 << 4,  // Disable zoom (mouse wheel).

		Flags_Default = Flags_ShowGrid | Flags_ShowRuler | Flags_ShowHighlight
	};

	CurveEditor();

	void addCurve(Curve* _curve_, const ImColor& _color);
	void selectCurve(const Curve* _curve_);

	bool drawEdit(const vec2& _sizePixels, float _t, Flags _flags);

private:

 // editor
	vec2      m_windowBeg, m_windowEnd, m_windowSize;
	vec2      m_regionBeg, m_regionEnd, m_regionSize;
	int       m_selectedEndpoint;
	int       m_dragEndpoint;
	bool      m_showAllCurves;
	int       m_dragComponent;
	vec2      m_dragOffset;
	bvec2     m_dragRuler;
	bool      m_editEndpoint;
	bool      m_isDragging;
	uint32    m_editFlags;

	bool checkEditFlag(Flags _flag) { return (m_editFlags & _flag) != 0; }
	bool isInside(const vec2& _point, const vec2& _min, const vec2& _max);
	bool isInside(const vec2& _point, const vec2& _origin, float _radius);
	vec2 curveToRegion(const vec2& _pos);
	vec2 curveToWindow(const vec2& _pos);
	vec2 regionToCurve(const vec2& _pos);
	vec2 windowToCurve(const vec2& _pos);
	void fit(int _dim);
	bool editCurve();
	void drawBackground();
	void drawGrid();
	void drawCurve(int _curveIndex);
	void drawSampler(float _t);
	void drawRuler();

 // curves, cache
	typedef eastl::vector<vec2> DrawCache;
	eastl::vector<DrawCache>  m_drawCaches;  // piecewise linear approximations for drawing
	eastl::vector<Curve*>     m_curves;
	eastl::vector<ImU32>      m_curveColors;
	int                       m_selectedCurve;

	void updateCache(int _curveIndex);
	void subdivide(int _curveIndex, const Curve::Endpoint& _p0, const Curve::Endpoint& _p1, float _maxError = 0.001f, int _limit = 64);

}; // class CurveEditor

} // namespace frm

#endif // frm_Curve_h
