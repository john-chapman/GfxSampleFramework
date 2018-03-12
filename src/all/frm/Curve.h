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
// 2D Bezier curve (for edit/serialize) plus a piecewise linear approximation 
// (for fast runtime evaluation).
// 
// The Bezier representation is flat list of 'endpoints' (EP); each EP contains 
// 3 components: the 'value point' (VP) through which the curve will pass, plus 
// 2 'control points' (CP) which describe the in/out tangent of the curve at
// the VP.
//
// When sampling, Bezier CPs are constrained to lie within their containing 
// segment. This is necessary to ensure a 1:1 maping between the curve input 
// and output (loops are prohibited).
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

	friend bool Serialize(apt::Serializer& _serializer_, Curve& _curve_);

 // Bezier
	// Insert a new endpoint with the given value, return its index.
	int   insert(const Endpoint& _endpoint);
	int   insert(float _valueX, float _valueY);
	// Move the specified component on endpoint by setting its value, return the new index.
	int   move(int _endpointIndex, Component _component, const vec2& _value);
	int   moveX(int _endpointIndex, Component _component, float _value);
	void  moveY(int _endpointIndex, Component _component, float _value);
	// Erase the specified endpoint.
	void  erase(int _endpointIndex);
	// Apply the wrap mode to _t.
	float wrap(float _t) const;
	// Constraint endpoint values in [_min, _max].
	void  setValueConstraint(const vec2& _min, const vec2& _max);
	
	// Bezier endpoint access.
	int   getBezierEndpointCount() const              { return (int)m_bezier.size(); }
	const Endpoint& getBezierEndpoint(int _i) const   { return m_bezier[_i]; }

 // Piecewise
	// Evaluate the piecewise representation at _t (which is implicitly wrapped).
	float evaluate(float _t) const;

	// Max error controls the number of segments in the piecewise approximation.
	void  setMaxError(float _maxError)                { m_maxError = _maxError; updatePiecewise(); }
	float getMaxError() const                         { return m_maxError; }

	// Piecewise endpoint access.
	int   getPiecewiseEndpointCount() const           { return (int)m_piecewise.size(); }
	const vec2& getPiecewiseEndpoint(int _i) const    { return m_piecewise[_i]; }

private:
	vec2  m_endpointMin, m_endpointMax;     // endpoint bounding box, including CPs
	vec2  m_valueMin, m_valueMax;           // endpoint bounding box, excluding CPs
	Wrap  m_wrap;
	vec2  m_constrainMin, m_constrainMax;   // limit endpoint values
	float m_maxError;

	eastl::vector<Endpoint> m_bezier;       // for edit/serializer
	eastl::vector<vec2>     m_piecewise;    // for runtime evaluation

	int  findInsertIndex(float _t);
	int  findBezierSegmentStartIndex(float _t) const;
	int  findPiecewiseSegmentStartIndex(float _t) const;
	void updateExtentsAndConstrain(int _modified); // applies additional constraints, e.g. synchronize endpoints if Wrap_Repeat
	void copyValueAndTangent(const Endpoint& _src, Endpoint& dst_);
	void constrainCp(vec2& _cp_, const vec2& _vp, float _x0, float _x1); // move _cp_ towards _vp such that _x0 <= _cp_.x <= _x1

	// Update the piecewise approximation.
	void updatePiecewise();
	void subdivide(const Endpoint& _p0, const Endpoint& _p1, int _limit = 64);

}; // class Curve

////////////////////////////////////////////////////////////////////////////////
// CurveEditor
// Simultaneously edit one or more curves.
// \todo Multiple EP select.
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
		Flags_ShowSampler   = 1 << 3,  // Show sample point at _t parameter to drawEdit.
		Flags_NoPan         = 1 << 4,  // Disable pan (middle click).
		Flags_NoZoom        = 1 << 5,  // Disable zoom (mouse wheel).

		Flags_Default = Flags_ShowGrid | Flags_ShowRuler | Flags_ShowHighlight | Flags_ShowSampler
	};

	CurveEditor();

	void addCurve(Curve* _curve_, const ImColor& _color);
	void selectCurve(const Curve* _curve_);
	void reset();
	bool drawEdit(const vec2& _sizePixels, float _t, int _flags);

private:

 // editor
	vec2      m_windowBeg, m_windowEnd, m_windowSize;
	vec2      m_regionBeg, m_regionEnd, m_regionSize;
	int       m_selectedEndpoint;
	int       m_dragEndpoint;
	int       m_dragComponent;
	bool      m_showAllCurves;
	vec2      m_dragOffset;
	bvec2     m_dragRuler;
	bool      m_editEndpoint;
	bool      m_isDragging;
	uint32    m_editFlags;

	bool checkEditFlag(Flags _flag) const { return (m_editFlags & _flag) != 0; }
	bool isInside(const vec2& _point, const vec2& _min, const vec2& _max) const;
	bool isInside(const vec2& _point, const vec2& _origin, float _radius) const;
	vec2 curveToRegion(const vec2& _pos) const;
	vec2 curveToWindow(const vec2& _pos) const;
	vec2 regionToCurve(const vec2& _pos) const;
	vec2 windowToCurve(const vec2& _pos) const;
	void fit(int _dim);
	bool editCurve();
	void drawBackground();
	void drawGrid();
	void drawCurve(int _curveIndex);
	void drawSampler(float _t);
	void drawRuler();

 // curves
	eastl::vector<Curve*> m_curves;
	eastl::vector<ImU32>  m_curveColors;
	int                   m_selectedCurve;

}; // class CurveEditor

} // namespace frm

#endif // frm_Curve_h
