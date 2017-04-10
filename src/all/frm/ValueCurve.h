#pragma once
#ifndef frm_ValueCurve_h
#define frm_ValueCurve_h

#define ValueCurve_ENABLE_EDIT

#include <frm/def.h>
#include <frm/math.h>

#include <EASTL/vector.h>

namespace frm {

///////////////////////////////////////////////////////////////////////////////
// ValueBezier
// 2D Bezier curve. This is exclusively for edit/storage; the runtime curve 
// (ValueCurve) is a piecewise linear approximation which is cheaper to evaluate.
// ValueBezier needs to be robust, but not necessarily fast.
// 
// The Bezier representation is flat list of 'endpoints' (EP); each EP contains 
// 3 components: the 'value point' (VP) through which the curve will pass, plus 
// 2 'control points' (CP) which describe the in/out tangent of the curve at
// the VP.
//
// When sampling, CPs are constrained to lie within their containing segment. This 
// is necessary; we don't want a spatial curve, the X dimension must represent time.
//
// \todo Better tangent estimation on insert().
// \todo Allow decoupled CPs (to create cusps).
///////////////////////////////////////////////////////////////////////////////
class ValueBezier
{
	friend class ValueCurve;
	friend class ValueCurveEditor;

	enum Wrap
	{
		Wrap_Clamp,
		Wrap_Repeat,

		Wrap_Count
	};

	enum Component
	{
		Component_In,
		Component_Val,
		Component_Out,

		Component_Count
	};

	Wrap m_wrap;

	struct Endpoint
	{
		vec2 m_in;
		vec2 m_val;
		vec2 m_out;

		vec2& operator[](int _cmp) { return (&m_in)[_cmp]; }
	};
	
	// Move _cp towards _ep such that _x0 < _cp.x < _x1.
	static vec2 Constrain(const vec2& _cp, const vec2& _ep, float _x0, float _x1);

	int   insert(const vec2& _pos, float _tangentScale); 
	int   move(int _endpoint, int _component, const vec2& _pos);
	void  erase(int _i);
	void  updateExtents();
	float wrap(float _t) const;
	int   findSegmentStart(float _t) const;
	void  copyValueAndTangent(const Endpoint& _src, Endpoint& dst_);
	bool  serialize(apt::JsonSerializer& _serializer_);

	ValueBezier();

	eastl::vector<Endpoint> m_endpoints;
	vec2 m_min, m_max, m_range; // includes CPs

}; // class ValueBezier

///////////////////////////////////////////////////////////////////////////////
// ValueCurve
// Runtime animation curve generated from a ValueBezier. This needs to be fast.
// ValueCurve is a flat list of 'endpoints' (EP); each EP is a simple key/value
// pair.
//
// \todo Better subdivision process - it looks like a straight Bezier is 
//    generating too many segments (it's an issue with the error heuristic).
// \todo Linear search faster than binary for a small number of segments? Could
//   be faster in most cases if you supply an index hint.
///////////////////////////////////////////////////////////////////////////////
class ValueCurve
{
	friend class ValueCurveEditor;

public:
	enum Wrap
	{
		Wrap_Clamp  = ValueBezier::Wrap_Clamp,
		Wrap_Repeat = ValueBezier::Wrap_Repeat,

		Wrap_Count,
	};
		
	float sample(float _t) const;

	bool serialize(apt::JsonSerializer& _serializer_);

	const vec2& getMin() const     { return m_min;   }
	const vec2& getMax() const     { return m_max;   }
	const vec2& getRange() const   { return m_range; }

private:
	static const float kDefaultMaxError;

	eastl::vector<vec2> m_endpoints;
	vec2 m_min, m_max, m_range;
	Wrap m_wrap;
#ifdef ValueCurve_ENABLE_EDIT
	ValueBezier m_bezier; // only store the bezier if edit is enabled
#endif
	

	float wrap(float _t) const;
	int findSegmentStart(float _t) const;
	
	// Generate runtime curve from Bezier endpoints. _maxError is expressed as a fraction of the total value range of _bezier. 
	// A low value of _maxError will generate a more accurate fit but will increase the sampling cost.
	void fromBezier(const ValueBezier& _bezier, float _maxError = kDefaultMaxError);

	// Perform up to _limit recursive subdivisions of the Bezier curve p0 -> p1.
	void subdivide(const ValueBezier::Endpoint& p0, const ValueBezier::Endpoint& p1, float _maxError = kDefaultMaxError, int _limit = 12);

}; // class ValueCurve


#ifdef ValueCurve_ENABLE_EDIT

///////////////////////////////////////////////////////////////////////////////
// ValueCurveEditor
///////////////////////////////////////////////////////////////////////////////
class ValueCurveEditor
{
public:

ValueCurve m_curve; // tmp

	ValueCurveEditor();

	void draw(float _t);
	
private:
	vec2 m_regionBeg, m_regionEnd, m_regionSize; // viewing region start/size
	vec2 m_windowBeg, m_windowEnd, m_windowSize;
	int m_selectedEndpoint;
	int m_dragEndpoint;
	int m_dragComponent;	

	float m_gridSpacing; // pixels
	bool  m_showSampler;
	bool  m_showGrid;
	bool  m_showZeroAxis;
	bool  m_showRuler;

	vec2 curveToRegion(const vec2& _pos);
	vec2 curveToWindow(const vec2& _pos);
	vec2 regionToCurve(const vec2& _pos);
	vec2 windowToCurve(const vec2& _pos);
	bool isInside(const vec2& _point, const vec2& _min, const vec2& _max);
	void fit(int _dim);
	void drawBackground();
	void drawGrid();
	void drawRuler();
	void drawZeroAxis();
	void drawSampler(float _t);
};

#endif // ValueCurve_ENABLE_EDIT

} // namespace frm

#endif // frm_ValueCurve_h