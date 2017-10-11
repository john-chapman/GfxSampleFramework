#pragma once
#ifndef frm_Curve_h
#define frm_Curve_h

#include <frm/def.h>
#include <frm/math.h>

#include <EASTL/vector.h>

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
public:
	typedef int Index;
	static const Index kInvalidIndex = -1;

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
	Index insert(const Endpoint& _endpoint);

	// Move the specified component on endpoint by setting its value, return the new index.
	Index move(Index _endpoint, Component _component, const vec2& _value);

	// Erase the specified endpoint.
	void  erase(Index _endpoint);

	// Apply the wrap mode to _t.
	float wrap(float _t) const;

	Endpoint&       operator[](Index _i)       { APT_ASSERT(_i < (Index)m_endpoints.size()); return m_endpoints[_i]; }
	const Endpoint& operator[](Index _i) const { APT_ASSERT(_i < (Index)m_endpoints.size()); return m_endpoints[_i]; }


	enum EditFlags
	{
		EditFlags_None          = 0,
		EditFlags_ShowGrid      = 1 << 0,  // Show the background grid.
		EditFlags_ShowRuler     = 1 << 1,  // Show the edge ruler.
		EditFlags_ShowHighlight = 1 << 2,  // Show curve bounding box highlight.
		EditFlags_NoPan         = 1 << 3,  // Disable pan (middle click).
		EditFlags_NoZoom        = 1 << 4,  // Disable zoom (mouse wheel).

		EditFlags_Default = EditFlags_ShowGrid | EditFlags_ShowRuler | EditFlags_ShowHighlight
	};

	bool edit(const vec2& _sizePixels = vec2(-1.0f), float _t = 0.0f, EditFlags _flags = EditFlags_Default);

private:
	eastl::vector<Endpoint> m_endpoints;
	vec2 m_endpointMin, m_endpointMax; // endpoint bounding box, including CPs
	vec2 m_valueMin, m_valueMax;       // endpoint bounding box, excluding CPs
	Wrap m_wrap;

	Index findSegmentStart(float _t) const;
	void  updateExtentsAndConstrain(Index _modified); // applies additional constraints, e.g. synchronize endpoints if Wrap_Repeat
	void  copyValueAndTangent(const Endpoint& _src, Endpoint& dst_);

 // editor
	vec2   m_windowBeg, m_windowEnd, m_windowSize;
	vec2   m_regionBeg, m_regionEnd, m_regionSize;
	Index  m_selectedEndpoint;
	Index  m_dragEndpoint;
	int    m_dragComponent;
	vec2   m_dragOffset;
	bool   m_editEndpoint;
	bool   m_isDragging;
	uint32 m_editFlags;

	bool checkEditFlag(EditFlags _flag) { return (m_editFlags & _flag) != 0; }
	bool isInside(const vec2& _point, const vec2& _min, const vec2& _max);
	vec2 curveToRegion(const vec2& _pos);
	vec2 curveToWindow(const vec2& _pos);
	vec2 regionToCurve(const vec2& _pos);
	vec2 windowToCurve(const vec2& _pos);
	bool editCurve();
	void drawBackground();
	void drawGrid();
	void drawCurve();
	void drawSampler(float _t);
	void drawRuler();

 // piecewise linear approx (cached for draw)
	eastl::vector<vec2> m_cache;

	void updateCache();
	void subdivide(const Endpoint& _p0, const Endpoint& _p1, float _maxError = 0.001f, int _limit = 64);

}; // class Curve

} // namespace frm

#endif // frm_Curve_h
